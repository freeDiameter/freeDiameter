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

/* This file contains the definitions for internal use in the connection context files */

#ifndef _CNXCTX_H
#define _CNXCTX_H

/* The connection context structure */
struct cnxctx {
	char		cc_id[60];	/* The name of this connection */
	char		cc_remid[60];	/* Id of remote peer */
	
	int 		cc_socket;	/* The socket object of the connection -- <=0 if no socket is created */

	int 		cc_family;	/* AF_INET or AF_INET6 (mixed) */
	int 		cc_proto;	/* IPPROTO_TCP or IPPROTO_SCTP */
	
	uint32_t	cc_state;	/* True if the object is being destroyed: we don't send events anymore. access with fd_cnx_getstate() */
	#define 	CC_STATUS_CLOSING	1
	#define 	CC_STATUS_ERROR		2
	#define 	CC_STATUS_SIGNALED	4
	#define 	CC_STATUS_TLS		8

	pthread_t	cc_rcvthr;	/* thread for receiving messages on the connection */
	int		cc_loop;	/* tell the thread if it loops or stops after the first message is received */
	
	struct fifo *	cc_incoming;	/* FIFO queue of events received on the connection, FDEVP_CNX_* */
	struct fifo *	cc_alt;		/* alternate fifo to send FDEVP_CNX_* events to. */

	/* If cc_tls == true */
	struct {
		DiamId_t 			 cn;		/* If not NULL, remote certif will be checked to match this Common Name */
		int				 mode; 		/* GNUTLS_CLIENT / GNUTLS_SERVER */
		gnutls_session_t 		 session;	/* Session object (stream #0 in case of SCTP) */
	}		cc_tls_para;

	/* If cc_proto == SCTP */
	struct	{
		uint16_t str_out;	/* Out streams */
		uint16_t str_in;	/* In streams */
		uint16_t pairs;		/* max number of pairs ( = min(in, out)) */
		uint16_t next;		/* # of stream the next message will be sent to */
	} 		cc_sctp_para;

	/* If both conditions */
	struct {
		struct sctps_ctx *array; /* an array of cc_sctp_para.pairs elements -- the #0 is special (session is outside)*/
		struct sr_store	 *sess_store; /* Session data of the master session, to resume the children sessions */
	} 		cc_sctps_data;
};

void fd_cnx_markerror(struct cnxctx * conn);
uint32_t fd_cnx_getstate(struct cnxctx * conn);
int  fd_cnx_teststate(struct cnxctx * conn, uint32_t flag);
void fd_cnx_addstate(struct cnxctx * conn, uint32_t orstate);
void fd_cnx_setstate(struct cnxctx * conn, uint32_t abstate);
struct fifo * fd_cnx_target_queue(struct cnxctx * conn);


/* Socket */
ssize_t fd_cnx_s_recv(struct cnxctx * conn, void *buffer, size_t length);
void fd_cnx_s_setto(int sock);

/* TLS */
int fd_tls_rcvthr_core(struct cnxctx * conn, gnutls_session_t session);
int fd_tls_prepare(gnutls_session_t * session, int mode, char * priority, void * alt_creds);
#ifndef GNUTLS_VERSION_300
int fd_tls_verify_credentials(gnutls_session_t session, struct cnxctx * conn, int verbose);
#endif /* GNUTLS_VERSION_300 */

/* TCP */
int fd_tcp_create_bind_server( int * sock, sSA * sa, socklen_t salen );
int fd_tcp_listen( int sock );
int fd_tcp_client( int *sock, sSA * sa, socklen_t salen );
int fd_tcp_get_local_ep(int sock, sSS * ss, socklen_t *sl);
int fd_tcp_get_remote_ep(int sock, sSS * ss, socklen_t *sl);

#ifndef DISABLE_SCTP
/* SCTP */
int fd_sctp_create_bind_server( int * sock, int family, struct fd_list * list, uint16_t port );
int fd_sctp_listen( int sock );
int fd_sctp_client( int *sock, int no_ip6, uint16_t port, struct fd_list * list );
int fd_sctp_get_local_ep(int sock,  struct fd_list * list);
int fd_sctp_get_remote_ep(int sock, struct fd_list * list);
int fd_sctp_get_str_info( int sock, uint16_t *in, uint16_t *out, sSS *primary );
int fd_sctp_sendstr(struct cnxctx * conn, uint16_t strid, uint8_t * buf, size_t len);
int fd_sctp_recvmeta(struct cnxctx * conn, uint16_t * strid, uint8_t ** buf, size_t * len, int *event);

/* TLS over SCTP (multi-stream) */
struct sctps_ctx {
	struct cnxctx 	*parent; 	/* for info such as socket, conn name, event list */
	uint16_t	 strid;		/* Stream # of this session */
	struct fifo	*raw_recv;	/* Raw data received on this stream, for demux */
	struct {
		uint8_t *buf;
		size_t   bufsz;
		size_t   offset;
	} 		 partial;	/* If the pull function did not read the full content of first message in raw, it stores it here for next read call. */
	pthread_t	 thr;		/* Thread to decrypt raw data in this pair of streams */
	gnutls_session_t session;	/* TLS context using this pair of streams -- except if strid == 0, in that case session is outside the array */
};

int fd_sctps_init(struct cnxctx * conn);
int fd_sctps_handshake_others(struct cnxctx * conn, char * priority, void * alt_creds);
int fd_sctps_startthreads(struct cnxctx * conn, int others);
void fd_sctps_bye(struct cnxctx * conn);
void fd_sctps_waitthreadsterm(struct cnxctx * conn);
void fd_sctps_gnutls_deinit_others(struct cnxctx * conn);
void fd_sctps_stopthreads(struct cnxctx * conn);
void fd_sctps_destroy(struct cnxctx * conn);

#endif /* DISABLE_SCTP */

#endif /* _CNXCTX_H */

