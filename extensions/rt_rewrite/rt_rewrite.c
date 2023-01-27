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
		if ((new=store_new()) == NULL) {
			return -1;
		}
		new->avp = avp;
		new->target = target;
		new->next = store->next;
		store->next = new;
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
static void add_avps(msg_or_avp *msg, struct store *store) {
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
			set_target_to_avp_value(&next->value, next->target, store);
		}
		next = next->next;
	}
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
static int replace_avps(struct avp *avp, struct avp_match *subtree, struct store *store)
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
				/* if this should be deleted, mark as such */
				if (subtree_match->match_type == REWRITE_DROP) {
					fd_log_debug("%s: dropping AVP '%s'", MODULE_NAME, avp_name);
					delete = 1;
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
					if (replace_avps(child, subtree_match->children, store)) {
						fd_log_debug("%s: removing empty grouped AVP '%s'", MODULE_NAME, avp_name);
						delete = 1;
					}
				}
				else {
					/* if single, remove it and add replacement AVP in target structure */
					if (subtree_match->target) {
						struct avp_target *final = subtree_match->target;
						while (final->child) {
							final = final->child;
						}
						fd_log_debug("%s: moving AVP '%s' to '%s'", MODULE_NAME, avp_name, final->name);
						set_target_to_avp_value(header->avp_value, subtree_match->target, store);
						delete = 1;
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

	if (rt_rewrite_conf_handle(config_file) != 0) {
		fd_log_notice("%s: error reloading configuration, restoring previous configuration", MODULE_NAME);
		avp_match_free(avp_match_start);
		avp_match_start = old_match;
		avp_add_free(avp_add_start);
		avp_add_start = old_list;
	} else {
		avp_match_free(old_match);
		avp_add_free(old_list);
	}

	if (pthread_rwlock_unlock(&rt_rewrite_lock) != 0) {
		fd_log_error("%s: unlocking failed after config reload, exiting", MODULE_NAME);
		exit(1);
	}

	fd_log_error("%s: reloaded configuration", MODULE_NAME);

	in_signal_handler = 0;
}

static int rt_rewrite(void * cbdata, struct msg **msg)
{
	int ret;
	msg_or_avp *avp = NULL;
	struct store *store = NULL;

	/* nothing configured */
	if (avp_match_start == NULL) {
		return 0;
	}

	if (pthread_rwlock_wrlock(&rt_rewrite_lock) != 0) {
		fd_log_error("%s: locking failed, aborting message rewrite", MODULE_NAME);
		return errno;
	}

	if ((ret = fd_msg_parse_dict(*msg, fd_g_config->cnf_dict, NULL)) != 0) {
		fd_log_notice("%s: error parsing message", MODULE_NAME);
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return ret;
	}
	if ((ret=fd_msg_browse(*msg, MSG_BRW_FIRST_CHILD, &avp, NULL)) != 0) {
		fd_log_notice("internal error: message has no child");
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return ret;
	}
	if ((store=store_new()) == NULL) {
		fd_log_error("%s: malloc failure");
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return ENOMEM;
	}
	if (replace_avps(avp, avp_match_start->children, store) != 0) {
		fd_log_error("%s: replace AVP function failed", MODULE_NAME);
		store_free(store);
		pthread_rwlock_unlock(&rt_rewrite_lock);
		return -1;
	}
	add_avps(*msg, store);
	ret = store_apply(*msg, &store);

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

	fd_log_error("Extension 'Rewrite' initialized");
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
