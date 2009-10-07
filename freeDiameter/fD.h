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

/* This file contains the definitions for internal use in the freeDiameter daemon */

#ifndef _FD_H
#define _FD_H

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/freeDiameter.h>

/* Timeout for establishing a connection */
#ifndef CNX_TIMEOUT
#define  CNX_TIMEOUT	10	/* in seconds */
#endif /* CNX_TIMEOUT */

/* Timeout for receiving a CER after incoming connection is established */
#ifndef INCNX_TIMEOUT
#define  INCNX_TIMEOUT	 20	/* in seconds */
#endif /* INCNX_TIMEOUT */

/* Timeout for receiving a CEA after CER is sent */
#ifndef CEA_TIMEOUT
#define  CEA_TIMEOUT	10	/* in seconds */
#endif /* CEA_TIMEOUT */

/* The timeout value to wait for answer to a DPR */
#ifndef DPR_TIMEOUT
#define DPR_TIMEOUT 	15	/* in seconds */
#endif /* DPR_TIMEOUT */

/* Configuration */
int fd_conf_init();
void fd_conf_dump();
int fd_conf_parse();
int fddparse(struct fd_config * conf); /* yacc generated */

/* Extensions */
int fd_ext_add( char * filename, char * conffile );
int fd_ext_load();
void fd_ext_dump(void);
int fd_ext_fini(void);

/* Messages */
int fd_msg_init(void);

/* Global message queues */
extern struct fifo * fd_g_incoming; /* all messages received from other peers, except local messages (CER, ...) */
extern struct fifo * fd_g_outgoing; /* messages to be sent to other peers on the network following routing procedure */
extern struct fifo * fd_g_local; /* messages to be handled to local extensions */
/* Message queues */
int fd_queues_init(void);
int fd_queues_fini(void);

/* Create all the dictionary objects defined in the Diameter base RFC. */
int fd_dict_base_protocol(struct dictionary * dict);

/* Peers */
struct fd_peer { /* The "real" definition of the peer structure */
	
	/* The public data */
	struct peer_hdr	 p_hdr;
	
	/* Eye catcher, EYEC_PEER */
	int		 p_eyec;
	#define EYEC_PEER	0x373C9336
	
	/* Origin of this peer object, for debug */
	char		*p_dbgorig;
	
	/* Chaining in peers sublists */
	struct fd_list	 p_actives;	/* list of peers in the STATE_OPEN state -- faster routing creation */
	struct fd_list	 p_expiry; 	/* list of expiring peers, ordered by their timeout value */
	struct timespec	 p_exp_timer;	/* Timestamp where the peer will expire; updated each time activity is seen on the peer (except DW) */
	
	/* Some flags influencing the peer state machine */
	struct {
		unsigned pf_responder	: 1;	/* The local peer is responder on the connection */
		
		unsigned pf_dw_pending 	: 1;	/* A DWR message was sent and not answered yet */
		
		unsigned pf_cnx_pb	: 1;	/* The peer was disconnected because of watchdogs; must exchange 3 watchdogs before putting back to normal */
		unsigned pf_reopen_cnt	: 2;	/* remaining DW to be exchanged after re-established connection */
		
		/* to be completed */
		
	}		 p_flags;
	
	/* The events queue, peer state machine thread, timer for states timeouts */
	struct fifo	*p_events;	/* The mutex of this FIFO list protects also the state and timer information */
	pthread_t	 p_psm;
	struct timespec	 p_psm_timer;
	
	/* Received message queue, and thread managing reception of messages */
	struct fifo	*p_recv;
	pthread_t	 p_inthr;
	
	/* Outgoing message queue, and thread managing sending the messages */
	struct fifo	*p_tosend;
	pthread_t	 p_outthr;
	
	/* The next hop-by-hop id value for the link, only read & modified by p_outthr */
	uint32_t	 p_hbh;
	
	/* Sent requests (for fallback), list of struct sentreq ordered by hbh */
	struct fd_list	 p_sentreq;
	
	/* connection context: socket, callbacks and so on */
	struct cnxctx	*p_cnxctx;
	
	/* Callback on initial connection success / failure */
	void 		(*p_cb)(struct peer_info *, void *);
	void 		*p_cb_data;
	
};
#define CHECK_PEER( _p ) \
	(((_p) != NULL) && (((struct fd_peer *)(_p))->p_eyec == EYEC_PEER))

/* Events codespace for struct fd_peer->p_events */
enum {
	/* Dump all info about this peer in the debug log */
	 FDEVP_DUMP_ALL = 2000
	
	/* request to terminate this peer : disconnect, requeue all messages */
	,FDEVP_TERMINATE
	
	/* A message was received in the peer */
	,FDEVP_MSG_INCOMING
	
	/* The PSM state is expired */
	,FDEVP_PSM_TIMEOUT
};
const char * fd_pev_str(int event);
#define CHECK_EVENT( _e ) \
	(((int)(_e) >= FDEVP_DUMP_ALL) && ((int)(_e) <= FDEVP_PSM_TIMEOUT))

/* Structure to store a sent request */
struct sentreq {
	struct fd_list	chain; 	/* the "o" field points directly to the hop-by-hop of the request (uint32_t *)  */
	struct msg	*req;	/* A request that was sent and not yet answered. */
};

/* The connection context structure */
struct cnxctx {
	int 		cc_socket;	/* The socket object of the connection -- <=0 if no socket is created */
	
	struct fifo   **cc_events;	/* Location of the events list to send connection events */
	
	int 		cc_proto;	/* IPPROTO_TCP or IPPROTO_SCTP */
	int		cc_tls;		/* Is TLS already started ? */
	
	uint16_t	cc_port;	/* Remote port of the connection, when we are client */
	struct fd_list	cc_ep_remote;	/* The remote address(es) of the connection */
	struct fd_list	cc_ep_local;	/* The local address(es) of the connection */
	
	/* If cc_proto == SCTP */
	struct	{
		int		str_out;/* Out streams */
		int		str_in;	/* In streams */
		int		pairs;	/* max number of pairs ( = min(in, out)) */
		int		next;	/* # of stream the next message will be sent to */
	} 		cc_sctp_para;
	
	/* If cc_tls == true */
	struct {
		int				 mode; 		/* GNUTLS_CLIENT / GNUTLS_SERVER */
		gnutls_session_t 		 session;	/* Session object (stream #0 in case of SCTP) */
	}		cc_tls_para;
	
	/* If both conditions */
	struct {
		gnutls_session_t 		*res_sessions;	/* Sessions of other pairs of streams, resumed from the first */
		/* Buffers, threads, ... */
	}		cc_sctp_tls_para;
};

/* Functions */
int fd_peer_fini();
void fd_peer_dump_list(int details);
void fd_peer_dump(struct fd_peer * peer, int details);
int fd_peer_alloc(struct fd_peer ** ptr);
int fd_peer_free(struct fd_peer ** ptr);
/* fd_peer_add declared in freeDiameter.h */

/* Peer expiry */
int fd_p_expi_init(void);
int fd_p_expi_fini(void);
int fd_p_expi_update(struct fd_peer * peer );

/* Peer state machine */
int fd_psm_start();
int fd_psm_begin(struct fd_peer * peer );
int fd_psm_terminate(struct fd_peer * peer );
void fd_psm_abord(struct fd_peer * peer );

/* Server sockets */
void fd_servers_dump();
int fd_servers_start();
void fd_servers_stop();

/* Connection contexts */
struct cnxctx * fd_cnx_init(int sock, int proto);
int fd_cnx_handshake(struct cnxctx * conn, int mode);

/* SCTP */
#ifndef DISABLE_SCTP
int fd_sctp_create_bind_server( int * socket, uint16_t port );
int fd_sctp_get_str_info( int socket, int *in, int *out );

#endif /* DISABLE_SCTP */



#endif /* _FD_H */
