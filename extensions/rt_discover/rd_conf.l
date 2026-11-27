%{
#include "rt_discover.h"
#include "rd_conf.tab.h"
#include <limits.h>

#define YY_USER_ACTION { \
	yylloc->first_column = yylloc->last_column + 1; \
	yylloc->last_column = yylloc->first_column + yyleng - 1; \
}
#define YY_NO_INPUT
%}

%option bison-bridge bison-locations noyywrap nounput

%%
\n			{ yylloc->first_line++; yylloc->last_line++; yylloc->last_column = 0; }
([[:space:]]{-}[\n])+	;
#.*$			;
(?i:"Realm")		{ return REALM; }
(?i:"Peer")		{ return PEER; }
(?i:"No_IP")		{ return NOIP; }
(?i:"No_IPv6")		{ return NOIP6; }
(?i:"No_TCP")		{ return NOTCP; }
(?i:"No_SCTP")		{ return NOSCTP; }
(?i:"Prefer_TCP")	{ return PREFERTCP; }
(?i:"TcTimer")		{ return TCTIMER; }
(?i:"TwTimer")		{ return TWTIMER; }
(?i:"TLS_Prio")		{ return TLS_PRIO; }
[[:digit:]]+		{
				unsigned long value;
				char *end;
				errno = 0;
				value = strtoul(yytext, &end, 10);
				if (errno || *end || value > INT_MAX)
					return LEX_ERROR;
				yylval->integer = (int)value;
				return INTEGER;
			}
\"[^\"\n]*\"		{
				yylval->string = strndup(yytext + 1, yyleng - 2);
				if (!yylval->string)
					return LEX_ERROR;
				return QSTRING;
			}
[=;{}]			{ return yytext[0]; }
.			{ return LEX_ERROR; }
%%
