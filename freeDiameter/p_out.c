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
static int do_send(struct msg ** msg, struct cnxctx * cnx, uint32_t * hbh, struct sr_list * srl)
{
	struct msg_hdr * hdr;
	int msg_is_a_req;
	uint8_t * buf;
	size_t sz;
	int ret;
	
	TRACE_ENTRY("%p %p %p %p", msg, cnx, hbh, srl);
	
	/* Retrieve the message header */
	CHECK_FCT( fd_msg_hdr(*msg, &hdr) );
	
	msg_is_a_req = (hdr->msg_flags & CMD_FLAG_REQUEST);
	if (msg_is_a_req) {
		CHECK_PARAMS(hbh && srl);
		/* Alloc the hop-by-hop id and increment the value for next message */
		hdr->msg_hbhid = *hbh;
		*hbh = hdr->msg_hbhid + 1;
	}
	
	/* Create the message buffer */
	CHECK_FCT(fd_msg_bufferize( *msg, &buf, &sz ));
	
	/* Send the message */
	pthread_cleanup_push( free, buf );
	CHECK_FCT_DO( ret = fd_cnx_send(cnx, buf, sz), { free(buf); return ret; } );
	pthread_cleanup_pop(1);
	
	/* Save a request */
	if (msg_is_a_req) {
		CHECK_FCT_DO( fd_p_sr_store(srl, msg, &hdr->msg_hbhid),
			{
				fd_log_debug("The following request was sent successfully but not saved locally:\n" );
				fd_log_debug("	(as a result the matching answer will be discarded)\n" );
				fd_msg_dump_walk(NONE, *msg);
			} );
				
	}
	
	/* Free answers and unsaved requests */
	if (*msg) {
		CHECK_FCT( fd_msg_free(*msg) );
		*msg = NULL;
	}
	
	return 0;
}

static void cleanup_requeue(void * arg)
{
	struct msg *msg = arg;
	CHECK_FCT_DO(fd_fifo_post(fd_g_outgoing, &msg),
			CHECK_FCT_DO(fd_msg_free(msg), /* What can we do more? */));
}

/* The code of the "out" thread */
static void * out_thr(void * arg)
{
	struct fd_peer * peer = arg;
	ASSERT( CHECK_PEER(peer) );
	
	/* Set the thread name */
	{
		char buf[48];
		sprintf(buf, "OUT/%.*s", sizeof(buf) - 5, peer->p_hdr.info.pi_diamid);
		fd_log_threadname ( buf );
	}
	
	/* Loop until cancelation */
	while (1) {
		struct msg * msg;
		
		/* Retrieve next message to send */
		CHECK_FCT_DO( fd_fifo_get(peer->p_tosend, &msg), goto error );
		
		/* Now if we are cancelled, we requeue this message */
		pthread_cleanup_push(cleanup_requeue, msg);
		
		/* Send the message, log any error */
		CHECK_FCT_DO( do_send(&msg, peer->p_cnxctx, &peer->p_hbh, &peer->p_sr),
			{
				fd_log_debug("An error occurred while sending this message, it is lost:\n");
				fd_msg_dump_walk(NONE, msg);
				fd_msg_free(msg);
			} );
			
		/* Loop */
		pthread_cleanup_pop(0);
	}
	
error:
	/* It is not really a connection error, but the effect is the same, we are not able to send anymore message */
	CHECK_FCT_DO( fd_event_send(peer->p_events, FDEVP_CNX_ERROR, 0, NULL), /* What do we do if it fails? */ );
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
		CHECK_FCT( do_send(msg, cnx, hbh, peer ? &peer->p_sr : NULL) );
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
		
