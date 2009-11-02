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

/* The actual declaration of peer_state_str */
DECLARE_STATE_STR();

/* Helper for next macro */
#define case_str( _val )		\
	case _val : return #_val

DECLARE_PEV_STR();

/************************************************************************/
/*                      Delayed startup                                 */
/************************************************************************/
static int started = 0;
static pthread_mutex_t  started_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   started_cnd = PTHREAD_COND_INITIALIZER;

/* Wait for start signal */
static int fd_psm_waitstart()
{
	TRACE_ENTRY("");
	CHECK_POSIX( pthread_mutex_lock(&started_mtx) );
awake:	
	if (! started) {
		pthread_cleanup_push( fd_cleanup_mutex, &started_mtx );
		CHECK_POSIX( pthread_cond_wait(&started_cnd, &started_mtx) );
		pthread_cleanup_pop( 0 );
		goto awake;
	}
	CHECK_POSIX( pthread_mutex_unlock(&started_mtx) );
	return 0;
}

/* Allow the state machines to start */
int fd_psm_start()
{
	TRACE_ENTRY("");
	CHECK_POSIX( pthread_mutex_lock(&started_mtx) );
	started = 1;
	CHECK_POSIX( pthread_cond_broadcast(&started_cnd) );
	CHECK_POSIX( pthread_mutex_unlock(&started_mtx) );
	return 0;
}


/************************************************************************/
/*                 Manage the list of active peers                      */
/************************************************************************/

/* Enter/leave OPEN state */
static int enter_open_state(struct fd_peer * peer)
{
	struct fd_list * li;
	CHECK_PARAMS( FD_IS_LIST_EMPTY(&peer->p_actives) );
	
	/* Callback registered by the credential validator (fd_peer_validate_register) */
	if (peer->p_cb2) {
		CHECK_FCT_DO( (*peer->p_cb2)(&peer->p_hdr.info),
			{
				TRACE_DEBUG(FULL, "Validation failed, moving to state CLOSING");
				peer->p_hdr.info.pi_state = STATE_CLOSING;
				fd_psm_terminate(peer);
			} );
		peer->p_cb2 = NULL;
		return 0;
	}
	/* Insert in the active peers list */
	CHECK_POSIX( pthread_rwlock_wrlock(&fd_g_activ_peers_rw) );
	for (li = fd_g_activ_peers.next; li != &fd_g_activ_peers; li = li->next) {
		struct fd_peer * next_p = (struct fd_peer *)li->o;
		int cmp = strcmp(peer->p_hdr.info.pi_diamid, next_p->p_hdr.info.pi_diamid);
		if (cmp < 0)
			break;
	}
	fd_list_insert_before(li, &peer->p_actives);
	CHECK_POSIX( pthread_rwlock_unlock(&fd_g_activ_peers_rw) );
	
	/* Callback registered when the peer was added, by fd_peer_add */
	if (peer->p_cb) {
		TRACE_DEBUG(FULL, "Calling add callback for peer %s", peer->p_hdr.info.pi_diamid);
		(*peer->p_cb)(&peer->p_hdr.info, peer->p_cb_data);
		peer->p_cb = NULL;
		peer->p_cb_data = NULL;
	}
	
	/* Start the thread to handle outgoing messages */
	CHECK_FCT( fd_out_start(peer) );
	
	return 0;
}
static int leave_open_state(struct fd_peer * peer)
{
	/* Remove from active peers list */
	CHECK_POSIX( pthread_rwlock_wrlock(&fd_g_activ_peers_rw) );
	fd_list_unlink( &peer->p_actives );
	CHECK_POSIX( pthread_rwlock_unlock(&fd_g_activ_peers_rw) );
	
	/* Stop the "out" thread */
	CHECK_FCT( fd_out_stop(peer) );
	
	/* Failover the messages */
	fd_peer_failover_msg(peer);
	
	return 0;
}


/************************************************************************/
/*                      Helpers for state changes                       */
/************************************************************************/
/* Change state */
int fd_psm_change_state(struct fd_peer * peer, int new_state)
{
	int old;
	
	TRACE_ENTRY("%p %d(%s)", peer, new_state, STATE_STR(new_state));
	CHECK_PARAMS( CHECK_PEER(peer) );
	old = peer->p_hdr.info.pi_state;
	if (old == new_state)
		return 0;
	
	TRACE_DEBUG(FULL, "'%s'\t-> '%s'\t'%s'",
			STATE_STR(old),
			STATE_STR(new_state),
			peer->p_hdr.info.pi_diamid);
	
	if (old == STATE_OPEN) {
		CHECK_FCT( leave_open_state(peer) );
	}
	
	peer->p_hdr.info.pi_state = new_state;
	
	if (new_state == STATE_OPEN) {
		CHECK_FCT( enter_open_state(peer) );
	}
	
	if ((new_state == STATE_CLOSED) && (peer->p_hdr.info.pi_flags.persist == PI_PRST_NONE)) {
		CHECK_FCT( fd_event_send(peer->p_events, FDEVP_TERMINATE, 0, NULL) );
	}
	
	return 0;
}

/* Set timeout timer of next event */
void fd_psm_next_timeout(struct fd_peer * peer, int add_random, int delay)
{
	/* Initialize the timer */
	CHECK_POSIX_DO(  clock_gettime( CLOCK_REALTIME,  &peer->p_psm_timer ), ASSERT(0) );
	
	if (add_random) {
		if (delay > 2)
			delay -= 2;
		else
			delay = 0;

		/* Add a random value between 0 and 4sec */
		peer->p_psm_timer.tv_sec += random() % 4;
		peer->p_psm_timer.tv_nsec+= random() % 1000000000L;
		if (peer->p_psm_timer.tv_nsec > 1000000000L) {
			peer->p_psm_timer.tv_nsec -= 1000000000L;
			peer->p_psm_timer.tv_sec ++;
		}
	}
	
	peer->p_psm_timer.tv_sec += delay;
	
#ifdef SLOW_PSM
	/* temporary for debug */
	peer->p_psm_timer.tv_sec += 10;
#endif
}

/* Cleanup the peer */
void fd_psm_cleanup(struct fd_peer * peer)
{
	/* Move to CLOSED state */
	CHECK_FCT_DO( fd_psm_change_state(peer, STATE_CLOSED), /* continue */ );
	
	/* Destroy the connection */
	if (peer->p_cnxctx) {
		fd_cnx_destroy(peer->p_cnxctx);
		peer->p_cnxctx = NULL;
	}
	
	/* What else ? */
	TODO("...");
	
}


/************************************************************************/
/*                      The PSM thread                                  */
/************************************************************************/
/* Cancelation cleanup : set ZOMBIE state in the peer */
void cleanup_setstate(void * arg) 
{
	struct fd_peer * peer = (struct fd_peer *)arg;
	CHECK_PARAMS_DO( CHECK_PEER(peer), return );
	peer->p_hdr.info.pi_state = STATE_ZOMBIE;
	return;
}

/* The state machine thread (controler) */
static void * p_psm_th( void * arg )
{
	struct fd_peer * peer = (struct fd_peer *)arg;
	int created_started = started ? 1 : 0;
	int event;
	size_t ev_sz;
	void * ev_data;
	
	CHECK_PARAMS_DO( CHECK_PEER(peer), ASSERT(0) );
	
	pthread_cleanup_push( cleanup_setstate, arg );
	
	/* Set the thread name */
	{
		char buf[48];
		sprintf(buf, "PSM/%.*s", sizeof(buf) - 5, peer->p_hdr.info.pi_diamid);
		fd_log_threadname ( buf );
	}
	
	/* The state machine starts in CLOSED state */
	peer->p_hdr.info.pi_state = STATE_CLOSED;
	
	/* Wait that the PSM are authorized to start in the daemon */
	CHECK_FCT_DO( fd_psm_waitstart(), goto psm_end );
	
	/* Initialize the timer */
	if (peer->p_flags.pf_responder) {
		fd_psm_next_timeout(peer, 0, INCNX_TIMEOUT);
	} else {
		fd_psm_next_timeout(peer, created_started, 0);
	}
	
psm_loop:
	/* Get next event */
	CHECK_FCT_DO( fd_event_timedget(peer->p_events, &peer->p_psm_timer, FDEVP_PSM_TIMEOUT, &event, &ev_sz, &ev_data), goto psm_end );
	TRACE_DEBUG(FULL, "'%s'\t<-- '%s'\t(%p,%zd)\t'%s'",
			STATE_STR(peer->p_hdr.info.pi_state),
			fd_pev_str(event), ev_data, ev_sz,
			peer->p_hdr.info.pi_diamid);

	/* Now, the action depends on the current state and the incoming event */

	/* The following states are impossible */
	ASSERT( peer->p_hdr.info.pi_state != STATE_NEW );
	ASSERT( peer->p_hdr.info.pi_state != STATE_ZOMBIE );
	ASSERT( peer->p_hdr.info.pi_state != STATE_OPEN_HANDSHAKE ); /* because it exists only between two loops */

	/* Purge invalid events */
	if (!CHECK_PEVENT(event)) {
		TRACE_DEBUG(INFO, "Invalid event received in PSM '%s' : %d", peer->p_hdr.info.pi_diamid, event);
		goto psm_loop;
	}

	/* Handle the (easy) debug event now */
	if (event == FDEVP_DUMP_ALL) {
		fd_peer_dump(peer, ANNOYING);
		goto psm_loop;
	}

	/* Requests to terminate the peer object */
	if (event == FDEVP_TERMINATE) {
		switch (peer->p_hdr.info.pi_state) {
			case STATE_OPEN:
			case STATE_REOPEN:
				/* We cannot just close the conenction, we have to send a DPR first */
				CHECK_FCT_DO( fd_p_dp_initiate(peer), goto psm_end );
				goto psm_loop;
			
			/*	
			case STATE_CLOSING:
			case STATE_WAITCNXACK:
			case STATE_WAITCNXACK_ELEC:
			case STATE_WAITCEA:
			case STATE_SUSPECT:
			case STATE_CLOSED:
			*/
			default:
				/* In these cases, we just cleanup the peer object (if needed) and terminate */
				goto psm_end;
		}
	}
	
	/* A message was received */
	if (event == FDEVP_CNX_MSG_RECV) {
		struct msg * msg = NULL;
		struct msg_hdr * hdr;
		
		/* Parse the received buffer */
		CHECK_FCT_DO( fd_msg_parse_buffer( (void *)&ev_data, ev_sz, &msg), 
			{
				fd_log_debug("Received invalid data from peer '%s', closing the connection\n", peer->p_hdr.info.pi_diamid);
				CHECK_FCT_DO( fd_event_send(peer->p_events, FDEVP_CNX_ERROR, 0, NULL), goto psm_end );
				goto psm_loop;
			} );
		
		TRACE_DEBUG(FULL, "Received a message (%zdb) from '%s'", ev_sz, peer->p_hdr.info.pi_diamid);
		fd_msg_dump_walk(FULL + 1, msg);
	
		/* Extract the header */
		CHECK_FCT_DO( fd_msg_hdr(msg, &hdr), goto psm_end );
		
		/* If it is an answer, associate with the request */
		if (!(hdr->msg_flags & CMD_FLAG_REQUEST)) {
			struct msg * req;
			/* Search matching request (same hbhid) */
			CHECK_FCT_DO( fd_p_sr_fetch(&peer->p_sr, hdr->msg_hbhid, &req), goto psm_end );
			if (req == NULL) {
				fd_log_debug("Received a Diameter answer message with no corresponding sent request, discarding...\n");
				fd_msg_dump_walk(NONE, msg);
				fd_msg_free(msg);
				goto psm_loop;
			}
			
			/* Associate */
			CHECK_FCT_DO( fd_msg_answ_associate( msg, req ), goto psm_end );
		}
		
		/* Now handle non-link-local messages */
		if (fd_msg_is_routable(msg)) {
			/* If we are not in OPEN state, discard the message */
			if (peer->p_hdr.info.pi_state != STATE_OPEN) {
				fd_log_debug("Received a routable message while not in OPEN state from peer '%s', discarded.\n", peer->p_hdr.info.pi_diamid);
				fd_msg_dump_walk(NONE, msg);
				fd_msg_free(msg);
			} else {
				/* We received a valid message, update the expiry timer */
				CHECK_FCT_DO( fd_p_expi_update(peer), goto psm_end );

				/* Set the message source and add the Route-Record */
				CHECK_FCT_DO( fd_msg_source_set( msg, peer->p_hdr.info.pi_diamid, 1, fd_g_config->cnf_dict ), goto psm_end);

				/* Requeue to the global incoming queue */
				CHECK_FCT_DO(fd_fifo_post(fd_g_incoming, &msg), goto psm_end );
				
				/* Update the peer timer */
				if (!peer->p_flags.pf_dw_pending) {
					fd_psm_next_timeout(peer, 1, peer->p_hdr.info.pi_twtimer ?: fd_g_config->cnf_timer_tw);
				}
			}
			goto psm_loop;
		}
		
		/* Link-local message: They must be understood by our dictionary */
		{
			int ret;
			CHECK_FCT_DO( ret = fd_msg_parse_or_error( &msg ),
				{
					if ((ret == EBADMSG) && (msg != NULL)) {
						/* msg now contains an answer message to send back */
						CHECK_FCT_DO( fd_out_send(&msg, peer->p_cnxctx, peer), /* In case of error the message has already been dumped */ );
					}
					if (msg) {
						CHECK_FCT_DO( fd_msg_free(msg), /* continue */);
					}
					goto psm_loop;
				} );
		}
		
		ASSERT( hdr->msg_appl == 0 ); /* buggy fd_msg_is_routable() ? */
		
		/* Handle the LL message and update the expiry timer appropriately */
		switch (hdr->msg_code) {
			case CC_DEVICE_WATCHDOG:
				CHECK_FCT_DO( fd_p_dw_handle(&msg, peer), goto psm_end );
				break;
			
			case CC_DISCONNECT_PEER:
				CHECK_FCT_DO( fd_p_dp_handle(&msg, peer), goto psm_end );
				break;
			
			case CC_CAPABILITIES_EXCHANGE:
				CHECK_FCT_DO( fd_p_ce_handle(&msg, peer), goto psm_end );
				break;
			
			default:
				/* Unknown / unexpected / invalid message */
				TODO("Log, return error message if request");
		};
		
		/* At this point the message must have been fully handled already */
		if (msg) {
			fd_log_debug("Internal error: unhandled message (discarded)!\n");
			fd_msg_dump_walk(NONE, msg);
			fd_msg_free(msg);
		}
		
		goto psm_loop;
	}
	
	/* The connection object is broken */
	if (event == FDEVP_CNX_ERROR) {
		/* Cleanup the peer */
		fd_psm_cleanup(peer);
		
		/* Mark the connection problem */
		peer->p_flags.pf_cnx_pb = 1;
		
		/* Move to CLOSED */
		CHECK_FCT_DO( fd_psm_change_state(peer, STATE_CLOSED), goto psm_end );
		
		/* Reset the timer */
		fd_psm_next_timeout(peer, 1, peer->p_hdr.info.pi_tctimer ?: fd_g_config->cnf_timer_tc);
		
		/* Loop */
		goto psm_loop;
	}
	
	/* The connection notified a change in endpoints */
	if (event == FDEVP_CNX_EP_CHANGE) {
		/* Cleanup the remote LL and primary addresses */
		CHECK_FCT_DO( fd_ep_filter( &peer->p_hdr.info.pi_endpoints, EP_FL_CONF | EP_FL_DISC | EP_FL_ADV ), /* ignore the error */);
		CHECK_FCT_DO( fd_ep_clearflags( &peer->p_hdr.info.pi_endpoints, EP_FL_PRIMARY ), /* ignore the error */);
		
		/* Get the new ones */
		CHECK_FCT_DO( fd_cnx_getendpoints(peer->p_cnxctx, NULL, &peer->p_hdr.info.pi_endpoints), /* ignore the error */);
		
		if (TRACE_BOOL(ANNOYING)) {
			fd_log_debug("New remote endpoint(s):\n");
			fd_ep_dump(6, &peer->p_hdr.info.pi_endpoints);
		}
		
		/* Done */
		goto psm_loop;
	}
	
	/* A new connection was established and CER containing this peer id was received */
	if (event == FDEVP_CNX_INCOMING) {
		struct cnx_incoming * params = ev_data;
		ASSERT(params);
		
		/* Handle the message */
		CHECK_FCT_DO( fd_p_ce_handle_newCER(&params->cer, peer, &params->cnx, params->validate), goto psm_end );
		
		/* Cleanup if needed */
		if (params->cnx) {
			fd_cnx_destroy(params->cnx);
			params->cnx = NULL;
		}
		if (params->cer) {
			CHECK_FCT_DO( fd_msg_free(params->cer), );
			params->cer = NULL;
		}
		
		/* Loop */
		free(ev_data);
		goto psm_loop;
	}
	
	/* The timeout for the current state has been reached */
	if (event == FDEVP_PSM_TIMEOUT) {
		switch (peer->p_hdr.info.pi_state) {
			case STATE_OPEN:
			case STATE_REOPEN:
				CHECK_FCT_DO( fd_p_dw_timeout(peer), goto psm_end );
				break;
				
			case STATE_CLOSED:
				TODO("Initiate a new connection");
				break;
				
			case STATE_CLOSING:
			case STATE_SUSPECT:
			case STATE_WAITCNXACK:
			case STATE_WAITCEA:
				/* Destroy the connection, restart the timer to a new connection attempt */
				fd_psm_cleanup(peer);
				fd_psm_next_timeout(peer, 1, peer->p_hdr.info.pi_tctimer ?: fd_g_config->cnf_timer_tc);
				CHECK_FCT_DO( fd_psm_change_state(peer, STATE_CLOSED), goto psm_end );
				break;
				
			case STATE_WAITCNXACK_ELEC:
				TODO("Abort initiating side, handle the receiver side");
				break;
		}
	}
	
	/* Default action : the handling has not yet been implemented. [for debug only] */
	TODO("Missing handler in PSM : '%s'\t<-- '%s'", STATE_STR(peer->p_hdr.info.pi_state), fd_pev_str(event));
	if (event == FDEVP_PSM_TIMEOUT) {
		/* We have not handled timeout in this state, let's postpone next alert */
		fd_psm_next_timeout(peer, 0, 60);
	}
	
	goto psm_loop;

psm_end:
	fd_psm_cleanup(peer);
	pthread_cleanup_pop(1); /* set STATE_ZOMBIE */
	peer->p_psm = (pthread_t)NULL;
	pthread_detach(pthread_self());
	return NULL;
}


/************************************************************************/
/*                      Functions to control the PSM                    */
/************************************************************************/
/* Create the PSM thread of one peer structure */
int fd_psm_begin(struct fd_peer * peer )
{
	TRACE_ENTRY("%p", peer);
	
	/* Check the peer and state are OK */
	CHECK_PARAMS( CHECK_PEER(peer) && (peer->p_hdr.info.pi_state == STATE_NEW) );
	
	/* Create the PSM controler thread */
	CHECK_POSIX( pthread_create( &peer->p_psm, NULL, p_psm_th, peer ) );
	
	/* We're done */
	return 0;
}

/* End the PSM (clean ending) */
int fd_psm_terminate(struct fd_peer * peer )
{
	TRACE_ENTRY("%p", peer);
	CHECK_PARAMS( CHECK_PEER(peer) );
	
	if (peer->p_hdr.info.pi_state != STATE_ZOMBIE) {
		CHECK_FCT( fd_event_send(peer->p_events, FDEVP_TERMINATE, 0, NULL) );
	} else {
		TRACE_DEBUG(FULL, "Peer '%s' was already terminated", peer->p_hdr.info.pi_diamid);
	}
	return 0;
}

/* End the PSM & cleanup the peer structure */
void fd_psm_abord(struct fd_peer * peer )
{
	TRACE_ENTRY("%p", peer);
	
	/* Cancel PSM thread */
	CHECK_FCT_DO( fd_thr_term(&peer->p_psm), /* continue */ );
	
	/* Cancel the OUT thread */
	CHECK_FCT_DO( fd_out_stop(peer), /* continue */ );
	
	/* Cleanup the connection */
	if (peer->p_cnxctx) {
		fd_cnx_destroy(peer->p_cnxctx);
	}
	
	/* Failover the messages */
	fd_peer_failover_msg(peer);
	
	/* Empty the events list, this might leak some memory, but we only do it on exit, so... */
	fd_event_destroy(&peer->p_events, free);
	
	/* More cleanups are performed in fd_peer_free */
	return;
}

