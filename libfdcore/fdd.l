/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2011, WIDE Project and NICT								 *
* All rights reserved.											 *
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

/* Lex configuration parser.
 *
 * This file defines the token for parsing the daemon's configuration file
 * Note that each extension has a separate independant configuration file.
 *
 * Note : This module is NOT thread-safe. All processing must be done from one thread only.
 */
%{
/* Include the daemon's header files */
#include "fdcore-internal.h"
/* Include yacc tokens definitions */
#include "fdd.tab.h"

/* Update the column information */
#ifdef DEBUG_LEX
#define YY_USER_ACTION { 						\
	yylloc->first_column = yylloc->last_column + 1; 		\
	yylloc->last_column = yylloc->first_column + yyleng - 1;	\
	TRACE_DEBUG(FULL, 						\
		"(%d:%d-%d:%d) matched rule %d, length=%d, txt='%s'\n",	\
		yylloc->first_line, yylloc->first_column, 		\
		yylloc->last_line, yylloc->last_column, 		\
		yy_act, yyleng, yytext); 				\
}
#else /* DEBUG_LEX */
#define YY_USER_ACTION { 						\
	yylloc->first_column = yylloc->last_column + 1; 		\
	yylloc->last_column = yylloc->first_column + yyleng - 1;	\
}
#endif

/* %option noinput ? */
#define YY_NO_INPUT
%}

%option bison-bridge bison-locations
%option noyywrap
%option nounput

/* Quoted string. Multilines do not match. */
qstring		\"[^\"\n]*\"

%%

<*>\n			{ 
				/* Update the line count */
				yylloc->first_line++; 
				yylloc->last_line++; 
				yylloc->last_column=0; 
			} 

<*>([[:space:]]{-}[\n])+	; /* Eat all spaces, not new lines */
<*>#.*$			; /* Eat all comments */

{qstring}		{
				/* First copy the string without the quotes for use in the yacc parser */
				CHECK_MALLOC_DO(	yylval->string = strdup(yytext+1), /* This allocates one useless tail char but... it's easier :D */ 
							return LEX_ERROR  );/* on error, trig an error in yacc parser */

				yylval->string[yyleng-2] = '\0';
				
				/* the yacc parser will check the string is valid */
				return QSTRING;
			}
			
[[:digit:]]+		{
				/* Convert this to an integer value */
				int ret = sscanf(yytext, "%i", &yylval->integer);
				if (ret != 1) {
					/* No matching: an error occurred */
					fprintf(stderr, "Unable to convert the value '%s' to a valid number: %s\n", yytext, strerror(errno));
					return LEX_ERROR; /* trig an error in yacc parser */
					/* Maybe we could REJECT instead of failing here? */
				}
				return INTEGER;
			}
				
	/* Full words tokens (keywords) */
(?i:"Identity")		{ return IDENTITY;	}
(?i:"Realm")		{ return REALM;   	}
(?i:"Port")		{ return PORT;    	}
(?i:"SecPort")		{ return SECPORT;  	}
(?i:"No_IPv6")		{ return NOIP6;		}
(?i:"No_IP")		{ return NOIP;		}
(?i:"No_TCP")		{ return NOTCP;		}
(?i:"No_SCTP")		{ return NOSCTP;	}
(?i:"Prefer_TCP")	{ return PREFERTCP;	}
(?i:"TLS_old_method")	{ return OLDTLS;	}
(?i:"SCTP_streams")	{ return SCTPSTREAMS;	}
(?i:"AppServThreads")	{ return APPSERVTHREADS;}
(?i:"ListenOn")		{ return LISTENON;	}
(?i:"TcTimer")		{ return TCTIMER;	}
(?i:"TwTimer")		{ return TWTIMER;	}
(?i:"NoRelay")		{ return NORELAY;	}
(?i:"LoadExtension")	{ return LOADEXT;	}
(?i:"ConnectPeer")	{ return CONNPEER;	}
(?i:"ConnectTo")	{ return CONNTO;	}
(?i:"No_TLS")		{ return NOTLS;		}
(?i:"TLS_Cred")		{ return TLS_CRED;	}
(?i:"TLS_CA")		{ return TLS_CA;	}
(?i:"TLS_CRL")		{ return TLS_CRL;	}
(?i:"TLS_Prio")		{ return TLS_PRIO;	}
(?i:"TLS_DH_bits")	{ return TLS_DH_BITS;	}
(?i:"TLS_DH_file")	{ return TLS_DH_FILE;	}


	/* Valid single characters for yyparse */
<*>[=,:;{}]		{ return yytext[0]; }

	/* Unrecognized token */
<*>[[:alnum:]]+		|	/* This rule is only useful to print a complete token in error messages */
	/* Unrecognized character */
<*>.			{ 
				fprintf(stderr, "Unrecognized text on line %d col %d: '%s'.\n", yylloc->first_line, yylloc->first_column, yytext);
			 	return LEX_ERROR; 
			}

%%
