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
				TRACE_DEBUG(FULL, "Validation failed, terminating the connection");
				fd_psm_terminate(peer, "DO_NOT_WANT_TO_TALK_TO_YOU" );
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
	
	/* Update the expiry timer now */
	CHECK_FCT( fd_p_expi_update(peer) );
	
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

/* Cleanup pending events in the peer */
void fd_psm_events_free(struct fd_peer * peer)
{
	struct fd_event * ev;
	/* Purge all events, and free the associated data if any */
	while (fd_fifo_tryget( peer->p_events, &ev ) == 0) {
		switch (ev->code) {
			case FDEVP_CNX_ESTABLISHED: {
				fd_cnx_destroy(ev->data);
			}
			break;
			
			case FDEVP_TERMINATE:
				/* Do not free the string since it is a constant */
			break;
			
			case FDEVP_CNX_INCOMING: {
				struct cnx_incoming * evd = ev->data;
				CHECK_FCT_DO( fd_msg_free(evd->cer), /* continue */);
				fd_cnx_destroy(evd->cnx);
			}
			default:
				free(ev->data);
		}
		free(ev);
	}
}


/* Change state */
int fd_psm_change_state(struct fd_peer * peer, int new_state)
{
	int old;
	
	TRACE_ENTRY("%p %d(%s)", peer, new_state, STATE_STR(new_state));
	CHECK_PARAMS( CHECK_PEER(peer) );
	old = peer->p_hdr.info.runtime.pir_state;
	if (old == new_state)
		return 0;
	
	TRACE_DEBUG(FULL, "'%s'\t-> '%s'\t'%s'",
			STATE_STR(old),
			STATE_STR(new_state),
			peer->p_hdr.info.pi_diamid);
	
	peer->p_hdr.info.runtime.pir_state = new_state;
	
	if (old == STATE_OPEN) {
		CHECK_FCT( leave_open_state(peer) );
	}
	
	if (new_state == STATE_OPEN) {
		CHECK_FCT( enter_open_state(peer) );
	}
	
	if (new_state == STATE_CLOSED) {
		/* Purge event list */
		fd_psm_events_free(peer);
		
		/* If the peer is not persistant, we destroy it */
		if (peer->p_hdr.info.config.pic_flags.persist == PI_PRST_NONE) {
			CHECK_FCT( fd_event_send(peer->p_events, FDEVP_TERMINATE, 0, NULL) );
		}
	}
	
	return 0;
}

/* Set timeout timer of next event */
void fd_psm_next_timeout(struct fd_peer * peer, int add_random, int delay)
{
	TRACE_DEBUG(FULL, "Peer timeout reset to %d seconds%s", delay, add_random ? " (+/- 2)" : "" );
	
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
void fd_psm_cleanup(struct fd_peer * peer, int terminate)
{
	/* Move to CLOSED state: failover messages, stop OUT thread, unlink peer from active list */
	if (peer->p_hdr.info.runtime.pir_state != STATE_ZOMBIE) {
		CHECK_FCT_DO( fd_psm_change_state(peer, STATE_CLOSED), /* continue */ );
	}
	
	fd_p_cnx_abort(peer, terminate);
	
	fd_p_ce_clear_cnx(peer, NULL);
	
	if (peer->p_receiver) {
		fd_cnx_destroy(peer->p_receiver);
		peer->p_receiver = NULL;
	}
	
	if (terminate) {
		fd_psm_events_free(peer);
		CHECK_FCT_DO( fd_fifo_del(&peer->p_events), /* continue */ );
	}
	
}


/************************************************************************/
/*                      The PSM thread                                  */
/************************************************************************/
/* Cancelation cleanup : set ZOMBIE state in the peer */
void cleanup_setstate(void * arg) 
{
	struct fd_peer * peer = (struct fd_peer *)arg;
	CHECK_PARAMS_DO( CHECK_PEER(peer), return );
	peer->p_hdr.info.runtime.pir_state = STATE_ZOMBIE;
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
		sprintf(buf, "PSM/%.*s", (int)sizeof(buf) - 5, peer->p_hdr.info.pi_diamid);
		fd_log_threadname ( buf );
	}
	
	/* The state machine starts in CLOSED state */
	peer->p_hdr.info.runtime.pir_state = STATE_CLOSED;
	
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
	TRACE_DEBUG(FULL, "'%s' in state '%s' waiting for next event.",
			peer->p_hdr.info.pi_diamid, STATE_STR(peer->p_hdr.info.runtime.pir_state));
	CHECK_FCT_DO( fd_event_timedget(peer->p_events, &peer->p_psm_timer, FDEVP_PSM_TIMEOUT, &event, &ev_sz, &ev_data), goto psm_end );
	TRACE_DEBUG(FULL, "'%s'\t<-- '%s'\t(%p,%zd)\t'%s'",
			STATE_STR(peer->p_hdr.info.runtime.pir_state),
			fd_pev_str(event), ev_data, ev_sz,
			peer->p_hdr.info.pi_diamid);

	/* Now, the action depends on the current state and the incoming event */

	/* The following states are impossible */
	ASSERT( peer->p_hdr.info.runtime.pir_state != STATE_NEW );
	ASSERT( peer->p_hdr.info.runtime.pir_state != STATE_ZOMBIE );
	ASSERT( peer->p_hdr.info.runtime.pir_state != STATE_OPEN_HANDSHAKE ); /* because it should exist only between two loops */

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
		switch (peer->p_hdr.info.runtime.pir_state) {
			case STATE_OPEN:
			case STATE_REOPEN:
				/* We cannot just close the conenction, we have to send a DPR first */
				CHECK_FCT_DO( fd_p_dp_initiate(peer, ev_data), goto psm_end );
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
		
		/* If the current state does not allow receiving messages, just drop it */
		if (peer->p_hdr.info.runtime.pir_state == STATE_CLOSED) {
			TRACE_DEBUG(FULL, "Purging message in queue while in CLOSED state (%zdb)", ev_sz);
			free(ev_data);
			goto psm_loop;
		}
		
		/* Parse the received buffer */
		CHECK_FCT_DO( fd_msg_parse_buffer( (void *)&ev_data, ev_sz, &msg), 
			{
				fd_log_debug("Received invalid data from peer '%s', closing the connection\n", peer->p_hdr.info.pi_diamid);
				free(ev_data);
				CHECK_FCT_DO( fd_event_send(peer->p_events, FDEVP_CNX_ERROR, 0, NULL), goto psm_reset );
				goto psm_loop;
			} );
		
		TRACE_DEBUG(FULL, "Received a message (%zdb) from '%s'", ev_sz, peer->p_hdr.info.pi_diamid);
		fd_msg_dump_one(FULL, msg);
	
		/* Extract the header */
		CHECK_FCT_DO( fd_msg_hdr(msg, &hdr), goto psm_end );
		
		/* If it is an answer, associate with the request or drop */
		if (!(hdr->msg_flags & CMD_FLAG_REQUEST)) {
			struct msg * req;
			/* Search matching request (same hbhid) */
			CHECK_FCT_DO( fd_p_sr_fetch(&peer->p_sr, hdr->msg_hbhid, &req), goto psm_end );
			if (req == NULL) {
				fd_log_debug("Received a Diameter answer message with no corresponding sent request, discarding.\n");
				fd_msg_dump_walk(NONE, msg);
				fd_msg_free(msg);
				goto psm_loop;
			}
			
			/* Associate */
			CHECK_FCT_DO( fd_msg_answ_associate( msg, req ), goto psm_end );
		}
		
		/* Now handle non-link-local messages */
		if (fd_msg_is_routable(msg)) {
			switch (peer->p_hdr.info.runtime.pir_state) {
				/* To maximize compatibility -- should not be a security issue here */
				case STATE_REOPEN:
				case STATE_SUSPECT:
				case STATE_CLOSING:
					TRACE_DEBUG(FULL, "Accepted a message while not in OPEN state... ");
				/* The standard situation : */
				case STATE_OPEN:
					/* We received a valid routable message, update the expiry timer */
					CHECK_FCT_DO( fd_p_expi_update(peer), goto psm_end );

					/* Set the message source and add the Route-Record */
					CHECK_FCT_DO( fd_msg_source_set( msg, peer->p_hdr.info.pi_diamid, 1, fd_g_config->cnf_dict ), goto psm_end);

					/* Requeue to the global incoming queue */
					CHECK_FCT_DO(fd_fifo_post(fd_g_incoming, &msg), goto psm_end );

					/* Update the peer timer (only in OPEN state) */
					if ((peer->p_hdr.info.runtime.pir_state == STATE_OPEN) && (!peer->p_flags.pf_dw_pending)) {
						fd_psm_next_timeout(peer, 1, peer->p_hdr.info.config.pic_twtimer ?: fd_g_config->cnf_timer_tw);
					}
					break;
					
				/* In other states, we discard the message, it is either old or invalid to send it for the remote peer */
				case STATE_WAITCNXACK:
				case STATE_WAITCNXACK_ELEC:
				case STATE_WAITCEA:
				case STATE_CLOSED:
				default:
					/* In such case, just discard the message */
					fd_log_debug("Received a routable message while not in OPEN state from peer '%s', discarded.\n", peer->p_hdr.info.pi_diamid);
					fd_msg_dump_walk(NONE, msg);
					fd_msg_free(msg);
			}
			goto psm_loop;
		}
		
		/* Link-local message: They must be understood by our dictionary, otherwise we return an error */
		{
			int ret = fd_msg_parse_or_error( &msg );
			if (ret != EBADMSG) {
				CHECK_FCT_DO( ret, goto psm_end );
			} else {
				if (msg) {
					/* Send the error back to the peer */
					CHECK_FCT_DO( fd_out_send(&msg, NULL, peer), /* In case of error the message has already been dumped */ );
					if (msg) {
						CHECK_FCT_DO( fd_msg_free(msg), goto psm_end);
					}
				} else {
					/* We received an invalid answer, let's disconnect */
					CHECK_FCT_DO( fd_event_send(peer->p_events, FDEVP_CNX_ERROR, 0, NULL), goto psm_reset );
				}
				goto psm_loop;
			}
		}
		
		/* Handle the LL message and update the expiry timer appropriately */
		switch (hdr->msg_code) {
			case CC_CAPABILITIES_EXCHANGE:
				CHECK_FCT_DO( fd_p_ce_msgrcv(&msg, (hdr->msg_flags & CMD_FLAG_REQUEST), peer), goto psm_reset );
				break;
			
			case CC_DISCONNECT_PEER:
				CHECK_FCT_DO( fd_p_dp_handle(&msg, (hdr->msg_flags & CMD_FLAG_REQUEST), peer), goto psm_reset );
				if (peer->p_hdr.info.runtime.pir_state == STATE_CLOSING)
					goto psm_end;
				break;
			
			case CC_DEVICE_WATCHDOG:
				CHECK_FCT_DO( fd_p_dw_handle(&msg, (hdr->msg_flags & CMD_FLAG_REQUEST), peer), goto psm_reset );
				break;
			
			default:
				/* Unknown / unexpected / invalid message */
				fd_log_debug("Received an unknown local message from peer '%s', discarded.\n", peer->p_hdr.info.pi_diamid);
				fd_msg_dump_walk(NONE, msg);
				if (hdr->msg_flags & CMD_FLAG_REQUEST) {
					do {
						/* Reply with an error code */
						CHECK_FCT_DO( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, &msg, MSGFL_ANSW_ERROR ), break );

						/* Set the error code */
						CHECK_FCT_DO( fd_msg_rescode_set(msg, "DIAMETER_INVALID_HDR_BITS", NULL, NULL, 1 ), break );

						/* Send the answer */
						CHECK_FCT_DO( fd_out_send(&msg, peer->p_cnxctx, peer), break );
					} while (0);
				} else {
					/* We did ASK for it ??? */
					fd_log_debug("Invalid PXY flag in answer header ?\n");
				}
				
				/* Cleanup the message if not done */
				if (msg) {
					CHECK_FCT_DO( fd_msg_free(msg), /* continue */);
					msg = NULL;
				}
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
		switch (peer->p_hdr.info.runtime.pir_state) {
			case STATE_WAITCNXACK_ELEC:
				/* Abort the initiating side */
				fd_p_cnx_abort(peer, 0);
				/* Process the receiver side */
				CHECK_FCT_DO( fd_p_ce_process_receiver(peer), goto psm_end );
				break;
			
			case STATE_WAITCEA:
			case STATE_OPEN:
			case STATE_REOPEN:
			case STATE_WAITCNXACK:
			case STATE_SUSPECT:
			default:
				/* Mark the connection problem */
				peer->p_flags.pf_cnx_pb = 1;
				
				/* Destroy the connection, restart the timer to a new connection attempt */
				fd_psm_next_timeout(peer, 1, peer->p_hdr.info.config.pic_tctimer ?: fd_g_config->cnf_timer_tc);
				
			case STATE_CLOSED:
				goto psm_reset;
				
			case STATE_CLOSING:
				/* We sent a DPR so we are terminating, do not wait for DPA */
				goto psm_end;
				
		}
		goto psm_loop;
	}
	
	/* The connection notified a change in endpoints */
	if (event == FDEVP_CNX_EP_CHANGE) {
		/* We actually don't care if we are in OPEN state here... */
		
		/* Cleanup the remote LL and primary addresses */
		CHECK_FCT_DO( fd_ep_filter( &peer->p_hdr.info.pi_endpoints, EP_FL_CONF | EP_FL_DISC | EP_FL_ADV ), /* ignore the error */);
		CHECK_FCT_DO( fd_ep_clearflags( &peer->p_hdr.info.pi_endpoints, EP_FL_PRIMARY ), /* ignore the error */);
		
		/* Get the new ones */
		CHECK_FCT_DO( fd_cnx_getendpoints(peer->p_cnxctx, NULL, &peer->p_hdr.info.pi_endpoints), /* ignore the error */);
		
		/* We do not support local endpoints change currently, but it could be added here if needed */
		
		if (TRACE_BOOL(ANNOYING)) {
			TRACE_DEBUG(ANNOYING, "New remote endpoint(s):" );
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
	
	/* A new connection has been established with the remote peer */
	if (event == FDEVP_CNX_ESTABLISHED) {
		struct cnxctx * cnx = ev_data;
		
		/* Release the resources of the connecting thread */
		CHECK_POSIX_DO( pthread_join( peer->p_ini_thr, NULL), /* ignore, it is not a big deal */);
		peer->p_ini_thr = (pthread_t)NULL;
		
		switch (peer->p_hdr.info.runtime.pir_state) {
			case STATE_WAITCNXACK_ELEC:
			case STATE_WAITCNXACK:
				fd_p_ce_handle_newcnx(peer, cnx);
				break;
				
			default:
				/* Just abort the attempt and continue */
				TRACE_DEBUG(FULL, "Connection attempt successful but current state is %s, closing...", STATE_STR(peer->p_hdr.info.runtime.pir_state));
				fd_cnx_destroy(cnx);
		}
		
		goto psm_loop;
	}
	
	/* A new connection has not been established with the remote peer */
	if (event == FDEVP_CNX_FAILED) {
		
		/* Release the resources of the connecting thread */
		CHECK_POSIX_DO( pthread_join( peer->p_ini_thr, NULL), /* ignore, it is not a big deal */);
		peer->p_ini_thr = (pthread_t)NULL;
		
		switch (peer->p_hdr.info.runtime.pir_state) {
			case STATE_WAITCNXACK_ELEC:
				/* Abort the initiating side */
				fd_p_cnx_abort(peer, 0);
				/* Process the receiver side */
				CHECK_FCT_DO( fd_p_ce_process_receiver(peer), goto psm_end );
				break;
				
			case STATE_WAITCNXACK:
				/* Go back to CLOSE */
				fd_psm_next_timeout(peer, 1, peer->p_hdr.info.config.pic_tctimer ?: fd_g_config->cnf_timer_tc);
				goto psm_reset;
				
			default:
				/* Just ignore */
				TRACE_DEBUG(FULL, "Connection attempt failed but current state is %s, ignoring...", STATE_STR(peer->p_hdr.info.runtime.pir_state));
		}
		
		goto psm_loop;
	}
	
	/* The timeout for the current state has been reached */
	if (event == FDEVP_PSM_TIMEOUT) {
		switch (peer->p_hdr.info.runtime.pir_state) {
			case STATE_OPEN:
			case STATE_REOPEN:
				CHECK_FCT_DO( fd_p_dw_timeout(peer), goto psm_end );
				goto psm_loop;
				
			case STATE_CLOSED:
				CHECK_FCT_DO( fd_psm_change_state(peer, STATE_WAITCNXACK), goto psm_end );
				fd_psm_next_timeout(peer, 0, CNX_TIMEOUT);
				CHECK_FCT_DO( fd_p_cnx_init(peer), goto psm_end );
				goto psm_loop;
				
			case STATE_SUSPECT:
				/* Mark the connection problem */
				peer->p_flags.pf_cnx_pb = 1;
				
			case STATE_CLOSING:
			case STATE_WAITCNXACK:
			case STATE_WAITCEA:
				/* Destroy the connection, restart the timer to a new connection attempt */
				fd_psm_next_timeout(peer, 1, peer->p_hdr.info.config.pic_tctimer ?: fd_g_config->cnf_timer_tc);
				goto psm_reset;
				
			case STATE_WAITCNXACK_ELEC:
				/* Abort the initiating side */
				fd_p_cnx_abort(peer, 0);
				/* Process the receiver side */
				CHECK_FCT_DO( fd_p_ce_process_receiver(peer), goto psm_end );
				goto psm_loop;
		}
	}
	
	/* Default action : the handling has not yet been implemented. [for debug only] */
	TRACE_DEBUG(INFO, "Missing handler in PSM for '%s'\t<-- '%s'", STATE_STR(peer->p_hdr.info.runtime.pir_state), fd_pev_str(event));
psm_reset:
	if (peer->p_flags.pf_delete)
		goto psm_end;
	fd_psm_cleanup(peer, 0);
	goto psm_loop;
	
psm_end:
	fd_psm_cleanup(peer, 1);
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
	CHECK_PARAMS( CHECK_PEER(peer) && (peer->p_hdr.info.runtime.pir_state == STATE_NEW) );
	
	/* Create the FIFO for events */
	CHECK_FCT( fd_fifo_new(&peer->p_events) );
	
	/* Create the PSM controler thread */
	CHECK_POSIX( pthread_create( &peer->p_psm, NULL, p_psm_th, peer ) );
	
	/* We're done */
	return 0;
}

/* End the PSM (clean ending) */
int fd_psm_terminate(struct fd_peer * peer, char * reason )
{
	TRACE_ENTRY("%p", peer);
	CHECK_PARAMS( CHECK_PEER(peer) );
	
	if (peer->p_hdr.info.runtime.pir_state != STATE_ZOMBIE) {
		CHECK_FCT( fd_event_send(peer->p_events, FDEVP_TERMINATE, 0, reason) );
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
	
	/* Cleanup the data */
	fd_psm_cleanup(peer, 1);
	
	/* Destroy the event list */
	CHECK_FCT_DO( fd_fifo_del(&peer->p_events), /* continue */ );
	
	/* Remaining cleanups are performed in fd_peer_free */
	return;
}

