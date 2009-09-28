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

/* Configuration */
int fd_conf_init();
void fd_conf_dump();
int fd_conf_parse();
int fddparse(struct fd_config * conf); /* yacc generated */

/* Extensions */
int fd_ext_init();
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
	
	/* Mutex that protect this peer structure */
	pthread_mutex_t	 p_mtx;
	
	/* Reference counter -- freed only when this reaches 0 */
	unsigned	 p_refcount;
	
	/* Chaining in peers sublists */
	struct fd_list	 p_expiry; 	/* list of expiring peers, ordered by their timeout value */
	struct fd_list	 p_actives;	/* list of peers in the STATE_OPEN state -- faster routing creation */
	
	/* The next hop-by-hop id value for the link */
	uint32_t	 p_hbh;
	
	/* Some flags influencing the peer state machine */
	struct {
		unsigned pf_responder	: 1;	/* The local peer is responder on the connection */
		
		unsigned pf_dw_pending 	: 1;	/* A DWR message was sent and not answered yet */
		
		unsigned pf_cnx_pb	: 1;	/* The peer was disconnected because of watchdogs; must exchange 3 watchdogs before putting back to normal */
		unsigned pf_reopen_cnt	: 2;	/* remaining DW to be exchanged after re-established connection */
		
		/* to be completed */
		
	}		 p_flags;
	
	/* The events queue, peer state machine thread, timer for states timeouts */
	struct fifo	*p_events;
	pthread_t	 p_psm;
	struct timespec	 p_psm_timer;
	
	/* Received message queue, and thread managing reception of messages */
	struct fifo	*p_recv;
	pthread_t	 p_inthr;
	
	/* Outgoing message queue, and thread managing sending the messages */
	struct fifo	*p_tosend;
	pthread_t	 p_outthr;
	
	/* Sent requests (for fallback), list of struct sentreq ordered by hbh */
	struct fd_list	 p_sentreq;
	
	/* connection context: socket & other metadata */
	struct cnxctx	*p_cnxctx;
	
};
#define CHECK_PEER( _p ) \
	(((_p) != NULL) && (((struct fd_peer *)(_p))->p_eyec == EYEC_PEER))

/* Events codespace for struct fd_peer->p_events */
enum {
	/* request to terminate this peer : disconnect, requeue all messages */
	 FDEVP_TERMINATE = 2000
	
	/* Dump all info about this peer in the debug log */
	,FDEVP_DUMP_ALL
	
	/* A message was received in the peer */
	,FDEVP_MSG_INCOMING
};

/* Structure to store a sent request */
struct sentreq {
	struct fd_list	chain; 	/* the "o" field points directly to the hop-by-hop of the request (uint32_t *)  */
	struct msg	*req;	/* A request that was sent and not yet answered. */
};

/* Functions */
int fd_peer_init();
void fd_peer_dump_list(int details);
int fd_peer_start();
int fd_peer_waitstart();



#endif /* _FD_H */
