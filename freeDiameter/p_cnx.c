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


/* The thread that attempts the connection */
static void * connect_thr(void * arg)
{
	struct fd_peer * peer = arg;
	struct cnxctx * cnx = NULL;
	
	
	
	/* Use the flags in the peer to select the protocol */
	
	TODO("loop on fd_cnx_cli_connect_tcp or fd_cnx_cli_connect_sctp");
	
	
	/* Now, we have an established connection in cnx */
	
	pthread_cleanup_push((void *)fd_cnx_destroy, cnx);
	
	/* Handshake if needed (secure port) */
	
	
	
	/* Upon success, generate FDEVP_CNX_ESTABLISHED */
	CHECK_FCT_DO( fd_event_send(peer->p_events, FDEVP_CNX_ESTABLISHED, 0, cnx), goto fatal_error );
	pthread_cleanup_pop(0);
	
	return NULL;
fatal_error:
	/* Cleanup the connection */
	fd_cnx_destroy(cnx);

	/* Generate a termination event */
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	
	return NULL;
}


/* Initiate a connection attempt to a remote peer */
int fd_p_cnx_init(struct fd_peer * peer)
{
	/* Start the connect thread */
	CHECK_FCT( pthread_create(&peer->p_ini_thr, NULL, connect_thr, peer) );
	return 0;
}
