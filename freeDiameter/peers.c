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

/* Global list of peers */
struct fd_list   fd_g_peers = FD_LIST_INITIALIZER(fd_g_peers);
pthread_rwlock_t fd_g_peers_rw = PTHREAD_RWLOCK_INITIALIZER;


/* Alloc / reinit a peer structure. if *ptr is not NULL, it must already point to a valid struct fd_peer. */
int fd_peer_alloc(struct fd_peer ** ptr)
{
	struct fd_peer *p;
	
	TRACE_ENTRY("%p", ptr);
	CHECK_PARAMS(ptr);
	
	if (*ptr) {
		p = *ptr;
	} else {
		CHECK_MALLOC( p = malloc(sizeof(struct fd_peer)) );
		*ptr = p;
	}
	
	/* Now initialize the content */
	memset(p, 0, sizeof(struct fd_peer));
	
	fd_list_init(&p->p_hdr.chain, p);
	
	fd_list_init(&p->p_hdr.info.pi_endpoints, NULL);
	fd_list_init(&p->p_hdr.info.pi_apps, NULL);
	
	p->p_eyec = EYEC_PEER;
	fd_list_init(&p->p_expiry, p);
	fd_list_init(&p->p_actives, p);
	p->p_hbh = lrand48();
	CHECK_FCT( fd_fifo_new(&p->p_events) );
	CHECK_FCT( fd_fifo_new(&p->p_recv) );
	CHECK_FCT( fd_fifo_new(&p->p_tosend) );
	fd_list_init(&p->p_sentreq, p);
	
	return 0;
}

/* Add a new peer entry */
int fd_peer_add ( struct peer_info * info, char * orig_dbg, void (*cb)(struct peer_info *, void *), void * cb_data )
{
	struct fd_peer *p = NULL;
	struct fd_list * li;
	int ret = 0;
	TRACE_ENTRY("%p %p %p %p", info, orig_dbg, cb, cb_data);
	CHECK_PARAMS(info && info->pi_diamid);
	
	/* Create a structure to contain the new peer information */
	CHECK_FCT( fd_peer_alloc(&p) );
	
	/* Copy the informations from the parameters received */
	CHECK_MALLOC( p->p_hdr.info.pi_diamid = strdup(info->pi_diamid) );
	if (info->pi_realm) {
		CHECK_MALLOC( p->p_hdr.info.pi_realm = strdup(info->pi_realm) );
	}
	
	p->p_hdr.info.pi_flags.pro3 = info->pi_flags.pro3;
	p->p_hdr.info.pi_flags.pro4 = info->pi_flags.pro4;
	p->p_hdr.info.pi_flags.alg  = info->pi_flags.alg;
	p->p_hdr.info.pi_flags.sec  = info->pi_flags.sec;
	p->p_hdr.info.pi_flags.exp  = info->pi_flags.exp;
	
	p->p_hdr.info.pi_lft     = info->pi_lft;
	p->p_hdr.info.pi_streams = info->pi_streams;
	p->p_hdr.info.pi_port    = info->pi_port;
	p->p_hdr.info.pi_tctimer = info->pi_tctimer;
	p->p_hdr.info.pi_twtimer = info->pi_twtimer;
	
	/* Move the items from one list to the other */
	if (info->pi_endpoints.next)
		while (!FD_IS_LIST_EMPTY( &info->pi_endpoints ) ) {
			li = info->pi_endpoints.next;
			fd_list_unlink(li);
			fd_list_insert_before(&p->p_hdr.info.pi_endpoints, li);
		}
	
	/* The internal data */
	if (orig_dbg) {
		CHECK_MALLOC( p->p_dbgorig = strdup(orig_dbg) );
	} else {
		CHECK_MALLOC( p->p_dbgorig = strdup("unknown") );
	}
	p->p_cb = cb;
	p->p_cb_data = cb_data;
	
	/* Ok, now check if we don't already have an entry with the same Diameter Id, and insert this one */
	CHECK_POSIX( pthread_rwlock_wrlock(&fd_g_peers_rw) );
	
	for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
		struct fd_peer * prev = (struct fd_peer *)li;
		int cmp = strcasecmp( p->p_hdr.info.pi_diamid, prev->p_hdr.info.pi_diamid );
		if (cmp < 0)
			continue;
		if (cmp == 0)
			ret = EEXIST;
		break;
	}
	
	/* We can insert the new peer object */
	if (! ret) {
		/* Update expiry list */
		CHECK_FCT_DO( ret = fd_p_expi_update( p ), goto out );
		
		/* Insert the new element in the list */
		fd_list_insert_before( li, &p->p_hdr.chain );
	}

out:	
	CHECK_POSIX( pthread_rwlock_unlock(&fd_g_peers_rw) );
	if (ret) {
		CHECK_FCT( fd_peer_free(&p) );
	} else {
		CHECK_FCT( fd_psm_begin(p) );
	}
	return ret;
}


#define free_null( _v ) 	\
	if (_v) {		\
		free(_v);	\
		(_v) = NULL;	\
	}
	
#define free_list( _l ) 						\
	while (!FD_IS_LIST_EMPTY(_l)) {					\
		struct fd_list * __li = ((struct fd_list *)(_l))->next;	\
		fd_list_unlink(__li);					\
		free(__li);						\
	}

/* Destroy a structure once all cleanups have been performed */
int fd_peer_free(struct fd_peer ** ptr)
{
	struct fd_peer *p;
	void * t;
	
	TRACE_ENTRY("%p", ptr);
	CHECK_PARAMS(ptr);
	p = *ptr;
	*ptr = NULL;
	CHECK_PARAMS(p);
	
	CHECK_PARAMS( FD_IS_LIST_EMPTY(&p->p_hdr.chain) );
	
	free_null(p->p_hdr.info.pi_diamid); 
	free_null(p->p_hdr.info.pi_realm); 
	free_list( &p->p_hdr.info.pi_endpoints );
	/* Assume the security data is already freed */
	free_null(p->p_hdr.info.pi_prodname);
	free_list( &p->p_hdr.info.pi_apps );
	
	free_null(p->p_dbgorig);
	ASSERT(FD_IS_LIST_EMPTY(&p->p_expiry));
	ASSERT(FD_IS_LIST_EMPTY(&p->p_actives));
	
	CHECK_FCT( fd_thr_term(&p->p_psm) );
	while ( fd_fifo_tryget(p->p_events, &t) == 0 ) {
		struct fd_event * ev = t;
		TRACE_DEBUG(FULL, "Found event %d(%p) in queue of peer %p being destroyed", ev->code, ev->data, p);
		free(ev);
	}
	CHECK_FCT( fd_fifo_del(&p->p_events) );
	
	CHECK_FCT( fd_thr_term(&p->p_inthr) );
	while ( fd_fifo_tryget(p->p_recv, &t) == 0 ) {
		struct msg * m = t;
		TRACE_DEBUG(FULL, "Found message %p in incoming queue of peer %p being destroyed", m, p);
		/* We simply destroy, the remote peer will re-send to someone else...*/
		CHECK_FCT(fd_msg_free(m));
	}
	CHECK_FCT( fd_fifo_del(&p->p_recv) );
	
	CHECK_FCT( fd_thr_term(&p->p_outthr) );
	while ( fd_fifo_tryget(p->p_tosend, &t) == 0 ) {
		struct msg * m = t;
		TRACE_DEBUG(FULL, "Found message %p in outgoing queue of peer %p being destroyed, requeue", m, p);
		/* We simply requeue in global, the routing thread will re-handle it. */
		
	}
	CHECK_FCT( fd_fifo_del(&p->p_tosend) );
	
	while (!FD_IS_LIST_EMPTY(&p->p_sentreq)) {
		struct sentreq * sr = (struct sentreq *)(p->p_sentreq.next);
		fd_list_unlink(&sr->chain);
		TRACE_DEBUG(FULL, "Found message %p in list of sent requests to peer %p being destroyed, requeue (fallback)", sr->req, p);
		CHECK_FCT(fd_fifo_post(fd_g_outgoing, &sr->req));
		free(sr);
	}
	
	if (p->p_cnxctx) {
		TODO("destroy p->p_cnxctx");
	}
	
	if (p->p_cb)
		(*p->p_cb)(NULL, p->p_cb_data);
	
	free(p);
	
	return 0;
}

/* Terminate peer module (destroy all peers) */
int fd_peer_fini()
{
	struct fd_list * li;
	struct fd_list purge = FD_LIST_INITIALIZER(purge); /* Store zombie peers here */
	int list_empty;
	struct timespec	wait_until, now;
	
	TRACE_ENTRY();
	
	CHECK_FCT_DO(fd_p_expi_fini(), /* continue */);
	
	TRACE_DEBUG(INFO, "Sending terminate signal to all peer connections");
	
	CHECK_FCT_DO( pthread_rwlock_wrlock(&fd_g_peers_rw), /* continue */ );
	for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
		struct fd_peer * peer = (struct fd_peer *)li;
		
		if (peer->p_hdr.info.pi_state != STATE_ZOMBIE) {
			CHECK_FCT_DO( fd_psm_terminate(peer), /* continue */ );
		} else {
			li = li->prev; /* to avoid breaking the loop */
			fd_list_unlink(&peer->p_hdr.chain);
			fd_list_insert_before(&purge, &peer->p_hdr.chain);
		}
	}
	list_empty = FD_IS_LIST_EMPTY(&fd_g_peers);
	CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );
	
	if (!list_empty) {
		CHECK_SYS(  clock_gettime(CLOCK_REALTIME, &now)  );
		TRACE_DEBUG(INFO, "Waiting for connections shutdown... (%d sec max)", DPR_TIMEOUT);
		wait_until.tv_sec  = now.tv_sec + DPR_TIMEOUT;
		wait_until.tv_nsec = now.tv_nsec;
	}
	
	while ((!list_empty) && (TS_IS_INFERIOR(&now, &wait_until))) {
		
		/* Allow the PSM(s) to execute */
		pthread_yield();
		
		/* Remove zombie peers */
		CHECK_FCT_DO( pthread_rwlock_wrlock(&fd_g_peers_rw), /* continue */ );
		for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
			struct fd_peer * peer = (struct fd_peer *)li;
			if (peer->p_hdr.info.pi_state == STATE_ZOMBIE) {
				li = li->prev; /* to avoid breaking the loop */
				fd_list_unlink(&peer->p_hdr.chain);
				fd_list_insert_before(&purge, &peer->p_hdr.chain);
			}
		}
		list_empty = FD_IS_LIST_EMPTY(&fd_g_peers);
		CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );
	}
	
	if (!list_empty) {
		TRACE_DEBUG(INFO, "Forcing connections shutdown");
		CHECK_FCT_DO( pthread_rwlock_wrlock(&fd_g_peers_rw), /* continue */ );
		while (!FD_IS_LIST_EMPTY(&fd_g_peers)) {
			struct fd_peer * peer = (struct fd_peer *)(fd_g_peers.next);
			fd_psm_abord(peer);
			fd_list_unlink(&peer->p_hdr.chain);
			fd_list_insert_before(&purge, &peer->p_hdr.chain);
		}
		CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );
	}
	
	/* Free memory objects of all peers */
	while (!FD_IS_LIST_EMPTY(&purge)) {
		struct fd_peer * peer = (struct fd_peer *)(purge.next);
		fd_list_unlink(&peer->p_hdr.chain);
		fd_peer_free(&peer);
	}
	
	return 0;
}

/* Dump info of one peer */
void fd_peer_dump(struct fd_peer * peer, int details)
{
	if (peer->p_eyec != EYEC_PEER) {
		fd_log_debug("  Invalid peer @ %p !\n", peer);
		return;
	}

	fd_log_debug(">  %s\t%s", STATE_STR(peer->p_hdr.info.pi_state), peer->p_hdr.info.pi_diamid);
	if (details > INFO) {
		fd_log_debug("\t(rlm:%s)", peer->p_hdr.info.pi_realm);
		if (peer->p_hdr.info.pi_prodname)
			fd_log_debug("\t['%s' %u]", peer->p_hdr.info.pi_prodname, peer->p_hdr.info.pi_firmrev);
	}
	fd_log_debug("\n");
	if (details > FULL) {
		/* Dump all info */
		fd_log_debug("\tEntry origin : %s\n", peer->p_dbgorig);
		fd_log_debug("\tFlags : %s%s%s%s%s - %s%s%s\n", 
				peer->p_hdr.info.pi_flags.pro3 == PI_P3_DEFAULT ? "" :
					(peer->p_hdr.info.pi_flags.pro3 == PI_P3_IP ? "IP." : "IPv6."),
				peer->p_hdr.info.pi_flags.pro4 == PI_P4_DEFAULT ? "" :
					(peer->p_hdr.info.pi_flags.pro4 == PI_P4_TCP ? "TCP." : "SCTP."),
				peer->p_hdr.info.pi_flags.alg ? "PrefTCP." : "",
				peer->p_hdr.info.pi_flags.sec == PI_SEC_DEFAULT ? "" :
					(peer->p_hdr.info.pi_flags.sec == PI_SEC_NONE ? "IPSec." : "InbandTLS."),
				peer->p_hdr.info.pi_flags.exp ? "Expire." : "",
				peer->p_hdr.info.pi_flags.inband & PI_INB_NONE ? "InbandIPsecOK." : "",
				peer->p_hdr.info.pi_flags.inband & PI_INB_TLS ?  "InbandTLSOK." : "",
				peer->p_hdr.info.pi_flags.relay ? "Relay (0xffffff)" : "No relay"
				);
		fd_log_debug("\tLifetime : %d sec\n", peer->p_hdr.info.pi_lft);
		
		TODO("Dump remaining useful information");
	}
}

/* Dump the list of peers */
void fd_peer_dump_list(int details)
{
	struct fd_list * li;
	
	fd_log_debug("Dumping list of peers :\n");
	CHECK_FCT_DO( pthread_rwlock_rdlock(&fd_g_peers_rw), /* continue */ );
	
	for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
		struct fd_peer * np = (struct fd_peer *)li;
		fd_peer_dump(np, details);
	}
	
	CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );
}

