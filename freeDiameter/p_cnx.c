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

/* This file contains code used by a peer state machine to initiate a connection to remote peer */

struct next_conn {
	struct fd_list	chain;
	int		proto;	/* Protocol of the next attempt */
	sSS		ss;	/* The address, only for TCP */
	uint16_t	port;	/* The port, for SCTP (included in ss for TCP) */
	int		dotls;	/* Handshake TLS after connection ? */
};

static int prepare_connection_list(struct fd_peer * peer)
{
	/* Resolve peer address(es) if needed */
	if (FD_IS_LIST_EMPTY(&peer->p_hdr.info.pi_endpoints)) {
		struct addrinfo hints, *ai, *aip;
		int ret;

		memset(&hints, 0, sizeof(hints));
		hints.ai_flags = AI_ADDRCONFIG;
		ret = getaddrinfo(peer->p_hdr.info.pi_diamid, NULL, &hints, &ai);
		if (ret) {
			fd_log_debug("Unable to resolve address for peer '%s' (%s), aborting this connection\n", peer->p_hdr.info.pi_diamid, gai_strerror(ret));
			fd_psm_terminate( peer );
			return 0;
		}
		
		for (aip = ai; aip != NULL; aip = aip->ai_next) {
			CHECK_FCT( fd_ep_add_merge( &peer->p_hdr.info.pi_endpoints, aip->ai_addr, aip->ai_addrlen, EP_FL_DISC ) );
		}
		freeaddrinfo(ai);
	}
	
	/* Remove addresses from unwanted family */
	if (peer->p_hdr.info.config.pic_flags.pro3) {
		CHECK_FCT( fd_ep_filter_family(
					&peer->p_hdr.info.pi_endpoints, 
					(peer->p_hdr.info.config.pic_flags.pro3 == PI_P3_IP) ? 
						AF_INET 
						: AF_INET6));
	}
	
	TODO("Prepare the list in peer->p_connparams obeying the flags");
	
	TODO("Return an error if the list is empty in the end");
	
	return ENOTSUP;
}

static __inline__ void failed_connection_attempt(struct fd_peer * peer)
{
	/* Simply remove the first item in the list */
	struct fd_list * li = peer->p_connparams.next;
	fd_list_unlink(li);
	free(li);
}

static void empty_connection_list(struct fd_peer * peer)
{
	/* Remove all items */
	while (!FD_IS_LIST_EMPTY(&peer->p_connparams)) {
		failed_connection_attempt(peer);
	}
}


/* The thread that attempts the connection */
static void * connect_thr(void * arg)
{
	struct fd_peer * peer = arg;
	struct cnxctx * cnx = NULL;
	struct next_conn * nc = NULL;
	
	TRACE_ENTRY("%p", arg);
	CHECK_PARAMS_DO( CHECK_PEER(peer), return NULL );

	do {
		/* Rebuild the list if needed, if it is empty */
		if (FD_IS_LIST_EMPTY(&peer->p_connparams)) {
			CHECK_FCT_DO( prepare_connection_list(peer), goto fatal_error );
			if (FD_IS_LIST_EMPTY(&peer->p_connparams))
				/* Already logged and peer terminated */
				return NULL;
		}
		
		/* Attempt connection to the first entry */
		nc = (struct next_conn *)(peer->p_connparams.next);
		
		switch (nc->proto) {
			case IPPROTO_TCP:
				cnx = fd_cnx_cli_connect_tcp((sSA *)&nc->ss, sSSlen(&nc->ss));
				break;
#ifndef DISABLE_SCTP			
			case IPPROTO_SCTP:
				cnx = fd_cnx_cli_connect_sctp((peer->p_hdr.info.config.pic_flags.pro3 == PI_P3_IP) ?: fd_g_config->cnf_flags.no_ip6, nc->port, &peer->p_hdr.info.pi_endpoints);
				break;
#endif /* DISABLE_SCTP */
		}
		
		if (cnx)
			break;
		
		/* Pop these parameters and continue */
		failed_connection_attempt(peer);
		
		pthread_testcancel();
		
	} while (!cnx); /* and until cancellation */
	
	/* Now, we have an established connection in cnx */
	
	pthread_cleanup_push((void *)fd_cnx_destroy, cnx);
	
	/* Handshake if needed (secure port) */
	if (nc->dotls) {
		CHECK_FCT_DO( fd_cnx_handshake(cnx, GNUTLS_CLIENT, peer->p_hdr.info.config.pic_priority, NULL),
			{
				/* Handshake failed ...  */
				fd_log_debug("TLS Handshake failed with peer '%s', resetting the connection\n", peer->p_hdr.info.pi_diamid);
				fd_cnx_destroy(cnx);
				empty_connection_list(peer);
				fd_ep_filter(&peer->p_hdr.info.pi_endpoints, EP_FL_CONF);
				return NULL;
			} );
	}
	
	/* Upon success, generate FDEVP_CNX_ESTABLISHED */
	CHECK_FCT_DO( fd_event_send(peer->p_events, FDEVP_CNX_ESTABLISHED, 0, cnx), goto fatal_error );
	
	pthread_cleanup_pop(0);
	
	return NULL;
	
fatal_error:
	/* Cleanup the connection */
	if (cnx)
		fd_cnx_destroy(cnx);

	/* Generate a termination event */
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	
	return NULL;
}


/* Initiate a connection attempt to a remote peer */
int fd_p_cnx_init(struct fd_peer * peer)
{
	TRACE_ENTRY("%p", peer);
	
	/* Start the connect thread */
	CHECK_FCT( pthread_create(&peer->p_ini_thr, NULL, connect_thr, peer) );
	return 0;
}

/* Cancel a connection attempt */
void fd_p_cnx_abort(struct fd_peer * peer, int cleanup_all)
{
	TRACE_ENTRY("%p %d", peer, cleanup_all);
	CHECK_PARAMS_DO( CHECK_PEER(peer), return );
	
	if (peer->p_ini_thr != (pthread_t)NULL) {
		CHECK_FCT_DO( fd_thr_term(&peer->p_ini_thr), /* continue */);
		failed_connection_attempt(peer);
	}
	
	if (cleanup_all) {
		empty_connection_list(peer);
	}
}
