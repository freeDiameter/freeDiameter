/**********************************************************************************************************
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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#include "rt_rewrite_conf_support.h"

/* Forward declaration, comes from parser */
int yyparse(char * conffile);

/* in libfdproto/dictionary.c and internal headers */
extern const char * type_base_name[];

/* work-in-progress source for MAP/DROP */
static struct avp_match *source_target = NULL;
/* work-in-progress target for MAP */
static struct avp_target *dest_target = NULL;
/* work-in-progress target for ADD */
static struct avp_add *add_target = NULL;

/* name for work-in-progress variable */



static char *variable_name = NULL;

struct variable_data_type {
	char *name;
	enum dict_avp_basetype basetype;
};

/* list of variables found so far */
static struct variable_data_type *variable_data = NULL;

/* work-in-progress target for VARIABLE */
static struct avp_match *variable_target = NULL;


struct avp_condition *condition_target = NULL;

static char *dump_avp_value(union avp_value *value, enum dict_avp_basetype basetype);
static char *dump_condition(struct avp_condition *condition);

static char *full_target_name(struct avp_target *target)
{
	char *output;
	if (target == NULL) {
		return NULL;
	}
	if ((output=strdup(target->name)) == NULL) {
		fd_log_error("rt_rewrite: full_target_name: strdup failed: %s", strerror(errno));
		return NULL;
	}
	for (target=target->child; target != NULL; target=target->child) {
		char *new_output = NULL;
		if (asprintf(&new_output, "%s/%s", output, target->name) == -1) {
			fd_log_error("rt_rewrite: full_target_name: asprintf failed: %s", strerror(errno));
			free(output);
			return NULL;
		}
		free(output);
		output = new_output;
	}
	return output;
}

/* add 'target' to the end of string 'prefix'. return new string. Does NOT take ownership of 'prefix' on success. */
static char *add_target_to(struct avp_target *target, const char *prefix)
{
	char *output = NULL;
	char *target_string;
	if ((target_string=full_target_name(target)) == NULL) {
		return NULL;
	}
	if (asprintf(&output, "%s%s/%s", prefix ? prefix : "", prefix ? " -> " : "", target_string) == -1) {
		free(target_string);
		fd_log_error("rt_rewrite: add_target_to: setup: asprintf failed: %s", strerror(errno));
		return NULL;
	}
	free(target_string);
	return output;
}

/*
 * Compare types of AVPs 'source' and 'dest' for compatibility.
 * Mappings are only allowed between AVPs of the same basetype.
 */
static void compare_avp_type(const char *source, const char *dest)
{
	struct dict_object *model_source, *model_dest;
	struct dict_avp_data dictdata_source, dictdata_dest;

	if (fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, source, &model_source, ENOENT) != 0) {
		fd_log_error("Unable to find '%s' AVP in the loaded dictionaries", source);
		return;
	}
	if (fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, dest, &model_dest, ENOENT) != 0) {
		fd_log_error("Unable to find '%s' AVP in the loaded dictionaries", dest);
		return;
	}
	fd_dict_getval(model_source, &dictdata_source);
	fd_dict_getval(model_dest, &dictdata_dest);
	if (dictdata_source.avp_basetype != dictdata_dest.avp_basetype) {
		if (dictdata_source.avp_basetype == AVP_TYPE_OCTETSTRING) {
			fd_log_error("rt_rewrite: type mismatch: %s (type %s) mapped to %s (type %s): OctetString cannot be mapped to non-OctetString type", source, type_base_name[dictdata_source.avp_basetype], dest, type_base_name[dictdata_dest.avp_basetype]);
			return;
		}
		fd_log_error("rt_rewrite: type mismatch: %s (type %s) mapped to %s (type %s) (continuing anyway)", source, type_base_name[dictdata_source.avp_basetype], dest, type_base_name[dictdata_dest.avp_basetype]);
	}
	return;
}

/* Recursively check all AVPs used in mappings for compatibility. */
void compare_avp_types(struct avp_match *start)
{
	struct avp_match *iter;
	for (iter=start; iter != NULL; iter=iter->next) {
		struct avp_action *action;
		compare_avp_types(iter->children);

		action = iter->actions;
		while (action) {
			/* only for MAP */
			if (action->target) {
				struct avp_target *final;
				final = action->target;
				while (final->child) {
					final = final->child;
				}
				compare_avp_type(iter->name, final->name);
			}
			action = action->next;
		}
	}
	return;
}


/* Print "ADD" part of configuration, so users can verify the config from the logs. */
void dump_add_config(struct avp_add *start)
{
	struct avp_add *iter;
	for (iter=start; iter; iter=iter->next) {
		char *result = NULL;
		char *value = NULL;
		char *condition_str = NULL;

		if ((result=add_target_to(iter->target, NULL)) == NULL) {
			return;
		}
		if ((value=dump_avp_value(&iter->value, iter->basetype)) == NULL) {
			free(result);
			return;
		}
		condition_str = dump_condition(iter->condition);
		fd_log_notice("ADD (for %s): %s = %s%s", iter->request.cmd_name, result, value, condition_str ? condition_str : "");
		free(condition_str);
		free(value);
		free(result);
	}
}

/* return a static string for printing 'comparator' */
static const char *dump_comparator(enum comparators comparator) {
	switch (comparator) {
	case LESS:
		return "<";
	case LESS_EQUAL:
		return "<=";
	case EQUAL:
		return "=";
	case MORE_EQUAL:
		return ">=";
	case MORE:
		return ">";
	}
	/* can't happen, but some compilers don't recognize this */
	return "";
}

/* return string describing 'condition'. must be free()d by caller */
static char *dump_condition(struct avp_condition *condition) {
	char *result = NULL;
	char *value = NULL;

	/* no use if no variable name or no condition */
	if (variable_data == NULL || condition == NULL) {
		return NULL;
	}
	if ((value=dump_avp_value(&condition->value, condition->basetype)) == NULL) {
		return NULL;
	}
	if (asprintf(&result, " IF %s %s %s", variable_data[condition->variable_index].name,
		     dump_comparator(condition->comparator), value) == -1) {
		fd_log_error("asprintf failed");
		free(value);
		return NULL;
	}
	free(value);
	return result;
}

/* Print out configuration, so users can verify the config from the logs. */
void dump_config(struct avp_match *start, const char *prefix)
{
	char *new_prefix = NULL;
	struct avp_match *iter;

	for (iter=start; iter != NULL; iter=iter->next) {
		struct avp_action *action;
		int is_top = strcmp(iter->name, "TOP") == 0;
		if (asprintf(&new_prefix,"%s%s%s", prefix, is_top ? "" : "/", is_top ? "" : iter->name) == -1) {
			fd_log_error("rt_rewrite: dump_config: asprintf failed: %s", strerror(errno));
			return;
		}
		dump_config(iter->children, new_prefix);
		action = iter->actions;
		while (action) {
			if (action->target) {
				char *condition_str = NULL;
				char *result = add_target_to(action->target, new_prefix);
				if (result == NULL) {
					return;
				}
				condition_str = dump_condition(action->condition);
				fd_log_notice("MAP %s%s", result, condition_str ? condition_str : "");
				free(condition_str);
				free(result);
			}
			if (action->match_type == REWRITE_DROP) {
				char *condition_str = dump_condition(action->condition);
				fd_log_notice("DROP %s%s", new_prefix, condition_str ? condition_str : "");
				free(condition_str);
			}
			action = action->next;
		}
		if (iter->variable_index >= 0) {
			if (variable_data) {
				fd_log_notice("VARIABLE %s from %s", variable_data[iter->variable_index].name, new_prefix);
			} else {
				fd_log_notice("VARIABLE from %s", new_prefix);
			}
		}
		free(new_prefix);
		new_prefix = NULL;
	}
	return;
}

/* Check if AVP exists; return 1 if grouped, 0 if other AVP, and -1 if it doesn't exist */
static int verify_avp(const char *name)
{
	struct dict_object *model;
	struct dict_avp_data dictdata;

	if (fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, name, &model, ENOENT) != 0) {
		fd_log_error("Unable to find '%s' AVP in the loaded dictionaries", name);
		return -1;
	}
	fd_dict_getval(model, &dictdata);
	if (dictdata.avp_basetype == AVP_TYPE_GROUPED) {
		return 1;
	}
	return 0;
}

/* Create new avp target structure with name 'name'. Taken ownership of 'name'. */
static struct avp_target *avp_target_new(const char *name) {
	struct avp_target *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	if ((ret->name=strdup(name)) == NULL) {
		fd_log_error("strdup error");
		free(ret);
		return NULL;
	}
	ret->child = NULL;
	return ret;
}

/* Free avp_target structure (recursively). */
static void avp_target_free(struct avp_target *target) {
	struct avp_target *iter;

	for (iter=target; iter != NULL; ) {
		struct avp_target *next;
		free(iter->name);
		next = iter->child;
		free(iter);
		iter = next;
	}
}

/* create new avp_condition (defaults are hard here) */
static struct avp_condition *avp_condition_new(void) {
	struct avp_condition *result;

	if ((result=malloc(sizeof(*result))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	result->basetype = AVP_TYPE_GROUPED; /* invalid default */
	result->comparator = EQUAL;
	memset(&result->value, 0, sizeof(result->value));
	result->variable_index = -1;
	return result;
}

/* free struct avp_condition */
static void avp_condition_free(struct avp_condition *condition) {
	if (condition == NULL) {
		return;
	}
	if (condition->basetype == AVP_TYPE_OCTETSTRING) {
		free(condition->value.os.data);
	}
	free(condition);
}

/* Recursively free avp_action */
static void avp_action_free(struct avp_action *action) {
	struct avp_action *iter;

	iter = action;
	while (iter) {
		struct avp_action *next;
		avp_condition_free(iter->condition);
		avp_target_free(iter->target);
		next = iter->next;
		free(iter);
		iter = next;
	}
}

static struct avp_action *avp_action_new(void) {
	struct avp_action *result;

	if ((result=malloc(sizeof(*result))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	result->condition = NULL;
	result->match_type = REWRITE_MAP;
	result->next = NULL;
	result->target = NULL;
	return result;
}


/* Create new avp_match structure with name 'name'. */
struct avp_match *avp_match_new(const char *name) {
	struct avp_match *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	if ((ret->name=strdup(name)) == NULL) {
		fd_log_error("strdup error");
		free(ret);
		return NULL;
	}
	ret->next = NULL;
	ret->children = NULL;

	ret->actions = NULL;
	ret->variable_index = -1;
	return ret;
}

/* Free avp_match structure (recursively). */
void avp_match_free(struct avp_match *match) {
	struct avp_match *iter;

	for (iter=match; iter != NULL; ) {
		struct avp_match *next;
		free(iter->name);
		next = iter->next;
		avp_match_free(iter->children);
		avp_action_free(iter->actions);
		free(iter);
		iter = next;
	}
}

/* add new avp_action 'action' to avp_match 'match' */
void avp_match_add_action(struct avp_match *match, struct avp_action *action) {
	if (match->actions == NULL) {
		match->actions = action;
		return;
	}
	struct avp_action *dest = match->actions;
	while (dest->next != NULL) {
		dest = dest->next;
	}
	dest->next = action;
}

/* return existing avp_match node for 'name' at 'target' or at the same level as it, or create new one */
static struct avp_match *avp_match_add_next_to(const char *name, struct avp_match *target)
{
	struct avp_match *iter, *prev;

	if (target == NULL) {
		return avp_match_new(name);
	}

	for (prev=iter=target; iter != NULL; iter=iter->next) {
		if (strcmp(iter->name, name) == 0) {
			return iter;
		}
		prev = iter;
	}
	prev->next = avp_match_new(name);
	return prev->next;
}

/* add avp_match for 'name' below 'target' */
static int avp_match_add(struct avp_match **target, const char *name)
{
	struct avp_match *temp;
	if (verify_avp(name) < 0) {
		return -1;
	}
	temp = avp_match_add_next_to(name, (*target)->children);
	if ((*target)->children == NULL) {
		(*target)->children = temp;
	}
	*target = temp;
	return 0;
}

/*
 * Add next level AVP 'name' to 'source_target', by adding it to the
 * common match tree and updating the 'source_target' pointer.
 */
int source_add(const char *name)
{
	if (source_target == NULL) {
		source_target = avp_match_start;
	}
	return avp_match_add(&source_target, name);
}

/* build tree for destination */
int dest_add(const char *name)
{
	struct avp_target *temp;

	if (verify_avp(name) < 0) {
		return -1;
	}
	if ((temp=avp_target_new(name)) == NULL) {
		dest_target = NULL;
		source_target = NULL;
		return -1;
	}
	if (dest_target == NULL) {
		dest_target = temp;
	} else {
		struct avp_target *dest;
		dest = dest_target;
		while (dest->child) {
			dest = dest->child;
		}
		dest->child = temp;
	}
	return 0;
}

int map_finish(void)
{
	struct avp_action *action;
	if (source_target == NULL) {
		return 0;
	}

	if ((action=avp_action_new()) == NULL) {
		return -1;
	}
	action->condition = condition_target;
	action->match_type = REWRITE_MAP;
	action->target = dest_target;
	avp_match_add_action(source_target, action);

	source_target = NULL;
	dest_target = NULL;
	condition_target = NULL;
	return 0;
}

/* create new avp_add structure and initialize it */
static struct avp_add *avp_add_new(const char *request_name)
{
	struct dict_object *request_do;
	struct avp_add *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	if ((fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, request_name, &request_do, ENOENT) != 0)
	    || (fd_dict_getval(request_do, &ret->request) != 0)) {
		free(ret);
		fd_log_error("rt_rewrite: unknown command '%s' in ADD rule", request_name);
		return NULL;
	}
	ret->target = NULL;
	memset(&ret->value, 0, sizeof(ret->value));
	ret->basetype = AVP_TYPE_GROUPED; /* invalid */
	ret->next = NULL;
	return ret;
}

/* free complete avp_add list */
void avp_add_free(struct avp_add *item)
{
	while (item) {
		struct avp_add *next;
		avp_target_free(item->target);
		if (item->basetype == AVP_TYPE_OCTETSTRING) {
			free(item->value.os.data);
		}
		next = item->next;
		free(item);
		item = next;
	}
}

int add_request(const char *request_name)
{
	if ((add_target=avp_add_new(request_name)) == NULL) {
		return -1;
	}
	return 0;
}

/* convert 'input' to an integer between 'min' and 'max', or report an error and return 0 */
static long long convert_string_to_integer(const char *input, long long min, long long max) {
	char *ep;
	errno = 0;
	long long lval = strtoll(input, &ep, 10);
	if (ep == input) {
		fd_log_error("rt_rewrite: conversion to integer failed: '%s' is not a number", input);
		return 0;
	}
	if (*ep != '\0') {
		fd_log_notice("rt_rewrite: conversion to integer problem: '%s' ends with non-number '%s' (ignored)", input, ep);
	}
	if (errno == ERANGE || lval < min || max < lval) {
		fd_log_error("rt_rewrite: conversion to integer problem: '%s' -> %lld not in range [%lld, %lld]", input, lval, min, max);
		return 0;
	}
	if (errno != 0) {
		fd_log_error("rt_rewrite: conversion to integer problem: %s", strerror(errno));
		return 0;
	}
	return lval;
}

static int get_basetype_for_avp(const char *name, enum dict_avp_basetype *result) {
	struct dict_object *avp_do;
	struct dict_avp_data dictdata;
	int ret;

	if ((ret=fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, name, &avp_do, ENOENT)) != 0) {
		fd_log_error("AVP '%s' not in dictionary: %s", name, strerror(ret));
		return -1;
	}
	if (fd_dict_getval(avp_do, &dictdata) != 0) {
		fd_log_error("internal error, target AVP '%s' has invalid dictionary entry");
		return -1;
	}

	*result = dictdata.avp_basetype;
	return 0;
}

/* return a string printing an avp_value 'value' of type 'basetype'. String must be free()d by caller */
static char *dump_avp_value(union avp_value *value, enum dict_avp_basetype basetype) {
	char *result = NULL;
	switch (basetype) {
	case AVP_TYPE_GROUPED:
		fd_log_error("cannot dump avp value of type grouped AVP");
		break;
	case AVP_TYPE_FLOAT32:
	case AVP_TYPE_FLOAT64:
		fd_log_error("unsupported AVP type 'float'");
		break;
        case AVP_TYPE_OCTETSTRING:
		asprintf(&result, "%.*s", (int)value->os.len, value->os.data);
		break;
	case AVP_TYPE_INTEGER32:
		asprintf(&result, "%d", value->i32);
		break;
	case AVP_TYPE_INTEGER64:
		asprintf(&result, "%" PRIi64, value->i64);
		break;
	case AVP_TYPE_UNSIGNED32:
		asprintf(&result, "%u", value->u32);
		break;
	case AVP_TYPE_UNSIGNED64:
		asprintf(&result, "%" PRIu64, value->u64);
		break;
	}
	return result;
}

/*
 * convert 'value' to an avp_value of type 'basetype'.
 * result is returned in 'result' parameter
 * returns 0 on success or -1 on error
 */
static int convert_string_to_avp_value_basetype(const char *value, enum dict_avp_basetype basetype, union avp_value *result)
{
	if (result == NULL) {
		return -1;
	}
	switch(basetype) {
	case AVP_TYPE_GROUPED:
		fd_log_error("cannot convert '%s' to a grouped AVP", value);
		return -1;
	case AVP_TYPE_FLOAT32:
	case AVP_TYPE_FLOAT64:
		fd_log_error("unsupported type 'float'");
		return -1;
        case AVP_TYPE_OCTETSTRING:
                (*result).os.data = (uint8_t *)strdup(value);
                (*result).os.len  = strlen(value);
		break;
	case AVP_TYPE_INTEGER32:
		(*result).i32 = convert_string_to_integer(value, (-0x7fffffff-1), 0x7fffffff);
		break;
	case AVP_TYPE_INTEGER64:
		(*result).i64 = convert_string_to_integer(value, (-0x7fffffffffffffffLL-1), 0x7fffffffffffffffLL);
		break;
	case AVP_TYPE_UNSIGNED32:
		(*result).u32 = convert_string_to_integer(value, 0, 0xffffffff);
		break;
	case AVP_TYPE_UNSIGNED64:
		(*result).u64 = convert_string_to_integer(value, 0, 0xffffffffffffffffLL);
		break;
	}
	return 0;
}

/*
 * Convert string 'value' to appropriate avp_value for 'target'.
 * Return -1 on error, 0 on success.
 */
static int convert_string_to_avp_value(const char *value, struct avp_target *target, union avp_value *val, enum dict_avp_basetype *basetype)
{
	struct avp_target *final = target;

	while (final->child) {
		final = final->child;
	}
	if (get_basetype_for_avp(final->name, basetype) < 0) {
		return -1;
	}
	if (convert_string_to_avp_value_basetype(value, *basetype, val) < 0) {
		return -1;
	}
	return 0;
}

int add_finish(const char *value)
{
	struct avp_add *position;

	if (convert_string_to_avp_value(value, dest_target, &add_target->value, &add_target->basetype) < 0) {
		return -1;
	}
	add_target->target = dest_target;
	dest_target = NULL;
	add_target->condition = condition_target;
	condition_target = NULL;
	if (avp_add_start) {
		position = avp_add_start;
		while (position->next != NULL) {
			position = position->next;
		}
		position->next = add_target;
	} else {
		avp_add_start = add_target;
	}
	return 0;
}

/* mark as to-drop */
int drop_finish(void)
{
	struct avp_action *action;

	if (source_target == NULL) {
		return 0;
	}

	if ((action=avp_action_new()) == NULL) {
		return -1;
	}
	action->condition = condition_target;
	action->match_type = REWRITE_DROP;
	action->target = NULL;;
	avp_match_add_action(source_target, action);

	source_target = NULL;
	condition_target = NULL;

	return 0;
}

/*
 * Add next level AVP 'name' to 'variable_target', by adding it to the
 * common match tree and updating the 'variable_target' pointer.
 */
int variable_add(const char *name)
{
	if (variable_target == NULL) {
		variable_target = avp_variable_start;
	}
	return avp_match_add(&variable_target, name);
}

/* get index for variable with name 'variable_name' */
int variable_get_index(const char *name) {
	for (int i = 0; i < avp_variable_count; i++) {
		if (strcmp(variable_data[i].name, name) == 0) {
			return i;
		}
	}

	return -1;
}

/* save the variable name for later use */
int variable_set_name(const char *name)
{
	if (variable_name != NULL) {
		fd_log_error("parse error: multiple variable names defined");
		return -1;
	}
	if (variable_get_index(name) != -1) {
		fd_log_error("duplicate VARIABLE name '%s'", name);
		return -1;
	}

	if ((variable_name=strdup(name)) == NULL) {
		fd_log_error("strdup error");
		return -1;
	}
	return 0;
}

/* create a variable (re-uses dest_target code to avoid duplication) */
int variable_finish(void)
{
	struct variable_data_type *new_variables;
	enum dict_avp_basetype basetype;

	if (get_basetype_for_avp(variable_target->name, &basetype) < 0) {
		return -1;
	}
	if (variable_target == NULL || variable_name == NULL) {
		fd_log_error("bug in variable_finish() - no variable or name");
		return -1;
	}
	/* extend array */
	if ((new_variables=realloc(variable_data, (avp_variable_count+1) * sizeof(*variable_data))) == NULL) {
		fd_log_error("realloc error");
		return -1;
	}
	variable_data = new_variables;
	variable_data[avp_variable_count].name = variable_name;
	variable_data[avp_variable_count].basetype = basetype;
	variable_target->variable_index = avp_variable_count;
	avp_variable_count++;
	variable_name = NULL;
	variable_target = NULL;

	return 0;
}

/* delete variable names after parsing config file, not needed afterwards */
void variable_names_cleanup(void) {
	if (variable_data == NULL) {
		return;
	}
	for (size_t i = 0; i < avp_variable_count; i++) {
		free(variable_data[i].name);
	}
	free(variable_data);
	variable_data = NULL;
}

/* add condition to be used by next ADD/DROP/MAP */
int condition(const char *name, enum comparators comparator, const char *value) {
	int variable_index;
	struct avp_condition *cond;

	if ((variable_index=variable_get_index(name)) == -1) {
		fd_log_error("unknown variable '%s' in condition", name);
		return -1;
	}
	if (variable_data[variable_index].basetype == AVP_TYPE_OCTETSTRING && comparator != EQUAL) {
		fd_log_error("variable '%s' of type OctetString can only be compared with '='", name);
		return -1;
	}
	if ((cond=avp_condition_new()) == NULL) {
		return -1;
	}
	if (convert_string_to_avp_value_basetype(value, variable_data[variable_index].basetype, &cond->value) < 0) {
		fd_log_error("failed to convert comparison value '%s' for variable '%s'", value, name);
		free(cond);
		return -1;
	}
	cond->basetype = variable_data[variable_index].basetype;
	cond->comparator = comparator;
	cond->variable_index = variable_index;

	condition_target = cond;
	return 0;
}
