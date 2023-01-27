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
#include <freeDiameter/extension.h>

#include "rt_rewrite_conf_support.h"
#include "rt_rewrite_conf.tab.h"	/* bison is not smart enough to define the YYLTYPE before including this code, so... */

/* Forward declaration */
int yyparse(char * conffile);

/* The Lex parser prototype */
int rt_rewrite_conflex(YYSTYPE *lvalp, YYLTYPE *llocp);

/* Parse the configuration file */
int rt_rewrite_conf_handle(char * conffile)
{
	extern FILE * rt_rewrite_confin;
	int ret;
	char *top;

	TRACE_ENTRY("%p", conffile);

	TRACE_DEBUG (FULL, "Parsing configuration file: '%s'", conffile);

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
		fd_log_error("rt_rewrite: error occurred, message logged -- configuration file");
		avp_match_free(avp_match_start);
		avp_match_start = NULL;
		return ret;
	}

	rt_rewrite_confrestart(rt_rewrite_confin);
	ret = yyparse(conffile);

	fclose(rt_rewrite_confin);

	if (ret != 0) {
		fd_log_error("rt_rewrite: unable to parse the configuration file (%d)", ret);
		avp_match_free(avp_match_start);
		avp_match_start = NULL;
		avp_add_free(avp_add_start);
		avp_add_start = NULL;
		return EINVAL;
	}

	compare_avp_types(avp_match_start);
	dump_config(avp_match_start, "");
	dump_add_config(avp_add_start);

	return 0;
}


/* Function to report the errors */
void yyerror (YYLTYPE *ploc, char * conffile, char const *s)
{
	fd_log_error("rt_rewrite: error in configuration parsing");

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
%token 		ADD
%token 		DROP
%token 		MAP


/* -------------------------------------- */
%%

 /* The grammar definition */
rules:			/* empty ok */
			| rules map
			| rules drop
			| rules add
			;

/* source -> destination mapping */
map:			MAP '=' source_part '>' dest_part { map_finish(); }
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

drop_part: 		drop_part ':' QSTRING { if (source_add($3) < 0) { YYERROR; } }
			| QSTRING { if (source_add($1) < 0) { YYERROR; } }
			;

/* for adding an AVP */
add:   			ADD '=' command '/' add_avp_part '=' add_value { }
			';'
			;

command:		QSTRING { if (add_request($1) < 0) { YYERROR; } }
			;

add_avp_part:		add_avp_part ':' QSTRING { if (dest_add($3) < 0) { YYERROR; } }
			| QSTRING { if (dest_add($1) < 0) { YYERROR; } }
			;

add_value:		QSTRING { if (add_finish($1) < 0) { YYERROR; } }
			;
