/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Thomas Klausner <tk@giga.or.at>								 *
*													 *
* Copyright (c) 2019, Thomas Klausner									 *
* All rights reserved.											 *
* 													 *
* Written under contract by Effortel Technologies SA, http://effortel.com/                               *
* 													 *
* Redistribution and use of this software in source and binary forms, with or without modification, are  *
* permitted provided that the following conditions are met:						 *
* 													 *
* * Redistributions of source code must retain the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer.										 *
*    													 *
* * Redistributions in binary form must reproduce the above 						 *
*   copyright notice, this list of conditions and the 							 *
*   following disclaimer in the documentation and/or other						 *
*   materials provided with the distribution.								 *
* 													 *
* * Neither the name of the WIDE Project or NICT nor the 						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of WIDE Project and 						 *
*   NICT.												 *
* 													 *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 	 *
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 	 *
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
* TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.								 *
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
#include "dbg_loglevel.h"
#include "dbg_loglevel_conf.tab.h"

/* Forward declaration */
int dbg_loglevel_confparse(char *conffile);
void dbg_loglevel_confrestart(FILE *input_file);

/* Parse the configuration file */
int dbg_loglevel_conf_handle(char * conffile)
{
	extern FILE *dbg_loglevel_confin;
	int ret;

	TRACE_ENTRY("%p", conffile);

	TRACE_DEBUG(FULL, "Parsing configuration file: %s...", conffile);

	dbg_loglevel_confin = fopen(conffile, "r");
	if (dbg_loglevel_confin == NULL) {
		ret = errno;
		TRACE_ERROR("Unable to open extension configuration file %s for reading: %s", conffile, strerror(ret));
		return ret;
	}

	dbg_loglevel_confrestart(dbg_loglevel_confin);
	ret = dbg_loglevel_confparse(conffile);

	fclose(dbg_loglevel_confin);

	if (ret != 0) {
		TRACE_ERROR( "Unable to parse the configuration file %s.", conffile);
		return EINVAL;
	} else {
		TRACE_DEBUG(FULL, "[dbg_loglevel] LogLevel: %d.", fd_g_debug_lvl);
	}

	return 0;
}

/* The Lex parser prototype */
int dbg_loglevel_conflex(YYSTYPE *lvalp, YYLTYPE *llocp);

/* Function to report the errors */
void yyerror (YYLTYPE *ploc, char * conffile, char const *s)
{
	TRACE_DEBUG(INFO, "Error in configuration parsing");

	if (ploc->first_line != ploc->last_line)
		fd_log_error("%s:%d.%d-%d.%d : %s", conffile, ploc->first_line, ploc->first_column, ploc->last_line, ploc->last_column, s);
	else if (ploc->first_column != ploc->last_column)
		fd_log_error("%s:%d.%d-%d : %s", conffile, ploc->first_line, ploc->first_column, ploc->last_column, s);
	else
		fd_log_error("%s:%d.%d : %s", conffile, ploc->first_line, ploc->first_column, s);
}

%}

/* Values returned by lex for token */
%union {
	int		integer;
}

/* In case of error in the lexical analysis */
%token 		LEX_ERROR

/* A (de)quoted string (malloc'd in lex parser; it must be freed after use) */
%token <integer> INTEGER

/* Tokens */
%token 		LOGLEVEL


/* -------------------------------------- */
%%

	/* The grammar definition */
conffile:		/* empty is OK */
			| conffile size
			| conffile errors
			{
				yyerror(&yylloc, conffile, "An error occurred while parsing the configuration file");
				YYABORT;
			}
			;

			/* Lexical or syntax error */
errors:			LEX_ERROR
			| error
			;

size:		LOGLEVEL '=' INTEGER ';'
			{
				fd_g_debug_lvl=$3;
			}
			;
