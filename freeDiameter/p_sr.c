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

#include "fD.h"

#ifndef SR_DEBUG_LVL
#define SR_DEBUG_LVL ANNOYING
#endif /* SR_DEBUG_LVL */

/* Structure to store a sent request */
struct sentreq {
	struct fd_list	chain; 	/* the "o" field points directly to the hop-by-hop of the request (uint32_t *)  */
	struct msg	*req;	/* A request that was sent and not yet answered. */
	uint32_t	prevhbh;/* The value to set in the hbh header when the message is retrieved */
};

/* Find an element in the list, or the following one */
static struct fd_list * find_or_next(struct fd_list * srlist, uint32_t hbh, int * match)
{
	struct fd_list * li;
	*match = 0;
	for (li = srlist->next; li != srlist; li = li->next) {
		uint32_t * nexthbh = li->o;
		if (*nexthbh < hbh)
			continue;
		if (*nexthbh == hbh)
			*match = 1;
		break;
	}
	return li;
}

static void srl_dump(const char * text, struct fd_list * srlist)
{
	struct fd_list * li;
	if (!TRACE_BOOL(SR_DEBUG_LVL))
		return;
	fd_log_debug("%sSentReq list @%p:\n", text, srlist);
	for (li = srlist->next; li != srlist; li = li->next) {
		struct sentreq * sr = (struct sentreq *)li;
		uint32_t * nexthbh = li->o;
		fd_log_debug(" - Next req (%x):\n", *nexthbh);
		fd_msg_dump_one(SR_DEBUG_LVL + 1, sr->req);
	}
}

/* Store a new sent request */
int fd_p_sr_store(struct sr_list * srlist, struct msg **req, uint32_t *hbhloc, uint32_t hbh_restore)
{
	struct sentreq * sr;
	struct fd_list * next;
	int match;
	
	TRACE_ENTRY("%p %p %p %x", srlist, req, hbhloc, hbh_restore);
	CHECK_PARAMS(srlist && req && *req && hbhloc);
	
	CHECK_MALLOC( sr = malloc(sizeof(struct sentreq)) );
	memset(sr, 0, sizeof(struct sentreq));
	fd_list_init(&sr->chain, hbhloc);
	sr->req = *req;
	sr->prevhbh = hbh_restore;
	
	/* Search the place in the list */
	CHECK_POSIX( pthread_mutex_lock(&srlist->mtx) );
	next = find_or_next(&srlist->srs, *hbhloc, &match);
	if (match) {
		TRACE_DEBUG(INFO, "A request with the same hop-by-hop Id was already sent: error");
		free(sr);
		CHECK_POSIX_DO( pthread_mutex_unlock(&srlist->mtx), /* ignore */ );
		return EINVAL;
	}
	
	/* Save in the list */
	*req = NULL;
	fd_list_insert_before(next, &sr->chain);
	srl_dump("Saved new request, ", &srlist->srs);
	CHECK_POSIX( pthread_mutex_unlock(&srlist->mtx) );
	return 0;
}

/* Fetch a request by hbh */
int fd_p_sr_fetch(struct sr_list * srlist, uint32_t hbh, struct msg **req)
{
	struct sentreq * sr;
	int match;
	
	TRACE_ENTRY("%p %x %p", srlist, hbh, req);
	CHECK_PARAMS(srlist && req);
	
	/* Search the request in the list */
	CHECK_POSIX( pthread_mutex_lock(&srlist->mtx) );
	srl_dump("Fetching a request, ", &srlist->srs);
	sr = (struct sentreq *)find_or_next(&srlist->srs, hbh, &match);
	if (!match) {
		TRACE_DEBUG(INFO, "There is no saved request with this hop-by-hop id (%x)", hbh);
		*req = NULL;
	} else {
		/* Restore hop-by-hop id */
		*((uint32_t *)sr->chain.o) = sr->prevhbh;
		/* Unlink */
		fd_list_unlink(&sr->chain);
		*req = sr->req;
		free(sr);
	}
	CHECK_POSIX( pthread_mutex_unlock(&srlist->mtx) );

	/* Done */
	return 0;
}

/* Failover requests (free or requeue routables) */
void fd_p_sr_failover(struct sr_list * srlist)
{
	CHECK_POSIX_DO( pthread_mutex_lock(&srlist->mtx), /* continue anyway */ );
	while (!FD_IS_LIST_EMPTY(&srlist->srs)) {
		struct sentreq * sr = (struct sentreq *)(srlist->srs.next);
		fd_list_unlink(&sr->chain);
		if (fd_msg_is_routable(sr->req)) {
			struct msg_hdr * hdr = NULL;
			
			/* Set the 'T' flag */
			CHECK_FCT_DO(fd_msg_hdr(sr->req, &hdr), /* continue */);
			if (hdr)
				hdr->msg_flags |= CMD_FLAG_RETRANSMIT;
			
			/* Requeue for sending to another peer */
			CHECK_FCT_DO(fd_fifo_post(fd_g_outgoing, &sr->req),
					CHECK_FCT_DO(fd_msg_free(sr->req), /* What can we do more? */));
		} else {
			/* Just free the request... */
			CHECK_FCT_DO(fd_msg_free(sr->req), /* Ignore */);
		}
		free(sr);
	}
	CHECK_POSIX_DO( pthread_mutex_unlock(&srlist->mtx), /* continue anyway */ );
}

