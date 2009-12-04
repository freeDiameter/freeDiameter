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

/********************************************************************************/
/*              First part : handling the extensions callbacks                  */
/********************************************************************************/

/* Lists of the callbacks, and locks to protect them */
static pthread_rwlock_t rt_fwd_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct fd_list 	rt_fwd_list = FD_LIST_INITIALIZER_O(rt_fwd_list, &rt_fwd_lock);

static pthread_rwlock_t rt_out_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct fd_list 	rt_out_list = FD_LIST_INITIALIZER_O(rt_out_list, &rt_out_lock);

/* Items in the lists are the same */
struct rt_hdl {
	struct fd_list	chain;	/* link in the rt_fwd_list or rt_out_list */
	void *		cbdata;	/* the registered data */
	union {
		int	order;	/* This value is used to sort the list */
		int 	dir;	/* It is the direction for FWD handlers */
		int	prio;	/* and the priority for OUT handlers */
	};
	union {
		int (*rt_fwd_cb)(void * cbdata, struct msg ** msg);
		int (*rt_out_cb)(void * cbdata, struct msg * msg, struct fd_list * candidates);
	};
};	

/* Add a new entry in the list */
static int add_ordered(struct rt_hdl * new, struct fd_list * list)
{
	/* The list is ordered by prio parameter */
	struct fd_list * li;
	
	CHECK_POSIX( pthread_rwlock_wrlock(list->o) );
	
	for (li = list->next; li != list; li = li->next) {
		struct rt_hdl * h = (struct rt_hdl *) li;
		if (new->order <= h->order)
			break;
	}
	
	fd_list_insert_before(li, &new->chain);
	
	CHECK_POSIX( pthread_rwlock_unlock(list->o) );
}

/* Register a new FWD callback */
int fd_rt_fwd_register ( int (*rt_fwd_cb)(void * cbdata, struct msg ** msg), void * cbdata, enum fd_rt_fwd_dir dir, struct fd_rt_fwd_hdl ** handler )
{
	struct rt_hdl * new;
	
	TRACE_ENTRY("%p %p %d %p", rt_fwd_cb, cbdata, dir, handler);
	CHECK_PARAMS( rt_fwd_cb );
	CHECK_PARAMS( (dir >= RT_FWD_REQ) && ( dir <= RT_FWD_ANS) );
	
	/* Create a new container */
	CHECK_MALLOC(new = malloc(sizeof(struct rt_hdl)));
	memset(new, 0, sizeof(struct rt_hdl));
	
	/* Write the content */
	fd_list_init(&new->chain, NULL);
	new->cbdata 	= cbdata;
	new->dir    	= dir;
	new->rt_fwd_cb 	= rt_fwd_cb;
	
	/* Save this in the list */
	CHECK_FCT( add_ordered(new, &rt_fwd_list) );
	
	/* Give it back to the extension if needed */
	if (handler)
		*handler = (void *)new;
	
	return 0;
}

/* Remove it */
int fd_rt_fwd_unregister ( struct fd_rt_fwd_hdl * handler, void ** cbdata )
{
	struct rt_hdl * del;
	TRACE_ENTRY( "%p %p", handler, cbdata);
	CHECK_PARAMS( handler );
	
	del = (struct rt_hdl *)handler;
	CHECK_PARAMS( del->chain.head == &rt_fwd_list );
	
	/* Unlink */
	CHECK_POSIX( pthread_rwlock_wrlock(&rt_fwd_lock) );
	fd_list_unlink(&del->chain);
	CHECK_POSIX( pthread_rwlock_unlock(&rt_fwd_lock) );
	
	if (cbdata)
		*cbdata = del->cbdata;
	
	free(del);
	return 0;
}

/* Register a new OUT callback */
int fd_rt_out_register ( int (*rt_out_cb)(void * cbdata, struct msg * msg, struct fd_list * candidates), void * cbdata, int priority, struct fd_rt_out_hdl ** handler )
{
	struct rt_hdl * new;
	
	TRACE_ENTRY("%p %p %d %p", rt_out_cb, cbdata, priority, handler);
	CHECK_PARAMS( rt_out_cb );
	
	/* Create a new container */
	CHECK_MALLOC(new = malloc(sizeof(struct rt_hdl)));
	memset(new, 0, sizeof(struct rt_hdl));
	
	/* Write the content */
	fd_list_init(&new->chain, NULL);
	new->cbdata 	= cbdata;
	new->prio    	= priority;
	new->rt_out_cb 	= rt_out_cb;
	
	/* Save this in the list */
	CHECK_FCT( add_ordered(new, &rt_out_list) );
	
	/* Give it back to the extension if needed */
	if (handler)
		*handler = (void *)new;
	
	return 0;
}

/* Remove it */
int fd_rt_out_unregister ( struct fd_rt_out_hdl * handler, void ** cbdata )
{
	struct rt_hdl * del;
	TRACE_ENTRY( "%p %p", handler, cbdata);
	CHECK_PARAMS( handler );
	
	del = (struct rt_hdl *)handler;
	CHECK_PARAMS( del->chain.head == &rt_out_list );
	
	/* Unlink */
	CHECK_POSIX( pthread_rwlock_wrlock(&rt_out_lock) );
	fd_list_unlink(&del->chain);
	CHECK_POSIX( pthread_rwlock_unlock(&rt_out_lock) );
	
	if (cbdata)
		*cbdata = del->cbdata;
	
	free(del);
	return 0;
}

/********************************************************************************/
/*                   Second part : the routing threads                          */
/********************************************************************************/

/* Note: in the first version, we only create one thread of each kind.
 We could improve the scalability by using the threshold feature of the queues fd_g_incoming and fd_g_outgoing
 ( fd_g_local is managed by the dispatch thread ) to create additional threads if a queue is filling up.
 */

/* Test if a User-Name AVP contains a Decorated NAI -- RFC4282, draft-ietf-dime-nai-routing-04 */
static int is_decorated_NAI(union avp_value * un)
{
	int i;
	TRACE_ENTRY("%p", un);
	
	/* If there was no User-Name, we return false */
	if (un == NULL)
		return 0;
	
	/* Search if there is a '!' before any '@' -- do we need to check it contains a '.' ? */
	for (i = 0; i < un->os.len; i++) {
		if ( un->os.data[i] == (unsigned char) '!' )
			return 1;
		if ( un->os.data[i] == (unsigned char) '@' )
			break;
		if ( un->os.data[i] == (unsigned char) '\\' )
			i++; /* next one was escaped */
	}
	
	return 0;
}

/* Create new User-Name and Destination-Realm values */
static int process_decorated_NAI(union avp_value * un, union avp_value * dr)
{
	int i, at_idx = 0, sep_idx = 0;
	unsigned char * old_un;
	TRACE_ENTRY("%p %p", un, dr);
	CHECK_PARAMS(un && dr);
	
	/* Save the decorated User-Name, for example 'homerealm.example.net!user@otherrealm.example.net' */
	old_un = un->os.data;
	
	/* Search the positions of the first '!' and the '@' in the string */
	for (i = 0; i < un->os.len; i++) {
		if ( (!sep_idx) && (old_un[i] == (unsigned char) '!') )
			sep_idx = i;
		if ( old_un[i] == (unsigned char) '@' ) {
			at_idx = i;
			break;
		}
		if ( un->os.data[i] == (unsigned char) '\\' )
			i++; /* next one is escaped */
	}
	
	CHECK_PARAMS( 0 < sep_idx < at_idx < un->os.len);
	
	/* Create the new User-Name value */
	CHECK_MALLOC( un->os.data = malloc( at_idx ) );
	memcpy( un->os.data, old_un + sep_idx + 1, at_idx - sep_idx ); /* user@ */
	memcpy( un->os.data + at_idx - sep_idx, old_un, sep_idx ); /* homerealm.example.net */
	
	/* Create the new Destination-Realm value */
	CHECK_MALLOC( dr->os.data = realloc(dr->os.data, sep_idx) );
	memcpy( dr->os.data, old_un, sep_idx );
	dr->os.len = sep_idx;
	
	TRACE_DEBUG(FULL, "Processed Decorated NAI '%.*s' into '%.*s' (%.*s)",
				un->os.len, old_un,
				at_idx, un->os.data,
				dr->os.len, dr->os.data);
	
	un->os.len = at_idx;
	free(old_un);
	
	return 0;
}



/* Function to return an error to an incoming request */
static int return_error(struct msg * msg, char * error_code, char * error_message, struct avp * failedavp)
{
	struct fd_peer * peer;

	/* Get the source of the message */
	{
		char * id;
		CHECK_FCT( fd_msg_source_get( msg, &id ) );
		
		/* Search the peer with this id */
		CHECK_FCT( fd_peer_getbyid( id, (void *)&peer ) );
		
		if (!peer) {
			TRACE_DEBUG(INFO, "Unable to send error '%s' to deleted peer '%s' in reply to:", error_code, id);
			fd_msg_dump_walk(INFO, msg);
			fd_msg_free(msg);
			return 0;
		}
	}
	
	/* Create the error message */
	CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, &msg, MSGFL_ANSW_ERROR ) );

	/* Set the error code */
	CHECK_FCT( fd_msg_rescode_set(msg, error_code, error_message, failedavp, 1 ) );

	/* Send the answer */
	CHECK_FCT( fd_out_send(&msg, NULL, peer) );
	
	/* Done */
	return 0;
}


/* The (routing-in) thread -- see description in freeDiameter.h */
static void * routing_in_thr(void * arg)
{
	TRACE_ENTRY("%p", arg);
	
	/* Set the thread name */
	if (arg) {
		char buf[48];
		snprintf(buf, sizeof(buf), "Routing-IN %p", arg);
		fd_log_threadname ( buf );
	} else {
		fd_log_threadname ( "Routing-IN" );
	}
	
	/* Main thread loop */
	do {
		struct msg * msg;
		struct msg_hdr * hdr;
		int is_req = 0;
		int is_err = 0;
		char * qry_src = NULL;
		
		/* Test if we were told to stop */
		pthread_testcancel();
		
		/* Get the next message from the incoming queue */
		CHECK_FCT_DO( fd_fifo_get ( fd_g_incoming, &msg ), goto fatal_error );
		
		if (TRACE_BOOL(FULL)) {
			TRACE_DEBUG(FULL, "Picked next message:");
			fd_msg_dump_one(FULL, msg);
		}
		
		/* Read the message header */
		CHECK_FCT_DO( fd_msg_hdr(msg, &hdr), goto fatal_error );
		is_req = hdr->msg_flags & CMD_FLAG_REQUEST;
		is_err = hdr->msg_flags & CMD_FLAG_ERROR;
		
		/* Handle incorrect bits */
		if (is_req && is_err) {
			CHECK_FCT_DO( return_error( msg, "DIAMETER_INVALID_HDR_BITS", "R & E bits were set", NULL), goto fatal_error );
			continue;
		}
		
		/* If it is a request, we must analyze its content to decide what we do with it */
		if (is_req) {
			struct avp * avp, *un = NULL;
			union avp_value * un_val = NULL, *dr_val = NULL;
			enum status { UNKNOWN, YES, NO };
			/* Are we Destination-Host? */
			enum status is_dest_host = UNKNOWN;
			/* Are we Destination-Realm? */
			enum status is_dest_realm = UNKNOWN;
			/* Do we support the application of the message? */
			enum status is_local_app = UNKNOWN;
			
			/* Check if we have local support for the message application */
			if ( (hdr->msg_appl == 0) || (hdr->msg_appl == AI_RELAY) ) {
				TRACE_DEBUG(INFO, "Received a routable message with application id 0, returning DIAMETER_APPLICATION_UNSUPPORTED");
				CHECK_FCT_DO( return_error( msg, "DIAMETER_APPLICATION_UNSUPPORTED", "Routable message with application id 0 or relay", NULL), goto fatal_error );
				continue;
			} else {
				struct fd_app * app;
				CHECK_FCT_DO( fd_app_check(&fd_g_config->cnf_apps, hdr->msg_appl, &app), goto fatal_error );
				is_local_app = (app ? YES : NO);
			}
			
			/* Parse the message for Dest-Host and Dest-Realm */
			CHECK_FCT_DO(  fd_msg_browse(msg, MSG_BRW_FIRST_CHILD, &avp, NULL), goto fatal_error  );
			while (avp) {
				struct avp_hdr * ahdr;
				CHECK_FCT_DO(  fd_msg_avp_hdr( avp, &ahdr ), goto fatal_error  );
				
				if (! (ahdr->avp_flags & AVP_FLAG_VENDOR)) {
					switch (ahdr->avp_code) {
						case AC_DESTINATION_HOST:
							/* Parse this AVP */
							CHECK_FCT_DO( fd_msg_parse_dict ( avp, fd_g_config->cnf_dict ), goto fatal_error );
							ASSERT( ahdr->avp_value );
							/* Compare the Destination-Host AVP of the message with our identity */
							if (ahdr->avp_value->os.len != fd_g_config->cnf_diamid_len) {
								is_dest_host = NO;
							} else {
								is_dest_host = (strncasecmp(fd_g_config->cnf_diamid, (char *)ahdr->avp_value->os.data, fd_g_config->cnf_diamid_len) 
											? NO : YES);
							}
							break;
							
						case AC_DESTINATION_REALM:
							/* Parse this AVP */
							CHECK_FCT_DO( fd_msg_parse_dict ( avp, fd_g_config->cnf_dict ), goto fatal_error );
							ASSERT( ahdr->avp_value );
							dr_val = ahdr->avp_value;
							/* Compare the Destination-Realm AVP of the message with our identity */
							if (ahdr->avp_value->os.len != fd_g_config->cnf_diamrlm_len) {
								is_dest_realm = NO;
							} else {
								is_dest_realm = (strncasecmp(fd_g_config->cnf_diamrlm, (char *)ahdr->avp_value->os.data, fd_g_config->cnf_diamrlm_len) 
											? NO : YES);
							}
							break;
						
						case AC_USER_NAME:
							/* Parse this AVP */
							CHECK_FCT_DO( fd_msg_parse_dict ( avp, fd_g_config->cnf_dict ), goto fatal_error );
							ASSERT( ahdr->avp_value );
							un = avp;
							un_val = ahdr->avp_value;
							break;
					}
				}
				
				if ((is_dest_host != UNKNOWN) && (is_dest_realm != UNKNOWN) && un)
					break;
				
				/* Go to next AVP */
				CHECK_FCT_DO(  fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL), goto fatal_error  );
			}
			
			/* OK, now decide what we do with the request */
			
			/* Handle the missing routing AVPs first */
			if ( is_dest_realm == UNKNOWN ) {
				CHECK_FCT_DO( return_error( msg, "DIAMETER_COMMAND_UNSUPPORTED", "Non-routable message not supported (invalid bit ? missing Destination-Realm ?)", NULL), goto fatal_error );
				continue;
			}
			
			/* If we are listed as Destination-Host */
			if (is_dest_host == YES) {
				if (is_local_app == YES) {
					/* Ok, give the message to the dispatch thread */
					CHECK_FCT_DO(fd_fifo_post(fd_g_local, &msg), goto fatal_error );
				} else {
					/* We don't support the application, reply an error */
					CHECK_FCT_DO( return_error( msg, "DIAMETER_APPLICATION_UNSUPPORTED", NULL, NULL), goto fatal_error );
				}
				continue;
			}
			
			/* If the message is explicitely for someone else */
			if ((is_dest_host == NO) || (is_dest_realm == NO)) {
				if (fd_g_config->cnf_flags.no_fwd) {
					CHECK_FCT_DO( return_error( msg, "DIAMETER_UNABLE_TO_DELIVER", "This peer is not an agent", NULL), goto fatal_error );
					continue;
				}
			} else {
			/* Destination-Host was not set, and Destination-Realm is matching : we may handle or pass to a fellow peer */
				
				/* test for decorated NAI  (draft-ietf-dime-nai-routing-04 section 4.4) */
				if (is_decorated_NAI(un_val)) {
					/* Handle the decorated NAI */
					CHECK_FCT_DO( process_decorated_NAI(un_val, dr_val),
						{
							/* If the process failed, we assume it is because of the AVP format */
							CHECK_FCT_DO( return_error( msg, "DIAMETER_INVALID_AVP_VALUE", "Failed to process decorated NAI", un), goto fatal_error );
							continue;
						} );
					
					/* We have transformed the AVP, now submit it again in the queue */
					CHECK_FCT_DO(fd_fifo_post(fd_g_incoming, &msg), goto fatal_error );
					continue;
				}
  
				if (is_local_app == YES) {
					/* Handle localy since we are able to */
					CHECK_FCT_DO(fd_fifo_post(fd_g_local, &msg), goto fatal_error );
					continue;
				}
				
				if (fd_g_config->cnf_flags.no_fwd) {
					/* We return an error */
					CHECK_FCT_DO( return_error( msg, "DIAMETER_APPLICATION_UNSUPPORTED", NULL, NULL), goto fatal_error );
					continue;
				}
			}
			
			/* From that point, for requests, we will call the registered callbacks, then forward to another peer */
			
		} else {
			/* The message is an answer */
			struct msg * qry;
			
			/* Retrieve the corresponding query and its origin */
			CHECK_FCT_DO( fd_msg_answ_getq( msg, &qry ), goto fatal_error );
			CHECK_FCT_DO( fd_msg_source_get( qry, &qry_src ), goto fatal_error );
			
			if ((!qry_src) && (!is_err)) {
				/* The message is a normal answer to a request issued localy, we do not call the callbacks chain on it. */
				CHECK_FCT_DO(fd_fifo_post(fd_g_local, &msg), goto fatal_error );
				continue;
			}
			
			/* From that point, for answers, we will call the registered callbacks, then pass it to the dispatch module or forward it */
		}
		
		/* Call all registered callbacks for this message */
		{
			struct fd_list * li;
			
			CHECK_FCT_DO( pthread_rwlock_rdlock( &rt_fwd_lock ), goto fatal_error );
			pthread_cleanup_push( fd_cleanup_rwlock, &rt_fwd_lock );
			
			/* requests: dir = 1 & 2 => in order; answers = 3 & 2 => in reverse order */
			for (	li = (is_req ? rt_fwd_list.next : rt_fwd_list.prev) ; msg && (li != &rt_fwd_list) ; li = (is_req ? li->next : li->prev) ) {
				struct rt_hdl * rh = (struct rt_hdl *)li;
				
				if (is_req && (rh->dir > RT_FWD_ALL))
					break;
				if ((!is_req) && (rh->dir < RT_FWD_ALL))
					break;
				
				/* Ok, call this cb */
				TRACE_DEBUG(ANNOYING, "Calling next FWD callback on %p : %p", msg, rh->rt_fwd_cb);
				CHECK_FCT_DO( (*rh->rt_fwd_cb)(rh->cbdata, &msg),
					{
						TRACE_DEBUG(INFO, "A FWD routing callback returned an error, message discarded.");
						fd_msg_dump_walk(INFO, msg);
						fd_msg_free(msg);
						msg = NULL;
					} );
			}
			
			pthread_cleanup_pop(0);
			CHECK_FCT_DO( pthread_rwlock_unlock( &rt_fwd_lock ), goto fatal_error );
			
			/* If a callback has handled the message, we stop now */
			if (!msg)
				continue;
		}
		
		/* Now handle the message to the next step: either forward to another peer, or for local delivery */
		if (is_req || qry_src) {
			CHECK_FCT_DO(fd_fifo_post(fd_g_outgoing, &msg), goto fatal_error );
		} else {
			CHECK_FCT_DO(fd_fifo_post(fd_g_local, &msg), goto fatal_error );
		}
		
		/* We're done with this message */
	} while (1);
	
fatal_error:
	TRACE_DEBUG(INFO, "An error occurred in routing module! IN thread is terminating...");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	return NULL;
}


/* The (routing-out) thread -- see description in freeDiameter.h */
static void * routing_out_thr(void * arg)
{
	TRACE_ENTRY("%p", arg);
	
	/* Set the thread name */
	if (arg) {
		char buf[48];
		snprintf(buf, sizeof(buf), "Routing-OUT %p", arg);
		fd_log_threadname ( buf );
	} else {
		fd_log_threadname ( "Routing-OUT" );
	}
	
	/* Main thread loop */
	do {
		struct msg * msg;
		struct msg_hdr * hdr;
		int is_req = 0;
		
		/* Test if we were told to stop */
		pthread_testcancel();
		
		/* Get the next message from the ougoing queue */
		CHECK_FCT_DO( fd_fifo_get ( fd_g_outgoing, &msg ), goto fatal_error );
		
		if (TRACE_BOOL(FULL)) {
			TRACE_DEBUG(FULL, "Picked next message:");
			fd_msg_dump_one(FULL, msg);
		}
		
		/* Read the message header */
		CHECK_FCT_DO( fd_msg_hdr(msg, &hdr), goto fatal_error );
		is_req = hdr->msg_flags & CMD_FLAG_REQUEST;
		
		/* For answers, the routing is very easy */
		if ( ! is_req ) {
			struct msg * qry;
			char * qry_src = NULL;
			struct fd_peer * peer = NULL;
			
			/* Retrieve the corresponding query and its origin */
			CHECK_FCT_DO( fd_msg_answ_getq( msg, &qry ), goto fatal_error );
			CHECK_FCT_DO( fd_msg_source_get( qry, &qry_src ), goto fatal_error );
			
			ASSERT( qry_src ); /* if it is NULL, the message should have been in the LOCAL queue! */
			
			/* Find the peer corresponding to this name */
			CHECK_FCT_DO( fd_peer_getbyid( qry_src, (void *) &peer ), goto fatal_error );
			if ((!peer) || (peer->p_hdr.info.runtime.pir_state != STATE_OPEN)) {
				TRACE_DEBUG(INFO, "Unable to forward answer message to peer '%s', deleted or not in OPEN state.", qry_src);
				fd_msg_dump_walk(INFO, msg);
				fd_msg_free(msg);
				continue;
			}
			
			/* Push the message into this peer */
			CHECK_FCT_DO( fd_out_send(&msg, NULL, peer), goto fatal_error );
			
			/* We're done with this answer */
			continue;
		}
		
		/* The message is a request */
		TODO("use struct rt_data and fd_msg_rt_get");
		
		/* We're done with this message */
	} while (1);
	
fatal_error:
	TRACE_DEBUG(INFO, "An error occurred in routing module! OUT thread is terminating...");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	return NULL;
}



/* Initialize the routing module */
int fd_rt_init(void)
{
	TODO("Start the routing threads");
	
	/* Later: TODO("Set the thresholds for the IN and OUT queues to create more routing threads as needed"); */
	return ENOTSUP;
}

/* Terminate the routing module */
int fd_rt_fini(void)
{
	TODO("Stop the routing threads");
	return ENOTSUP;
}



