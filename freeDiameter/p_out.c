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

/* Alloc a new hbh for requests, bufferize the message and send on the connection, save in sentreq if provided */
static int do_send(struct msg ** msg, struct cnxctx * cnx, uint32_t * hbh, struct fd_list * sentreq)
{
	TRACE_ENTRY("%p %p %p %p", msg, cnx, hbh, sentreq);
	
	TODO("If message is a request");
		TODO("Alloc new *hbh");
	
	TODO("Bufferize the message, send it");
	
	TODO("Save in sentreq or free")
	
	return ENOTSUP;
}

/* The code of the "out" thread */
static void * out_thr(void * arg)
{
	TODO("Pick next message in peer->p_tosend");
	TODO("do_send, log errors");
	TODO("In case of cancellation, requeue the message");
	return NULL;
error:
	TODO(" Send an event to the peer ");
	return NULL;
}

/* Wrapper to sending a message either by out thread (peer in OPEN state) or directly; cnx or peer must be provided */
int fd_out_send(struct msg ** msg, struct cnxctx * cnx, struct fd_peer * peer)
{
	TRACE_ENTRY("%p %p %p", msg, cnx, peer);
	CHECK_PARAMS( msg && *msg && (cnx || (peer && peer->p_cnxctx)));
	
	if (peer && (peer->p_hdr.info.pi_state == STATE_OPEN)) {
		/* Normal case: just queue for the out thread to pick it up */
		CHECK_FCT( fd_fifo_post(peer->p_tosend, msg) );
		
	} else {
		uint32_t *hbh = NULL;
		
		/* In other cases, the thread is not running, so we handle the sending directly */
		if (peer)
			hbh = &peer->p_hbh;

		if (!cnx)
			cnx = peer->p_cnxctx;

		/* Do send the message */
		CHECK_FCT( do_send(msg, cnx, hbh, peer ? &peer->p_sentreq : NULL) );
	}
	
	return 0;
}

/* Start the "out" thread that picks messages in p_tosend and send them on p_cnxctx */
int fd_out_start(struct fd_peer * peer)
{
	TRACE_ENTRY("%p", peer);
	CHECK_PARAMS( CHECK_PEER(peer) && (peer->p_outthr == (pthread_t)NULL) );
	
	CHECK_POSIX( pthread_create(&peer->p_outthr, NULL, out_thr, peer) );
	
	return 0;
}

/* Stop that thread */
int fd_out_stop(struct fd_peer * peer)
{
	TRACE_ENTRY("%p", peer);
	CHECK_PARAMS( CHECK_PEER(peer) );
	
	CHECK_FCT( fd_thr_term(&peer->p_outthr) );
	
	return 0;
}
		
