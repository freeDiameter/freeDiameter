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

/* The maximum size of Diameter message we accept to receive (<= 2^24) to avoid too big mallocs in case of trashed headers */
#ifndef DIAMETER_MSG_SIZE_MAX
#define DIAMETER_MSG_SIZE_MAX	65535	/* in bytes */
#endif /* DIAMETER_MSG_SIZE_MAX */

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
 *    - fd_cnx_receive, fd_cnx_send : exchange messages on this connection (send is synchronous, receive is not, but blocking).
 *    - fd_cnx_recv_setaltfifo : when a message is received, the event is sent to an external fifo list. fd_cnx_receive does not work when the alt_fifo is set.
 *    - fd_cnx_getid : retrieve a descriptive string for the connection (for debug)
 *    - fd_cnx_getremoteid : identification of the remote peer (IP address or fqdn)
 *    - fd_cnx_getcred : get the remote peer TLS credentials, after handshake
 *    - fd_cnx_getendpoints : get the endpoints (IP) of the connection
 *
 * 4) End
 *    - fd_cnx_destroy
 */


/*******************************************/
/*     Creation of a connection object     */
/*******************************************/

/* Initialize a context structure */
static struct cnxctx * fd_cnx_init(int full)
{
	struct cnxctx * conn = NULL;

	TRACE_ENTRY("%d", full);

	CHECK_MALLOC_DO( conn = malloc(sizeof(struct cnxctx)), return NULL );
	memset(conn, 0, sizeof(struct cnxctx));

	if (full) {
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

/* Same function for SCTP, with a list of local endpoints to bind to */
struct cnxctx * fd_cnx_serv_sctp(uint16_t port, struct fd_list * ep_list)
{
#ifdef DISABLE_SCTP
	TRACE_DEBUG(INFO, "This function should never been called when SCTP is disabled...");
	ASSERT(0);
	CHECK_FCT_DO( ENOTSUP, return NULL);
#else /* DISABLE_SCTP */
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
#endif /* DISABLE_SCTP */
}

/* Allow clients to connect on the server socket */
int fd_cnx_serv_listen(struct cnxctx * conn)
{
	CHECK_PARAMS( conn );

	switch (conn->cc_proto) {
		case IPPROTO_TCP:
			CHECK_FCT(fd_tcp_listen(conn->cc_socket));
			break;

#ifndef DISABLE_SCTP
		case IPPROTO_SCTP:
			CHECK_FCT(fd_sctp_listen(conn->cc_socket));
			break;
#endif /* DISABLE_SCTP */

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
	
	/* Accept the new connection -- this is blocking until new client enters or cancellation */
	CHECK_SYS_DO( cli_sock = accept(serv->cc_socket, (sSA *)&ss, &ss_len), return NULL );
	
	if (TRACE_BOOL(INFO)) {
		fd_log_debug("%s : accepted new client [", fd_cnx_getid(serv));
		sSA_DUMP_NODE( &ss, NI_NUMERICHOST );
		fd_log_debug("].\n");
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
		if (rc) {
			snprintf(addrbuf, sizeof(addrbuf), "[err:%s]", gai_strerror(rc));
			portbuf[0] = '\0';
		}
		
		snprintf(cli->cc_id, sizeof(cli->cc_id), "Incoming %s [%s]:%s (%d) @ serv (%d)", 
				IPPROTO_NAME(cli->cc_proto), 
				addrbuf, portbuf, 
				cli->cc_socket, serv->cc_socket);
		
		/* Name for log messages */
		rc = getnameinfo((sSA *)&ss, sizeof(sSS), cli->cc_remid, sizeof(cli->cc_remid), NULL, 0, 0);
		if (rc)
			snprintf(cli->cc_remid, sizeof(cli->cc_remid), "[err:%s]", gai_strerror(rc));
	}

#ifndef DISABLE_SCTP
	/* SCTP-specific handlings */
	if (cli->cc_proto == IPPROTO_SCTP) {
		/* Retrieve the number of streams */
		CHECK_FCT_DO( fd_sctp_get_str_info( cli->cc_socket, &cli->cc_sctp_para.str_in, &cli->cc_sctp_para.str_out, NULL ), goto error );
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

/* Client side: connect to a remote server -- cancelable */
struct cnxctx * fd_cnx_cli_connect_tcp(sSA * sa /* contains the port already */, socklen_t addrlen)
{
	int sock;
	struct cnxctx * cnx = NULL;
	
	TRACE_ENTRY("%p %d", sa, addrlen);
	CHECK_PARAMS_DO( sa && addrlen, return NULL );
	
	/* Create the socket and connect, which can take some time and/or fail */
	CHECK_FCT_DO( fd_tcp_client( &sock, sa, addrlen ), return NULL );
	
	if (TRACE_BOOL(INFO)) {
		fd_log_debug("Connection established to server '");
		sSA_DUMP_NODE_SERV( sa, NI_NUMERICSERV);
		fd_log_debug("' (TCP:%d).\n", sock);
	}
	
	/* Once the socket is created successfuly, prepare the remaining of the cnx */
	CHECK_MALLOC_DO( cnx = fd_cnx_init(1), { shutdown(sock, SHUT_RDWR); return NULL; } );
	
	cnx->cc_socket = sock;
	cnx->cc_proto  = IPPROTO_TCP;
	
	/* Generate the names for the object */
	{
		char addrbuf[INET6_ADDRSTRLEN];
		char portbuf[10];
		int  rc;
		
		/* Numeric values for debug */
		rc = getnameinfo(sa, addrlen, addrbuf, sizeof(addrbuf), portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc) {
			snprintf(addrbuf, sizeof(addrbuf), "[err:%s]", gai_strerror(rc));
			portbuf[0] = '\0';
		}
		
		snprintf(cnx->cc_id, sizeof(cnx->cc_id), "Client of TCP server [%s]:%s (%d)", addrbuf, portbuf, cnx->cc_socket);
		
		/* Name for log messages */
		rc = getnameinfo(sa, addrlen, cnx->cc_remid, sizeof(cnx->cc_remid), NULL, 0, 0);
		if (rc)
			snprintf(cnx->cc_remid, sizeof(cnx->cc_remid), "[err:%s]", gai_strerror(rc));
	}
	
	return cnx;

error:
	fd_cnx_destroy(cnx);
	return NULL;
}

/* Same for SCTP, accepts a list of remote addresses to connect to (see sctp_connectx for how they are used) */
struct cnxctx * fd_cnx_cli_connect_sctp(int no_ip6, uint16_t port, struct fd_list * list)
{
#ifdef DISABLE_SCTP
	TRACE_DEBUG(INFO, "This function should never been called when SCTP is disabled...");
	ASSERT(0);
	CHECK_FCT_DO( ENOTSUP, return NULL);
#else /* DISABLE_SCTP */
	int sock;
	struct cnxctx * cnx = NULL;
	sSS primary;
	
	TRACE_ENTRY("%p", list);
	CHECK_PARAMS_DO( list && !FD_IS_LIST_EMPTY(list), return NULL );
	
	CHECK_FCT_DO( fd_sctp_client( &sock, no_ip6, port, list ), return NULL );
	
	/* Once the socket is created successfuly, prepare the remaining of the cnx */
	CHECK_MALLOC_DO( cnx = fd_cnx_init(1), { shutdown(sock, SHUT_RDWR); return NULL; } );
	
	cnx->cc_socket = sock;
	cnx->cc_proto  = IPPROTO_SCTP;
	
	/* Retrieve the number of streams and primary address */
	CHECK_FCT_DO( fd_sctp_get_str_info( sock, &cnx->cc_sctp_para.str_in, &cnx->cc_sctp_para.str_out, &primary ), goto error );
	if (cnx->cc_sctp_para.str_out > cnx->cc_sctp_para.str_in)
		cnx->cc_sctp_para.pairs = cnx->cc_sctp_para.str_out;
	else
		cnx->cc_sctp_para.pairs = cnx->cc_sctp_para.str_in;
	
	if (TRACE_BOOL(INFO)) {
		fd_log_debug("Connection established to server '");
		sSA_DUMP_NODE_SERV( &primary, NI_NUMERICSERV);
		fd_log_debug("' (SCTP:%d, %d/%d streams).\n", sock, cnx->cc_sctp_para.str_in, cnx->cc_sctp_para.str_out);
	}
	
	/* Generate the names for the object */
	{
		char addrbuf[INET6_ADDRSTRLEN];
		char portbuf[10];
		int  rc;
		
		/* Numeric values for debug */
		rc = getnameinfo((sSA *)&primary, sizeof(sSS), addrbuf, sizeof(addrbuf), portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
		if (rc) {
			snprintf(addrbuf, sizeof(addrbuf), "[err:%s]", gai_strerror(rc));
			portbuf[0] = '\0';
		}
		
		snprintf(cnx->cc_id, sizeof(cnx->cc_id), "Client of SCTP server [%s]:%s (%d)", addrbuf, portbuf, cnx->cc_socket);
		
		/* Name for log messages */
		rc = getnameinfo((sSA *)&primary, sizeof(sSS), cnx->cc_remid, sizeof(cnx->cc_remid), NULL, 0, 0);
		if (rc)
			snprintf(cnx->cc_remid, sizeof(cnx->cc_remid), "[err:%s]", gai_strerror(rc));
	}
	
	return cnx;

error:
	fd_cnx_destroy(cnx);
	return NULL;
#endif /* DISABLE_SCTP */
}

/* Return a string describing the connection, for debug */
char * fd_cnx_getid(struct cnxctx * conn)
{
	CHECK_PARAMS_DO( conn, return "" );
	return conn->cc_id;
}

/* Return the protocol of a connection */
int fd_cnx_getproto(struct cnxctx * conn)
{
	CHECK_PARAMS_DO( conn, return 0 );
	return conn->cc_proto;
}

/* Set the hostname to check during handshake */
void fd_cnx_sethostname(struct cnxctx * conn, char * hn)
{
	CHECK_PARAMS_DO( conn, return );
	conn->cc_tls_para.cn = hn;
}

/* Return the TLS state of a connection */
int fd_cnx_getTLS(struct cnxctx * conn)
{
	CHECK_PARAMS_DO( conn, return 0 );
	return conn->cc_tls;
}

/* Get the list of endpoints (IP addresses) of the local and remote peers on this connection */
int fd_cnx_getendpoints(struct cnxctx * conn, struct fd_list * local, struct fd_list * remote)
{
	TRACE_ENTRY("%p %p %p", conn, local, remote);
	CHECK_PARAMS(conn);
	
	if (local) {
		/* Retrieve the local endpoint(s) of the connection */
		switch (conn->cc_proto) {
			case IPPROTO_TCP: {
				sSS ss;
				socklen_t sl;
				CHECK_FCT(fd_tcp_get_local_ep(conn->cc_socket, &ss, &sl));
				CHECK_FCT(fd_ep_add_merge( local, (sSA *)&ss, sl, EP_FL_LL | EP_FL_PRIMARY));
			}
			break;

			#ifndef DISABLE_SCTP
			case IPPROTO_SCTP: {
				CHECK_FCT(fd_sctp_get_local_ep(conn->cc_socket, local));
			}
			break;
			#endif /* DISABLE_SCTP */

			default:
				CHECK_PARAMS(0);
		}
	}
	
	if (remote) {
		/* Check we have a full connection object, not a listening socket (with no remote) */
		CHECK_PARAMS( conn->cc_incoming );
		
		/* Retrieve the peer endpoint(s) of the connection */
		switch (conn->cc_proto) {
			case IPPROTO_TCP: {
				sSS ss;
				socklen_t sl;
				CHECK_FCT(fd_tcp_get_remote_ep(conn->cc_socket, &ss, &sl));
				CHECK_FCT(fd_ep_add_merge( remote, (sSA *)&ss, sl, EP_FL_LL | EP_FL_PRIMARY ));
			}
			break;

			#ifndef DISABLE_SCTP
			case IPPROTO_SCTP: {
				CHECK_FCT(fd_sctp_get_remote_ep(conn->cc_socket, remote));
			}
			break;
			#endif /* DISABLE_SCTP */

			default:
				CHECK_PARAMS(0);
		}
	}

	return 0;
}


/* Get a string describing the remote peer address (ip address or fqdn) */
char * fd_cnx_getremoteid(struct cnxctx * conn)
{
	CHECK_PARAMS_DO( conn, return "" );
	return conn->cc_remid;
}


/**************************************/
/*     Use of a connection object     */
/**************************************/

/* Receiver thread (TCP & noTLS) : incoming message is directly saved into the target queue */
static void * rcvthr_notls_tcp(void * arg)
{
	struct cnxctx * conn = arg;
	
	TRACE_ENTRY("%p", arg);
	CHECK_PARAMS_DO(conn && (conn->cc_socket > 0), goto out);
	
	/* Set the thread name */
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "Receiver (%d) TCP/noTLS)", conn->cc_socket);
		fd_log_threadname ( buf );
	}
	
	ASSERT( conn->cc_proto == IPPROTO_TCP );
	ASSERT( conn->cc_tls == 0 );
	ASSERT( Target_Queue(conn) );
	
	/* Receive from a TCP connection: we have to rebuild the message boundaries */
	do {
		uint8_t header[4];
		uint8_t * newmsg;
		size_t  length;
		ssize_t ret = 0;
		size_t	received = 0;

		do {
			ret = recv(conn->cc_socket, &header[received], sizeof(header) - received, 0);
			if (ret <= 0) {
				CHECK_SYS_DO(ret, /* continue */);
				goto error; /* Stop the thread, the recipient of the event will cleanup */
			}

			received += ret;
		} while (received < sizeof(header));

		length = ((size_t)header[1] << 16) + ((size_t)header[2] << 8) + (size_t)header[3];

		/* Check the received word is a valid begining of a Diameter message */
		if ((header[0] != DIAMETER_VERSION)	/* defined in <libfreeDiameter.h> */
		   || (length > DIAMETER_MSG_SIZE_MAX)) { /* to avoid too big mallocs */
			/* The message is suspect */
			TRACE_DEBUG(INFO, "Received suspect header [ver: %d, size: %zd], assume disconnection", (int)header[0], length);
			goto error; /* Stop the thread, the recipient of the event will cleanup */
		}

		/* Ok, now we can really receive the data */
		CHECK_MALLOC_DO(  newmsg = malloc( length ), goto error );
		memcpy(newmsg, header, sizeof(header));

		while (received < length) {
			pthread_cleanup_push(free, newmsg); /* In case we are canceled, clean the partialy built buffer */
			ret = recv(conn->cc_socket, newmsg + received, length - received, 0);
			pthread_cleanup_pop(0);

			if (ret <= 0) {
				CHECK_SYS_DO(ret, /* continue */);
				free(newmsg);
				goto error; /* Stop the thread, the recipient of the event will cleanup */
			}
			received += ret;
		}
		
		/* We have received a complete message, send it */
		CHECK_FCT_DO( fd_event_send( Target_Queue(conn), FDEVP_CNX_MSG_RECV, length, newmsg), /* continue or destroy everything? */);
		
	} while (conn->cc_loop);
	
out:
	TRACE_DEBUG(FULL, "Thread terminated");	
	return NULL;
error:
	CHECK_FCT_DO( fd_event_send( Target_Queue(conn), FDEVP_CNX_ERROR, 0, NULL), /* continue or destroy everything? */);
	goto out;
}

#ifndef DISABLE_SCTP
/* Receiver thread (SCTP & noTLS) : incoming message is directly saved into cc_incoming, no need to care for the stream ID */
static void * rcvthr_notls_sctp(void * arg)
{
	struct cnxctx * conn = arg;
	uint8_t * buf;
	size_t    bufsz;
	int	  event;
	
	TRACE_ENTRY("%p", arg);
	CHECK_PARAMS_DO(conn && (conn->cc_socket > 0), goto out);
	
	/* Set the thread name */
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "Receiver (%d) SCTP/noTLS)", conn->cc_socket);
		fd_log_threadname ( buf );
	}
	
	ASSERT( conn->cc_proto == IPPROTO_SCTP );
	ASSERT( conn->cc_tls == 0 );
	ASSERT( Target_Queue(conn) );
	
	do {
		CHECK_FCT_DO( fd_sctp_recvmeta(conn->cc_socket, NULL, &buf, &bufsz, &event), goto error );
		if (event == FDEVP_CNX_ERROR) {
			goto error;
		}
		
		CHECK_FCT_DO( fd_event_send( Target_Queue(conn), event, bufsz, buf), goto error );
		
	} while (conn->cc_loop);
	
out:
	TRACE_DEBUG(FULL, "Thread terminated");	
	return NULL;
error:
	CHECK_FCT_DO( fd_event_send( Target_Queue(conn), FDEVP_CNX_ERROR, 0, NULL), /* continue or destroy everything? */);
	goto out;
}
#endif /* DISABLE_SCTP */

/* Returns 0 on error, received data size otherwise (always >= 0) */
static ssize_t fd_tls_recv_handle_error(struct cnxctx * conn, gnutls_session_t session, void * data, size_t sz)
{
	ssize_t ret;
again:	
	CHECK_GNUTLS_DO( ret = gnutls_record_recv(session, data, sz),
		{
			switch (ret) {
				case GNUTLS_E_REHANDSHAKE: 
					CHECK_GNUTLS_DO( ret = gnutls_handshake(session),
						{
							if (TRACE_BOOL(INFO)) {
								fd_log_debug("TLS re-handshake failed on socket %d (%s) : %s\n", conn->cc_socket, conn->cc_id, gnutls_strerror(ret));
							}
							ret = 0;
							goto end;
						} );

				case GNUTLS_E_AGAIN:
				case GNUTLS_E_INTERRUPTED:
					goto again;

				default:
					TRACE_DEBUG(INFO, "This TLS error is not handled, assume unrecoverable error");
					ret = 0;
			}
		} );
end:	
	return ret;
}

/* The function that receives TLS data and re-builds a Diameter message -- it exits only on error or cancelation */
int fd_tls_rcvthr_core(struct cnxctx * conn, gnutls_session_t session)
{
	/* No guarantee that GnuTLS preserves the message boundaries, so we re-build it as in TCP */
	do {
		uint8_t header[4];
		uint8_t * newmsg;
		size_t  length;
		ssize_t ret = 0;
		size_t	received = 0;

		do {
			ret = fd_tls_recv_handle_error(conn, session, &header[received], sizeof(header) - received);
			if (ret == 0) {
				/* The connection is closed */
				goto out;
			}
			received += ret;
		} while (received < sizeof(header));

		length = ((size_t)header[1] << 16) + ((size_t)header[2] << 8) + (size_t)header[3];

		/* Check the received word is a valid beginning of a Diameter message */
		if ((header[0] != DIAMETER_VERSION)	/* defined in <libfreeDiameter.h> */
		   || (length > DIAMETER_MSG_SIZE_MAX)) { /* to avoid too big mallocs */
			/* The message is suspect */
			TRACE_DEBUG(INFO, "Received suspect header [ver: %d, size: %zd], assume disconnection", (int)header[0], length);
			goto out;
		}

		/* Ok, now we can really receive the data */
		CHECK_MALLOC(  newmsg = malloc( length ) );
		memcpy(newmsg, header, sizeof(header));

		while (received < length) {
			pthread_cleanup_push(free, newmsg); /* In case we are canceled, clean the partialy built buffer */
			ret = fd_tls_recv_handle_error(conn, session, newmsg + received, length - received);
			pthread_cleanup_pop(0);

			if (ret == 0) {
				free(newmsg);
				goto out; /* Stop the thread, the recipient of the event will cleanup */
			}
			received += ret;
		}
		
		/* We have received a complete message, send it */
		CHECK_FCT_DO( fd_event_send( Target_Queue(conn), FDEVP_CNX_MSG_RECV, length, newmsg), /* continue or destroy everything? */);
		
	} while (1);
out:
	return ENOTCONN;
}

/* Receiver thread (TLS & 1 stream SCTP or TCP) : gnutls directly handles the socket, save records into the target queue */
static void * rcvthr_tls_single(void * arg)
{
	struct cnxctx * conn = arg;
	
	TRACE_ENTRY("%p", arg);
	CHECK_PARAMS_DO(conn && (conn->cc_socket > 0), goto error);
	
	/* Set the thread name */
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "Receiver (%d) TLS/ single stream)", conn->cc_socket);
		fd_log_threadname ( buf );
	}
	
	ASSERT( conn->cc_tls == 1 );
	ASSERT( Target_Queue(conn) );
	
	CHECK_FCT_DO(fd_tls_rcvthr_core(conn, conn->cc_tls_para.session), /* continue */);
error:
	CHECK_FCT_DO( fd_event_send( Target_Queue(conn), FDEVP_CNX_ERROR, 0, NULL), /* continue or destroy everything? */);
	TRACE_DEBUG(FULL, "Thread terminated");	
	return NULL;
}

/* Start receving messages in clear (no TLS) on the connection */
int fd_cnx_start_clear(struct cnxctx * conn, int loop)
{
	TRACE_ENTRY("%p %i", conn, loop);
	
	CHECK_PARAMS( conn && Target_Queue(conn) && (!conn->cc_tls) && (!conn->cc_loop));
	
	/* Save the loop request */
	conn->cc_loop = loop;
	
	/* Release resources in case of a previous call was already made */
	CHECK_FCT_DO( fd_thr_term(&conn->cc_rcvthr), /* continue */);
	
	switch (conn->cc_proto) {
		case IPPROTO_TCP:
			/* Start the tcp_notls thread */
			CHECK_POSIX( pthread_create( &conn->cc_rcvthr, NULL, rcvthr_notls_tcp, conn ) );
			break;
#ifndef DISABLE_SCTP
		case IPPROTO_SCTP:
			/* Start the tcp_notls thread */
			CHECK_POSIX( pthread_create( &conn->cc_rcvthr, NULL, rcvthr_notls_sctp, conn ) );
			break;
#endif /* DISABLE_SCTP */
		default:
			TRACE_DEBUG(INFO, "Unknown protocol: %d", conn->cc_proto);
			return ENOTSUP;
	}
			
	return 0;
}

/* Prepare a gnutls session object for handshake */
int fd_tls_prepare(gnutls_session_t * session, int mode, char * priority, void * alt_creds)
{
	/* Create the master session context */
	CHECK_GNUTLS_DO( gnutls_init (session, mode), return ENOMEM );

	/* Set the algorithm suite */
	if (priority) {
		const char * errorpos;
		CHECK_GNUTLS_DO( gnutls_priority_set_direct( *session, priority, &errorpos ), 
			{ TRACE_DEBUG(INFO, "Error in priority string '%s' at position: '%s'\n", priority, errorpos); return EINVAL; } );
	} else {
		CHECK_GNUTLS_DO( gnutls_priority_set( *session, fd_g_config->cnf_sec_data.prio_cache ), return EINVAL );
	}

	/* Set the credentials of this side of the connection */
	CHECK_GNUTLS_DO( gnutls_credentials_set (*session, GNUTLS_CRD_CERTIFICATE, alt_creds ?: fd_g_config->cnf_sec_data.credentials), return EINVAL );

	/* Request the remote credentials as well */
	if (mode == GNUTLS_SERVER) {
		gnutls_certificate_server_set_request (*session, GNUTLS_CERT_REQUIRE);
	}
	
	return 0;
}

/* Verify remote credentials after successful handshake (return 0 if OK, EINVAL otherwise) */
int fd_tls_verify_credentials(gnutls_session_t session, struct cnxctx * conn)
{
	int ret, i;
	const gnutls_datum_t *cert_list;
	unsigned int cert_list_size;
	gnutls_x509_crt_t cert;
	time_t now;
	
	/* First, use built-in verification */
	CHECK_GNUTLS_DO( gnutls_certificate_verify_peers2 (session, &ret), return EINVAL );
	if (ret) {
		if (TRACE_BOOL(INFO)) {
			fd_log_debug("TLS: Remote certificate invalid on socket %d (Remote: '%s')(Connection: '%s') :\n", conn->cc_socket, conn->cc_remid, conn->cc_id);
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
	
	/* Code from http://www.gnu.org/software/gnutls/manual/gnutls.html#Verifying-peer_0027s-certificate */
	if (gnutls_certificate_type_get (session) != GNUTLS_CRT_X509)
		return EINVAL;
	
	cert_list = gnutls_certificate_get_peers (session, &cert_list_size);
	if (cert_list == NULL)
		return EINVAL;
	
	now = time(NULL);

	/* Check validity of all the certificates */
	for (i = 0; i < cert_list_size; i++)
	{
		time_t deadline;
		
		CHECK_GNUTLS_DO( gnutls_x509_crt_init (&cert), return EINVAL);
		CHECK_GNUTLS_DO( gnutls_x509_crt_import (cert, &cert_list[i], GNUTLS_X509_FMT_DER), return EINVAL);
		
		deadline = gnutls_x509_crt_get_expiration_time(cert);
		if ((deadline != (time_t)-1) && (deadline < now)) {
			if (TRACE_BOOL(INFO)) {
				fd_log_debug("TLS: Remote certificate invalid on socket %d (Remote: '%s')(Connection: '%s') :\n", conn->cc_socket, conn->cc_remid, conn->cc_id);
				fd_log_debug(" - The certificate %d in the chain is expired\n", i);
			}
			return EINVAL;
		}
		
		deadline = gnutls_x509_crt_get_activation_time(cert);
		if ((deadline != (time_t)-1) && (deadline > now)) {
			if (TRACE_BOOL(INFO)) {
				fd_log_debug("TLS: Remote certificate invalid on socket %d (Remote: '%s')(Connection: '%s') :\n", conn->cc_socket, conn->cc_remid, conn->cc_id);
				fd_log_debug(" - The certificate %d in the chain is not yet activated\n", i);
			}
			return EINVAL;
		}
		
		if ((i == 0) && (conn->cc_tls_para.cn)) {
			if (!gnutls_x509_crt_check_hostname (cert, conn->cc_tls_para.cn)) {
				if (TRACE_BOOL(INFO)) {
					fd_log_debug("TLS: Remote certificate invalid on socket %d (Remote: '%s')(Connection: '%s') :\n", conn->cc_socket, conn->cc_remid, conn->cc_id);
					fd_log_debug(" - The certificate hostname does not match '%s'\n", conn->cc_tls_para.cn);
				}
				return EINVAL;
			}
		}
		
		gnutls_x509_crt_deinit (cert);
	}

	return 0;
}

/* TLS handshake a connection; no need to have called start_clear before. Reception is active if handhsake is successful */
int fd_cnx_handshake(struct cnxctx * conn, int mode, char * priority, void * alt_creds)
{
	TRACE_ENTRY( "%p %d", conn, mode);
	CHECK_PARAMS( conn && (!conn->cc_tls) && ( (mode == GNUTLS_CLIENT) || (mode == GNUTLS_SERVER) ) && (!conn->cc_loop) );

	/* Save the mode */
	conn->cc_tls_para.mode = mode;
	
	/* Cancel receiving thread if any -- it should already be terminated anyway, we just release the resources */
	CHECK_FCT_DO( fd_thr_term(&conn->cc_rcvthr), /* continue */);
	
	/* Once TLS handshake is done, we don't stop after the first message */
	conn->cc_loop = 1;
	
	/* Prepare the master session credentials and priority */
	CHECK_FCT( fd_tls_prepare(&conn->cc_tls_para.session, mode, priority, alt_creds) );

	/* Special case: multi-stream TLS is not natively managed in GNU TLS, we use a wrapper library */
	if (conn->cc_sctp_para.pairs > 1) {
#ifdef DISABLE_SCTP
		ASSERT(0);
		CHECK_FCT( ENOTSUP );
#else /* DISABLE_SCTP */
		/* Initialize the wrapper, start the demux thread */
		CHECK_FCT( fd_sctps_init(conn) );
#endif /* DISABLE_SCTP */
	} else {
		/* Set the socket info in the session */
		gnutls_transport_set_ptr (conn->cc_tls_para.session, (gnutls_transport_ptr_t) conn->cc_socket);
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
		CHECK_FCT( fd_tls_verify_credentials(conn->cc_tls_para.session, conn) );
	}

	/* Multi-stream TLS: handshake other streams as well */
	if (conn->cc_sctp_para.pairs > 1) {
#ifndef DISABLE_SCTP
		/* Resume all additional sessions from the master one. */
		CHECK_FCT(fd_sctps_handshake_others(conn, priority, alt_creds));
		
		/* Mark the connection as protected from here */
		conn->cc_tls = 1;

		/* Start decrypting the messages from all threads and queuing them in target queue */
		CHECK_FCT(fd_sctps_startthreads(conn));
#endif /* DISABLE_SCTP */
	} else {
		/* Mark the connection as protected from here */
		conn->cc_tls = 1;

		/* Start decrypting the data */
		CHECK_POSIX( pthread_create( &conn->cc_rcvthr, NULL, rcvthr_tls_single, conn ) );
	}
	
	return 0;
}

/* Retrieve TLS credentials of the remote peer, after handshake */
int fd_cnx_getcred(struct cnxctx * conn, const gnutls_datum_t **cert_list, unsigned int *cert_list_size)
{
	TRACE_ENTRY("%p %p %p", conn, cert_list, cert_list_size);
	CHECK_PARAMS( conn && (conn->cc_tls) && cert_list && cert_list_size );
	
	/* This function only works for X.509 certificates. */
	CHECK_PARAMS( gnutls_certificate_type_get (conn->cc_tls_para.session) == GNUTLS_CRT_X509 );
	
	*cert_list = gnutls_certificate_get_peers (conn->cc_tls_para.session, cert_list_size);
	if (*cert_list == NULL) {
		TRACE_DEBUG(INFO, "No certificate was provided by remote peer / an error occurred.");
		return EINVAL;
	}

	TRACE_DEBUG( FULL, "Remote peer provided %d certificates.\n", *cert_list_size);
	
	return 0;
}

/* Receive next message. if timeout is not NULL, wait only until timeout. This function only pulls from a queue, mgr thread is filling that queue aynchrounously. */
int fd_cnx_receive(struct cnxctx * conn, struct timespec * timeout, unsigned char **buf, size_t * len)
{
	int    ev;
	size_t ev_sz;
	void * ev_data;
	
	TRACE_ENTRY("%p %p %p %p", conn, timeout, buf, len);
	CHECK_PARAMS(conn && (conn->cc_socket > 0) && buf && len);
	CHECK_PARAMS(conn->cc_rcvthr != (pthread_t)NULL);
	CHECK_PARAMS(conn->cc_alt == NULL);

	/* Now, pull the first event */
get_next:
	if (timeout) {
		CHECK_FCT( fd_event_timedget(conn->cc_incoming, timeout, FDEVP_PSM_TIMEOUT, &ev, &ev_sz, &ev_data) );
	} else {
		CHECK_FCT( fd_event_get(conn->cc_incoming, &ev, &ev_sz, &ev_data) );
	}
	
	switch (ev) {
		case FDEVP_CNX_MSG_RECV:
			/* We got one */
			*len = ev_sz;
			*buf = ev_data;
			return 0;
			
		case FDEVP_PSM_TIMEOUT:
			TRACE_DEBUG(FULL, "Timeout event received");
			return ETIMEDOUT;
			
		case FDEVP_CNX_EP_CHANGE:
			/* We ignore this event */
			goto get_next;
			
		case FDEVP_CNX_ERROR:
			TRACE_DEBUG(FULL, "Received ERROR event on the connection");
			return ENOTCONN;
	}
	
	TRACE_DEBUG(INFO, "Received unexpected event %d (%s)", ev, fd_pev_str(ev));
	return EINVAL;
}

/* Set an alternate FIFO list to send FDEVP_CNX_* events to */
int fd_cnx_recv_setaltfifo(struct cnxctx * conn, struct fifo * alt_fifo)
{
	TRACE_ENTRY( "%p %p", conn, alt_fifo );
	CHECK_PARAMS( conn && alt_fifo && conn->cc_incoming );
	
	/* The magic function does it all */
	CHECK_FCT( fd_fifo_move( &conn->cc_incoming, alt_fifo, &conn->cc_alt ) );
	
	return 0;
}

/* Wrapper around gnutls_record_recv to handle some error codes */
static ssize_t fd_tls_send_handle_error(struct cnxctx * conn, gnutls_session_t session, void * data, size_t sz)
{
	ssize_t ret;
again:	
	CHECK_GNUTLS_DO( ret = gnutls_record_send(session, data, sz),
		{
			switch (ret) {
				case GNUTLS_E_REHANDSHAKE: 
					CHECK_GNUTLS_DO( ret = gnutls_handshake(session),
						{
							if (TRACE_BOOL(INFO)) {
								fd_log_debug("TLS re-handshake failed on socket %d (%s) : %s\n", conn->cc_socket, conn->cc_id, gnutls_strerror(ret));
							}
							goto end;
						} );

				case GNUTLS_E_AGAIN:
				case GNUTLS_E_INTERRUPTED:
					goto again;

				default:
					TRACE_DEBUG(INFO, "This TLS error is not handled, assume unrecoverable error");
			}
		} );
end:	
	return ret;
}



/* Send function when no multi-stream is involved, or sending on stream #0 (send() always use stream 0)*/
static int send_simple(struct cnxctx * conn, unsigned char * buf, size_t len)
{
	ssize_t ret;
	size_t sent = 0;
	TRACE_ENTRY("%p %p %zd", conn, buf, len);
	do {
		if (conn->cc_tls) {
			CHECK_GNUTLS_DO( ret = fd_tls_send_handle_error(conn, conn->cc_tls_para.session, buf + sent, len - sent), return ENOTCONN );
		} else {
			CHECK_SYS( ret = send(conn->cc_socket, buf + sent, len - sent, 0) ); /* better to replace with sendmsg for atomic sending? */
		}
		sent += ret;
	} while ( sent < len );
	return 0;
}

/* Send a message -- this is synchronous -- and we assume it's never called by several threads at the same time, so we don't protect. */
int fd_cnx_send(struct cnxctx * conn, unsigned char * buf, size_t len)
{
	TRACE_ENTRY("%p %p %zd", conn, buf, len);
	
	CHECK_PARAMS(conn && (conn->cc_socket > 0) && buf && len);

	TRACE_DEBUG(FULL, "Sending %zdb %sdata on connection %s", len, conn->cc_tls ? "TLS-protected ":"", conn->cc_id);
	
	switch (conn->cc_proto) {
		case IPPROTO_TCP:
			CHECK_FCT( send_simple(conn, buf, len) );
			break;
		
#ifndef DISABLE_SCTP
		case IPPROTO_SCTP: {
			int multistr = 0;
			
			if ((conn->cc_sctp_para.str_out > 1) && ((! conn->cc_tls) || (conn->cc_sctp_para.pairs > 1)))  {
				/* Update the id of the stream we will send this message on */
				conn->cc_sctp_para.next += 1;
				conn->cc_sctp_para.next %= (conn->cc_tls ? conn->cc_sctp_para.pairs : conn->cc_sctp_para.str_out);
				multistr = 1;
			}
			
			if ((!multistr) || (conn->cc_sctp_para.next == 0)) {
				CHECK_FCT( send_simple(conn, buf, len) );
			} else {
				if (!conn->cc_tls) {
					CHECK_FCT( fd_sctp_sendstr(conn->cc_socket, conn->cc_sctp_para.next, buf, len) );
				} else {
					/* push the record to the appropriate session */
					ssize_t ret;
					size_t sent = 0;
					ASSERT(conn->cc_sctps_data.array != NULL);
					do {
						CHECK_GNUTLS_DO( ret = fd_tls_send_handle_error(conn, conn->cc_sctps_data.array[conn->cc_sctp_para.next].session, buf + sent, len - sent), return ENOTCONN );
						sent += ret;
					} while ( sent < len );
				}
			}
		}
		break;
#endif /* DISABLE_SCTP */
	
		default:
			TRACE_DEBUG(INFO, "Unknwon protocol: %d", conn->cc_proto);
			return ENOTSUP;	/* or EINVAL... */
	}
	
	return 0;
}


/**************************************/
/*     Destruction of connection      */
/**************************************/

/* Destroy a conn structure, and shutdown the socket */
void fd_cnx_destroy(struct cnxctx * conn)
{
	TRACE_ENTRY("%p", conn);
	
	CHECK_PARAMS_DO(conn, return);

	/* In case of TLS, stop receiver thread, then close properly the gnutls session */
	if ((conn->cc_tls) && (conn->cc_sctp_para.pairs > 1)) {
#ifndef DISABLE_SCTP
		/* Multi-stream TLS: Stop all decipher threads, but not the demux thread */
		fd_sctps_stopthreads(conn);
#endif /* DISABLE_SCTP */
	} else {
		/* Stop the decoding thread */
		CHECK_FCT_DO( fd_thr_term(&conn->cc_rcvthr), /* continue */ );
	}
	
	/* Terminate properly the TLS session(s) */
	if (conn->cc_tls) {
		/* Master session */
		CHECK_GNUTLS_DO( gnutls_bye(conn->cc_tls_para.session, GNUTLS_SHUT_RDWR), /* Continue */ );
		gnutls_deinit(conn->cc_tls_para.session);
		
#ifndef DISABLE_SCTP
		if (conn->cc_sctp_para.pairs > 1) {
			/* Multi-stream TLS: destroy the wrapper and stop the demux thread */
			fd_sctps_destroy(conn);
		}
#endif /* DISABLE_SCTP */
		
	}
	
	/* Shut the connection down */
	if (conn->cc_socket > 0) {
		shutdown(conn->cc_socket, SHUT_RDWR);
		close(conn->cc_socket);
		conn->cc_socket = -1;
	}
	
	/* Empty and destroy FIFO list */
	if (conn->cc_incoming) {
		fd_event_destroy( &conn->cc_incoming, free );
	}
	
	/* Free the object */
	free(conn);
	
	/* Done! */
	return;
}
