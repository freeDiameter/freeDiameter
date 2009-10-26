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
#include "cnxctx.h"

#include <netinet/sctp.h>
#include <sys/uio.h>

/* Size of buffer to receive ancilliary data. May need to be enlarged if more sockopt are set... */
#ifndef CMSG_BUF_LEN
#define CMSG_BUF_LEN	1024
#endif /* CMSG_BUF_LEN */

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
		
		TRACE_DEBUG(FULL, "SCTP_EVENTS : sctp_data_io_event          : %hhu", event.sctp_data_io_event);
		TRACE_DEBUG(FULL, "       	 sctp_association_event      : %hhu", event.sctp_association_event);
		TRACE_DEBUG(FULL, "       	 sctp_address_event	     : %hhu", event.sctp_address_event);
		TRACE_DEBUG(FULL, "       	 sctp_send_failure_event     : %hhu", event.sctp_send_failure_event);
		TRACE_DEBUG(FULL, "       	 sctp_peer_error_event       : %hhu", event.sctp_peer_error_event);
		TRACE_DEBUG(FULL, "       	 sctp_shutdown_event	     : %hhu", event.sctp_shutdown_event);
		TRACE_DEBUG(FULL, "       	 sctp_partial_delivery_event : %hhu", event.sctp_partial_delivery_event);
		TRACE_DEBUG(FULL, "       	 sctp_adaptation_layer_event : %hhu", event.sctp_adaptation_layer_event);
		// TRACE_DEBUG(FULL, "             sctp_authentication_event    : %hhu", event.sctp_authentication_event);
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
		TRACE_DEBUG(FULL, "Def SCTP_INITMSG : sinit_num_ostreams   : %hu", init.sinit_num_ostreams);
		TRACE_DEBUG(FULL, "                   sinit_max_instreams  : %hu", init.sinit_max_instreams);
		TRACE_DEBUG(FULL, "                   sinit_max_attempts   : %hu", init.sinit_max_attempts);
		TRACE_DEBUG(FULL, "                   sinit_max_init_timeo : %hu", init.sinit_max_init_timeo);
		#endif /* DEBUG_SCTP */

		/* Set the init options -- need to receive SCTP_COMM_UP to confirm the requested parameters */
		init.sinit_num_ostreams	  = fd_g_config->cnf_sctp_str;	/* desired number of outgoing streams */
		init.sinit_max_init_timeo = CNX_TIMEOUT * 1000;

		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_INITMSG, &init, sizeof(init))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_INITMSG, &init, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_INITMSG : sinit_num_ostreams   : %hu", init.sinit_num_ostreams);
		TRACE_DEBUG(FULL, "                   sinit_max_instreams  : %hu", init.sinit_max_instreams);
		TRACE_DEBUG(FULL, "                   sinit_max_attempts   : %hu", init.sinit_max_attempts);
		TRACE_DEBUG(FULL, "                   sinit_max_init_timeo : %hu", init.sinit_max_init_timeo);
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
		TRACE_DEBUG(FULL, "Def SCTP_RTOINFO : srto_initial : %u", rtoinfo.srto_initial);
		TRACE_DEBUG(FULL, "                   srto_max     : %u", rtoinfo.srto_max);
		TRACE_DEBUG(FULL, "                   srto_min     : %u", rtoinfo.srto_min);
		#endif /* DEBUG_SCTP */

		rtoinfo.srto_max     = fd_g_config->cnf_timer_tw * 500 - 1000;	/* Maximum retransmit timer (in ms) (set to Tw / 2 - 1) */

		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_RTOINFO, &rtoinfo, sizeof(rtoinfo))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_RTOINFO, &rtoinfo, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_RTOINFO : srto_initial : %u", rtoinfo.srto_initial);
		TRACE_DEBUG(FULL, "                   srto_max     : %u", rtoinfo.srto_max);
		TRACE_DEBUG(FULL, "                   srto_min     : %u", rtoinfo.srto_min);
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
		TRACE_DEBUG(FULL, "Def SCTP_ASSOCINFO : sasoc_asocmaxrxt               : %hu", assoc.sasoc_asocmaxrxt);
		TRACE_DEBUG(FULL, "                     sasoc_number_peer_destinations : %hu", assoc.sasoc_number_peer_destinations);
		TRACE_DEBUG(FULL, "                     sasoc_peer_rwnd                : %u" , assoc.sasoc_peer_rwnd);
		TRACE_DEBUG(FULL, "                     sasoc_local_rwnd               : %u" , assoc.sasoc_local_rwnd);
		TRACE_DEBUG(FULL, "                     sasoc_cookie_life              : %u" , assoc.sasoc_cookie_life);
		#endif /* DEBUG_SCTP */

		assoc.sasoc_asocmaxrxt = 5;	/* Maximum retransmission attempts: we want fast detection of errors */
		
		/* Set the option to the socket */
		CHECK_SYS(  setsockopt(sk, IPPROTO_SCTP, SCTP_ASSOCINFO, &assoc, sizeof(assoc))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, IPPROTO_SCTP, SCTP_ASSOCINFO, &assoc, &sz)  );
		TRACE_DEBUG(FULL, "New SCTP_ASSOCINFO : sasoc_asocmaxrxt               : %hu", assoc.sasoc_asocmaxrxt);
		TRACE_DEBUG(FULL, "                     sasoc_number_peer_destinations : %hu", assoc.sasoc_number_peer_destinations);
		TRACE_DEBUG(FULL, "                     sasoc_peer_rwnd                : %u" , assoc.sasoc_peer_rwnd);
		TRACE_DEBUG(FULL, "                     sasoc_local_rwnd               : %u" , assoc.sasoc_local_rwnd);
		TRACE_DEBUG(FULL, "                     sasoc_cookie_life              : %u" , assoc.sasoc_cookie_life);
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
		TRACE_DEBUG(FULL, "Def SO_LINGER : l_onoff  : %d", linger.l_onoff);
		TRACE_DEBUG(FULL, "                l_linger : %d", linger.l_linger);
		#endif /* DEBUG_SCTP */
		
		linger.l_onoff	= 0;	/* Do not activate the linger */
		linger.l_linger = 0;	/* Return immediately when closing (=> abort) */
		
		/* Set the option */
		CHECK_SYS(  setsockopt(sk, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger))  );
		
		#ifdef DEBUG_SCTP
		/* Check new values */
		CHECK_SYS(  getsockopt(sk, SOL_SOCKET, SO_LINGER, &linger, &sz)  );
		TRACE_DEBUG(FULL, "New SO_LINGER : l_onoff  : %d", linger.l_onoff);
		TRACE_DEBUG(FULL, "                l_linger : %d", linger.l_linger);
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

#ifndef SCTP_USE_MAPPED_ADDRESSES
		v4mapped = 0;	/* We don't want v4 mapped addresses */
#else /* SCTP_USE_MAPPED_ADDRESSES */
		v4mapped = 1;	/* but we may have to, otherwise the bind fails in some environments */
#endif /* SCTP_USE_MAPPED_ADDRESSES */
		
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
	
	/* In case of no_ip4, force the v6only option -- is it a valid option for SCTP ? */
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
			sSA     * sa;
			sSA4	*sin;
			sSA6	*sin6;
			uint8_t *buf;
		} ptr;
		union {
			sSA     * sa;
			uint8_t * buf;
		} sar;
		int count = 0; /* number of sock addr in sar array */
		size_t offset = 0;
		struct fd_list * li;
		
		sar.buf = NULL;
		
		/* Create a flat array from the list of configured addresses */
		for (li = list->next; li != list; li = li->next) {
			struct fd_endpoint * ep = (struct fd_endpoint *)li;
			size_t sz = 0;
			
			if (! (ep->flags & EP_FL_CONF))
				continue;
			
			count++;
			
			/* Size of the new SA we are adding (sar may contain a mix of sockaddr_in and sockaddr_in6) */
#ifndef SCTP_USE_MAPPED_ADDRESSES
			if (ep->sa.sa_family == AF_INET6)
#else /* SCTP_USE_MAPPED_ADDRESSES */
			if (family == AF_INET6)
#endif /* SCTP_USE_MAPPED_ADDRESSES */
				sz = sizeof(sSA6);
			else
				sz = sizeof(sSA4);
			
			/* augment sar to contain the additional info */
			CHECK_MALLOC( sar.buf = realloc(sar.buf, offset + sz)  );
			
			ptr.buf = sar.buf + offset; /* place of the new SA */
			offset += sz; /* update to end of sar */
			
			if (sz == sizeof(sSA4)) {
				memcpy(ptr.buf, &ep->sin, sz);
				ptr.sin->sin_port = htons(port);
			} else {
				if (ep->sa.sa_family == AF_INET) { /* We must map the address */ 
					memset(ptr.buf, 0, sz);
					ptr.sin6->sin6_family = AF_INET6;
					IN6_ADDR_V4MAP( &ptr.sin6->sin6_addr.s6_addr, ep->sin.sin_addr.s_addr );
				} else {
					memcpy(ptr.sin6, &ep->sin6, sz);
				}
				ptr.sin6->sin6_port = htons(port);
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
			ptr.buf = sar.buf;
			fd_log_debug("Calling sctp_bindx with the following address array:\n");
			for (i = 0; i < count; i++) {
				TRACE_DEBUG_sSA(FULL, "    - ", ptr.sa, NI_NUMERICHOST | NI_NUMERICSERV, "" );
				ptr.buf += (ptr.sa->sa_family == AF_INET) ? sizeof(sSA4) : sizeof(sSA6) ;
			}
		}
		#endif /* DEBUG_SCTP */
		
		/* Bind to this array */
		CHECK_SYS(  sctp_bindx(*sock, sar.sa, count, SCTP_BINDX_ADD_ADDR)  );
		
		/* We don't need sar anymore */
		free(sar.buf);
	}
	
	/* Now, the server is bound, set remaining sockopt */
	CHECK_FCT( fd_setsockopt_postbind(*sock, bind_default) );
	
	#ifdef DEBUG_SCTP
	/* Debug: show all local listening addresses */
	if (TRACE_BOOL(FULL)) {
		sSA *sar;
		union {
			sSA	*sa;
			uint8_t *buf;
		} ptr;
		int sz;
		
		CHECK_SYS(  sz = sctp_getladdrs(*sock, 0, &sar)  );
		
		fd_log_debug("SCTP server bound on :\n");
		for (ptr.sa = sar; sz-- > 0; ptr.buf += (ptr.sa->sa_family == AF_INET) ? sizeof(sSA4) : sizeof(sSA6)) {
			TRACE_DEBUG_sSA(FULL, "    - ", ptr.sa, NI_NUMERICHOST | NI_NUMERICSERV, "" );
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

/* Create a client socket and connect to remote server */
int fd_sctp_client( int *sock, int no_ip6, uint16_t port, struct fd_list * list )
{
	int family;
	int count = 0;
	size_t offset = 0, sz;
	union {
		uint8_t *buf;
		sSA	*sa;
	} sar;
	union {
		uint8_t *buf;
		sSA	*sa;
		sSA4	*sin;
		sSA6	*sin6;
	} ptr;
	struct fd_list * li;
	int ret;
	
	sar.buf = NULL;
	
	TRACE_ENTRY("%p %i %hu %p", sock, no_ip6, port, list);
	CHECK_PARAMS( sock && list && (!FD_IS_LIST_EMPTY(list)) );
	
	if (no_ip6) {
		family = AF_INET;
	} else {
		family = AF_INET6;
	}
	
	/* Create the socket */
	CHECK_SYS( *sock = socket(family, SOCK_STREAM, IPPROTO_SCTP) );
	
	/* Cleanup if we are cancelled */
	pthread_cleanup_push(fd_cleanup_socket, sock);
	
	/* Set the socket options */
	CHECK_FCT_DO( ret = fd_setsockopt_prebind(*sock), goto fail );
	
	/* Create the array of addresses for sctp_connectx */
	for (li = list->next; li != list; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *) li;
		
		count++;
		
		/* Size of the new SA we are adding (sar may contain a mix of sockaddr_in and sockaddr_in6) */
#ifndef SCTP_USE_MAPPED_ADDRESSES
		if (ep->sa.sa_family == AF_INET6)
#else /* SCTP_USE_MAPPED_ADDRESSES */
		if (family == AF_INET6)
#endif /* SCTP_USE_MAPPED_ADDRESSES */
			sz = sizeof(sSA6);
		else
			sz = sizeof(sSA4);
		
		/* augment sar to contain the additional info */
		CHECK_MALLOC_DO( sar.buf = realloc(sar.buf, offset + sz), { ret = ENOMEM; goto fail; }  );

		ptr.buf = sar.buf + offset; /* place of the new SA */
		offset += sz; /* update to end of sar */
			
		if (sz == sizeof(sSA4)) {
			memcpy(ptr.buf, &ep->sin, sz);
			ptr.sin->sin_port = htons(port);
		} else {
			if (ep->sa.sa_family == AF_INET) { /* We must map the address */ 
				memset(ptr.buf, 0, sz);
				ptr.sin6->sin6_family = AF_INET6;
				IN6_ADDR_V4MAP( &ptr.sin6->sin6_addr.s6_addr, ep->sin.sin_addr.s_addr );
			} else {
				memcpy(ptr.sin6, &ep->sin6, sz);
			}
			ptr.sin6->sin6_port = htons(port);
		}
	}
	
	/* Try connecting */
	TRACE_DEBUG(FULL, "Attempting SCTP connection (%d addresses attempted)...", count);
	CHECK_SYS_DO( sctp_connectx(*sock, sar.sa, count), { ret = errno; goto fail; } );
	free(sar.buf); sar.buf = NULL;
	
	/* Set the remaining sockopts */
	CHECK_FCT_DO( ret = fd_setsockopt_postbind(*sock, 1), goto fail );
	
	/* Done! */
	pthread_cleanup_pop(0);
	return 0;
	
fail:
	if (*sock > 0) {
		shutdown(*sock, SHUT_RDWR);
		*sock = -1;
	}
	free(sar.buf);
	return ret;
}

/* Retrieve streams information from a connected association -- optionaly provide the primary address */
int fd_sctp_get_str_info( int sock, uint16_t *in, uint16_t *out, sSS *primary )
{
	struct sctp_status status;
	socklen_t sz = sizeof(status);
	
	TRACE_ENTRY("%d %p %p %p", sock, in, out, primary);
	CHECK_PARAMS( (sock > 0) && in && out );
	
	/* Read the association parameters */
	memset(&status, 0, sizeof(status));
	CHECK_SYS(  getsockopt(sock, IPPROTO_SCTP, SCTP_STATUS, &status, &sz) );
	if (sz != sizeof(status))
	{
		TRACE_DEBUG(INFO, "Invalid size of socket option: %d / %g", sz, sizeof(status));
		return ENOTSUP;
	}
	#ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "SCTP_STATUS : sstat_state                  : %i" , status.sstat_state);
	TRACE_DEBUG(FULL, "              sstat_rwnd  	              : %u" , status.sstat_rwnd);
	TRACE_DEBUG(FULL, "		 sstat_unackdata	      : %hu", status.sstat_unackdata);
	TRACE_DEBUG(FULL, "		 sstat_penddata 	      : %hu", status.sstat_penddata);
	TRACE_DEBUG(FULL, "		 sstat_instrms  	      : %hu", status.sstat_instrms);
	TRACE_DEBUG(FULL, "		 sstat_outstrms 	      : %hu", status.sstat_outstrms);
	TRACE_DEBUG(FULL, "		 sstat_fragmentation_point    : %u" , status.sstat_fragmentation_point);
	TRACE_DEBUG_sSA(FULL, "		 sstat_primary.spinfo_address : ", &status.sstat_primary.spinfo_address, NI_NUMERICHOST | NI_NUMERICSERV, "" );
	TRACE_DEBUG(FULL, "		 sstat_primary.spinfo_state   : %d" , status.sstat_primary.spinfo_state);
	TRACE_DEBUG(FULL, "		 sstat_primary.spinfo_cwnd    : %u" , status.sstat_primary.spinfo_cwnd);
	TRACE_DEBUG(FULL, "		 sstat_primary.spinfo_srtt    : %u" , status.sstat_primary.spinfo_srtt);
	TRACE_DEBUG(FULL, "		 sstat_primary.spinfo_rto     : %u" , status.sstat_primary.spinfo_rto);
	TRACE_DEBUG(FULL, "		 sstat_primary.spinfo_mtu     : %u" , status.sstat_primary.spinfo_mtu);
	#endif /* DEBUG_SCTP */
	
	*in = status.sstat_instrms;
	*out = status.sstat_outstrms;
	
	if (primary)
		memcpy(primary, &status.sstat_primary.spinfo_address, sizeof(sSS));
	
	return 0;
}

/* Get the list of local endpoints of the socket */
int fd_sctp_get_local_ep(int sock, struct fd_list * list)
{
	union {
		sSA	*sa;
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
		socklen_t sl;
		switch (ptr.sa->sa_family) {
			case AF_INET:	sl = sizeof(sSA4); break;
			case AF_INET6:	sl = sizeof(sSA6); break;
			default:
				TRACE_DEBUG(INFO, "Unkown address family returned in sctp_getladdrs: %d", ptr.sa->sa_family);
		}
				
		CHECK_FCT( fd_ep_add_merge( list, ptr.sa, sl, EP_FL_LL ) );
		ptr.buf += sl;
		count --;
	}
	
	/* Free the list */
	sctp_freeladdrs(data);
	
	/* Now get the primary address, the add function will take care of merging with existing entry */
	{
		 
		struct sctp_status status;
		socklen_t sz = sizeof(status);
		int ret;
		
		memset(&status, 0, sizeof(status));
		/* Attempt to use SCTP_STATUS message to retrieve the primary address */
		ret = getsockopt(sock, IPPROTO_SCTP, SCTP_STATUS, &status, &sz);
		if (sz != sizeof(status))
			ret = -1;
		sz = sizeof(sSS);
		if (ret < 0)
		{
			/* Fallback to getsockname -- not recommended by draft-ietf-tsvwg-sctpsocket-19#section-7.4 */
			CHECK_SYS(getsockname(sock, (sSA *)&status.sstat_primary.spinfo_address, &sz));
		}
			
		CHECK_FCT( fd_ep_add_merge( list, (sSA *)&status.sstat_primary.spinfo_address, sz, EP_FL_PRIMARY ) );
	}
	
	return 0;
}

/* Get the list of remote endpoints of the socket */
int fd_sctp_get_remote_ep(int sock, struct fd_list * list)
{
	union {
		sSA	*sa;
		uint8_t	*buf;
	} ptr;
	
	sSA * data;
	int count;
	
	TRACE_ENTRY("%d %p", sock, list);
	CHECK_PARAMS(list);
	
	/* Read the list on the socket */
	CHECK_SYS( count = sctp_getpaddrs(sock, 0, &data)  );
	ptr.sa = data;
	
	while (count) {
		socklen_t sl;
		switch (ptr.sa->sa_family) {
			case AF_INET:	sl = sizeof(sSA4); break;
			case AF_INET6:	sl = sizeof(sSA6); break;
			default:
				TRACE_DEBUG(INFO, "Unkown address family returned in sctp_getpaddrs: %d", ptr.sa->sa_family);
		}
				
		CHECK_FCT( fd_ep_add_merge( list, ptr.sa, sl, EP_FL_LL ) );
		ptr.buf += sl;
		count --;
	}
	
	/* Free the list */
	sctp_freepaddrs(data);
	
	/* Now get the primary address, the add function will take care of merging with existing entry */
	{
		sSS ss;
		socklen_t sl = sizeof(sSS);
	
		CHECK_SYS(getpeername(sock, (sSA *)&ss, &sl));
		CHECK_FCT( fd_ep_add_merge( list, (sSA *)&ss, sl, EP_FL_PRIMARY ) );
	}
	
	/* Done! */
	return 0;
}

/* Send a buffer over a specified stream */
int fd_sctp_sendstr(int sock, uint16_t strid, uint8_t * buf, size_t len)
{
	struct msghdr mhdr;
	struct iovec  iov;
	struct {
		struct cmsghdr 		hdr;
		struct sctp_sndrcvinfo	sndrcv;
	} anci;
	ssize_t ret;
	
	TRACE_ENTRY("%d %hu %p %g", sock, strid, buf, len);
	
	memset(&mhdr, 0, sizeof(mhdr));
	memset(&iov,  0, sizeof(iov));
	memset(&anci, 0, sizeof(anci));
	
	/* IO Vector: message data */
	iov.iov_base = buf;
	iov.iov_len  = len;
	
	/* Anciliary data: specify SCTP stream */
	anci.hdr.cmsg_len   = sizeof(anci);
	anci.hdr.cmsg_level = IPPROTO_SCTP;
	anci.hdr.cmsg_type  = SCTP_SNDRCV;
	anci.sndrcv.sinfo_stream = strid;
	/* note : we could store other data also, for example in .sinfo_ppid for remote peer or in .sinfo_context for errors. */
	
	/* We don't use mhdr.msg_name here; it could be used to specify an address different from the primary */
	
	mhdr.msg_iov    = &iov;
	mhdr.msg_iovlen = 1;
	
	mhdr.msg_control    = &anci;
	mhdr.msg_controllen = sizeof(anci);
	
	#ifdef DEBUG_SCTP
	TRACE_DEBUG(FULL, "Sending %db data on stream %hu of socket %d", len, strid, sock);
	#endif /* DEBUG_SCTP */
	
	CHECK_SYS( ret = sendmsg(sock, &mhdr, 0) );
	ASSERT( ret == len ); /* There should not be partial delivery with sendmsg... */
	
	return 0;
}

/* Receive the next data from the socket, or next notification */
int fd_sctp_recvmeta(int sock, uint16_t * strid, uint8_t ** buf, size_t * len, int *event)
{
	ssize_t 		 ret = 0;
	struct msghdr 		 mhdr;
	char   			 ancidata[ CMSG_BUF_LEN ];
	struct iovec 		 iov;
	uint8_t			*data = NULL;
	size_t 			 bufsz = 0, datasize = 0;
	size_t			 mempagesz = sysconf(_SC_PAGESIZE); /* We alloc buffer by memory pages for efficiency */
	
	TRACE_ENTRY("%d %p %p %p %p", sock, strid, buf, len, event);
	CHECK_PARAMS( (sock > 0) && buf && len && event );
	
	/* Cleanup out parameters */
	*buf = NULL;
	*len = 0;
	*event = 0;
	
	/* Prepare header for receiving message */
	memset(&mhdr, 0, sizeof(mhdr));
	mhdr.msg_iov    = &iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control    = &ancidata;
	mhdr.msg_controllen = sizeof(ancidata);
	
	/* We will loop while all data is not received. */
incomplete:
	if (datasize == bufsz) {
		/* The buffer is full, enlarge it */
		bufsz += mempagesz;
		CHECK_MALLOC( data = realloc(data, bufsz) );
	}
	/* the new data will be received following the preceding */
	memset(&iov,  0, sizeof(iov));
	iov.iov_base = data + datasize ;
	iov.iov_len  = bufsz - datasize;

	/* Receive data from the socket */
	pthread_cleanup_push(free, data);
	ret = recvmsg(sock, &mhdr, 0);
	pthread_cleanup_pop(0);
	
	/* Handle errors */
	if (ret <= 0) { /* Socket is closed, or an error occurred */
		CHECK_SYS_DO(ret, /* to log in case of error */);
		free(data);
		*event = FDEVP_CNX_ERROR;
		return 0;
	}
	
	/* Update the size of data we received */
	datasize += ret;

	/* SCTP provides an indication when we received a full record; loop if it is not the case */
	if ( ! (mhdr.msg_flags & MSG_EOR) ) {
		goto incomplete;
	}
	
	/* Handle the case where the data received is a notification */
	if (mhdr.msg_flags & MSG_NOTIFICATION) {
		union sctp_notification * notif = (union sctp_notification *) data;
		
		switch (notif->sn_header.sn_type) {
			
			case SCTP_ASSOC_CHANGE:
				#ifdef DEBUG_SCTP
				TRACE_DEBUG(FULL, "Received SCTP_ASSOC_CHANGE notification");
				TRACE_DEBUG(FULL, "    state : %hu", notif->sn_assoc_change.sac_state);
				TRACE_DEBUG(FULL, "    error : %hu", notif->sn_assoc_change.sac_error);
				TRACE_DEBUG(FULL, "    instr : %hu", notif->sn_assoc_change.sac_inbound_streams);
				TRACE_DEBUG(FULL, "   outstr : %hu", notif->sn_assoc_change.sac_outbound_streams);
				#endif /* DEBUG_SCTP */
				
				*event = FDEVP_CNX_EP_CHANGE;
				break;
	
			case SCTP_PEER_ADDR_CHANGE:
				#ifdef DEBUG_SCTP
				TRACE_DEBUG(FULL, "Received SCTP_PEER_ADDR_CHANGE notification");
				TRACE_DEBUG_sSA(FULL, "    intf_change : ", &(notif->sn_paddr_change.spc_aaddr), NI_NUMERICHOST | NI_NUMERICSERV, "" );
				TRACE_DEBUG(FULL, "          state : %d", notif->sn_paddr_change.spc_state);
				TRACE_DEBUG(FULL, "          error : %d", notif->sn_paddr_change.spc_error);
				#endif /* DEBUG_SCTP */
				
				*event = FDEVP_CNX_EP_CHANGE;
				break;
	
			case SCTP_SEND_FAILED:
				#ifdef DEBUG_SCTP
				TRACE_DEBUG(FULL, "Received SCTP_SEND_FAILED notification");
				TRACE_DEBUG(FULL, "    len : %hu", notif->sn_send_failed.ssf_length);
				TRACE_DEBUG(FULL, "    err : %d",  notif->sn_send_failed.ssf_error);
				#endif /* DEBUG_SCTP */
				
				*event = FDEVP_CNX_ERROR;
				break;
			
			case SCTP_REMOTE_ERROR:
				#ifdef DEBUG_SCTP
				TRACE_DEBUG(FULL, "Received SCTP_REMOTE_ERROR notification");
				TRACE_DEBUG(FULL, "    err : %hu", ntohs(notif->sn_remote_error.sre_error));
				TRACE_DEBUG(FULL, "    len : %hu", ntohs(notif->sn_remote_error.sre_length));
				#endif /* DEBUG_SCTP */
				
				*event = FDEVP_CNX_ERROR;
				break;
	
			case SCTP_SHUTDOWN_EVENT:
				#ifdef DEBUG_SCTP
				TRACE_DEBUG(FULL, "Received SCTP_SHUTDOWN_EVENT notification");
				#endif /* DEBUG_SCTP */
				
				*event = FDEVP_CNX_ERROR;
				break;
			
			default:	
				TRACE_DEBUG(FULL, "Received unknown notification %d, assume error", notif->sn_header.sn_type);
				*event = FDEVP_CNX_ERROR;
		}
		
		free(data);
		return 0;
	}
	
	/* From this point, we have received a message */
	*event = FDEVP_CNX_MSG_RECV;
	*buf = data;
	*len = datasize;
	
	if (strid) {
		struct cmsghdr 		*hdr;
		struct sctp_sndrcvinfo	*sndrcv;
		
		/* Handle the anciliary data */
		for (hdr = CMSG_FIRSTHDR(&mhdr); hdr; hdr = CMSG_NXTHDR(&mhdr, hdr)) {

			/* We deal only with anciliary data at SCTP level */
			if (hdr->cmsg_level != IPPROTO_SCTP) {
				TRACE_DEBUG(FULL, "Received some anciliary data at level %d, skipped", hdr->cmsg_level);
				continue;
			}
			
			/* Also only interested in SCTP_SNDRCV message for the moment */
			if (hdr->cmsg_type != SCTP_SNDRCV) {
				TRACE_DEBUG(FULL, "Anciliary block IPPROTO_SCTP / %d, skipped", hdr->cmsg_type);
				continue;
			}
			
			sndrcv = (struct sctp_sndrcvinfo *) CMSG_DATA(hdr);
			#ifdef DEBUG_SCTP
			TRACE_DEBUG(FULL, "Anciliary block IPPROTO_SCTP / SCTP_SNDRCV");
			TRACE_DEBUG(FULL, "    sinfo_stream    : %hu", sndrcv->sinfo_stream);
			TRACE_DEBUG(FULL, "    sinfo_ssn       : %hu", sndrcv->sinfo_ssn);
			TRACE_DEBUG(FULL, "    sinfo_flags     : %hu", sndrcv->sinfo_flags);
			/* TRACE_DEBUG(FULL, "    sinfo_pr_policy : %hu", sndrcv->sinfo_pr_policy); */
			TRACE_DEBUG(FULL, "    sinfo_ppid      : %u" , sndrcv->sinfo_ppid);
			TRACE_DEBUG(FULL, "    sinfo_context   : %u" , sndrcv->sinfo_context);
			/* TRACE_DEBUG(FULL, "    sinfo_pr_value  : %u" , sndrcv->sinfo_pr_value); */
			TRACE_DEBUG(FULL, "    sinfo_tsn       : %u" , sndrcv->sinfo_tsn);
			TRACE_DEBUG(FULL, "    sinfo_cumtsn    : %u" , sndrcv->sinfo_cumtsn);
			#endif /* DEBUG_SCTP */

			*strid = sndrcv->sinfo_stream;
		}
	}
	
	return 0;
}
