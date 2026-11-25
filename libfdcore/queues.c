/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2020, WIDE Project and NICT								 *
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

#include "fdcore-internal.h"

/* The global message queues */
struct fifo * fd_g_incoming = NULL;
struct fifo * fd_g_outgoing = NULL;
struct fifo * fd_g_local = NULL;

static pthread_mutex_t work_mtx = PTHREAD_MUTEX_INITIALIZER;
static unsigned long work_pending;
static unsigned long work_generation;
static int work_quiescing;
static int work_stopped;
static struct fd_hook_data_hdl *work_message_hdl;

static void work_message_fini(struct fd_hook_permsgdata *data)
{
	struct fd_work_message *work = (void *)data;
	if (work->admitted)
		fd_work_done(NULL);
}

int fd_work_message_get(struct msg *msg, struct fd_work_message **work)
{
	CHECK_PARAMS(work_message_hdl);
	*work = (void *)fd_hook_get_pmd(work_message_hdl, msg);
	return *work ? 0 : ENOMEM;
}

void fd_work_message_start(struct fd_work_message *work)
{
	if (!work->admitted) {
		fd_work_start();
		work->admitted = 1;
	}
}

void fd_work_message_cancel(struct fd_work_message *work)
{
	if (work->admitted) {
		work->admitted = 0;
		fd_work_done(NULL);
	}
}

void fd_work_start(void)
{
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	work_pending++;
	work_generation++;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
}

void fd_work_done(void *unused)
{
	(void)unused;
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	ASSERT(work_pending > 0);
	work_pending--;
	work_generation++;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
}

void fd_work_snapshot(unsigned long *generation, unsigned long *pending)
{
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	*generation = work_generation;
	*pending = work_pending;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
}

void fd_work_quiesce(void)
{
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	work_quiescing = 1;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
}

int fd_work_is_quiescing(void)
{
	int quiescing;
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	quiescing = work_quiescing;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
	return quiescing;
}

void fd_work_stop(void)
{
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	work_stopped = 1;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
}

int fd_work_stop_if_idle(unsigned long generation)
{
	int stopped;
	CHECK_POSIX_DO(pthread_mutex_lock(&work_mtx), ASSERT(0));
	stopped = !work_pending && work_generation == generation;
	if (stopped)
		work_stopped = 1;
	CHECK_POSIX_DO(pthread_mutex_unlock(&work_mtx), ASSERT(0));
	return stopped;
}

int fd_work_admit(int local_request, int worker)
{
	int ret = 0;
	CHECK_POSIX(pthread_mutex_lock(&work_mtx));
	if (work_stopped || (work_quiescing && local_request && !worker)) {
		ret = EBUSY;
	} else {
		work_pending++;
		work_generation++;
	}
	CHECK_POSIX(pthread_mutex_unlock(&work_mtx));
	return ret;
}

int fd_work_post(struct fifo *queue, void **item, int nonblock)
{
	int ret;
	fd_work_start();
	pthread_cleanup_push(fd_work_done, NULL);
	ret = nonblock ? fd_fifo_post_noblock(queue, item) : fd_fifo_post_int(queue, item);
	pthread_cleanup_pop(ret != 0);
	return ret;
}

/* Initialize the message queues. */
int fd_queues_init(void)
{
	TRACE_ENTRY();
	CHECK_FCT(fd_hook_data_register(sizeof(struct fd_work_message), NULL,
				       work_message_fini, &work_message_hdl));
	CHECK_FCT( fd_fifo_new ( &fd_g_incoming, fd_g_config->cnf_qin_limit ) );
	CHECK_FCT( fd_fifo_new ( &fd_g_outgoing, fd_g_config->cnf_qout_limit ) );
	CHECK_FCT( fd_fifo_new ( &fd_g_local,    fd_g_config->cnf_qlocal_limit ) );
	return 0;
}

/* Resize according to values given in configuration file */
int fd_queues_init_after_conf(void)
{
	TRACE_ENTRY();
	CHECK_FCT( fd_fifo_set_max ( fd_g_incoming, fd_g_config->cnf_qin_limit ) );
	CHECK_FCT( fd_fifo_set_max ( fd_g_outgoing, fd_g_config->cnf_qout_limit ) );
	CHECK_FCT( fd_fifo_set_max ( fd_g_local,    fd_g_config->cnf_qlocal_limit ) );
	return 0;
}

/* Destroy a queue after emptying it (and dumping the content) */
int fd_queues_fini(struct fifo ** queue)
{
	struct msg * msg;
	int ret = 0;
	
	TRACE_ENTRY("%p", queue);
	
	/* Note : the threads that post into this queue should already been stopped before this !!! */
	
	CHECK_PARAMS(queue);
	if (*queue == NULL)
		return 0; /* the queue was not already initialized */

	/* Empty all contents */
	while (1) {
		/* Check if there is a message in the queue */
		ret = fd_fifo_tryget(*queue, &msg);
		if (ret == EWOULDBLOCK)
			break;
		CHECK_FCT(ret);
		
		/* We got one! */
		fd_hook_call(HOOK_MESSAGE_DROPPED, msg, NULL, "Message lost because framework is terminating.", fd_msg_pmdl_get(msg));
		fd_msg_free(msg);
		fd_work_done(NULL);
	}
	
	/* Now, delete the empty queue */
	CHECK_FCT( fd_fifo_del ( queue ) );
	
	return 0;
}
