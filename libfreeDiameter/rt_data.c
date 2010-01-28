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

/* Routing module helpers.
 * 
 * This file provides support for the rt_data structure manipulation.
 */

#include "libfD.h"

/* Structure that contains the routing data for a message */
struct rt_data {
	int		extracted;	/* if 0, candidates is ordered by diamid, otherwise the order is unspecified */
	struct fd_list	candidates;	/* All the candidates. Items are struct rtd_candidate. */
	struct fd_list	errors;		/* All errors received from other peers for this message */
};

/* Items of the errors list */
struct rtd_error {
	struct fd_list	chain;	/* link in the list, ordered by nexthop */
	char * 		nexthop;/* the peer the message was sent to */
	char *		erh;	/* the origin of the error */
	uint32_t	code;	/* the error code */
};

/* Create a new structure to store routing data */
int  fd_rtd_init(struct rt_data ** rtd)
{
	struct rt_data *new;
	TRACE_ENTRY("%p", rtd);
	CHECK_PARAMS(rtd);
	
	/* Alloc the structure */
	CHECK_MALLOC( new = malloc(sizeof(struct rt_data)) );
	memset(new, 0, sizeof(struct rt_data) );
	fd_list_init(&new->candidates, new);
	fd_list_init(&new->errors, new);
	
	*rtd = new;
	return 0;
}

/* Destroy the routing data */
void fd_rtd_free(struct rt_data ** rtd)
{
	struct rt_data *old;
	
	TRACE_ENTRY("%p", rtd);
	CHECK_PARAMS_DO(rtd, return );
	
	old = *rtd;
	*rtd = NULL;
	
	while (!FD_IS_LIST_EMPTY(&old->candidates)) {
		struct rtd_candidate * c = (struct rtd_candidate *) old->candidates.next;
		
		fd_list_unlink(&c->chain);
		free(c->diamid);
		free(c);
	}
	
	while (!FD_IS_LIST_EMPTY(&old->errors)) {
		struct rtd_error * c = (struct rtd_error *) old->errors.next;
		
		fd_list_unlink(&c->chain);
		free(c->nexthop);
		free(c->erh);
		free(c);
	}
	
	free(old);
	
	return;
}

/* Add a peer to the candidates list */
int  fd_rtd_candidate_add(struct rt_data * rtd, char * peerid)
{
	struct fd_list * prev;
	struct rtd_candidate * new;
	
	TRACE_ENTRY("%p %p", rtd, peerid);
	CHECK_PARAMS(rtd && peerid);
	
	/* Since the peers are ordered when they are added (fd_g_activ_peers) we search for the position from the end -- this should be efficient */
	for (prev = rtd->candidates.prev; prev != &rtd->candidates; prev = prev->prev) {
		struct rtd_candidate * cp = (struct rtd_candidate *) prev;
		int cmp = strcmp(peerid, cp->diamid);
		if (cmp > 0)
			break;
		if (cmp == 0)
			/* The candidate is already in the list */
			return 0;
	}
	
	CHECK_MALLOC( new = malloc(sizeof(struct rtd_candidate)) );
	memset(new, 0, sizeof(struct rtd_candidate) );
	fd_list_init(&new->chain, NULL);
	CHECK_MALLOC( new->diamid = strdup(peerid) );
	
	fd_list_insert_after(prev, &new->chain);
	
	return 0;
}

/* Remove a peer from the candidates (if it is found) */
void fd_rtd_candidate_del(struct rt_data * rtd, char * peerid, size_t sz /* if !0, peerid does not need to be \0 terminated */)
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %p %zd", rtd, peerid, sz);
	CHECK_PARAMS_DO( rtd && peerid , return );
	
	for (li = rtd->candidates.next; li != &rtd->candidates; li = li->next) {
		struct rtd_candidate * c = (struct rtd_candidate *) li;
		int cmp;
		if (sz) {
			cmp = strncmp(peerid, c->diamid, sz);
			if (!cmp) {
				int len = strlen(c->diamid);
				if (sz < len)
					cmp = -1;
				else if (sz == len)
					cmp = 0;
				else cmp = 1;
			}
		} else {
			cmp = strcmp(peerid, c->diamid);
		}
		
		if (!cmp) {
			/* Found it! Remove it */
			fd_list_unlink(&c->chain);
			free(c->diamid);
			free(c);
			break;
		}
		
		if (cmp > 0)
			continue;
		
		/* The list is ordered only if not extracted */
		if (! rtd->extracted)
			break;
	}
	
	return;
}

/* If a peer returned a protocol error for this message, save it so that we don't try to send it there again */
int  fd_rtd_error_add(struct rt_data * rtd, char * sentto, uint8_t * origin, size_t originsz, uint32_t rcode)
{
	struct fd_list * li;
	int match = 0;
	
	TRACE_ENTRY("%p %p %p %d", rtd, sentto, origin, rcode);
	CHECK_PARAMS( rtd && sentto ); /* origin may be NULL */
	
	/* First add the new error entry */
	for (li = rtd->errors.next; li != &rtd->errors; li = li->next) {
		struct rtd_error * e = (struct rtd_error *) li;
		int cmp = strcmp(sentto, e->nexthop);
		if (cmp > 0)
			continue;
		if (!cmp)
			match = 1;
		break;
	}
	
	/* If we already had this entry, we should not have sent the message again to this peer... anyway... */
	if (!match) {
		/* Add a new entry in the error list */
		struct rtd_error * new;
		CHECK_MALLOC( new = malloc(sizeof(struct rtd_error)) );
		memset(new, 0, sizeof(struct rtd_error));
		fd_list_init(&new->chain, NULL);
		CHECK_MALLOC(new->nexthop = strdup(sentto));
		if (origin) {
			CHECK_MALLOC( new->erh = malloc(originsz + 1) );
			memcpy(new->erh, origin, originsz);
			new->erh[originsz] = '\0';
		}
		new->code = rcode;
		fd_list_insert_before(li, &new->chain);
	}
	
	/* Finally, remove this (these) peers from the candidate list */
	fd_rtd_candidate_del(rtd, sentto, 0);
	if (origin)
		fd_rtd_candidate_del(rtd, origin, originsz);
	
	/* Done! */
	return 0;
}

/* Extract the list of valid candidates, and initialize their scores */
void fd_rtd_candidate_extract(struct rt_data * rtd, struct fd_list ** candidates, int ini_score)
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %p", rtd, candidates);
	CHECK_PARAMS_DO( candidates, return );
	CHECK_PARAMS_DO( rtd, { *candidates = NULL; return; } );
	
	*candidates = &rtd->candidates;
	
	/* Reset all scores to INITIAL score */
	for (li = rtd->candidates.next; li != &rtd->candidates; li = li->next) {
		struct rtd_candidate * c = (struct rtd_candidate *) li;
		c->score = ini_score;
	}
	
	rtd->extracted = 1;
	return;
}

/* Reorder the list of peers */
int  fd_rtd_candidate_reorder(struct fd_list * candidates)
{
	struct fd_list unordered = FD_LIST_INITIALIZER(unordered), *li;
	
	TRACE_ENTRY("%p", candidates);
	CHECK_PARAMS( candidates );
	
	/* First, move all items from candidates to the undordered list */
	fd_list_move_end(&unordered, candidates);
	
	/* Now extract each element from unordered and add it back to list ordered by score */
	while (!FD_IS_LIST_EMPTY(&unordered)) {
		struct rtd_candidate * c = (struct rtd_candidate *) unordered.next;
		
		fd_list_unlink(&c->chain);
		
		/* Find the position in ordered candidates list */
		for (li = candidates->next; li != candidates; li = li->next) {
			struct rtd_candidate * cnext = (struct rtd_candidate *) li;
			if (cnext->score >= c->score)
				break;
		}
		
		/* Add the element there */
		fd_list_insert_before(li, &c->chain);
	}
	
	return 0;
}

