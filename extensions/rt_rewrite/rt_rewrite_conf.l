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

/* Tokenizer
 *
 */

%{
#include "rt_rewrite.h"
/* Include yacc tokens definitions */
#include "rt_rewrite_conf.tab.h"

/* Update the column information */
#define YY_USER_ACTION { 						\
	yylloc->first_column = yylloc->last_column + 1; 		\
	yylloc->last_column = yylloc->first_column + yyleng - 1;	\
}

/* Avoid warning with newer flex */
#define YY_NO_INPUT

%}

qstring		\"[^\"\n]*\"


%option bison-bridge bison-locations
%option noyywrap
%option nounput

%%

	/* Update the line count */
\n			{
				yylloc->first_line++;
				yylloc->last_line++;
				yylloc->last_column=0;
			}

	/* Eat all spaces but not new lines */
([[:space:]]{-}[\n])+	;
	/* Eat all comments */
#.*$			;


	/* Recognize quoted strings */
{qstring}		{
				/* Match a quoted string. */
				CHECK_MALLOC_DO( yylval->string = strdup(yytext+1),
				{
					fd_log_error("Unable to copy the string '%s': %s", yytext, strerror(errno));
					return LEX_ERROR; /* trig an error in yacc parser */
				} );
				yylval->string[strlen(yytext) - 2] = '\0';
				return QSTRING;
			}


	/* The key words */
(?i:ADD)	 	{	return ADD;	}
(?i:DROP)	 	{	return DROP;	}
(?i:IF)		 	{	return IF;	}
(?i:MAP)	 	{	return MAP;	}
(?i:VARIABLE)	 	{	return VARIABLE;	}
	/* Valid single characters for yyparse */
[=/:;<>]			{ return yytext[0]; }

	/* Unrecognized sequence, if it did not match any previous pattern */
[^[:space:]\"=/:;<>\n]+	{
				fd_log_error("Unrecognized text on line %d col %d: '%s'.", yylloc->first_line, yylloc->first_column, yytext);
			 	return LEX_ERROR;
			}

%%
