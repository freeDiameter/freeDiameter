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

/* List of active peers */
struct fd_list   fd_g_activ_peers = FD_LIST_INITIALIZER(fd_g_activ_peers);	/* peers linked by their p_actives oredered by p_diamid */
pthread_rwlock_t fd_g_activ_peers_rw = PTHREAD_RWLOCK_INITIALIZER;

/* List of validation callbacks (registered with fd_peer_validate_register) */
static struct fd_list validators = FD_LIST_INITIALIZER(validators);	/* list items are simple fd_list with "o" pointing to the callback */
static pthread_rwlock_t validators_rw = PTHREAD_RWLOCK_INITIALIZER;


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
	p->p_hdr.info.pi_port    = info->pi_port;
	p->p_hdr.info.pi_tctimer = info->pi_tctimer;
	p->p_hdr.info.pi_twtimer = info->pi_twtimer;
	
	if (info->pi_sec_data.priority) {
		CHECK_MALLOC( p->p_hdr.info.pi_sec_data.priority = strdup(info->pi_sec_data.priority) );
	}
	
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
		struct fd_peer * next = (struct fd_peer *)li;
		int cmp = strcasecmp( p->p_hdr.info.pi_diamid, next->p_hdr.info.pi_diamid );
		if (cmp > 0)
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
	
	CHECK_FCT( fd_thr_term(&p->p_outthr) );
	while ( fd_fifo_tryget(p->p_tosend, &t) == 0 ) {
		struct msg * m = t;
		TRACE_DEBUG(FULL, "Found message %p in outgoing queue of peer %p being destroyed, requeue", m, p);
		/* We simply requeue in global, the routing thread will re-handle it. */
		CHECK_FCT(fd_fifo_post(fd_g_outgoing, &m));
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
		fd_cnx_destroy(p->p_cnxctx);
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
		CHECK_SYS(  clock_gettime(CLOCK_REALTIME, &now)  );
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
	
	/* Now empty the validators list */
	CHECK_FCT_DO( pthread_rwlock_wrlock(&validators_rw), /* continue */ );
	while (!FD_IS_LIST_EMPTY( &validators )) {
		struct fd_list * v = validators.next;
		fd_list_unlink(v);
		free(v);
	}
	CHECK_FCT_DO( pthread_rwlock_unlock(&validators_rw), /* continue */ );
	
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
				peer->p_hdr.info.pi_flags.inband_none ? "InbandIPsec." : "",
				peer->p_hdr.info.pi_flags.inband_tls ?  "InbandTLS." : "",
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

/* Handle an incoming CER request on a new connection */
int fd_peer_handle_newCER( struct msg ** cer, struct cnxctx ** cnx )
{
	struct msg * msg;
	struct dict_object *avp_oh_model;
	avp_code_t code = AC_ORIGIN_HOST;
	struct avp *avp_oh;
	struct avp_hdr * avp_hdr;
	struct fd_list * li;
	int found = 0;
	int ret = 0;
	struct fd_peer * peer;
	struct cnx_incoming * ev_data;
	
	TRACE_ENTRY("%p %p", cer, cnx);
	CHECK_PARAMS(cer && *cer && cnx && *cnx);
	
	msg = *cer; 
	
	/* Find the Diameter Identity of the remote peer in the message */
	CHECK_FCT( fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_CODE, &code, &avp_oh_model, ENOENT) );
	CHECK_FCT( fd_msg_search_avp ( msg, avp_oh_model, &avp_oh ) );
	CHECK_FCT( fd_msg_avp_hdr ( avp_oh, &avp_hdr ) );
	
	/* Search if we already have this peer id in our list */
	CHECK_POSIX( pthread_rwlock_rdlock(&fd_g_peers_rw) );
	
	for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
		peer = (struct fd_peer *)li;
		int cmp = strncasecmp( avp_hdr->avp_value->os.data, peer->p_hdr.info.pi_diamid, avp_hdr->avp_value->os.len );
		if (cmp > 0)
			continue;
		if (cmp == 0)
			found = 1;
		break;
	}
	
	if (!found) {
		/* Create a new peer entry for this new remote peer */
		peer = NULL;
		CHECK_FCT_DO( ret = fd_peer_alloc(&peer), goto out );
		
		/* Set the peer Diameter Id and the responder flag parameters */
		CHECK_MALLOC_DO( peer->p_hdr.info.pi_diamid = malloc(avp_hdr->avp_value->os.len + 1), { ret = ENOMEM; goto out; } );
		CHECK_MALLOC_DO( peer->p_dbgorig = strdup(fd_cnx_getid(*cnx)), { ret = ENOMEM; goto out; } );
		peer->p_flags.pf_responder = 1;
		
		/* Upgrade the lock to write lock */
		CHECK_POSIX_DO( ret = pthread_rwlock_wrlock(&fd_g_peers_rw), goto out );
		
		/* Insert the new peer in the list (the PSM will take care of setting the expiry after validation) */
		fd_list_insert_before( li, &peer->p_hdr.chain );
		
		/* Release the write lock */
		CHECK_POSIX_DO( ret = pthread_rwlock_unlock(&fd_g_peers_rw), goto out );
		
		/* Start the PSM, which will receive the event bellow */
		CHECK_FCT_DO( ret = fd_psm_begin(peer), goto out );
	}
		
	/* Send the new connection event to the PSM */
	CHECK_MALLOC_DO( ev_data = malloc(sizeof(struct cnx_incoming)), { ret = ENOMEM; goto out; } );
	memset(ev_data, 0, sizeof(ev_data));
	
	ev_data->cer = msg;
	ev_data->cnx = *cnx;
	ev_data->validate = !found;
	
	CHECK_FCT_DO( ret = fd_event_send(peer->p_events, FDEVP_CNX_INCOMING, sizeof(ev_data), ev_data), goto out );
	
out:	
	CHECK_POSIX( pthread_rwlock_unlock(&fd_g_peers_rw) );

	if (ret == 0) {
		/* Reset the "out" parameters, so that they are not cleanup on function return. */
		*cer = NULL;
		*cnx = NULL;
	}
	
	return ret;
}

/* Save a callback to accept / reject incoming unknown peers */
int fd_peer_validate_register ( int (*peer_validate)(struct peer_info * /* info */, int * /* auth */, int (**cb2)(struct peer_info *)) )
{
	struct fd_list * v;
	
	TRACE_ENTRY("%p", peer_validate);
	CHECK_PARAMS(peer_validate);
	
	/* Alloc a new entry */
	CHECK_MALLOC( v = malloc(sizeof(struct fd_list)) );
	fd_list_init( v, peer_validate );
	
	/* Add at the beginning of the list */
	CHECK_FCT( pthread_rwlock_wrlock(&validators_rw) );
	fd_list_insert_after(&validators, v);
	CHECK_FCT( pthread_rwlock_unlock(&validators_rw));
	
	/* Done! */
	return 0;
}

/* Validate a peer by calling the callbacks in turn -- return 0 if the peer is validated, ! 0 in case of error (>0) or if the peer is rejected (-1) */
int fd_peer_validate( struct fd_peer * peer )
{
	int ret = 0;
	struct fd_list * v;
	
	CHECK_FCT( pthread_rwlock_rdlock(&validators_rw) );
	for (v = validators.next; v != &validators; v = v->next) {
		int auth = 0;
		pthread_cleanup_push(fd_cleanup_rwlock, &validators_rw);
		CHECK_FCT_DO( ret = ((int(*)(struct peer_info *, int *, int (**)(struct peer_info *)))(v->o)) (&peer->p_hdr.info, &auth, &peer->p_cb2), goto out );
		pthread_cleanup_pop(0);
		if (auth) {
			ret = (auth > 0) ? 0 : -1;
			goto out;
		}
		peer->p_cb2 = NULL;
	}
	
	/* No callback has given a firm result, the default is to reject */
	ret = -1;
out:
	CHECK_FCT( pthread_rwlock_unlock(&validators_rw));
	return ret;
}
