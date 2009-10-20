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
#include <netinet/sctp.h>
#include <sys/uio.h>

/* Pre-binding socket options -- # streams read in config */
static int fd_setsockopt_prebind(int sk)
{
	#ifdef DEBUG_SCTP
	socklen_t sz;
	#endif /* DEBUG_SCTP */
	
	TRACE_ENTRY( "%d", sk);
	
	CHECK_PARAMS( sk > 0 );
	
	/* Subscribe to some notifications */
	{
		struct sctp_event_subscribe event;

		memset(&event, 0, sizeof(event));
		event.sctp_data_io_event	= 1;	/* to receive the stream ID in SCTP_SNDRCV ancilliary data on message reception */
		event.sctp_association_event	= 0;	/* new or closed associations (mostly for one-to-many style sockets) */
		event.sctp_address_event	= 1;	/* address changes */
		event.sctp_send_failure_event	= 1;	/* delivery failures */
		event.sctp_peer_error_event	= 1;	/* remote peer sends an error */
		event.sctp_shutdown_event	= 1;	/* peer has sent a SHUTDOWN */
		event.sctp_partial_delivery_event = 1;	/* a partial delivery is aborted, probably indicating the connection is being shutdown */
		// event.sctp_adaptation_layer_event = 0;	/* adaptation layer notifications */
		// event.sctp_authentication_event = 0;	/* when new key is made active */

		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_EVENTS, &event, sizeof(event)) );
		
		#ifdef DEBUG_SCTP
		sz = sizeof(event);
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_EVENTS, &event, &sz) );
		if (sz != sizeof(event))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(event));
			return ENOTSUP;
		}
		
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_data_io_event          : %hhu", event.sctp_data_io_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_association_event      : %hhu", event.sctp_association_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_address_event          : %hhu", event.sctp_address_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_send_failure_event     : %hhu", event.sctp_send_failure_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_peer_error_event       : %hhu", event.sctp_peer_error_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_shutdown_event         : %hhu", event.sctp_shutdown_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_partial_delivery_event : %hhu", event.sctp_partial_delivery_event);
		TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_adaptation_layer_event : %hhu", event.sctp_adaptation_layer_event);
		// TRACE_DEBUG(FULL, "SCTP_EVENTS sctp_authentication_event   : %hhu", event.sctp_authentication_event);
		#endif /* DEBUG_SCTP */
		
	}
	
	/* Set the INIT parameters, such as number of streams */
	{
		struct sctp_initmsg init;
		memset(&init, 0, sizeof(init));
		
		#ifdef DEBUG_SCTP
		sz = sizeof(init);
		
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_INITMSG, &init, &sz)  );
		if (sz != sizeof(init))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(init));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_INITMSG sinit_num_ostreams   : %hu", init.sinit_num_ostreams);
		TRACE_DEBUG(FULL, "Def SCTP_INITMSG sinit_max_instreams  : %hu", init.sinit_max_instreams);
		TRACE_DEBUG(FULL, "Def SCTP_INITMSG sinit_max_attempts   : %hu", init.sinit_max_attempts);
		TRACE_DEBUG(FULL, "Def SCTP_INITMSG sinit_max_init_timeo : %hu", init.sinit_max_init_timeo);
		#endif /* DEBUG_SCTP */

		/* Set the init options -- need to receive SCTP_COMM_UP to confirm the requested parameters */
		init.sinit_num_ostreams	  = fd_g_config->cnf_sctp_str;	/* desired number of outgoing streams */
		init.sinit_max_init_timeo = CNX_TIMEOUT * 1000;

		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_INITMSG, &init, sizeof(init))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_INITMSG, &init, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_INITMSG sinit_num_ostreams   : %hu", init.sinit_num_ostreams);
		TRACE_DEBUG(FULL, "New SCTP_INITMSG sinit_max_instreams  : %hu", init.sinit_max_instreams);
		TRACE_DEBUG(FULL, "New SCTP_INITMSG sinit_max_attempts   : %hu", init.sinit_max_attempts);
		TRACE_DEBUG(FULL, "New SCTP_INITMSG sinit_max_init_timeo : %hu", init.sinit_max_init_timeo);
		#endif /* DEBUG_SCTP */
	}
	
	/* Set the SCTP_DISABLE_FRAGMENTS option, required for TLS */
	#ifdef SCTP_DISABLE_FRAGMENTS
	{
		int nofrag;
		
		#ifdef DEBUG_SCTP
		sz = sizeof(nofrag);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_DISABLE_FRAGMENTS, &nofrag, &sz)  );
		if (sz != sizeof(nofrag))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(nofrag));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_DISABLE_FRAGMENTS value : %s", nofrag ? "true" : "false");
		#endif /* DEBUG_SCTP */

		nofrag = 0;	/* We turn ON the fragmentation */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_DISABLE_FRAGMENTS, &nofrag, sizeof(nofrag))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_DISABLE_FRAGMENTS, &nofrag, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_DISABLE_FRAGMENTS value : %s", nofrag ? "true" : "false");
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_DISABLE_FRAGMENTS */
	# error "TLS requires support of SCTP_DISABLE_FRAGMENTS"
	#endif /* SCTP_DISABLE_FRAGMENTS */
	
	
	/* Set the RETRANSMIT parameters */
	#ifdef SCTP_RTOINFO
	{
		struct sctp_rtoinfo rtoinfo;
		memset(&rtoinfo, 0, sizeof(rtoinfo));

		#ifdef DEBUG_SCTP
		sz = sizeof(rtoinfo);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_RTOINFO, &rtoinfo, &sz)  );
		if (sz != sizeof(rtoinfo))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(rtoinfo));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_RTOINFO srto_initial : %u", rtoinfo.srto_initial);
		TRACE_DEBUG(FULL, "Def SCTP_RTOINFO srto_max     : %u", rtoinfo.srto_max);
		TRACE_DEBUG(FULL, "Def SCTP_RTOINFO srto_min     : %u", rtoinfo.srto_min);
		#endif /* DEBUG_SCTP */

		rtoinfo.srto_max     = fd_g_config->cnf_timer_tw * 500 - 1000;	/* Maximum retransmit timer (in ms) (set to Tw / 2 - 1) */

		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_RTOINFO, &rtoinfo, sizeof(rtoinfo))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_RTOINFO, &rtoinfo, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_RTOINFO srto_initial : %u", rtoinfo.srto_initial);
		TRACE_DEBUG(FULL, "New SCTP_RTOINFO srto_max     : %u", rtoinfo.srto_max);
		TRACE_DEBUG(FULL, "New SCTP_RTOINFO srto_min     : %u", rtoinfo.srto_min);
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_RTOINFO */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SCTP_RTOINFO");
	# endif /* DEBUG_SCTP */
	#endif /* SCTP_RTOINFO */
	
	/* Set the ASSOCIATION parameters */
	#ifdef SCTP_ASSOCINFO
	{
		struct sctp_assocparams assoc;
		memset(&assoc, 0, sizeof(assoc));

		#ifdef DEBUG_SCTP
		sz = sizeof(assoc);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_ASSOCINFO, &assoc, &sz)  );
		if (sz != sizeof(assoc))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(assoc));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_ASSOCINFO sasoc_asocmaxrxt               : %hu", assoc.sasoc_asocmaxrxt);
		TRACE_DEBUG(FULL, "Def SCTP_ASSOCINFO sasoc_number_peer_destinations : %hu", assoc.sasoc_number_peer_destinations);
		TRACE_DEBUG(FULL, "Def SCTP_ASSOCINFO sasoc_peer_rwnd                : %u" , assoc.sasoc_peer_rwnd);
		TRACE_DEBUG(FULL, "Def SCTP_ASSOCINFO sasoc_local_rwnd               : %u" , assoc.sasoc_local_rwnd);
		TRACE_DEBUG(FULL, "Def SCTP_ASSOCINFO sasoc_cookie_life              : %u" , assoc.sasoc_cookie_life);
		#endif /* DEBUG_SCTP */

		assoc.sasoc_asocmaxrxt = 5;	/* Maximum retransmission attempts: we want fast detection of errors */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_ASSOCINFO, &assoc, sizeof(assoc))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_ASSOCINFO, &assoc, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_ASSOCINFO sasoc_asocmaxrxt               : %hu", assoc.sasoc_asocmaxrxt);
		TRACE_DEBUG(FULL, "New SCTP_ASSOCINFO sasoc_number_peer_destinations : %hu", assoc.sasoc_number_peer_destinations);
		TRACE_DEBUG(FULL, "New SCTP_ASSOCINFO sasoc_peer_rwnd                : %u" , assoc.sasoc_peer_rwnd);
		TRACE_DEBUG(FULL, "New SCTP_ASSOCINFO sasoc_local_rwnd               : %u" , assoc.sasoc_local_rwnd);
		TRACE_DEBUG(FULL, "New SCTP_ASSOCINFO sasoc_cookie_life              : %u" , assoc.sasoc_cookie_life);
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_ASSOCINFO */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SCTP_ASSOCINFO");
	# endif /* DEBUG_SCTP */
	#endif /* SCTP_ASSOCINFO */
	
	
	/* The SO_LINGER option will be re-set if we want to perform SCTP ABORT */
	#ifdef SO_LINGER
	{
		struct linger linger;
		memset(&linger, 0, sizeof(linger));
		
		#ifdef DEBUG_SCTP
		sz = sizeof(linger);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, SOL_SOCKET, SO_LINGER, &linger, &sz)  );
		if (sz != sizeof(linger))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(linger));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SO_LINGER l_onoff  : %d", linger.l_onoff);
		TRACE_DEBUG(FULL, "Def SO_LINGER l_linger : %d", linger.l_linger);
		#endif /* DEBUG_SCTP */
		
		linger.l_onoff	= 0;	/* Do not activate the linger */
		linger.l_linger = 0;	/* Return immediately when closing (=> abort) */
		
		/* Set the option */
		CHECK_SYS(  setsockopt(sk, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, SOL_SOCKET, SO_LINGER, &linger, &sz)  );
		TRACE_DEBUG(FULL, "New SO_LINGER l_onoff  : %d", linger.l_onoff);
		TRACE_DEBUG(FULL, "New SO_LINGER l_linger : %d", linger.l_linger);
		#endif /* DEBUG_SCTP */
	}
	#else /* SO_LINGER */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SO_LINGER");
	# endif /* DEBUG_SCTP */
	#endif /* SO_LINGER */
	
	/* Set the NODELAY option (Nagle-like algorithm) */
	#ifdef SCTP_NODELAY
	{
		int nodelay;
		
		#ifdef DEBUG_SCTP
		sz = sizeof(nodelay);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, &sz)  );
		if (sz != sizeof(nodelay))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(nodelay));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_NODELAY value : %s", nodelay ? "true" : "false");
		#endif /* DEBUG_SCTP */

		nodelay = 0;	/* We turn ON the Nagle algorithm (probably the default already) */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, sizeof(nodelay))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_NODELAY, &nodelay, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_NODELAY value : %s", nodelay ? "true" : "false");
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_NODELAY */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SCTP_NODELAY");
	# endif /* DEBUG_SCTP */
	#endif /* SCTP_NODELAY */
	
	/* Set the interleaving option */
	#ifdef SCTP_FRAGMENT_INTERLEAVE
	{
		int interleave;
		
		#ifdef DEBUG_SCTP
		sz = sizeof(interleave);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_FRAGMENT_INTERLEAVE, &interleave, &sz)  );
		if (sz != sizeof(interleave))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(interleave));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_FRAGMENT_INTERLEAVE value : %d", interleave);
		#endif /* DEBUG_SCTP */

		#if 0
		interleave = 2;	/* Allow partial delivery on several streams at the same time, since we are stream-aware in our security modules */
		#else /* 0 */
		interleave = 1;	/* hmmm actually, we are not yet capable of handling this, and we don t need it. */
		#endif /* 0 */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_FRAGMENT_INTERLEAVE, &interleave, sizeof(interleave))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_FRAGMENT_INTERLEAVE, &interleave, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_FRAGMENT_INTERLEAVE value : %d", interleave);
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_FRAGMENT_INTERLEAVE */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SCTP_FRAGMENT_INTERLEAVE");
	# endif /* DEBUG_SCTP */
	#endif /* SCTP_FRAGMENT_INTERLEAVE */
	
	/* Set the v4 mapped addresses option */
	#ifdef SCTP_I_WANT_MAPPED_V4_ADDR
	{
		int v4mapped;
		
		#ifdef DEBUG_SCTP
		sz = sizeof(v4mapped);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR, &v4mapped, &sz)  );
		if (sz != sizeof(v4mapped))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(v4mapped));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_I_WANT_MAPPED_V4_ADDR value : %s", v4mapped ? "true" : "false");
		#endif /* DEBUG_SCTP */

		v4mapped = 0;	/* We don't want v4 mapped addresses */
		v4mapped = 1;	/* but we have to, otherwise the bind fails in linux currently ... (Ok, It'd be better with a cmake test, any volunteer?) */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR, &v4mapped, sizeof(v4mapped))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_I_WANT_MAPPED_V4_ADDR, &v4mapped, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_I_WANT_MAPPED_V4_ADDR value : %s", v4mapped ? "true" : "false");
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_I_WANT_MAPPED_V4_ADDR */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SCTP_I_WANT_MAPPED_V4_ADDR");
	# endif /* DEBUG_SCTP */
	#endif /* SCTP_I_WANT_MAPPED_V4_ADDR */
			   
			   
	/* Other settable options (draft-ietf-tsvwg-sctpsocket-17):
	   SO_RCVBUF			size of receiver window
	   SO_SNDBUF			size of pending data to send
	   SCTP_AUTOCLOSE		for one-to-many only
	   SCTP_SET_PEER_PRIMARY_ADDR	ask remote peer to use this local address as primary
	   SCTP_PRIMARY_ADDR		use this address as primary locally
	   SCTP_ADAPTATION_LAYER	set adaptation layer indication 
	   SCTP_PEER_ADDR_PARAMS	control heartbeat per peer address
	   SCTP_DEFAULT_SEND_PARAM	parameters for the sendto() call
	   SCTP_MAXSEG			max size of fragmented segments -- bound to PMTU
	   SCTP_AUTH_CHUNK		request authentication of some type of chunk
	    SCTP_HMAC_IDENT		authentication algorithms
	    SCTP_AUTH_KEY		set a shared key
	    SCTP_AUTH_ACTIVE_KEY	set the active key
	    SCTP_AUTH_DELETE_KEY	remove a key
	    SCTP_AUTH_DEACTIVATE_KEY	will not use that key anymore
	   SCTP_DELAYED_SACK		control delayed acks
	   SCTP_PARTIAL_DELIVERY_POINT	control partial delivery size
	   SCTP_USE_EXT_RCVINFO		use extended receive info structure (information about the next message if available)
	   SCTP_MAX_BURST		number of packets that can be burst emitted
	   SCTP_CONTEXT			save a context information along with the association.
	   SCTP_EXPLICIT_EOR		enable sending one message across several send calls
	   SCTP_REUSE_PORT		share one listening port with several sockets
	   
	   read-only options:
	   SCTP_STATUS			retrieve info such as number of streams, pending packets, state, ...
	   SCTP_GET_PEER_ADDR_INFO	get information about a specific peer address of the association.
	   SCTP_PEER_AUTH_CHUNKS	list of chunks the remote peer wants authenticated
	   SCTP_LOCAL_AUTH_CHUNKS	list of chunks the local peer wants authenticated
	   SCTP_GET_ASSOC_NUMBER	number of associations in a one-to-many socket
	   SCTP_GET_ASSOC_ID_LIST	list of these associations
	*/
	
	/* In case of no_ip4, force the v6only option */
	#ifdef IPV6_V6ONLY
	if (fd_g_config->cnf_flags.no_ip4) {
		int opt = 1;
		CHECK_SYS(setsockopt(sk, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)));
	}
	#endif /* IPV6_V6ONLY */
	
	return 0;
}


/* Post-binding socket options */
static int fd_setsockopt_postbind(int sk, int bound_to_default)
{
	TRACE_ENTRY( "%d %d", sk, bound_to_default);
	
	CHECK_PARAMS( (sk > 0) );
	
	/* Set the ASCONF option */
	#ifdef SCTP_AUTO_ASCONF
	{
		int asconf;
		
		#ifdef DEBUG_SCTP
		socklen_t sz;
		
		sz = sizeof(asconf);
		/* Read socket defaults */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_AUTO_ASCONF, &asconf, &sz)  );
		if (sz != sizeof(asconf))
		{
			TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %d", sz, (socklen_t)sizeof(asconf));
			return ENOTSUP;
		}
		TRACE_DEBUG(FULL, "Def SCTP_AUTO_ASCONF value : %s", asconf ? "true" : "false");
		#endif /* DEBUG_SCTP */

		asconf = bound_to_default ? 1 : 0;	/* allow automatic use of added or removed addresses in the association (for bound-all sockets) */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_AUTO_ASCONF, &asconf, sizeof(asconf))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_AUTO_ASCONF, &asconf, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_AUTO_ASCONF value : %s", asconf ? "true" : "false");
		#endif /* DEBUG_SCTP */
	}
	#else /* SCTP_AUTO_ASCONF */
	# ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Skipping SCTP_AUTO_ASCONF");
	# endif /* DEBUG_SCTP */
	#endif /* SCTP_AUTO_ASCONF */
	
	return 0;
}

/* Create a socket server and bind it according to daemon s configuration */
int fd_sctp_create_bind_server( int * sock, struct fd_list * list, uint16_t port )
{
	int family;
	int bind_default;
	
	TRACE_ENTRY("%p %p %hu", sock, list, port);
	CHECK_PARAMS(sock);
	
	if (fd_g_config->cnf_flags.no_ip6) {
		family = AF_INET;
	} else {
		family = AF_INET6; /* can create socket for both IP and IPv6 */
	}
	
	/* Create the socket */
	CHECK_SYS( *sock = socket(family, SOCK_STREAM, IPPROTO_SCTP) );
	
	/* Set pre-binding socket options, including number of streams etc... */
	CHECK_FCT( fd_setsockopt_prebind(*sock) );
	
	bind_default = (! list) || (FD_IS_LIST_EMPTY(list)) ;
redo:
	if ( bind_default ) {
		/* Implicit endpoints : bind to default addresses */
		union {
			sSS  ss;
			sSA  sa;
			sSA4 sin;
			sSA6 sin6;
		} s;
		
		/* 0.0.0.0 and [::] are all zeros */
		memset(&s, 0, sizeof(s));
		
		s.sa.sa_family = family;
		
		if (family == AF_INET)
			s.sin.sin_port = htons(port);
		else
			s.sin6.sin6_port = htons(port);
		
		CHECK_SYS( bind(*sock, &s.sa, sizeof(s)) );
		
	} else {
		/* Explicit endpoints to bind to from config */
		union {
			sSA  	*sa;
			sSA4 	*sin;
			sSA6 	*sin6;
		} sar; /* build the list of endpoints to bind to */
		int count = 0; /* number of sock addr in sar array */
		struct fd_list * li;
		
		sar.sa = NULL;
		
		/* Create a flat array from the list of configured addresses */
		for (li = list->next; li != list; li = li->next) {
			struct fd_endpoint * ep = (struct fd_endpoint *)li;
			
			if ( ! ep->meta.conf )
				continue;
			
			count++;
			if (fd_g_config->cnf_flags.no_ip6) {
				ASSERT(ep->sa.sa_family == AF_INET);
				CHECK_MALLOC( sar.sa = realloc(sar.sa, count * sizeof(sSA4))  );
				memcpy(&sar.sin[count - 1], &ep->sin, sizeof(sSA4));
				sar.sin[count - 1].sin_port = htons(port);
			} else {
				/* Pass all addresses as IPv6, eventually mapped -- we already filtered out IPv4 addresses if no_ip4 flag is set */
				CHECK_MALLOC( sar.sa = realloc(sar.sa, count * sizeof(sSA6))  );
				if (ep->sa.sa_family == AF_INET) {
					memset(&sar.sin6[count - 1], 0, sizeof(sSA6));
					sar.sin6[count - 1].sin6_family = AF_INET6;
					IN6_ADDR_V4MAP( &sar.sin6[count - 1].sin6_addr.s6_addr, ep->sin.sin_addr.s_addr );
				} else {
					memcpy(&sar.sin6[count - 1], &ep->sin6, sizeof(sSA6));
				}
				sar.sin6[count - 1].sin6_port = htons(port);
			}
		}
		
		if (!count) {
			/* None of the addresses in the list came from configuration, we bind to default */
			bind_default = 1;
			goto redo;
		}
		
		# ifdef DEBUG_SCTP
		if (TRACE_BOOL(FULL)) {
			int i;
			fd_log_debug("Calling sctp_bindx with the following array:\n");
			for (i = 0; i < count; i++) {
				fd_log_debug("    - ");
				sSA_DUMP_NODE_SERV( (sar.sa[0].sa_family == AF_INET) ? (sSA *)(&sar.sin[i]) : (sSA *)(&sar.sin6[i]), NI_NUMERICHOST | NI_NUMERICSERV );
				fd_log_debug("\n");
			}
		}
		#endif /* DEBUG_SCTP */
		
		CHECK_SYS(  sctp_bindx(*sock, sar.sa, count, SCTP_BINDX_ADD_ADDR)  );
		
	}
	
	/* Now, the server is bound, set remaining sockopt */
	CHECK_FCT( fd_setsockopt_postbind(*sock, bind_default) );
	
	#ifdef DEBUG_SCTP
	/* Debug: show all local listening addresses */
	if (TRACE_BOOL(FULL)) {
		sSA *sa, *sar;
		int sz;
		
		CHECK_SYS(  sz = sctp_getladdrs(*sock, 0, &sar)  );
		
		fd_log_debug("SCTP server bound on :\n");
		for (sa = sar; sz-- > 0; sa = (sSA *)(((uint8_t *)sa) + ((sa->sa_family == AF_INET) ? sizeof(sSA4) : sizeof(sSA6)))) {
			fd_log_debug("    - ");
			sSA_DUMP_NODE_SERV( sa, NI_NUMERICHOST | NI_NUMERICSERV );
			fd_log_debug("\n");
		}
		sctp_freeladdrs(sar);
	}
	#endif /* DEBUG_SCTP */

	return 0;
}

/* Allow clients connections on server sockets */
int fd_sctp_listen( int sock )
{
	TRACE_ENTRY("%d", sock);
	CHECK_SYS( listen(sock, 5) );
	return 0;
}

/* Retrieve streams information from a connected association */
int fd_sctp_get_str_info( int socket, int *in, int *out )
{
	TODO("Retrieve streams info from the socket");
	
	return ENOTSUP;
}

/* Get the list of local endpoints of the socket */
int fd_sctp_get_local_ep(int sock, struct fd_list * list)
{
	union {
		sSA	*sa;
		sSA4	*sin;
		sSA6	*sin6;
		uint8_t	*buf;
	} ptr;
	
	sSA * data;
	int count;
	
	TRACE_ENTRY("%d %p", sock, list);
	CHECK_PARAMS(list);
	
	/* Read the list on the socket */
	CHECK_SYS( count = sctp_getladdrs(sock, 0, &data)  );
	ptr.sa = data;
	
	while (count) {
		TODO("get the data from ptr");
		TODO("Increment ptr to the next sa in data");
		
		count --;
	}
	
	/* And now, free the list and return */
	sctp_freeladdrs(data);
	
	return ENOTSUP;
}

/* Get the list of remote endpoints of the socket */
int fd_sctp_get_remote_ep(int sock, struct fd_list * list)
{
	TODO("SCTP: sctp_getpaddrs");
		
	
	return ENOTSUP;
}

