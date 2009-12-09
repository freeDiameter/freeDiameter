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

/* Add an application into the peer's supported apps */
int fd_disp_app_support ( struct dict_object * app, struct dict_object * vendor, int auth, int acct )
{
	application_id_t aid = 0;
	vendor_id_t	 vid = 0;
	
	TRACE_ENTRY("%p %p %d %d", app, vendor, auth, acct);
	CHECK_PARAMS( app && (auth || acct) );
	
	{
		enum dict_object_type type = 0;
		struct dict_application_data data;
		CHECK_FCT( fd_dict_gettype(app, &type) );
		CHECK_PARAMS( type == DICT_APPLICATION );
		CHECK_FCT( fd_dict_getval(app, &data) );
		aid = data.application_id;
	}

	if (vendor) {
		enum dict_object_type type = 0;
		struct dict_vendor_data data;
		CHECK_FCT( fd_dict_gettype(vendor, &type) );
		CHECK_PARAMS( type == DICT_VENDOR );
		CHECK_FCT( fd_dict_getval(vendor, &data) );
		vid = data.vendor_id;
	}
	
	return fd_app_merge(&fd_g_config->cnf_apps, aid, vid, auth, acct);
}


/* The dispatching thread(s) */

enum thread_state { INITIAL = 0, RUNNING = 1, TERMINATED = 2 };
static void cleanup_state(void * state_loc)
{
	if (state_loc)
		*(enum thread_state *)state_loc = TERMINATED;
}

static pthread_mutex_t order_lock = PTHREAD_MUTEX_INITIALIZER;
static enum { RUN = 0, STOP = 1 } order_val;

static void * dispatch_thread(void * arg)
{
	TRACE_ENTRY("%p", arg);
	
	/* Set the thread name */
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "Dispatch %p", arg);
		fd_log_threadname ( buf );
	}

	pthread_cleanup_push( cleanup_state, arg );
	
	/* Mark the thread running */
	*(enum thread_state *)arg = RUNNING;
	
	do {
		struct msg * msg;
		struct msg_hdr * hdr;
		int is_req = 0;
		struct session * sess;
		enum disp_action action;
		const char * ec = NULL;
		const char * em = NULL;
		
		/* Test the environment */
		{
			int must_stop;
			CHECK_POSIX_DO( pthread_mutex_lock(&order_lock), goto end ); /* we lock to flush the caches */
			must_stop = (order_val == STOP);
			CHECK_POSIX_DO( pthread_mutex_unlock(&order_lock), goto end );
			if (must_stop)
				goto end;
			
			pthread_testcancel();
		}
		
		/* Ok, we are allowed to run */
		
		/* Get the next message from the queue */
		CHECK_FCT_DO( fd_fifo_get ( fd_g_local, &msg ), goto fatal_error );
		
		if (TRACE_BOOL(FULL)) {
			TRACE_DEBUG(FULL, "Picked next message");
			fd_msg_dump_one(ANNOYING, msg);
		}
		
		/* Read the message header */
		CHECK_FCT_DO( fd_msg_hdr(msg, &hdr), goto fatal_error );
		is_req = hdr->msg_flags & CMD_FLAG_REQUEST;
		
		/* Note: if the message is for local delivery, we should test for duplicate
		  (draft-asveren-dime-dupcons-00). This may conflict with path validation decisions, no clear answer yet */
		
		/* At this point, we need to understand the message content, so parse it */
		{
			int ret;
			CHECK_FCT_DO( ret = fd_msg_parse_or_error( &msg ),
				{
					/* in case of error, the message is already dump'd */
					if ((ret == EBADMSG) && (msg != NULL)) {
						/* msg now contains the answer message to send back */
						CHECK_FCT_DO( fd_fifo_post(fd_g_outgoing, &msg), goto fatal_error );
					}
					if (msg) {	/* another error happen'd */
						TRACE_DEBUG(INFO, "An unexpected error occurred (%s), discarding a message:", strerror(ret));
						fd_msg_dump_walk(INFO, msg);
						CHECK_FCT_DO( fd_msg_free(msg), /* continue */);
					}
					/* Go to the next message */
					continue;
				} );
		}
		
		/* First, if the original request was registered with a callback and we receive the answer, call it. */
		if ( ! is_req ) {
			struct msg * qry;
			void (*anscb)(void *, struct msg **) = NULL;
			void * data = NULL;
			
			/* Retrieve the corresponding query */
			CHECK_FCT_DO( fd_msg_answ_getq( msg, &qry ), goto fatal_error );
			
			/* Retrieve any registered handler */
			CHECK_FCT_DO( fd_msg_anscb_get( qry, &anscb, &data ), goto fatal_error );
			
			/* If a callback was registered, pass the message to it */
			if (anscb != NULL) {
				
				TRACE_DEBUG(FULL, "Calling callback registered when query was sent (%p, %p)", anscb, data);
				(*anscb)(data, &msg);
				
				if (msg == NULL) {
					/* Ok, the message is now handled, we can skip to the next one */
					continue;
				}
			}
		}
		
		/* Retrieve the session of the message */
		CHECK_FCT_DO( fd_msg_sess_get(fd_g_config->cnf_dict, msg, &sess, NULL), goto fatal_error );
		
		/* Now, call any callback registered for the message */
		CHECK_FCT_DO( fd_msg_dispatch ( &msg, sess, &action, &ec), goto fatal_error );
		
		/* Now, act depending on msg and action and ec */
		if (!msg)
			continue;
		
		switch ( action ) {
			case DISP_ACT_CONT:
				/* No callback has handled the message, let's reply with a generic error */
				em = "The message was not handled by any extension callback";
				ec = "DIAMETER_COMMAND_UNSUPPORTED";
			
			case DISP_ACT_ERROR:
				/* We have a problem with delivering the message */
				if (ec == NULL) {
					ec = "DIAMETER_UNABLE_TO_COMPLY";
				}
				
				if (!is_req) {
					TRACE_DEBUG(INFO, "Received an answer to a localy issued query, but no handler processed this answer!");
					fd_msg_dump_walk(INFO, msg);
					fd_msg_free(msg);
					msg = NULL;
					break;
				}
				
				/* Create an answer with the error code and message */
				CHECK_FCT_DO( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, &msg, 0 ), goto fatal_error );
				CHECK_FCT_DO( fd_msg_rescode_set(msg, (char *)ec, (char *)em, NULL, 1 ), goto fatal_error );
				
			case DISP_ACT_SEND:
				/* Now, send the message */
				CHECK_FCT_DO( fd_fifo_post(fd_g_outgoing, &msg), goto fatal_error );
		}
		
		/* We're done with this message */
	
	} while (1);
	
fatal_error:
	TRACE_DEBUG(INFO, "An error occurred in dispatch module! Thread is terminating...");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	
end:	
	/* Mark the thread as terminated */
	pthread_cleanup_pop(1);
	return NULL;
}

/* Later (same as routing): TODO("Set threshold on local queue"); */
static pthread_t my_dispatch = (pthread_t)NULL;
static enum thread_state my_state = INITIAL;

/* Initialize the Dispatch module */
int fd_disp_init(void)
{
	order_val = RUN;
	
	CHECK_POSIX( pthread_create( &my_dispatch, NULL, dispatch_thread, &my_state ) );
	
	return 0;
}

int fd_disp_cleanstop(void)
{
	CHECK_POSIX( pthread_mutex_lock(&order_lock) );
	order_val = STOP;
	CHECK_POSIX( pthread_mutex_unlock(&order_lock) );

	return 0;
}

int fd_disp_fini(void)
{
	/* Wait for a few milliseconds for the thread to complete, by monitoring my_state */
	TODO("Not implemented yet");

	/* Then if needed, cancel the thread */
	
	return ENOTSUP;
}

