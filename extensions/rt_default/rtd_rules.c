/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2009, WIDE Project and NICT								 *
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

#include "rt_default.h"

/* The regular expressions header */
#include <regex.h>

/* We will search for each candidate peer all the rules that are defined, and check which one applies to the message
 * Therefore our repository is organized hierarchicaly.
 *  At the top level, we have two lists of TARGETS (one for IDENTITY, one for REALM), ordered as follow:
 *   - first, the TARGETS defined with a regular expression. We will try matching all regexp to all candidates in the list.
 *   - then, the TARGETS defined by a plain text. We don't have to compare the whole list to each candidate since the list is ordered.
 *
 *  Under each TARGET element, we have the list of RULES that are defined for this target, ordered by CRITERIA type, then is_regex, then string value.
 *
 * Note: Except during configuration parsing and module termination, the lists are only ever accessed read-only, so we do not need a lock.
 */

/* Structure to hold the data that is used for matching. Note that in case is_regex is true, the string to be matched must be \0-terminated. */
struct match_data {
	int	 is_regex;	/* determines how the string is matched */
	char 	*plain;		/* match this value with strcasecmp if is_regex is false. The string is allocated by the lex tokenizer, must be freed at the end. */
	regex_t  preg;		/* match with regexec if is_regex is true. regfree must be called at the end. A copy of the original string is anyway saved in plain. */
};

/* Structure of a TARGET element */
struct target {
	struct fd_list		chain;	/* link in the top-level list */
	struct match_data	md;	/* the data to determine if the current candidate matches this element */
	struct fd_list		rules;	/* Sentinel for the list of rules applying to this target */
	/* note : we do not need the rtd_targ_type here, it is implied by the root of the list this target element is attached to */
};

/* Structure of a RULE element */
struct rule {
	struct fd_list		chain;	/* link in the parent target list */
	enum rtd_crit_type	type;	/* What kind of criteria the message must match for the rule to apply */
	struct match_data	md;	/* the data that the criteria must match, if it is different from RTD_CRI_ALL */
	int			score;	/* The score added to the candidate, if the message matches this criteria */
};

/* The sentinels for the TARGET lists */
static struct fd_list	TARGETS[RTD_TAR_MAX];



int rtd_init(void)
{
	TRACE_ENTRY();
	
	return 0;
}

void rtd_fini(void)
{
	TRACE_ENTRY();

}

int rtd_add(enum rtd_crit_type ct, char * criteria, enum rtd_targ_type tt, char * target, int score, int flags)
{
	TRACE_ENTRY("%d %p %d %p %d %x", ct, criteria, tt, target, score, flags);
	
	CHECK_PARAMS((ct < RTD_CRI_MAX) && ((ct == 0) || criteria) && (tt < RTD_TAR_MAX) && target);
	
	
	
	return 0;
}

int rtd_process( struct msg * msg, struct fd_list * candidates )
{
	return ENOTSUP;
}

void rtd_dump(void)
{
	fd_log_debug("[rt_default] Dumping rules repository...\n");
	fd_log_debug("[rt_default] End of dump\n");
}
