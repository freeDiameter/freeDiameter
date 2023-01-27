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

#include <stdlib.h>

#include "rt_rewrite_conf_support.h"

/* Forward declaration, comes from parser */
int yyparse(char * conffile);

/* copied from libfdproto/dictionary.c because the symbol is not public */
static const char * type_base_name[] = { /* must keep in sync with dict_avp_basetype */
	"Grouped", 	/* AVP_TYPE_GROUPED */
	"Octetstring", 	/* AVP_TYPE_OCTETSTRING */
	"Integer32", 	/* AVP_TYPE_INTEGER32 */
	"Integer64", 	/* AVP_TYPE_INTEGER64 */
	"Unsigned32", 	/* AVP_TYPE_UNSIGNED32 */
	"Unsigned64", 	/* AVP_TYPE_UNSIGNED64 */
	"Float32", 	/* AVP_TYPE_FLOAT32 */
	"Float64"	/* AVP_TYPE_FLOAT64 */
	};

static struct avp_match *source_target = NULL;
static struct avp_target *dest_target_top = NULL, *dest_target = NULL;
static struct avp_add *add_target = NULL;

static char * full_target_name(struct avp_target *target)
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

/* add 'target' to the end of string 'prefix'. return new string. Take ownership of 'prefix' on success. */
static char *add_target_to(struct avp_target *target, char *prefix)
{
	char *output = NULL;
	char *target_string;
	if ((target_string=full_target_name(target)) == NULL) {
		return NULL;
	}
	if (asprintf(&output, "%s -> /%s", prefix, target_string) == -1) {
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
		compare_avp_types(iter->children);
		if (iter->target) {
			struct avp_target *final;
			final = iter->target;
			while (final->child) {
				final = final->child;
			}
			compare_avp_type(iter->name, final->name);
		}
	}
	return;
}


/* Print "ADD" part of configuration, so users can verify the config from the logs. */
void dump_add_config(struct avp_add *start)
{
	struct avp_add *iter;
	for (iter=start; iter; iter=iter->next) {
		char *prefix = NULL;
		char *result = NULL;;
		if (asprintf(&prefix,"ADD (for %s): ", iter->request.cmd_name) == -1) {
			fd_log_error("rt_rewrite: dump_add_config: asprintf failed: %s", strerror(errno));
			return;
		}
		if ((result=add_target_to(iter->target, prefix)) == NULL) {
			free(prefix);
			return;
		}
		fd_log_notice(result);
		free(result);
	}
}

/* Print out configuration, so users can verify the config from the logs. */
void dump_config(struct avp_match *start, char *prefix)
{
	char *new_prefix = NULL;
	struct avp_match *iter;
	for (iter=start; iter != NULL; iter=iter->next) {
		if (strcmp(iter->name, "TOP") == 0) {
			if ((new_prefix=strdup(prefix)) == NULL) {
				fd_log_error("rt_rewrite: dump_config: strdup failed: %s", strerror(errno));
				return;
			}
		} else {
			if (asprintf(&new_prefix,"%s/%s", prefix, iter->name) == -1) {
				fd_log_error("rt_rewrite: dump_config: asprintf failed: %s", strerror(errno));
				return;
			}
		}
		dump_config(iter->children, new_prefix);
		if (iter->target) {
			char *result = add_target_to(iter->target, new_prefix);
			if (result == NULL) {
				free(new_prefix);
				return;
			}
			fd_log_notice("MAP %s", result);
		}
		if (iter->match_type == REWRITE_DROP) {
			fd_log_notice("DROP %s", new_prefix);
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
static struct avp_target *target_new(char *name) {
	struct avp_target *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	ret->name = name;
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

/* Create new avp_match structure with name 'name'. Taken ownership of 'name'. */
struct avp_match *avp_match_new(char *name) {
	struct avp_match *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	ret->name = name;
	ret->next = NULL;
	ret->children = NULL;
	ret->target = NULL;
	ret->match_type = REWRITE_MAP;
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
		avp_target_free(iter->target);
		free(iter);
		iter = next;
	}
}

static struct avp_match *avp_add_next_to(char *name, struct avp_match *target)
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

static int add(struct avp_match **target, char *name)
{
	struct avp_match *temp;
	if (verify_avp(name) < 0) {
		return -1;
	}
	temp = avp_add_next_to(name, (*target)->children);
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
int source_add(char *name)
{
	if (source_target == NULL) {
		source_target = avp_match_start;
	}
	return add(&source_target, name);
}

/* build tree for destination */
int dest_add(char *name)
{
	struct avp_target *temp;

	if (verify_avp(name) < 0) {
		return -1;
	}
	if ((temp=target_new(name)) == NULL) {
		dest_target_top = NULL;
		dest_target = NULL;
		source_target = NULL;
		return -1;
	}
	if (dest_target == NULL) {
		dest_target_top = temp;
	} else {
		dest_target->child = temp;
	}
	dest_target = temp;
	return 0;
}

void map_finish(void)
{
	if (source_target == NULL) {
		return;
	}
	source_target->target = dest_target_top;
	source_target = NULL;
	dest_target_top = NULL;
	dest_target = NULL;
}

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
	ret->free_os_data = 0;
	ret->next = NULL;
	return ret;
}

void avp_add_free(struct avp_add *item)
{
	if (item == NULL) {
		return;
	}
	avp_target_free(item->target);
	if (item->free_os_data) {
		free(item->value.os.data);
	}
	free(item);
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

/*
 * Convert string 'value' to appropriate avp_value for 'target'.
 * Return -1 on error, 0 on success.
 */
static int convert_string_to_avp_value(const char *value, struct avp_target *target, union avp_value *val, int *free_val)
{
	struct dict_object *avp_do;
	struct dict_avp_data dictdata;
	int ret;
	struct avp_target *final = target;

	while (final->child) {
		final = final->child;
	}
	if ((ret=fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME_ALL_VENDORS, final->name, &avp_do, ENOENT)) != 0) {
		fd_log_error("internal error, target AVP '%s' not in dictionary: %s", final->name, strerror(ret));
		return -1;
	}
	if (fd_dict_getval(avp_do, &dictdata) != 0) {
		fd_log_error("internal error, target AVP '%s' has invalid dictionary entry");
		return -1;
	}
	switch(dictdata.avp_basetype) {
	case AVP_TYPE_GROUPED:
		fd_log_error("rt_rewrite: invalid type 'grouped' for target AVP '%s'", final->name);
		break;
	case AVP_TYPE_FLOAT32:
	case AVP_TYPE_FLOAT64:
		fd_log_error("rt_rewrite: unsupported type 'float' for target AVP '%s'", final->name);
		break;
        case AVP_TYPE_OCTETSTRING:
                (*val).os.data = (uint8_t *)strdup(value);
                (*val).os.len  = strlen(value);
		break;
	case AVP_TYPE_INTEGER32:
		(*val).i32 = convert_string_to_integer(value, (-0x7fffffff-1), 0x7fffffff);
		break;
	case AVP_TYPE_INTEGER64:
		(*val).i64 = convert_string_to_integer(value, (-0x7fffffffffffffffLL-1), 0x7fffffffffffffffLL);
		break;
	case AVP_TYPE_UNSIGNED32:
		(*val).u32 = convert_string_to_integer(value, 0, 0xffffffff);
		break;
	case AVP_TYPE_UNSIGNED64:
		(*val).u64 = convert_string_to_integer(value, 0, 0xffffffffffffffffLL);
		break;
	}
	return 0;
}

int add_finish(const char *value)
{
	if (convert_string_to_avp_value(value, dest_target, &add_target->value, &add_target->free_os_data) < 0) {
		return -1;
	}
	add_target->target = dest_target;
	dest_target = NULL;
	if (avp_add_start) {
		add_target->next = avp_add_start->next;
		avp_add_start->next = add_target;
	} else {
		avp_add_start = add_target;
	}

	return 0;
}

/* mark as to-drop */
void drop_finish(void)
{
	if (source_target == NULL) {
		return;
	}
	source_target->match_type = REWRITE_DROP;
	source_target = NULL;
}
