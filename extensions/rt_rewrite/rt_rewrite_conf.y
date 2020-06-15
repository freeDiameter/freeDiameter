/*********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2018, Thomas Klausner                                                                    *
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

/* Yacc extension's configuration parser.
 */

/* For development only : */
%debug
%error-verbose

/* The parser receives the configuration file filename as parameter */
%parse-param {char * conffile}

/* Keep track of location */
%locations
%pure-parser

%{
#include "rt_rewrite.h"
#include "rt_rewrite_conf.tab.h"	/* bison is not smart enough to define the YYLTYPE before including this code, so... */

/* Forward declaration */
int yyparse(char * conffile);
void rt_rewrite_confrestart(FILE *input_file);

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

static struct avp_match *avp_match_new(char *name);

static struct avp_match *source_target = NULL, *drop_target = NULL;
static struct avp_target *dest_target = NULL;

static void print_target(struct avp_target *target, char *prefix)
{
	char *output = NULL;
	if (asprintf(&output, "%s -> /TOP/%s", prefix, target->name) == -1) {
		fd_log_error("rt_rewrite: print_target: setup:  asprintf failed: %s", strerror(errno));
		return;
	}
	for (target=target->child; target != NULL; target=target->child) {
		char *new_output = NULL;
		if (asprintf(&new_output, "%s/%s", output, target->name) == -1) {
			fd_log_error("rt_rewrite: print_target: asprintf failed: %s", strerror(errno));
			free(output);
			return;
		}
		free(output);
		output = new_output;
		new_output = NULL;
	}
	fd_log_debug(output);
	free(output);
	return;
}

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

static void compare_avp_types(struct avp_match *start)
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

static void dump_config(struct avp_match *start, char *prefix)
{
	char *new_prefix = NULL;
	struct avp_match *iter;
	for (iter=start; iter != NULL; iter=iter->next) {
		if (asprintf(&new_prefix, "%s/%s", prefix, iter->name) == -1) {
			fd_log_error("rt_rewrite: dump_config: asprintf failed: %s", strerror(errno));
			return;
		}
		dump_config(iter->children, new_prefix);
		if (iter->target) {
			print_target(iter->target, new_prefix);
		}
		if (iter->drop) {
			fd_log_debug("%s -> DROP", new_prefix);
		}
		free(new_prefix);
		new_prefix = NULL;
	}
	return;
}

/* Parse the configuration file */
int rt_rewrite_conf_handle(char * conffile)
{
	extern FILE * rt_rewrite_confin;
	int ret;
	char *top;

	TRACE_ENTRY("%p", conffile);

	TRACE_DEBUG (FULL, "Parsing configuration file: '%s'", conffile);

	/* to match other entries */
	if ((top=strdup("TOP")) == NULL) {
		fd_log_error("strdup error: %s", strerror(errno));
		return EINVAL;
	}
	if ((avp_match_start=avp_match_new(top)) == NULL) {
		fd_log_error("malloc error: %s", strerror(errno));
		free(top);
		return EINVAL;
	}
	rt_rewrite_confin = fopen(conffile, "r");
	if (rt_rewrite_confin == NULL) {
		ret = errno;
		fd_log_debug("Unable to open extension configuration file '%s' for reading: %s", conffile, strerror(ret));
		TRACE_DEBUG (INFO, "rt_rewrite: error occurred, message logged -- configuration file.");
		avp_match_free(avp_match_start);
		avp_match_start = NULL;
		return ret;
	}

	rt_rewrite_confrestart(rt_rewrite_confin);
	ret = yyparse(conffile);

	fclose(rt_rewrite_confin);

	if (ret != 0) {
		TRACE_DEBUG(INFO, "rt_rewrite: unable to parse the configuration file.");
		avp_match_free(avp_match_start);
		avp_match_start = NULL;
		return EINVAL;
	}

	compare_avp_types(avp_match_start);
	dump_config(avp_match_start, "");

	return 0;
}

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

static struct avp_match *avp_match_new(char *name) {
	struct avp_match *ret;

	if ((ret=malloc(sizeof(*ret))) == NULL) {
		fd_log_error("malloc error");
		return NULL;
	}
	ret->name = name;
	ret->next = NULL;
	ret->children = NULL;
	ret->target = NULL;
	ret->drop = 0;
	return ret;
}

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

static struct avp_match *add_avp_next_to(char *name, struct avp_match *target)
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
	temp = add_avp_next_to(name, (*target)->children);
	if ((*target)->children == NULL) {
		(*target)->children = temp;
	}
	*target = temp;
	return 0;
}

/* build tree for source */
static int source_add(char *name)
{
	if (source_target == NULL) {
		source_target = avp_match_start;
	}
	return add(&source_target, name);
}

/* build tree for destination */
static int dest_add(char *name)
{
	struct avp_target *temp;

	if (verify_avp(name) < 0) {
		return -1;
	}
	if ((temp=target_new(name)) == NULL) {
		dest_target = NULL;
		source_target = NULL;
		return -1;
	}
	if (dest_target == NULL) {
		dest_target = temp;
		source_target->target = dest_target;
		source_target = NULL;
		return 0;
	}
	dest_target->child = temp;
	dest_target = temp;
	return 0;
}

static void dest_finish(void)
{
	dest_target = NULL;
}

/* same as source_add, but for drop */
static int drop_add(char *name)
{
	if (drop_target == NULL) {
		drop_target = avp_match_start;
	}
	return add(&drop_target, name);
}

/* mark as to-drop */
static void drop_finish(void)
{
	drop_target->drop = 1;
	drop_target = NULL;
}

/* The Lex parser prototype */
int rt_rewrite_conflex(YYSTYPE *lvalp, YYLTYPE *llocp);

/* Function to report the errors */
void yyerror (YYLTYPE *ploc, char * conffile, char const *s)
{
	TRACE_DEBUG(INFO, "rt_rewrite: error in configuration parsing");

	if (ploc->first_line != ploc->last_line)
		fd_log_debug("%s:%d.%d-%d.%d : %s", conffile, ploc->first_line, ploc->first_column, ploc->last_line, ploc->last_column, s);
	else if (ploc->first_column != ploc->last_column)
		fd_log_debug("%s:%d.%d-%d : %s", conffile, ploc->first_line, ploc->first_column, ploc->last_column, s);
	else
		fd_log_debug("%s:%d.%d : %s", conffile, ploc->first_line, ploc->first_column, s);
}

%}

/* Values returned by lex for token */
%union {
	char 		*string;	/* The string is allocated by strdup in lex.*/
}

/* In case of error in the lexical analysis */
%token 		LEX_ERROR

/* A (de)quoted string (malloc'd in lex parser; it must be freed after use) */
%token <string>	 QSTRING

/* Tokens */
%token 		MAP
%token 		DROP


/* -------------------------------------- */
%%

 /* The grammar definition */
rules:			/* empty ok */
			| rules map
			| rules drop
			;

/* source -> destination mapping */
map:			MAP '=' source_part '>' dest_part { dest_finish(); }
			';'
			;

source_part: 		source_part ':' QSTRING { if (source_add($3) < 0) { YYERROR; } }
			| QSTRING { if (source_add($1) < 0) { YYERROR; } }
			;

dest_part: 		dest_part ':' QSTRING { if (dest_add($3) < 0) { YYERROR; } }
			| QSTRING { if (dest_add($1) < 0) { YYERROR; } }
			;

/* for dropping an AVP */
drop:			DROP '=' drop_part { drop_finish(); }
			';'
			;

drop_part: 		drop_part ':' QSTRING { if (drop_add($3) < 0) { YYERROR; } }
			| QSTRING { if (drop_add($1) < 0) { YYERROR; } }
			;
