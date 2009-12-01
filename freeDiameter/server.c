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

/* Server (listening) part of the daemon */

struct fd_list		FD_SERVERS = FD_LIST_INITIALIZER(FD_SERVERS);	/* The list of all server objects */
/* We don't need to protect this list, it is only accessed from the main daemon thread. */

/* Servers information */
struct server {
	struct fd_list	chain;		/* link in the FD_SERVERS list */

	struct cnxctx *	conn;		/* server connection context (listening socket) */
	int 		proto;		/* IPPROTO_TCP or IPPROTO_SCTP */
	int 		secur;		/* TLS is started immediatly after connection ? */
	
	pthread_t	thr;		/* The thread listening for new connections */
	int		status;		/* 0 : not created; 1 : running; 2 : terminated */
	
	struct fd_list	clients;	/* List of clients connected to this server, not yet identified */
	pthread_mutex_t	clients_mtx;	/* Mutex to protect the list of clients */
};

/* Client information (connecting peer for which we don't have the CER yet) */
struct client {
	struct fd_list	 chain;	/* link in the server's list of clients */
	struct cnxctx	*conn;	/* Parameters of the connection */
	struct timespec	 ts;	/* Deadline for receiving CER (after INCNX_TIMEOUT) */
	pthread_t	 thr; 	/* connection state machine */
};


/* Dump all servers information */
void fd_servers_dump()
{
	struct fd_list * li, *cli;
	
	fd_log_debug("Dumping servers list :\n");
	for (li = FD_SERVERS.next; li != &FD_SERVERS; li = li->next) {
		struct server * s = (struct server *)li;
		fd_log_debug("  Serv %p '%s': %s, %s, %s\n", 
				s, fd_cnx_getid(s->conn), 
				IPPROTO_NAME( s->proto ),
				s->secur ? "Secur" : "NotSecur",
				(s->status == 0) ? "Thread not created" :
				((s->status == 1) ? "Thread running" :
				((s->status == 2) ? "Thread terminated" :
							  "Thread status unknown")));
		/* Dump the client list of this server */
		(void) pthread_mutex_lock(&s->clients_mtx);
		for (cli = s->clients.next; cli != &s->clients; cli = cli->next) {
			struct client * c = (struct client *)cli;
			char bufts[128];
			fd_log_debug("     Connected: '%s' (timeout: %s)\n",
					fd_cnx_getid(c->conn),
					fd_log_time(&c->ts, bufts, sizeof(bufts)));
		}
		(void) pthread_mutex_unlock(&s->clients_mtx);
	}
}


/* The state machine to handle incoming connection before the remote peer is identified */
static void * client_sm(void * arg)
{
	struct client * c = arg;
	struct server * s = NULL;
	uint8_t       * buf = NULL;
	size_t 		bufsz;
	struct msg    * msg = NULL;
	struct msg_hdr *hdr = NULL;
	
	TRACE_ENTRY("%p", c);
	
	CHECK_PARAMS_DO(c && c->conn && c->chain.head, goto fatal_error );
	
	s = c->chain.head->o;
	
	/* Name the current thread */
	fd_log_threadname ( fd_cnx_getid(c->conn) );
	
	/* Handshake if we are a secure server port, or start clear otherwise */
	if (s->secur) {
		int ret = fd_cnx_handshake(c->conn, GNUTLS_SERVER, NULL, NULL);
		if (ret != 0) {
			if (TRACE_BOOL(INFO)) {
				fd_log_debug("TLS handshake failed for client '%s', connection aborted.\n", fd_cnx_getid(c->conn));
			}
			goto cleanup;
		}
	} else {
		CHECK_FCT_DO( fd_cnx_start_clear(c->conn, 0), goto cleanup );
	}
	
	/* Set the timeout to receive the first message */
	CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &c->ts), goto fatal_error );
	c->ts.tv_sec += INCNX_TIMEOUT;
	
	/* Receive the first Diameter message on the connection -- cleanup in case of timeout */
	CHECK_FCT_DO( fd_cnx_receive(c->conn, &c->ts, &buf, &bufsz), goto cleanup );
	
	TRACE_DEBUG(FULL, "Received %zdb from new client '%s'", bufsz, fd_cnx_getid(c->conn));
	
	/* Try parsing this message */
	CHECK_FCT_DO( fd_msg_parse_buffer( &buf, bufsz, &msg ), /* Parsing failed */ goto cleanup );
	
	/* We expect a CER, it must parse with our dictionary and rules */
	CHECK_FCT_DO( fd_msg_parse_rules( msg, fd_g_config->cnf_dict, NULL ), /* Parsing failed -- trace details ? */ goto cleanup );
	
	fd_msg_dump_walk(FULL, msg);
	
	/* Now check we received a CER */
	CHECK_FCT_DO( fd_msg_hdr ( msg, &hdr ), goto fatal_error );
	CHECK_PARAMS_DO( (hdr->msg_appl == 0) && (hdr->msg_flags & CMD_FLAG_REQUEST) && (hdr->msg_code == CC_CAPABILITIES_EXCHANGE),
		{ fd_log_debug("Connection '%s', expecting CER, received something else, closing...\n", fd_cnx_getid(c->conn)); goto cleanup; } );
	
	/* Finally, pass the information to the peers module which will handle it next */
	pthread_cleanup_push((void *)fd_cnx_destroy, c->conn);
	pthread_cleanup_push((void *)fd_msg_free, msg);
	CHECK_FCT_DO( fd_peer_handle_newCER( &msg, &c->conn ), goto cleanup );
	pthread_cleanup_pop(0);
	pthread_cleanup_pop(0);
	
	/* The end, we cleanup the client structure */
cleanup:
	/* Unlink the client structure */
	CHECK_POSIX_DO( pthread_mutex_lock(&s->clients_mtx), goto fatal_error );
	fd_list_unlink( &c->chain );
	CHECK_POSIX_DO( pthread_mutex_unlock(&s->clients_mtx), goto fatal_error );
	
	/* Destroy the connection object if present */
	if (c->conn)
		fd_cnx_destroy(c->conn);
	
	/* Cleanup the received buffer if any */
	free(buf);
	
	/* Cleanup the parsed message if any */
	if (msg) {
		CHECK_FCT_DO( fd_msg_free(msg), /* continue */);
	}
	
	/* Detach the thread, cleanup the client structure */
	pthread_detach(pthread_self());
	free(c);
	return NULL;
	
fatal_error:	/* This has effect to terminate the daemon */
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	return NULL;
}

/* The thread managing a server */
static void * serv_th(void * arg)
{
	struct server *s = (struct server *)arg;
	
	CHECK_PARAMS_DO(s, goto error);
	fd_log_threadname ( fd_cnx_getid(s->conn) );
	s->status = 1;
	
	/* Accept incoming connections */
	CHECK_FCT_DO( fd_cnx_serv_listen(s->conn), goto error );
	
	do {
		struct client * c = NULL;
		struct cnxctx * conn = NULL;
		
		/* Wait for a new client */
		CHECK_MALLOC_DO( conn = fd_cnx_serv_accept(s->conn), goto error );
		
		TRACE_DEBUG(FULL, "New connection accepted");
		
		/* Create a client structure */
		CHECK_MALLOC_DO( c = malloc(sizeof(struct client)), goto error );
		memset(c, 0, sizeof(struct client));
		fd_list_init(&c->chain, c);
		c->conn = conn;
		
		/* Save the client in the list */
		CHECK_POSIX_DO( pthread_mutex_lock( &s->clients_mtx ), goto error );
		fd_list_insert_before(&s->clients, &c->chain);
		CHECK_POSIX_DO( pthread_mutex_unlock( &s->clients_mtx ), goto error );

		/* Start the client thread */
		CHECK_POSIX_DO( pthread_create( &c->thr, NULL, client_sm, c ), goto error );
		
	} while (1);
	
error:	
	if (s)
		s->status = 2;
	/* Send error signal to the daemon */
	TRACE_DEBUG(INFO, "An error occurred in server module! Thread is terminating...");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );

	return NULL;
}


/* Create a new server structure */
static struct server * new_serv( int proto, int secur )
{
	struct server * new;
	
	/* New server structure */
	CHECK_MALLOC_DO( new = malloc(sizeof(struct server)), return NULL );
	
	memset(new, 0, sizeof(struct server));
	fd_list_init(&new->chain, new);
	new->proto = proto;
	new->secur = secur;
	CHECK_POSIX_DO( pthread_mutex_init(&new->clients_mtx, NULL), return NULL );
	fd_list_init(&new->clients, new);
	
	return new;
}

/* Start all the servers */
int fd_servers_start()
{
	struct server * s;
	
	int empty_conf_ep = FD_IS_LIST_EMPTY(&fd_g_config->cnf_endpoints);
	
	/* SCTP */
	if (!fd_g_config->cnf_flags.no_sctp) {
#ifdef DISABLE_SCTP
		ASSERT(0);
#else /* DISABLE_SCTP */
		
		/* Create the server on default port */
		CHECK_MALLOC( s = new_serv(IPPROTO_SCTP, 0) );
		CHECK_MALLOC( s->conn = fd_cnx_serv_sctp(fd_g_config->cnf_port, FD_IS_LIST_EMPTY(&fd_g_config->cnf_endpoints) ? NULL : &fd_g_config->cnf_endpoints) );
		fd_list_insert_before( &FD_SERVERS, &s->chain );
		CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );
		
		/* Retrieve the list of endpoints if it was empty */
		if (empty_conf_ep) {
			(void) fd_cnx_getendpoints(s->conn, &fd_g_config->cnf_endpoints, NULL);
			if (TRACE_BOOL(FULL)){
				fd_log_debug("Server bound on the following addresses :\n");
				fd_ep_dump( 5, &fd_g_config->cnf_endpoints );
			}
		}
		
		/* Create the server on secure port */
		CHECK_MALLOC( s = new_serv(IPPROTO_SCTP, 1) );
		CHECK_MALLOC( s->conn = fd_cnx_serv_sctp(fd_g_config->cnf_port_tls, FD_IS_LIST_EMPTY(&fd_g_config->cnf_endpoints) ? NULL : &fd_g_config->cnf_endpoints) );
		fd_list_insert_before( &FD_SERVERS, &s->chain );
		CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );
		
#endif /* DISABLE_SCTP */
	}
	
	/* TCP */
	if (!fd_g_config->cnf_flags.no_tcp) {
		
		if (empty_conf_ep) {
			/* Bind TCP servers on [0.0.0.0] */
			if (!fd_g_config->cnf_flags.no_ip4) {
				
				CHECK_MALLOC( s = new_serv(IPPROTO_TCP, 0) );
				CHECK_MALLOC( s->conn = fd_cnx_serv_tcp(fd_g_config->cnf_port, AF_INET, NULL) );
				fd_list_insert_before( &FD_SERVERS, &s->chain );
				CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );

				CHECK_MALLOC( s = new_serv(IPPROTO_TCP, 1) );
				CHECK_MALLOC( s->conn = fd_cnx_serv_tcp(fd_g_config->cnf_port_tls, AF_INET, NULL) );
				fd_list_insert_before( &FD_SERVERS, &s->chain );
				CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );
			}
			/* Bind TCP servers on [::] */
			if (!fd_g_config->cnf_flags.no_ip6) {
				
				CHECK_MALLOC( s = new_serv(IPPROTO_TCP, 0) );
				CHECK_MALLOC( s->conn = fd_cnx_serv_tcp(fd_g_config->cnf_port, AF_INET6, NULL) );
				fd_list_insert_before( &FD_SERVERS, &s->chain );
				CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );

				CHECK_MALLOC( s = new_serv(IPPROTO_TCP, 1) );
				CHECK_MALLOC( s->conn = fd_cnx_serv_tcp(fd_g_config->cnf_port_tls, AF_INET6, NULL) );
				fd_list_insert_before( &FD_SERVERS, &s->chain );
				CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );
			}
		} else {
			/* Create all endpoints -- check flags */
			struct fd_list * li;
			for (li = fd_g_config->cnf_endpoints.next; li != &fd_g_config->cnf_endpoints; li = li->next) {
				struct fd_endpoint * ep = (struct fd_endpoint *)li;
				sSA * sa = (sSA *) &ep->ss;
				if (! (ep->flags & EP_FL_CONF))
					continue;
				if (fd_g_config->cnf_flags.no_ip4 && (sa->sa_family == AF_INET))
					continue;
				if (fd_g_config->cnf_flags.no_ip6 && (sa->sa_family == AF_INET6))
					continue;
				
				CHECK_MALLOC( s = new_serv(IPPROTO_TCP, 0) );
				CHECK_MALLOC( s->conn = fd_cnx_serv_tcp(fd_g_config->cnf_port, sa->sa_family, ep) );
				fd_list_insert_before( &FD_SERVERS, &s->chain );
				CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );

				CHECK_MALLOC( s = new_serv(IPPROTO_TCP, 1) );
				CHECK_MALLOC( s->conn = fd_cnx_serv_tcp(fd_g_config->cnf_port_tls, sa->sa_family, ep) );
				fd_list_insert_before( &FD_SERVERS, &s->chain );
				CHECK_POSIX( pthread_create( &s->thr, NULL, serv_th, s ) );
			}
		}
	}
	
	return 0;
}

/* Terminate all the servers */
int fd_servers_stop()
{
	TODO("Not implemented");
	
	/* Loop on all servers */
		/* cancel thread */
		/* destroy server connection context */
		/* cancel and destroy all clients */
}