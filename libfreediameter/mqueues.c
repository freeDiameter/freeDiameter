/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2008, WIDE Project and NICT								 *
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

/* Messages queues module.
 *
 * The threads that call these functions must be in the cancellation state PTHREAD_CANCEL_ENABLE and type PTHREAD_CANCEL_DEFERRED.
 * This is the default state and type on thread creation.
 *
 * In order to destroy properly a queue, the application must:
 *  -> shutdown any process that can add into the queue first.
 *  -> pthread_cancel any thread that could be waiting on the queue.
 *  -> consume any message that is in the queue, using meq_tryget.
 *  -> then destroy the queue using meq_del.
 */

#include "libfd.h"

/* Definition of a message queue object */
struct mqueue {
	int		eyec;	/* An eye catcher, also used to check a queue is valid. MQ_EYEC */
	
	pthread_mutex_t	mtx;	/* Mutex protecting this queue */
	pthread_cond_t	cond;	/* condition variable of the list */
	
	struct fd_list	list;	/* sentinel for the list of messages */
	int		count;	/* number of objects in the list */
	int		thrs;	/* number of threads waiting for a new message (when count is 0) */
	
	uint16_t	high;	/* High level threshold (see libfreediameter.h for details) */
	uint16_t	low;	/* Low level threshhold */
	void 		*data;	/* Opaque pointer for threshold callbacks */
	void		(*h_cb)(struct mqueue *, void **); /* The callbacks */
	void		(*l_cb)(struct mqueue *, void **);
	int 		highest;/* The highest count value for which h_cb has been called */
};

/* The eye catcher value */
#define MQ_EYEC	0xe7ec1130

/* Macro to check a pointer */
#define CHECK_QUEUE( _queue ) (( (_queue) != NULL) && ( (_queue)->eyec == MQ_EYEC) )


/* Create a new message queue */
int fd_mq_new ( struct mqueue ** queue )
{
	struct mqueue * new;
	
	TRACE_ENTRY( "%p", queue );
	
	CHECK_PARAMS( queue );
	
	/* Create a new object */
	CHECK_MALLOC( new = malloc (sizeof (struct mqueue) )  );
	
	/* Initialize the content */
	memset(new, 0, sizeof(struct mqueue));
	
	new->eyec = MQ_EYEC;
	CHECK_POSIX( pthread_mutex_init(&new->mtx, NULL) );
	CHECK_POSIX( pthread_cond_init(&new->cond, NULL) );
	
	fd_list_init(&new->list, NULL);
	
	/* We're done */
	*queue = new;
	return 0;
}

/* Delete a message queue. It must be unused. */ 
int fd_mq_del ( struct mqueue  ** queue )
{
	struct mqueue * q;
	
	TRACE_ENTRY( "%p", queue );

	CHECK_PARAMS( queue && CHECK_QUEUE( *queue ) );
	
	q = *queue;
	
	CHECK_POSIX(  pthread_mutex_lock( &q->mtx )  );
	
	if ((q->count != 0) || (q->thrs != 0) || (q->data != NULL)) {
		TRACE_DEBUG(INFO, "The queue cannot be destroyed (%d, %d, %p)", q->count, q->thrs, q->data);
		CHECK_POSIX_DO(  pthread_mutex_unlock( &q->mtx ), /* no fallback */  );
		return EINVAL;
	}
	
	/* sanity check */
	ASSERT(FD_IS_LIST_EMPTY(&q->list));
	
	/* Ok, now invalidate the queue */
	q->eyec = 0xdead;
	
	/* And destroy it */
	CHECK_POSIX(  pthread_mutex_unlock( &q->mtx )  );
	
	CHECK_POSIX(  pthread_cond_destroy( &q->cond )  );
	
	CHECK_POSIX(  pthread_mutex_destroy( &q->mtx )  );
	
	free(q);
	*queue = NULL;
	
	return 0;
}

/* Get the length of the queue */
int fd_mq_length ( struct mqueue * queue, int * length )
{
	TRACE_ENTRY( "%p %p", queue, length );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_QUEUE( queue ) && length );
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
	/* Retrieve the count */
	*length = queue->count;
	
	/* Unlock */
	CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
	
	/* Done */
	return 0;
}

/* alternate version with no error checking */
int fd_mq_length_noerr ( struct mqueue * queue )
{
	if ( !CHECK_QUEUE( queue ) )
		return 0;
	
	return queue->count; /* Let's hope it's read atomically, since we are not locking... */
}

/* Set the thresholds of the queue */
int fd_mq_setthrhd ( struct mqueue * queue, void * data, uint16_t high, void (*h_cb)(struct mqueue *, void **), uint16_t low, void (*l_cb)(struct mqueue *, void **) )
{
	TRACE_ENTRY( "%p %p %hu %p %hu %p", queue, data, high, h_cb, low, l_cb );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_QUEUE( queue ) && (high > low) && (queue->data == NULL) );
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
	/* Save the values */
	queue->high = high;
	queue->low  = low;
	queue->data = data;
	queue->h_cb = h_cb;
	queue->l_cb = l_cb;
	
	/* Unlock */
	CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
	
	/* Done */
	return 0;
}

/* Post a new message in the queue */
int fd_mq_post ( struct mqueue * queue, struct msg ** msg )
{
	struct fd_list * new;
	int call_cb = 0;
	
	TRACE_ENTRY( "%p %p", queue, msg );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_QUEUE( queue ) && msg && *msg );
	
	/* Create a new list item */
	CHECK_MALLOC(  new = malloc (sizeof (struct fd_list))  );
	
	fd_list_init(new, *msg);
	*msg = NULL;
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
	/* Add the new message at the end */
	fd_list_insert_before( &queue->list, new);
	queue->count++;
	if (queue->high && ((queue->count % queue->high) == 0)) {
		call_cb = 1;
		queue->highest = queue->count;
	}
	
	/* Signal if threads are asleep */
	if (queue->thrs > 0) {
		CHECK_POSIX(  pthread_cond_signal(&queue->cond)  );
	}
	
	/* Unlock */
	CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
	
	/* Call high-watermark cb as needed */
	if (call_cb && queue->h_cb)
		(*queue->h_cb)(queue, &queue->data);
	
	/* Done */
	return 0;
}

/* Pop the first message from the queue */
static struct msg * mq_pop(struct mqueue * queue)
{
	struct msg * ret = NULL;
	struct fd_list * li;
	
	ASSERT( ! FD_IS_LIST_EMPTY(&queue->list) );
	
	fd_list_unlink(li = queue->list.next);
	queue->count--;
	ret = (struct msg *)(li->o);
	free(li);
	
	return ret;
}

/* Check if the low watermark callback must be called. */
static int test_l_cb(struct mqueue * queue)
{
	if ((queue->high == 0) || (queue->low == 0) || (queue->l_cb == 0))
		return 0;
	
	if (((queue->count % queue->high) == queue->low) && (queue->highest > queue->count)) {
		queue->highest -= queue->high;
		return 1;
	}
	
	return 0;
}

/* Try poping a message */
int fd_mq_tryget ( struct mqueue * queue, struct msg ** msg )
{
	int wouldblock = 0;
	int call_cb = 0;
	
	TRACE_ENTRY( "%p %p", queue, msg );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_QUEUE( queue ) && msg );
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
	/* Check queue status */
	if (queue->count > 0) {
		/* There are messages in the queue, so pick the first one */
		*msg = mq_pop(queue);
		call_cb = test_l_cb(queue);
	} else {
		wouldblock = 1;
		*msg = NULL;
	}
		
	/* Unlock */
	CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
	
	/* Call low watermark callback as needed */
	if (call_cb)
		(*queue->l_cb)(queue, &queue->data);
	
	/* Done */
	return wouldblock ? EWOULDBLOCK : 0;
}

/* This handler is called when a thread is blocked on a queue, and cancelled */
static void mq_cleanup(void * queue)
{
	struct mqueue * q = (struct mqueue *)queue;
	TRACE_ENTRY( "%p", queue );
	
	/* Check the parameter */
	if ( ! CHECK_QUEUE( q )) {
		TRACE_DEBUG(INFO, "Invalid queue, skipping handler");
		return;
	}
	
	/* The thread has been cancelled, therefore it does not wait on the queue anymore */
	q->thrs--;
	
	/* Now unlock the queue, and we're done */
	CHECK_POSIX_DO(  pthread_mutex_unlock( &q->mtx ),  /* nothing */  );
	
	/* End of cleanup handler */
	return;
}

/* The internal function for meq_timedget and meq_get */
static int mq_tget ( struct mqueue * queue, struct msg ** msg, int istimed, const struct timespec *abstime)
{
	int timedout = 0;
	int call_cb = 0;
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_QUEUE( queue ) && msg && (abstime || !istimed) );
	
	/* Initialize the msg value */
	*msg = NULL;
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
awaken:
	/* Check queue status */
	if (queue->count > 0) {
		/* There are messages in the queue, so pick the first one */
		*msg = mq_pop(queue);
		call_cb = test_l_cb(queue);
	} else {
		int ret = 0;
		/* We have to wait for a new message */
		queue->thrs++ ;
		pthread_cleanup_push( mq_cleanup, queue);
		if (istimed) {
			ret = pthread_cond_timedwait( &queue->cond, &queue->mtx, abstime );
		} else {
			ret = pthread_cond_wait( &queue->cond, &queue->mtx );
		}
		pthread_cleanup_pop(0);
		queue->thrs-- ;
		if (ret == 0)
			goto awaken;  /* test for spurious wake-ups */
		
		if (istimed && (ret == ETIMEDOUT)) {
			timedout = 1;
		} else {
			/* Unexpected error condition (means we need to debug) */
			ASSERT( ret == 0 /* never true */ );
		}
	}
	
	/* Unlock */
	CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
	
	/* Call low watermark callback as needed */
	if (call_cb)
		(*queue->l_cb)(queue, &queue->data);
	
	/* Done */
	return timedout ? ETIMEDOUT : 0;
}

/* Get the next available message, block until there is one */
int fd_mq_get ( struct mqueue * queue, struct msg ** msg )
{
	TRACE_ENTRY( "%p %p", queue, msg );
	return mq_tget(queue, msg, 0, NULL);
}

/* Get the next available message, block until there is one, or the timeout expires */
int fd_mq_timedget ( struct mqueue * queue, struct msg ** msg, const struct timespec *abstime )
{
	TRACE_ENTRY( "%p %p %p", queue, msg, abstime );
	return mq_tget(queue, msg, 1, abstime);
}

