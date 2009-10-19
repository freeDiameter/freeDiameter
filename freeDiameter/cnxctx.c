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

/* Connections contexts (cnxctx) in freeDiameter are wrappers around the sockets and TLS operations .
 * They are used to hide the details of the processing to the higher layers of the daemon.
 * They are always oriented on connections (TCP or SCTP), connectionless modes (UDP or SCTP) are not supported.
 */

/* Note: this file could be moved to libfreeDiameter instead, but since it uses gnuTLS we prefer to keep it in the daemon */

/* Lifetime of a cnxctx object:
 * 1) Creation
 *    a) a server socket:
 *       - create the object with fd_cnx_serv_tcp or fd_cnx_serv_sctp
 *       - start listening incoming connections: fd_cnx_serv_listen
 *       - accept new clients with fd_cnx_serv_accept.
 *    b) a client socket:
 *       - connect to a remote server with fd_cnx_cli_connect
 *
 * 2) Initialization
 *    - if TLS is started first, call fd_cnx_handshake
 *    - otherwise to receive clear messages, call fd_cnx_start_clear. fd_cnx_handshake can be called later.
 *
 * 3) Usage
 *    - fd_cnx_receive, fd_cnx_send : exchange messages on this connection (send is synchronous, receive is not).
 *    - fd_cnx_recv_setaltfifo : when a message is received, the event is sent to an external fifo list. fd_cnx_receive does not work when the alt_fifo is set.
 *    - fd_cnx_getid : retrieve a descriptive string for the connection (for debug)
 *    - fd_cnx_getremoteid : identification of the remote peer (IP address or fqdn)
 *    - fd_cnx_getcred : get the remote peer TLS credentials, after handshake
 *    - fd_cnx_getendpoints : get the endpoints (IP) of the connection
 *
 * 4) End
 *    - fd_cnx_destroy
 */

/* The connection context structure */
struct cnxctx {
	char		cc_id[60];	/* The name of this connection */
	char		cc_remid[60];	/* Id of remote peer */

	int 		cc_socket;	/* The socket object of the connection -- <=0 if no socket is created */

	int 		cc_proto;	/* IPPROTO_TCP or IPPROTO_SCTP */
	int		cc_tls;		/* Is TLS already started ? */

	struct fifo *	cc_events;	/* Events occuring on the connection */
	pthread_t	cc_mgr;		/* manager thread for the connection */
	struct fifo *	cc_incoming;	/* FIFO queue of messages received on the connection */
	struct fifo *	cc_alt;		/* alternate fifo to send FDEVP_CNX_MSG_RECV events to. */

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


/* Initialize a context structure */
static struct cnxctx * fd_cnx_init(int full)
{
	struct cnxctx * conn = NULL;

	TRACE_ENTRY("%d", full);

	CHECK_MALLOC_DO( conn = malloc(sizeof(struct cnxctx)), return NULL );
	memset(conn, 0, sizeof(struct cnxctx));

	if (full) {
		CHECK_FCT_DO( fd_fifo_new ( &conn->cc_events ), return NULL );
		CHECK_FCT_DO( fd_fifo_new ( &conn->cc_incoming ), return NULL );
	}

	return conn;
}

/* Create and bind a server socket to the given endpoint and port */
struct cnxctx * fd_cnx_serv_tcp(uint16_t port, int family, struct fd_endpoint * ep)
{
	struct cnxctx * cnx = NULL;
	sSS dummy;
	sSA * sa = (sSA *) &dummy;

	TRACE_ENTRY("%hu %d %p", port, family, ep);

	CHECK_PARAMS_DO( port, return NULL );
	CHECK_PARAMS_DO( ep || family, return NULL );
	CHECK_PARAMS_DO( (! family) || (family == AF_INET) || (family == AF_INET6), return NULL );
	CHECK_PARAMS_DO( (! ep) || (!family) || (ep->ss.ss_family == family), return NULL );

	/* The connection object */
	CHECK_MALLOC_DO( cnx = fd_cnx_init(0), return NULL );

	/* Prepare the socket address information */
	if (ep) {
		memcpy(sa, &ep->ss, sizeof(sSS));
	} else {
		memset(&dummy, 0, sizeof(dummy));
		sa->sa_family = family;
	}
	if (sa->sa_family == AF_INET) {
		((sSA4 *)sa)->sin_port = htons(port);
	} else {
		((sSA6 *)sa)->sin6_port = htons(port);
	}

	/* Create the socket */
	CHECK_FCT_DO( fd_tcp_create_bind_server( &cnx->cc_socket, sa, sizeof(sSS) ), goto error );

	/* Generate the name for the connection object */
	{
		char addrbuf[INET6_ADDRSTRLEN];
		int  rc;
		rc = getnameinfo(sa, sizeof(sSS), addrbuf, sizeof(addrbuf), NULL, 0, NI_NUMERICHOST);
		if (rc)
			snprintf(addrbuf, sizeof(addrbuf), "[err:%s]", gai_strerror(rc));
		snprintf(cnx->cc_id, sizeof(cnx->cc_id), "Srv TCP [%s]:%hu (%d)", addrbuf, port, cnx->cc_socket);
	}

	cnx->cc_proto = IPPROTO_TCP;

	return cnx;

error:
	fd_cnx_destroy(cnx);
	return NULL;
}
#ifndef DISABLE_SCTP
struct cnxctx * fd_cnx_serv_sctp(uint16_t port, struct fd_list * ep_list)
{
	struct cnxctx * cnx = NULL;
	sSS dummy;
	sSA * sa = (sSA *) &dummy;

	TRACE_ENTRY("%hu %p", port, ep_list);

	CHECK_PARAMS_DO( port, return NULL );

	/* The connection object */
	CHECK_MALLOC_DO( cnx = fd_cnx_init(0), return NULL );

	/* Create the socket */
	CHECK_FCT_DO( fd_sctp_create_bind_server( &cnx->cc_socket, ep_list, port ), goto error );

	/* Generate the name for the connection object */
	snprintf(cnx->cc_id, sizeof(cnx->cc_id), "Srv SCTP :%hu (%d)", port, cnx->cc_socket);

	cnx->cc_proto = IPPROTO_SCTP;

	return cnx;

error:
	fd_cnx_destroy(cnx);
	return NULL;
}
#endif /* DISABLE_SCTP */

/* Allow clients to connect on the server socket */
int fd_cnx_serv_listen(struct cnxctx * conn)
{
	CHECK_PARAMS( conn );

	switch (conn->cc_proto) {
		case IPPROTO_TCP:
			CHECK_FCT(fd_tcp_listen(conn->cc_socket));
			break;

		case IPPROTO_SCTP:
			CHECK_FCT(fd_sctp_listen(conn->cc_socket));
			break;

		default:
			CHECK_PARAMS(0);
	}

	return 0;
}

/* Accept a client (blocking until a new client connects) -- cancelable */
struct cnxctx * fd_cnx_serv_accept(struct cnxctx * serv)
{
	struct cnxctx * cli = NULL;
	sSS ss;
	socklen_t ss_len = sizeof(ss);
	int cli_sock = 0;
	struct fd_endpoint * ep;

	TRACE_ENTRY("%p", serv);
	CHECK_PARAMS_DO(serv, return NULL);
	
	CHECK_SYS_DO( cli_sock = accept(serv->cc_socket, (sSA *)&ss, &ss_len), return NULL );
	
	if (TRACE_BOOL(INFO)) {
		fd_log_debug("%s - new client [", fd_cnx_getid(serv));
		sSA_DUMP_NODE( &ss, AI_NUMERICHOST );
		fd_log_debug("] connected.\n");
	}
	
	CHECK_MALLOC_DO( cli = fd_cnx_init(1), { shutdown(cli_sock, SHUT_RDWR); return NULL; } );
	cli->cc_socket = cli_sock;
	cli->cc_proto = serv->cc_proto;
	
	/* Generate the name for the connection object */
	{
		char addrbuf[INET6_ADDRSTRLEN];
		char portbuf[10];
		int  rc;
		
		/* Numeric values for debug */
		rc = getnameinfo((sSA *)&ss, sizeof(sSS), addrbuf, sizeof(addrbuf), portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc)
			snprintf(addrbuf, sizeof(addrbuf), "[err:%s]", gai_strerror(rc));
		
		snprintf(cli->cc_id, sizeof(cli->cc_id), "Client %s [%s]:%s (%d) / serv (%d)", 
				IPPROTO_NAME(cli->cc_proto), 
				addrbuf, portbuf, 
				cli->cc_socket, serv->cc_socket);
		
		/* Textual value for log messages */
		rc = getnameinfo((sSA *)&ss, sizeof(sSS), cli->cc_remid, sizeof(cli->cc_remid), NULL, 0, NI_NUMERICHOST);
		if (rc)
			snprintf(cli->cc_remid, sizeof(cli->cc_remid), "[err:%s]", gai_strerror(rc));
	}

	/* SCTP-specific handlings */
#ifndef DISABLE_SCTP
	if (cli->cc_proto == IPPROTO_SCTP) {
		/* Retrieve the number of streams */
		CHECK_FCT_DO( fd_sctp_get_str_info( cli->cc_socket, &cli->cc_sctp_para.str_in, &cli->cc_sctp_para.str_out ), goto error );
		if (cli->cc_sctp_para.str_out > cli->cc_sctp_para.str_in)
			cli->cc_sctp_para.pairs = cli->cc_sctp_para.str_out;
		else
			cli->cc_sctp_para.pairs = cli->cc_sctp_para.str_in;
	}
#endif /* DISABLE_SCTP */

	return cli;
error:
	fd_cnx_destroy(cli);
	return NULL;
}

/* Client side: connect to a remote server */
struct cnxctx * fd_cnx_cli_connect(int proto, const sSA * sa,  socklen_t addrlen)
{

	TODO("...");
	return NULL;
}

/* Return a string describing the connection, for debug */
char * fd_cnx_getid(struct cnxctx * conn)
{
	CHECK_PARAMS_DO( conn, return "" );
	return conn->cc_id;
}

/* Start receving messages in clear (no TLS) on the connection */
int fd_cnx_start_clear(struct cnxctx * conn)
{

	TODO("...");
	return ENOTSUP;
}

/* TLS handshake a connection; no need to have called start_clear before. Reception is active if handhsake is successful */
int fd_cnx_handshake(struct cnxctx * conn, int mode, char * priority)
{
	TRACE_ENTRY( "%p %d", conn, mode);
	CHECK_PARAMS( conn && ( (mode == GNUTLS_CLIENT) || (mode == GNUTLS_SERVER) ) );

	/* Save the mode */
	conn->cc_tls_para.mode = mode;

	/* Create the master session context */
	CHECK_GNUTLS_DO( gnutls_init (&conn->cc_tls_para.session, mode), return ENOMEM );

	/* Set the algorithm suite */
	TODO("Use overwrite priority if non NULL");
	CHECK_GNUTLS_DO( gnutls_priority_set( conn->cc_tls_para.session, fd_g_config->cnf_sec_data.prio_cache ), return EINVAL );

	/* Set the credentials of this side of the connection */
	CHECK_GNUTLS_DO( gnutls_credentials_set (conn->cc_tls_para.session, GNUTLS_CRD_CERTIFICATE, fd_g_config->cnf_sec_data.credentials), return EINVAL );

	/* Request the remote credentials as well */
	if (mode == GNUTLS_SERVER) {
		gnutls_certificate_server_set_request (conn->cc_tls_para.session, GNUTLS_CERT_REQUIRE);
	}

	/* Set the socket info in the session */
	gnutls_transport_set_ptr (conn->cc_tls_para.session, (gnutls_transport_ptr_t) conn->cc_socket);

	/* Special case: multi-stream TLS is not natively managed in GNU TLS, we use a wrapper library */
	if ((conn->cc_proto == IPPROTO_SCTP) && (conn->cc_sctp_para.pairs > 0)) {
#ifndef DISABLE_SCTP
		TODO("Initialize the SCTP TLS wrapper");
		TODO("Set the lowat, push and pull functions");
#else /* DISABLE_SCTP */
		ASSERT(0);
#endif /* DISABLE_SCTP */
	}

	/* Handshake master session */
	{
		int ret;
		CHECK_GNUTLS_DO( ret = gnutls_handshake(conn->cc_tls_para.session),
			{
				if (TRACE_BOOL(INFO)) {
					fd_log_debug("TLS Handshake failed on socket %d (%s) : %s\n", conn->cc_socket, conn->cc_id, gnutls_strerror(ret));
				}
				return EINVAL;
			} );

		/* Now verify the remote credentials are valid -- only simple test here */
		CHECK_GNUTLS_DO( gnutls_certificate_verify_peers2 (conn->cc_tls_para.session, &ret), return EINVAL );
		if (ret) {
			if (TRACE_BOOL(INFO)) {
				fd_log_debug("TLS: Remote certificate invalid on socket %d (%s) :\n", conn->cc_socket, conn->cc_id);
				if (ret & GNUTLS_CERT_INVALID)
					fd_log_debug(" - The certificate is not trusted (unknown CA?)\n");
				if (ret & GNUTLS_CERT_REVOKED)
					fd_log_debug(" - The certificate has been revoked.\n");
				if (ret & GNUTLS_CERT_SIGNER_NOT_FOUND)
					fd_log_debug(" - The certificate hasn't got a known issuer.\n");
				if (ret & GNUTLS_CERT_SIGNER_NOT_CA)
					fd_log_debug(" - The certificate signer is not a CA, or uses version 1, or 3 without basic constraints.\n");
				if (ret & GNUTLS_CERT_INSECURE_ALGORITHM)
					fd_log_debug(" - The certificate signature uses a weak algorithm.\n");
			}
			return EINVAL;
		}
	}

	/* Other sessions in case of multi-stream SCTP are resumed from the master */
	if ((conn->cc_proto == IPPROTO_SCTP) && (conn->cc_sctp_para.pairs > 0)) {
#ifndef DISABLE_SCTP
		TODO("Init and resume all additional sessions from the master one.");
#endif /* DISABLE_SCTP */
	}

	TODO("Start the connection state machine thread");

	return 0;
}

/* Retrieve TLS credentials of the remote peer, after handshake */
int fd_cnx_getcred(struct cnxctx * conn, const gnutls_datum_t **cert_list, unsigned int *cert_list_size)
{

	TODO("...");
	return ENOTSUP;
}

/* Get the list of endpoints (IP addresses) of the local and remote peers on this conenction */
int fd_cnx_getendpoints(struct cnxctx * conn, struct fd_list * local, struct fd_list * remote)
{
	TRACE_ENTRY("%p %p %p", conn, local, remote);
	CHECK_PARAMS(conn);
	
	if (local) {
		/* Retrieve the local endpoint(s) of the connection */
		TODO("TCP : getsockname");
		TODO("SCTP: sctp_getladdrs / _sctp_getboundaddrs (waaad)");
	}
	
	if (remote) {
		/* Retrieve the peer endpoint(s) of the connection */
		TODO("TCP : getpeername");
		TODO("SCTP: sctp_getpaddrs");
		
	}

	return ENOTSUP;
}


/* Get a string describing the remote peer address (ip address or fqdn) */
char * fd_cnx_getremoteid(struct cnxctx * conn)
{
	CHECK_PARAMS_DO( conn, return "" );
	return conn->cc_remid;
}


/* Receive next message. if timeout is not NULL, wait only until timeout */
int fd_cnx_receive(struct cnxctx * conn, struct timespec * timeout, unsigned char **buf, size_t * len)
{

	TODO("...");
	return ENOTSUP;
}

/* Set / reset alternate FIFO list to send FDEVP_CNX_MSG_RECV to when message is received */
int fd_cnx_recv_setaltfifo(struct cnxctx * conn, struct fifo * alt_fifo)
{
	TRACE_ENTRY( "%p %p", conn, alt_fifo );
	CHECK_PARAMS( conn );
	
	/* Let's cross fingers that there is no race condition here... */
	conn->cc_alt = alt_fifo;
	
	return 0;
}

/* Send a message */
int fd_cnx_send(struct cnxctx * conn, unsigned char * buf, size_t len)
{

	TODO("...");
	return ENOTSUP;
}


/* Destroy a conn structure, and shutdown the socket */
void fd_cnx_destroy(struct cnxctx * conn)
{
	TRACE_ENTRY("%p", conn);
	
	CHECK_PARAMS_DO(conn, return);

	TODO("End TLS session(s) if started");
	
	TODO("Stop manager thread if running");
	
	/* Shut the connection down */
	if (conn->cc_socket > 0) {
		shutdown(conn->cc_socket, SHUT_RDWR);
	}
	
	TODO("Empty FIFO queues");
	
	/* Destroy FIFO lists */
	if (conn->cc_events)
		CHECK_FCT_DO( fd_fifo_del ( &conn->cc_events ), /* continue */ );
	if (conn->cc_incoming)
		CHECK_FCT_DO( fd_fifo_del ( &conn->cc_incoming ), /* continue */ );
	
	/* Free the object */
	free(conn);
	
	/* Done! */
	return;
}




