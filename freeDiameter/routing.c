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

/* Function to return an error to an incoming request */
static int return_error(struct msg * msg, char * error_code)
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
	CHECK_FCT( fd_msg_rescode_set(msg, error_code, NULL, NULL, 1 ) );

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
			CHECK_FCT_DO( return_error( msg, "DIAMETER_INVALID_HDR_BITS"), goto fatal_error );
			continue;
		}
		
		
		

	
	
	} while (1);
	
fatal_error:
	TRACE_DEBUG(INFO, "An error occurred in routing module! IN thread is terminating...");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	return NULL;
}

/* Note: after testing if the message is to be handled locally, we should test for decorated NAI 
  (draft-ietf-dime-nai-routing-04 section 4.4) */
  
/* Note2: if the message is still for local delivery, we should test for duplicate
  (draft-asveren-dime-dupcons-00). This may conflict with path validation decisions, no clear answer yet */



/* Initialize the routing module */
int fd_rt_init(void)
{
	TODO("Start the routing threads");
	return ENOTSUP;
}

/* Terminate the routing module */
int fd_rt_fini(void)
{
	TODO("Stop the routing threads");
	return ENOTSUP;
}



