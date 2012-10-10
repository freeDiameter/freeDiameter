/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2012, WIDE Project and NICT								 *
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

#include "fdcore-internal.h"

/* Structure to store a sent request */
struct sentreq {
	struct fd_list	chain; 	/* the "o" field points directly to the hop-by-hop of the request (uint32_t *)  */
	struct msg	*req;	/* A request that was sent and not yet answered. */
	uint32_t	prevhbh;/* The value to set in the hbh header when the message is retrieved */
	struct fd_list  expire; /* the list of expiring requests */
	struct timespec added_on; /* the time the request was added */
};

/* Find an element in the hbh list, or the following one */
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
	struct timespec now;
	
	if (!TRACE_BOOL(ANNOYING))
		return;
	
	fd_log_debug("%sSentReq list @%p:\n", text, srlist);
	
	CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &now), );
	
	for (li = srlist->next; li != srlist; li = li->next) {
		struct sentreq * sr = (struct sentreq *)li;
		uint32_t * nexthbh = li->o;
		
		fd_log_debug(" - Next req (hbh:%x): [since %ld.%06ld sec]\n", *nexthbh, 
			(now.tv_nsec >= sr->added_on.tv_nsec) ? (now.tv_sec - sr->added_on.tv_sec) : (now.tv_sec - sr->added_on.tv_sec - 1),
			(now.tv_nsec >= sr->added_on.tv_nsec) ? (now.tv_nsec - sr->added_on.tv_nsec) / 1000 : (now.tv_nsec - sr->added_on.tv_nsec + 1000000000) / 1000);
		
		fd_msg_dump_one(ANNOYING + 1, sr->req);
	}
}

/* (detached) thread that calls the anscb on expired messages. 
  We do it in a separate thread to avoid blocking the reception of new messages during this time */
static void * call_anscb_expire(void * arg) {
	struct msg * expired_req = arg;
	
	void (*anscb)(void *, struct msg **);
	void * data;
	
	TRACE_ENTRY("%p", arg);
	CHECK_PARAMS_DO( arg, return NULL );
	
	/* Set the thread name */
	fd_log_threadname ( "Expired req cb." );
	
	/* Log */
	TRACE_DEBUG(INFO, "The expiration timer for a request has been reached, abording this attempt now & calling cb...");
	
	/* Retrieve callback in the message */
	CHECK_FCT_DO( fd_msg_anscb_get( expired_req, &anscb, &data ), return NULL);
	ASSERT(anscb);
	
	/* Clean up this data from the message */
	CHECK_FCT_DO( fd_msg_anscb_associate( expired_req, NULL, NULL, NULL ), return NULL);

	/* Call it */
	(*anscb)(data, &expired_req);
	
	/* If the callback did not dispose of the message, do it now */
	if (expired_req) {
		fd_msg_log(FD_MSG_LOG_DROPPED, expired_req, "Expiration period completed without an answer, and the expiry callback did not dispose of the message.");
		CHECK_FCT_DO( fd_msg_free(expired_req), /* ignore */ );
	}
	
	/* Finish */
	return NULL;
}

/* thread that handles messages expiring. The thread is started only when needed */
static void * sr_expiry_th(void * arg) {
	struct sr_list * srlist = arg;
	struct msg * expired_req;
	pthread_attr_t detached;
	
	TRACE_ENTRY("%p", arg);
	CHECK_PARAMS_DO( arg, return NULL );
	
	/* Set the thread name */
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "ReqExp/%s", ((struct fd_peer *)(srlist->exp.o))->p_hdr.info.pi_diamid);
		fd_log_threadname ( buf );
	}
	
	CHECK_POSIX_DO( pthread_attr_init(&detached), return NULL );
	CHECK_POSIX_DO( pthread_attr_setdetachstate(&detached, PTHREAD_CREATE_DETACHED), return NULL );
	
	CHECK_POSIX_DO( pthread_mutex_lock(&srlist->mtx),  return NULL );
	pthread_cleanup_push( fd_cleanup_mutex, &srlist->mtx );
	
	do {
		struct timespec	now, *t;
		struct sentreq * first;
		pthread_t th;
		
		/* Check if there are expiring requests available */
		if (FD_IS_LIST_EMPTY(&srlist->exp)) {
			/* Just wait for a change or cancelation */
			CHECK_POSIX_DO( pthread_cond_wait( &srlist->cnd, &srlist->mtx ), goto error );
			/* Restart the loop on wakeup */
			continue;
		}
		
		/* Get the pointer to the request that expires first */
		first = (struct sentreq *)(srlist->exp.next->o);
		t = fd_msg_anscb_gettimeout( first->req );
		ASSERT(t);
		
		/* Get the current time */
		CHECK_SYS_DO(  clock_gettime(CLOCK_REALTIME, &now),  goto error  );

		/* If first request is not expired, we just wait until it happens */
		if ( TS_IS_INFERIOR( &now, t ) ) {
			
			CHECK_POSIX_DO2(  pthread_cond_timedwait( &srlist->cnd, &srlist->mtx, t ),  
					ETIMEDOUT, /* ETIMEDOUT is a normal return value, continue */,
					/* on other error, */ goto error );
	
			/* on wakeup, loop */
			continue;
		}
		
		/* Now, the first request in the list is expired; remove it and call the anscb for it in a new thread */
		fd_list_unlink(&first->chain);
		fd_list_unlink(&first->expire);
		expired_req = first->req;
		free(first);
		
		CHECK_POSIX_DO( pthread_create( &th, &detached, call_anscb_expire, expired_req ), goto error );

		/* loop */
	} while (1);
error:	
	; /* pthread_cleanup_pop sometimes expands as "} ..." and the label beofre this cause some compilers to complain... */
	pthread_cleanup_pop( 1 );
	ASSERT(0); /* we have encountered a problem, maybe time to signal the framework to terminate? */
	return NULL;
}


/* Store a new sent request */
int fd_p_sr_store(struct sr_list * srlist, struct msg **req, uint32_t *hbhloc, uint32_t hbh_restore)
{
	struct sentreq * sr;
	struct fd_list * next;
	int match;
	struct timespec * ts;
	
	TRACE_ENTRY("%p %p %p %x", srlist, req, hbhloc, hbh_restore);
	CHECK_PARAMS(srlist && req && *req && hbhloc);
	
	CHECK_MALLOC( sr = malloc(sizeof(struct sentreq)) );
	memset(sr, 0, sizeof(struct sentreq));
	fd_list_init(&sr->chain, hbhloc);
	sr->req = *req;
	sr->prevhbh = hbh_restore;
	fd_list_init(&sr->expire, sr);
	CHECK_SYS( clock_gettime(CLOCK_REALTIME, &sr->added_on) );
	
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
	
	/* In case of request with a timeout, also store in the timeout list */
	ts = fd_msg_anscb_gettimeout( sr->req );
	if (ts) {
		struct fd_list * li;
		struct timespec * t;
		
		/* browse srlist->exp from the end */
		for (li = srlist->exp.prev; li != &srlist->exp; li = li->prev) {
			struct sentreq * s = (struct sentreq *)(li->o);
			t = fd_msg_anscb_gettimeout( s->req );
			ASSERT( t ); /* sanity */
			if (TS_IS_INFERIOR(t, ts))
				break;
		}
		
		fd_list_insert_after(li, &sr->expire);
	
		/* if the thread does not exist yet, create it */
		if (srlist->thr == (pthread_t)NULL) {
			CHECK_POSIX_DO( pthread_create(&srlist->thr, NULL, sr_expiry_th, srlist), /* continue anyway */);
		} else {
			/* or, if added in first position, signal the condvar to update the sleep time of the thread */
			if (li == &srlist->exp) {
				CHECK_POSIX_DO( pthread_cond_signal(&srlist->cnd), /* continue anyway */);
			}
		}
	}
	
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
		fd_list_unlink(&sr->expire);
		*req = sr->req;
		free(sr);
	}
	CHECK_POSIX( pthread_mutex_unlock(&srlist->mtx) );
	
	/* do not stop the expire thread here, it might cause creating/destroying it very often otherwise */

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
		fd_list_unlink(&sr->expire);
		if (fd_msg_is_routable(sr->req)) {
			struct msg_hdr * hdr = NULL;
			int ret;
			
			/* Set the 'T' flag */
			CHECK_FCT_DO(fd_msg_hdr(sr->req, &hdr), /* continue */);
			if (hdr)
				hdr->msg_flags |= CMD_FLAG_RETRANSMIT;
			
			/* Requeue for sending to another peer */
			CHECK_FCT_DO( ret = fd_fifo_post(fd_g_outgoing, &sr->req),
				{
					fd_msg_log( FD_MSG_LOG_DROPPED, sr->req, "Internal error: error while requeuing during failover: %s", strerror(ret) );
					CHECK_FCT_DO(fd_msg_free(sr->req), /* What can we do more? */)
				});
		} else {
			/* Just free the request. */
			fd_msg_log( FD_MSG_LOG_DROPPED, sr->req, "Sent & unanswered local message discarded during failover." );
			CHECK_FCT_DO(fd_msg_free(sr->req), /* Ignore */);
		}
		free(sr);
	}
	/* The list of expiring requests must be empty now */
	ASSERT( FD_IS_LIST_EMPTY(&srlist->exp) );
	
	CHECK_POSIX_DO( pthread_mutex_unlock(&srlist->mtx), /* continue anyway */ );
	
	/* Terminate the expiry thread (must be done when the lock can be taken) */
	CHECK_FCT_DO( fd_thr_term(&srlist->thr), /* ignore error */ );
}

