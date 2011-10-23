/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2011, WIDE Project and NICT								 *
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

/* FIFO queues module.
 *
 * The threads that call these functions must be in the cancellation state PTHREAD_CANCEL_ENABLE and type PTHREAD_CANCEL_DEFERRED.
 * This is the default state and type on thread creation.
 *
 * In order to destroy properly a queue, the application must:
 *  -> shutdown any process that can add into the queue first.
 *  -> pthread_cancel any thread that could be waiting on the queue.
 *  -> consume any element that is in the queue, using fd_qu_tryget_int.
 *  -> then destroy the queue using fd_mq_del.
 */

#include "fdproto-internal.h"

/* Definition of a FIFO queue object */
struct fifo {
	int		eyec;	/* An eye catcher, also used to check a queue is valid. FIFO_EYEC */
	
	pthread_mutex_t	mtx;	/* Mutex protecting this queue */
	pthread_cond_t	cond_pull;	/* condition variable for pulling threads */
	pthread_cond_t	cond_push;	/* condition variable for pushing threads */
	
	struct fd_list	list;	/* sentinel for the list of elements */
	int		count;	/* number of objects in the list */
	int		thrs;	/* number of threads waiting for a new element (when count is 0) */
	
	int 		max;	/* maximum number of items to accept if not 0 */
	int		thrs_push; /* number of threads waitnig to push an item */
	
	uint16_t	high;	/* High level threshold (see libfreeDiameter.h for details) */
	uint16_t	low;	/* Low level threshhold */
	void 		*data;	/* Opaque pointer for threshold callbacks */
	void		(*h_cb)(struct fifo *, void **); /* The callbacks */
	void		(*l_cb)(struct fifo *, void **);
	int 		highest;/* The highest count value for which h_cb has been called */
	int		highest_ever; /* The max count value this queue has reached (for tweaking) */
};

/* The eye catcher value */
#define FIFO_EYEC	0xe7ec1130

/* Macro to check a pointer */
#define CHECK_FIFO( _queue ) (( (_queue) != NULL) && ( (_queue)->eyec == FIFO_EYEC) )


/* Create a new queue, with max number of items -- use 0 for no max */
int fd_fifo_new ( struct fifo ** queue, int max )
{
	struct fifo * new;
	
	TRACE_ENTRY( "%p", queue );
	
	CHECK_PARAMS( queue );
	
	/* Create a new object */
	CHECK_MALLOC( new = malloc (sizeof (struct fifo) )  );
	
	/* Initialize the content */
	memset(new, 0, sizeof(struct fifo));
	
	new->eyec = FIFO_EYEC;
	CHECK_POSIX( pthread_mutex_init(&new->mtx, NULL) );
	CHECK_POSIX( pthread_cond_init(&new->cond_pull, NULL) );
	CHECK_POSIX( pthread_cond_init(&new->cond_push, NULL) );
	new->max = max;
	
	fd_list_init(&new->list, NULL);
	
	/* We're done */
	*queue = new;
	return 0;
}

/* Dump the content of a queue */
void fd_fifo_dump(int level, char * name, struct fifo * queue, void (*dump_item)(int level, void * item))
{
	TRACE_ENTRY("%i %p %p %p", level, name, queue, dump_item);
	
	if (!TRACE_BOOL(level))
		return;
	
	fd_log_debug("Dumping queue '%s' (%p):\n", name ?: "?", queue);
	if (!CHECK_FIFO( queue )) {
		fd_log_debug("  Queue invalid!\n");
		if (queue)
			fd_log_debug("  (%x != %x)\n", queue->eyec, FIFO_EYEC);
		return;
	}
	
	CHECK_POSIX_DO(  pthread_mutex_lock( &queue->mtx ), /* continue */  );
	fd_log_debug("   %d elements in queue / %d threads waiting\n", queue->count, queue->thrs);
	fd_log_debug("   %d elements max / %d threads waiting to push\n", queue->max, queue->thrs_push);
	fd_log_debug("   thresholds: %d / %d (h:%d), cb: %p,%p (%p), highest: %d\n",
			queue->high, queue->low, queue->highest, 
			queue->h_cb, queue->l_cb, queue->data,
			queue->highest_ever);
	
	if (dump_item) {
		struct fd_list * li;
		int i = 0;
		for (li = queue->list.next; li != &queue->list; li = li->next) {
			fd_log_debug("  [%i] item %p in fifo %p:\n", i++, li->o, queue);
			(*dump_item)(level, li->o);
		}
	}
	CHECK_POSIX_DO(  pthread_mutex_unlock( &queue->mtx ), /* continue */  );
	
}

/* Delete a queue. It must be empty. */ 
int fd_fifo_del ( struct fifo  ** queue )
{
	struct fifo * q;
	int loops = 0;
	
	TRACE_ENTRY( "%p", queue );

	CHECK_PARAMS( queue && CHECK_FIFO( *queue ) );
	
	q = *queue;
	
	CHECK_POSIX(  pthread_mutex_lock( &q->mtx )  );
	
	if ((q->count != 0) || (q->data != NULL)) {
		TRACE_DEBUG(INFO, "The queue cannot be destroyed (%d, %p)", q->count, q->data);
		CHECK_POSIX_DO(  pthread_mutex_unlock( &q->mtx ), /* no fallback */  );
		return EINVAL;
	}
	
	/* Ok, now invalidate the queue */
	q->eyec = 0xdead;
	
	/* Have all waiting threads return an error */
	while (q->thrs) {
		CHECK_POSIX(  pthread_mutex_unlock( &q->mtx ));
		CHECK_POSIX(  pthread_cond_signal(&q->cond_pull)  );
		usleep(1000);
		
		CHECK_POSIX(  pthread_mutex_lock( &q->mtx )  );
		ASSERT( ++loops < 20 ); /* detect infinite loops */
	}
	
	/* sanity check */
	ASSERT(FD_IS_LIST_EMPTY(&q->list));
	
	/* And destroy it */
	CHECK_POSIX(  pthread_mutex_unlock( &q->mtx )  );
	
	CHECK_POSIX_DO(  pthread_cond_destroy( &q->cond_pull ),  );
	
	CHECK_POSIX_DO(  pthread_cond_destroy( &q->cond_push ),  );
	
	CHECK_POSIX_DO(  pthread_mutex_destroy( &q->mtx ),  );
	
	free(q);
	*queue = NULL;
	
	return 0;
}

/* Move the content of old into new, and update loc_update atomically. We leave the old queue empty but valid */
int fd_fifo_move ( struct fifo * old, struct fifo * new, struct fifo ** loc_update )
{
	int loops = 0;
	
	TRACE_ENTRY("%p %p %p", old, new, loc_update);
	CHECK_PARAMS( CHECK_FIFO( old ) && CHECK_FIFO( new ));
	
	CHECK_PARAMS( ! old->data );
	if (new->high) {
		TODO("Implement support for thresholds in fd_fifo_move...");
	}
	
	/* Update loc_update */
	if (loc_update)
		*loc_update = new;
	
	/* Lock the queues */
	CHECK_POSIX(  pthread_mutex_lock( &old->mtx )  );
	
	CHECK_PARAMS_DO( (! old->thrs_push), {
			pthread_mutex_unlock( &old->mtx );
			return EINVAL;
		} );
	
	CHECK_POSIX(  pthread_mutex_lock( &new->mtx )  );
	
	/* Any waiting thread on the old queue returns an error */
	old->eyec = 0xdead;
	while (old->thrs) {
		CHECK_POSIX(  pthread_mutex_unlock( &old->mtx ));
		CHECK_POSIX(  pthread_cond_signal( &old->cond_pull )  );
		usleep(1000);
		
		CHECK_POSIX(  pthread_mutex_lock( &old->mtx )  );
		ASSERT( loops < 20 ); /* detect infinite loops */
	}
	
	/* Move all data from old to new */
	fd_list_move_end( &new->list, &old->list );
	if (old->count && (!new->count)) {
		CHECK_POSIX(  pthread_cond_signal(&new->cond_pull)  );
	}
	new->count += old->count;
	
	/* Reset old */
	old->count = 0;
	old->eyec = FIFO_EYEC;
	
	/* Unlock, we're done */
	CHECK_POSIX(  pthread_mutex_unlock( &new->mtx )  );
	CHECK_POSIX(  pthread_mutex_unlock( &old->mtx )  );
	
	return 0;
}

/* Get the length of the queue */
int fd_fifo_length ( struct fifo * queue, int * length )
{
	TRACE_ENTRY( "%p %p", queue, length );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_FIFO( queue ) && length );
	
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
int fd_fifo_length_noerr ( struct fifo * queue )
{
	if ( !CHECK_FIFO( queue ) )
		return 0;
	
	return queue->count; /* Let's hope it's read atomically, since we are not locking... */
}

/* Set the thresholds of the queue */
int fd_fifo_setthrhd ( struct fifo * queue, void * data, uint16_t high, void (*h_cb)(struct fifo *, void **), uint16_t low, void (*l_cb)(struct fifo *, void **) )
{
	TRACE_ENTRY( "%p %p %hu %p %hu %p", queue, data, high, h_cb, low, l_cb );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_FIFO( queue ) && (high > low) && (queue->data == NULL) );
	
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


/* This handler is called when a thread is blocked on a queue, and cancelled */
static void fifo_cleanup_push(void * queue)
{
	struct fifo * q = (struct fifo *)queue;
	TRACE_ENTRY( "%p", queue );
	
	/* The thread has been cancelled, therefore it does not wait on the queue anymore */
	q->thrs_push--;
	
	/* Now unlock the queue, and we're done */
	CHECK_POSIX_DO(  pthread_mutex_unlock( &q->mtx ),  /* nothing */  );
	
	/* End of cleanup handler */
	return;
}


/* Post a new item in the queue */
int fd_fifo_post_int ( struct fifo * queue, void ** item )
{
	struct fd_list * new;
	int call_cb = 0;
	
	TRACE_ENTRY( "%p %p", queue, item );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_FIFO( queue ) && item && *item );
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
	if (queue->max) {
		while (queue->count >= queue->max) {
			int ret = 0;
			
			/* We have to wait for an item to be pulled */
			queue->thrs_push++ ;
			pthread_cleanup_push( fifo_cleanup_push, queue);
			ret = pthread_cond_wait( &queue->cond_push, &queue->mtx );
			pthread_cleanup_pop(0);
			queue->thrs_push-- ;
			
			ASSERT( ret == 0 );
		}
	}
	
	/* Create a new list item */
	CHECK_MALLOC_DO(  new = malloc (sizeof (struct fd_list)) , {
			pthread_mutex_unlock( &queue->mtx );
		} );
	
	fd_list_init(new, *item);
	*item = NULL;
	
	/* Add the new item at the end */
	fd_list_insert_before( &queue->list, new);
	queue->count++;
	if (queue->highest_ever < queue->count)
		queue->highest_ever = queue->count;
	if (queue->high && ((queue->count % queue->high) == 0)) {
		call_cb = 1;
		queue->highest = queue->count;
	}
	
	/* Signal if threads are asleep */
	if (queue->thrs > 0) {
		CHECK_POSIX(  pthread_cond_signal(&queue->cond_pull)  );
	}
	if (queue->thrs_push > 0) {
		/* cascade */
		CHECK_POSIX(  pthread_cond_signal(&queue->cond_push)  );
	}
	
	/* Unlock */
	CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
	
	/* Call high-watermark cb as needed */
	if (call_cb && queue->h_cb)
		(*queue->h_cb)(queue, &queue->data);
	
	/* Done */
	return 0;
}

/* Pop the first item from the queue */
static void * mq_pop(struct fifo * queue)
{
	void * ret = NULL;
	struct fd_list * li;
	
	ASSERT( ! FD_IS_LIST_EMPTY(&queue->list) );
	
	fd_list_unlink(li = queue->list.next);
	queue->count--;
	ret = li->o;
	free(li);
	
	if (queue->thrs_push) {
		CHECK_POSIX_DO( pthread_cond_signal( &queue->cond_push ), );
	}
	
	return ret;
}

/* Check if the low watermark callback must be called. */
static __inline__ int test_l_cb(struct fifo * queue)
{
	if ((queue->high == 0) || (queue->low == 0) || (queue->l_cb == 0))
		return 0;
	
	if (((queue->count % queue->high) == queue->low) && (queue->highest > queue->count)) {
		queue->highest -= queue->high;
		return 1;
	}
	
	return 0;
}

/* Try poping an item */
int fd_fifo_tryget_int ( struct fifo * queue, void ** item )
{
	int wouldblock = 0;
	int call_cb = 0;
	
	TRACE_ENTRY( "%p %p", queue, item );
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_FIFO( queue ) && item );
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
	/* Check queue status */
	if (queue->count > 0) {
got_item:
		/* There are elements in the queue, so pick the first one */
		*item = mq_pop(queue);
		call_cb = test_l_cb(queue);
	} else {
		if (queue->thrs_push > 0) {
			/* A thread is trying to push something, let's give it a chance */
			CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
			CHECK_POSIX(  pthread_cond_signal( &queue->cond_push )  );
			usleep(1000);
			CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
			if (queue->count > 0)
				goto got_item;
		}
		
		wouldblock = 1;
		*item = NULL;
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
static void fifo_cleanup(void * queue)
{
	struct fifo * q = (struct fifo *)queue;
	TRACE_ENTRY( "%p", queue );
	
	/* The thread has been cancelled, therefore it does not wait on the queue anymore */
	q->thrs--;
	
	/* Now unlock the queue, and we're done */
	CHECK_POSIX_DO(  pthread_mutex_unlock( &q->mtx ),  /* nothing */  );
	
	/* End of cleanup handler */
	return;
}

/* The internal function for fd_fifo_timedget and fd_fifo_get */
static int fifo_tget ( struct fifo * queue, void ** item, int istimed, const struct timespec *abstime)
{
	int timedout = 0;
	int call_cb = 0;
	
	/* Check the parameters */
	CHECK_PARAMS( CHECK_FIFO( queue ) && item && (abstime || !istimed) );
	
	/* Initialize the return value */
	*item = NULL;
	
	/* lock the queue */
	CHECK_POSIX(  pthread_mutex_lock( &queue->mtx )  );
	
awaken:
	/* Check queue status */
	if (!CHECK_FIFO( queue )) {
		/* The queue is being destroyed */
		CHECK_POSIX(  pthread_mutex_unlock( &queue->mtx )  );
		TRACE_DEBUG(FULL, "The queue is being destroyed -> EPIPE");
		return EPIPE;
	}
		
	if (queue->count > 0) {
		/* There are items in the queue, so pick the first one */
		*item = mq_pop(queue);
		call_cb = test_l_cb(queue);
	} else {
		int ret = 0;
		/* We have to wait for a new item */
		queue->thrs++ ;
		pthread_cleanup_push( fifo_cleanup, queue);
		if (istimed) {
			ret = pthread_cond_timedwait( &queue->cond_pull, &queue->mtx, abstime );
		} else {
			ret = pthread_cond_wait( &queue->cond_pull, &queue->mtx );
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

/* Get the next available item, block until there is one */
int fd_fifo_get_int ( struct fifo * queue, void ** item )
{
	TRACE_ENTRY( "%p %p", queue, item );
	return fifo_tget(queue, item, 0, NULL);
}

/* Get the next available item, block until there is one, or the timeout expires */
int fd_fifo_timedget_int ( struct fifo * queue, void ** item, const struct timespec *abstime )
{
	TRACE_ENTRY( "%p %p %p", queue, item, abstime );
	return fifo_tget(queue, item, 1, abstime);
}

