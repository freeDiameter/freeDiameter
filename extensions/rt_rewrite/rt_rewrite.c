/*********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2018, 2023 Thomas Klausner                                                               *
 * All rights reserved.                                                                                   *
 *                                                                                                        *
 * Written under contract by Effortel Technologies SA, http://effortel.com/                               *
 *                                                                                                        *
 * Redistribution and use of this software in source and binary forms, with or without modification, are  *
 * permitted provided that the following conditions are met:                                              *
 *                                                                                                        *
 * * Redistributions of source code must retain the above                                                 *
 *   copyright notice, this list of conditions and the                                                    *
 *   following disclaimer.                                                                                *
 *                                                                                                        *
 * * Redistributions in binary form must reproduce the above                                              *
 *   copyright notice, this list of conditions and the                                                    *
 *   following disclaimer in the documentation and/or other                                               *
 *   materials provided with the distribution.                                                            *
 *                                                                                                        *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT     *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
 *********************************************************************************************************/

#include <freeDiameter/extension.h>
#include "rt_rewrite.h"

#include <pthread.h>
#include <signal.h>

/*
 * Replace AVPs: put their values into other AVPs
 * Remove AVPs: drop them from a message
 * Add AVPs: add fixed-value AVPs to messages of particular type
 */

/* handler */
static struct fd_rt_fwd_hdl * rt_rewrite_handle = NULL;

static char *config_file;

/* structure containing information about data to replace */
struct avp_match *avp_match_start = NULL;

/* structure containing information about AVPs to be added */
struct avp_add *avp_add_start = NULL;

/* structure containing information about variables used in conditions in the config file */
struct avp_match *avp_variable_start = NULL;

/* how many variables there are */
size_t avp_variable_count = 0;

static pthread_rwlock_t rt_rewrite_lock;

#define MODULE_NAME "rt_rewrite"

/*
 * list of data from message that should be added to it after all
 * matches are identified, to avoid replacement loops or similar
 * issues.
 */
struct store {
	/* AVP to add */
	struct avp *avp;
	/* path to AVP container where it should be added, not owned -
	 * memory belong to config structure */
	struct avp_target *target;
	/* pointer to next store item */
	struct store *next;
};

/* for freeing an avp_value, we need to know its basetype, so we need a structure to keep it */
struct variable_value {
	union avp_value value;
	enum dict_avp_basetype basetype;
	int valid;
};

static int evaluate_condition(struct avp_condition *condition, struct variable_value *values);

/* create new store item */
static struct store *store_new(void) {
	struct store *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("%s: malloc failure", MODULE_NAME);
		return NULL;
	}
	ret->avp = NULL;
	ret->target = NULL;
	ret->next = NULL;

	return ret;
}

/* free store item */
static void store_free(struct store *store)
{
	while (store) {
		struct store *next;
		if (store->avp) {
			fd_msg_free((msg_or_avp *)store->avp);
		}
		/* target is not owned by us */
		store->target = NULL;
		next = store->next;
		free(store);
		store = next;
	}
	return;
}

/* TODO: convert to fd_msg_search_avp ? */
/* search in message 'where' for AVP 'what' and return result in 'avp'
 * returns 0 on success, -1 otherwise. */
static int fd_avp_search_avp(msg_or_avp *where, struct dict_object *what, struct avp **avp)
{
	struct avp *nextavp;
	struct dict_avp_data dictdata;
	enum dict_object_type dicttype;

	CHECK_PARAMS((fd_dict_gettype(what, &dicttype) == 0) && (dicttype == DICT_AVP));
	CHECK_FCT(fd_dict_getval(what, &dictdata));

	/* Loop on all top AVPs */
	CHECK_FCT(fd_msg_browse(where, MSG_BRW_FIRST_CHILD, (void *)&nextavp, NULL));
	while (nextavp) {
		struct avp_hdr *header = NULL;
		struct dict_object *model = NULL;
		if (fd_msg_avp_hdr(nextavp, &header) != 0) {
			fd_log_notice("%s: unable to get header for AVP", MODULE_NAME);
			return -1;
		}
		if (fd_msg_model(nextavp, &model) != 0) {
			fd_log_notice("%s: unable to get model for AVP (%d, vendor %d)", MODULE_NAME, header->avp_code, header->avp_vendor);
			return 0;
		}
		if (model == NULL) {
			fd_log_error("%s: unknown AVP (%d, vendor %d) (not in dictionary), skipping", MODULE_NAME, header->avp_code, header->avp_vendor);
		} else {
			if ((header->avp_code == dictdata.avp_code) && (header->avp_vendor == dictdata.avp_vendor)) {
				break;
			}
		}

		/* Otherwise move to next AVP in the message */
		CHECK_FCT(fd_msg_browse(nextavp, MSG_BRW_NEXT, (void *)&nextavp, NULL) );
	}

	if (avp)
		*avp = nextavp;

	if (nextavp) {
		return 0;
	} else {
		return ENOENT;
	}
}


/* in message 'msg', go to the location specified by 'target' and
 * return the last grouped AVP above it. Create any missing grouped
 * AVPs on the way down. */
static struct avp *find_container(msg_or_avp *msg, struct avp_target *target)
{
	msg_or_avp *location = msg;
	while (target->child) {
		struct dict_object *avp_do;
		struct avp *new_location = NULL;
		int ret;

		fd_log_debug("%s: looking for '%s'", MODULE_NAME, target->name);
		if ((ret=fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, target->name, &avp_do, ENOENT)) != 0) {
			fd_log_error("%s: target AVP '%s' not in dictionary: %s", MODULE_NAME, target->name, strerror(ret));
			return NULL;
		}
		if ((ret=fd_avp_search_avp(location, avp_do, (struct avp **)&new_location)) != 0) {
			fd_log_debug("%s: did not find '%s', adding it", MODULE_NAME, target->name);
			struct avp *avp = NULL;
			if ((ret = fd_msg_avp_new(avp_do, 0, &avp)) != 0) {
				fd_log_notice("%s: unable to create new '%s' AVP", MODULE_NAME, target->name);
				return NULL;
			}
			if ((ret=fd_msg_avp_add(location, MSG_BRW_LAST_CHILD, avp)) != 0) {
				/* TODO: free avp? */
				fd_log_error("%s: cannot add AVP '%s' to message: %s", MODULE_NAME, target->name, strerror(ret));
				return NULL;
			}
			fd_log_debug("%s: '%s' added", MODULE_NAME, target->name);
			/* now find it in the new place */
			continue;
		} else {
			location = new_location;
			fd_log_debug("%s: found '%s'", MODULE_NAME, target->name);
		}
		target = target->child;
	}
	fd_log_debug("%s: returning AVP for '%s'", MODULE_NAME, target->name);
	return location;
}

/* apply 'store' to message 'msg' - find the appropriate location and
 * add the AVP */
static int store_apply(msg_or_avp *msg, struct store **store)
{
	while (*store) {
		struct avp *container;
		struct store *next;
		if ((*store)->avp) {
			int ret;
			if ((container=find_container(msg, (*store)->target)) == NULL) {
				fd_log_error("%s: internal error, container not found", MODULE_NAME);
				return -1;
			}

			if ((ret=fd_msg_avp_add(container, MSG_BRW_LAST_CHILD, (*store)->avp)) != 0) {
				fd_log_error("%s: cannot add AVP '%s' to message: %s", MODULE_NAME, (*store)->target->name, strerror(ret));
				return -1;
			}
		}
		next = (*store)->next;
		free(*store);
		*store = next;
	}
	return 0;
}

/* add 'avp' as data for 'target' into the 'store' to later add it */
static int schedule_for_adding(struct avp *avp, struct avp_target *target, struct store *store)
{
	if (store->avp) {
		struct store *new;
		struct store *last;
		if ((new=store_new()) == NULL) {
			return -1;
		}
		new->avp = avp;
		new->target = target;
		new->next = NULL;
		last = store;
		while (last->next) {
			last = last->next;
		}
		last->next = new;
	} else {
		store->avp = avp;
		store->target = target;
	}
	fd_log_debug("%s: noted %s for later adding", MODULE_NAME, target->name);
	return 0;
}

/* create AVP for 'target' with value 'avp_value' and add it to the 'store' to later add it */
static void set_target_to_avp_value(union avp_value *avp_value, struct avp_target *target, struct store *store)
{
	struct dict_object *avp_do;
	struct avp *avp;
	int ret;
	struct avp_target *final = target;

	while (final->child) {
		final = final->child;
	}
	if ((ret=fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, final->name, &avp_do, ENOENT)) != 0) {
		fd_log_error("internal error, target AVP '%s' not in dictionary: %s", final->name, strerror(ret));
		return;
	}
	if ((ret=fd_msg_avp_new(avp_do, 0, &avp)) != 0) {
		fd_log_error("internal error, error creating structure for AVP '%s': %s", final->name, strerror(ret));
		return;
	}
	if ((ret=fd_msg_avp_setvalue(avp, avp_value)) != 0) {
		fd_log_error("internal error, cannot set value for AVP '%s': %s", final->name, strerror(ret));
		return;
	}
	if (schedule_for_adding(avp, target, store) != 0) {
		fd_log_error("internal error, cannot add AVP '%s' to message", final->name);
		return;
	}

	return;
}

/*
 * Apply avp_add_list for 'msg', add changes to 'store'.
 */
static void add_avps(msg_or_avp *msg, struct store *store, struct variable_value *values) {
	struct avp_add *next = avp_add_start;
	struct msg_hdr *hdr;

	/* Read the message header */
	if (fd_msg_hdr(msg, &hdr) != 0) {
		return;
	}
	while (next) {
		/*
		struct dict_object *request_do;
		struct dict_cmd_data request_data;
		if ((fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, next->request_type, &request_do, ENOENT) == 0)
		    && (fd_dict_getval(request_do, &request_data) == 0)
		*/
		if ((hdr->msg_code == next->request.cmd_code)
		    && ((hdr->msg_flags & next->request.cmd_flag_mask) == next->request.cmd_flag_val)) {
			if (evaluate_condition(next->condition, values)) {
				set_target_to_avp_value(&next->value, next->target, store);
			}
		}
		next = next->next;
	}
}

#define COMPARE_VARIABLE_TO_CONDITION(a, cmp, b) { switch(cmp) { \
	case LESS: \
		return (a) < (b); \
	case LESS_EQUAL: \
		return (a) <= (b); \
	case EQUAL: \
		return (a) == (b); \
	case MORE_EQUAL: \
		return (a) >= (b); \
	case MORE: \
		return (a) > (b); \
	} \
}

/*
 * look in 'subtree' at the current level for an AVP 'avp_name';
 * return corresponding subtree, or NULL if none
 */
static struct avp_match *avp_to_be_replaced(const char *avp_name, struct avp_match *subtree)
{
	struct avp_match *ret;
	for (ret=subtree; ret != NULL; ret=ret->next) {
		if (strcmp(ret->name, avp_name) == 0) {
			return ret;
		}
	}
	return NULL;
}

/* copy an avp_value into a variable_value */
static int avp_value_copy(enum dict_avp_basetype basetype, union avp_value *source, struct variable_value *target) {
	/* octetstrings need to be duplicated, rest can be copied as-is */
	if (basetype == AVP_TYPE_OCTETSTRING) {
		uint8_t *osstr;
		if ((osstr=malloc(source->os.len)) == NULL) {
			fd_log_error("%s: malloc error", MODULE_NAME);
			return -1;
		}
		memcpy(osstr, source->os.data, source->os.len);
		target->value.os.data = osstr;
		target->value.os.len = source->os.len;
	} else {
		memcpy(&target->value, source, sizeof(*source));
	}
	target->basetype = basetype;
	target->valid = 1;

	return 0;
}

/*
 * evaluate if a condition is true or not given the provided variables
 *
 * 'condition' is the condition, 'values' are the variables' values
 *
 * returns 1 if true, 0 if not, -1 on error
 */
static int evaluate_condition(struct avp_condition *condition, struct variable_value *values) {
	union avp_value *variable_value;

	/* if there is no condition, it is true */
	if (condition == NULL) {
		return 1;
	}

	/* if variable is not valid, comparison can't be true */
	if (!values[condition->variable_index].valid) {
		return 0;
	}

	variable_value = &values[condition->variable_index].value;
	switch (condition->basetype) {
	case AVP_TYPE_OCTETSTRING:
		if (condition->comparator == EQUAL) {
			if (variable_value->os.len == condition->value.os.len
			    && memcmp(variable_value->os.data, condition->value.os.data, condition->value.os.len) == 0) {
				return 1;
			} else {
				return 0;
			}
		}
		return -1;
	case AVP_TYPE_INTEGER32:
		COMPARE_VARIABLE_TO_CONDITION(variable_value->i32, condition->comparator, condition->value.i32);
		break;
	case AVP_TYPE_INTEGER64:
		COMPARE_VARIABLE_TO_CONDITION(variable_value->i64, condition->comparator, condition->value.i64);
		break;
	case AVP_TYPE_UNSIGNED32:
		COMPARE_VARIABLE_TO_CONDITION(variable_value->u32, condition->comparator, condition->value.u32);
		break;
	case AVP_TYPE_UNSIGNED64:
		COMPARE_VARIABLE_TO_CONDITION(variable_value->u64, condition->comparator, condition->value.u64);
		break;
	case AVP_TYPE_FLOAT32:
	case AVP_TYPE_FLOAT64:
	case AVP_TYPE_GROUPED:
		/* unsupported */
		return -1;
	}
	/* unreachable, but some compilers don't recognize this */
	return -1;
}

/*
 * recursively called function to evaluate the variables for the message
 *
 * 'avp' is the next avp to look at (usually the first one in the
 * current grouped AVP or message)
 *
 * 'subtree' is the part of the avp_match structure for variables corresponding to
 * the current subtree
 *
 * 'values' contains the array of values found so far
 */
static int evaluate_variables(struct avp *avp, struct avp_match *subtree, struct variable_value *values)
{
	/* for each AVP in message */
	while (avp) {
		struct avp_hdr *header = NULL;
		struct dict_object *model = NULL;
		const char *avp_name = NULL;
		struct avp *next;

		if (fd_msg_avp_hdr(avp, &header) != 0) {
			fd_log_notice("internal error: unable to get header for AVP");
			return -1;
		}
		if (fd_msg_model(avp, &model) != 0) {
			fd_log_notice("internal error: unable to get model for AVP (%d, vendor %d)", header->avp_code, header->avp_vendor);
			return -1;
		}
		if (model == NULL) {
			fd_log_error("unknown AVP (%d, vendor %d) (not in dictionary), skipping", header->avp_code, header->avp_vendor);
			return -1;
		} else {
			struct dict_avp_data dictdata;
			struct avp_match *subtree_match;
			enum dict_avp_basetype basetype = AVP_TYPE_OCTETSTRING;

			fd_dict_getval(model, &dictdata);
			avp_name = dictdata.avp_name;
			basetype = dictdata.avp_basetype;

			/* check if it exists in the subtree */
			if ((subtree_match = avp_to_be_replaced(avp_name, subtree))) {
				/* if grouped, dive down */
				if (basetype == AVP_TYPE_GROUPED) {
					msg_or_avp *child = NULL;

					fd_log_debug("%s: grouped AVP '%s'", MODULE_NAME, avp_name);
					if (fd_msg_browse(avp, MSG_BRW_FIRST_CHILD, &child, NULL) != 0) {
						fd_log_notice("internal error: unable to browse message at AVP (%d, vendor %d)", header->avp_code, header->avp_vendor);
						return -1;
					}

					if (evaluate_variables(child, subtree_match->children, values) < 0) {
						return -1;
					}
				}
				else {
					/* keep value if it's a variable */
					if (subtree_match->variable_index >= 0) {
						avp_value_copy(basetype, header->avp_value, &values[subtree_match->variable_index]);
					}
				}
			}
		}
		fd_msg_browse(avp, MSG_BRW_NEXT, &next, NULL);

		avp = next;
	}
	return 0;
}

/*
 * recursively called function to apply the replacements to the whole message
 *
 * 'avp' is the next avp to look at (usually the first one in the
 * current grouped AVP or message)
 *
 * 'subtree' is the part of the avp_match structure corresponding to
 * the current subtree
 *
 * 'store' is an output parameter containing the changes to apply
 * after the recursion has finished
 *
 * returns 1 if AVP will be empty due to 'DROP's and 'MAP's, 0
 * otherwise.
 */
static int replace_avps(struct avp *avp, struct avp_match *subtree, struct store *store, struct variable_value *values)
{
	int nothing_left = 1;
	/* for each AVP in message */
	while (avp) {
		struct avp_hdr *header = NULL;
		struct dict_object *model = NULL;
		const char *avp_name = NULL;
		struct avp *next;
		int delete = 0;

		if (fd_msg_avp_hdr(avp, &header) != 0) {
			fd_log_notice("internal error: unable to get header for AVP");
			return 0;
		}
		if (fd_msg_model(avp, &model) != 0) {
			fd_log_notice("internal error: unable to get model for AVP (%d, vendor %d)", header->avp_code, header->avp_vendor);
			return 0;
		}
		if (model == NULL) {
			fd_log_error("unknown AVP (%d, vendor %d) (not in dictionary), skipping", header->avp_code, header->avp_vendor);

		} else {
			struct dict_avp_data dictdata;
			struct avp_match *subtree_match;
			enum dict_avp_basetype basetype = AVP_TYPE_OCTETSTRING;

			fd_dict_getval(model, &dictdata);
			avp_name = dictdata.avp_name;
			basetype = dictdata.avp_basetype;
			/* check if it exists in the subtree */
			if ((subtree_match = avp_to_be_replaced(avp_name, subtree))) {
				struct avp_action *action;
				for (action = subtree_match->actions; action != NULL; action = action->next) {
					/* if this should be deleted, mark as such */
					if (action->match_type == REWRITE_DROP && evaluate_condition(action->condition, values)) {
						fd_log_debug("%s: dropping AVP '%s'", MODULE_NAME, avp_name);
						delete = 1;
					}
				}
				/* if grouped, dive down */
				if (basetype == AVP_TYPE_GROUPED) {
					msg_or_avp *child = NULL;

					fd_log_debug("%s: grouped AVP '%s'", MODULE_NAME, avp_name);
					if (fd_msg_browse(avp, MSG_BRW_FIRST_CHILD, &child, NULL) != 0) {
						fd_log_notice("internal error: unable to browse message at AVP (%d, vendor %d)", header->avp_code, header->avp_vendor);
						return 0;
					}

					/* replace_avps returns 1 if the AVP was emptied out */
					if (replace_avps(child, subtree_match->children, store, values)) {
						fd_log_debug("%s: removing empty grouped AVP '%s'", MODULE_NAME, avp_name);
						delete = 1;
					}
				}
				else {
					for (action = subtree_match->actions; action != NULL; action = action->next) {
						/* if single, remove it and add replacement AVP in target structure */
						if (action->target && evaluate_condition(action->condition, values)) {
							struct avp_target *final = action->target;
							while (final->child) {
								final = final->child;
							}
							fd_log_debug("%s: moving AVP '%s' to '%s'", MODULE_NAME, avp_name, final->name);
							set_target_to_avp_value(header->avp_value, action->target, store);
							delete = 1;
						}
					}
				}
			}
		}
		fd_msg_browse(avp, MSG_BRW_NEXT, &next, NULL);
		if (delete) {
			/* remove AVP from message */
			fd_msg_free((msg_or_avp *)avp);
		} else {
		    nothing_left = 0;
		}

		avp = next;
	}

	return nothing_left;
}

static volatile int in_signal_handler = 0;

/* signal handler */
static void sig_hdlr(void)
{
	struct avp_match *old_match;
	struct avp_add *old_list;
	struct avp_match *old_variables;
	size_t old_variable_count;

	if (in_signal_handler) {
		fd_log_error("%s: already handling a signal, ignoring new one", MODULE_NAME);
		return;
	}
	in_signal_handler = 1;

	if (pthread_rwlock_wrlock(&rt_rewrite_lock) != 0) {
		fd_log_error("%s: locking failed, aborting config reload", MODULE_NAME);
		return;
	}

	/* save old config in case reload goes wrong */
	old_match = avp_match_start;
	avp_match_start = NULL;
	old_list = avp_add_start;
	avp_add_start = NULL;
	old_variables = avp_variable_start;
	avp_variable_start = NULL;
	old_variable_count = avp_variable_count;
	avp_variable_count = 0;

	if (rt_rewrite_conf_handle(config_file) != 0) {
		fd_log_notice("%s: error reloading configuration, restoring previous configuration", MODULE_NAME);
		avp_match_free(avp_match_start);
		avp_match_start = old_match;
		avp_add_free(avp_add_start);
		avp_add_start = old_list;
		avp_match_free(avp_variable_start);
		avp_variable_start = old_variables;
		avp_variable_count = old_variable_count;
	} else {
		avp_match_free(old_match);
		avp_add_free(old_list);
		avp_match_free(old_variables);
	}

	if (pthread_rwlock_unlock(&rt_rewrite_lock) != 0) {
		fd_log_error("%s: unlocking failed after config reload, exiting", MODULE_NAME);
		exit(1);
	}

	fd_log_error("%s: reloaded configuration", MODULE_NAME);

	in_signal_handler = 0;
}

static struct variable_value *variable_store_new(void) {
	struct variable_value *values;

	if ((values=calloc(avp_variable_count, sizeof(*values))) == NULL) {
		fd_log_error("%s: malloc error", MODULE_NAME);
		return NULL;
	}
	for (size_t i = 0; i < avp_variable_count; i++) {
		values[i].valid = 0;
	}
	return values;
}

static void variable_store_free(struct variable_value *values) {
	for (size_t i = 0; i < avp_variable_count; i++) {
		if (values[i].valid == 0) {
			continue;
		}
		if (values[i].basetype == AVP_TYPE_OCTETSTRING) {
			free(values[i].value.os.data);
		}
	}

	free(values);
}

static int rt_rewrite(void * cbdata, struct msg **msg)
{
	int ret;
	msg_or_avp *avp = NULL;
	struct store *store = NULL;
	struct variable_value *values = NULL;
	struct fd_pei error_info;

	/* nothing configured */
	if (avp_match_start == NULL) {
		return 0;
	}

	if (pthread_rwlock_wrlock(&rt_rewrite_lock) != 0) {
		fd_log_error("%s: locking failed, aborting message rewrite", MODULE_NAME);
		return errno;
	}

	if ((ret=fd_msg_parse_dict(*msg, fd_g_config->cnf_dict, &error_info)) != 0) {
		fd_log_notice("%s: error parsing message", MODULE_NAME);
		pthread_rwlock_unlock(&rt_rewrite_lock);
                if (error_info.pei_errcode) {
			if (fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg,
                                                       MSGFL_ANSW_ERROR) != 0 ||
			    fd_msg_rescode_set(*msg, error_info.pei_errcode,
                                               error_info.pei_message,
                                               error_info.pei_avp, 2) != 0
			    || fd_msg_send(msg, NULL, NULL) != 0) {
				fd_log_error("%s: error creating error reply message",
					     MODULE_NAME);
			}
                }
                /* message has been handled, error was generated, stop processing */
                *msg = NULL;
		return 0;
	}

	/* evaluate variables */
	if ((ret=fd_msg_browse(*msg, MSG_BRW_FIRST_CHILD, &avp, NULL)) != 0) {
		fd_log_notice("internal error: message has no child");
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return ret;
	}
	if ((values=variable_store_new()) == NULL) {
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return -1;
	}
	if (evaluate_variables(avp, avp_variable_start->children, values) < 0) {
		fd_log_error("%s: error evaluating variables in message", MODULE_NAME);
		variable_store_free(values);
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return -1;
	}
	/* variable_store_dump(values); */
	/* actual message modifications */
	if ((ret=fd_msg_browse(*msg, MSG_BRW_FIRST_CHILD, &avp, NULL)) != 0) {
		fd_log_notice("internal error: message has no child");
		pthread_rwlock_unlock(&rt_rewrite_lock);
		variable_store_free(values);
		return ret;
	}
	if ((store=store_new()) == NULL) {
		fd_log_error("%s: malloc failure");
		variable_store_free(values);
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return ENOMEM;
	}
	if (replace_avps(avp, avp_match_start->children, store, values) != 0) {
		fd_log_error("%s: replace AVP function failed", MODULE_NAME);
		store_free(store);
		variable_store_free(values);
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return -1;
	}
	add_avps(*msg, store, values);
	ret = store_apply(*msg, &store);
	variable_store_free(values);

	if (pthread_rwlock_unlock(&rt_rewrite_lock) != 0) {
		fd_log_error("%s: unlocking failed, returning error", MODULE_NAME);
		return errno;
	}

	return ret;
}

/* entry point */
static int rt_rewrite_entry(char * conffile)
{
	int ret;
	config_file = conffile;

	pthread_rwlock_init(&rt_rewrite_lock, NULL);

	if (pthread_rwlock_wrlock(&rt_rewrite_lock) != 0) {
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Parse the configuration file */
	if ((ret=rt_rewrite_conf_handle(config_file)) != 0) {
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return ret;
	}

	if (pthread_rwlock_unlock(&rt_rewrite_lock) != 0) {
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Register reload callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, MODULE_NAME, sig_hdlr));

	/* Register the callback */
	if ((ret=fd_rt_fwd_register(rt_rewrite, NULL, RT_FWD_ALL, &rt_rewrite_handle)) != 0) {
		fd_log_error("Cannot register callback handler");
		return ret;
	}

	fd_log_notice("Extension 'Rewrite' initialized");
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	/* Unregister the callbacks */
	fd_rt_fwd_unregister(rt_rewrite_handle, NULL);
	return ;
}

EXTENSION_ENTRY("rt_rewrite", rt_rewrite_entry);
