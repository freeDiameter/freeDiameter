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

/* Create the PSM thread of one peer structure */
int fd_psm_begin(struct fd_peer * peer )
{
	TRACE_ENTRY("%p", peer);
	TODO("");
	return ENOTSUP;
}

/* End the PSM (clean ending) */
int fd_psm_terminate(struct fd_peer * peer )
{
	TRACE_ENTRY("%p", peer);
	TODO("");
	return ENOTSUP;
}

/* End the PSM violently */
void fd_psm_abord(struct fd_peer * peer )
{
	TRACE_ENTRY("%p", peer);
	TODO("");
	return;
}

