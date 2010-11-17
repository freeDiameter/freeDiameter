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

int fd_event_send(struct fifo *queue, int code, size_t datasz, void * data)
{
	struct fd_event * ev;
	CHECK_MALLOC( ev = malloc(sizeof(struct fd_event)) );
	ev->code = code;
	ev->size = datasz;
	ev->data = data;
	CHECK_FCT( fd_fifo_post(queue, &ev) );
	return 0;
}

int fd_event_get(struct fifo *queue, int *code, size_t *datasz, void ** data)
{
	struct fd_event * ev;
	CHECK_FCT( fd_fifo_get(queue, &ev) );
	if (code)
		*code = ev->code;
	if (datasz)
		*datasz = ev->size;
	if (data)
		*data = ev->data;
	free(ev);
	return 0;
}

int fd_event_timedget(struct fifo *queue, struct timespec * timeout, int timeoutcode, int *code, size_t *datasz, void ** data)
{
	struct fd_event * ev;
	int ret = 0;
	ret = fd_fifo_timedget(queue, &ev, timeout);
	if (ret == ETIMEDOUT) {
		if (code)
			*code = timeoutcode;
		if (datasz)
			*datasz = 0;
		if (data)
			*data = NULL;
	} else {
		CHECK_FCT( ret );
		if (code)
			*code = ev->code;
		if (datasz)
			*datasz = ev->size;
		if (data)
			*data = ev->data;
		free(ev);
	}
	return 0;
}

void fd_event_destroy(struct fifo **queue, void (*free_cb)(void * data))
{
	struct fd_event * ev;
	/* Purge all events, and free the associated data if any */
	while (fd_fifo_tryget( *queue, &ev ) == 0) {
		(*free_cb)(ev->data);
		free(ev);
	}
	CHECK_FCT_DO( fd_fifo_del(queue), /* continue */ );
	return ;
} 

const char * fd_ev_str(int event)
{
	switch (event) {
	#define case_str( _val )\
		case _val : return #_val
		case_str(FDEV_TERMINATE);
		case_str(FDEV_DUMP_DICT);
		case_str(FDEV_DUMP_EXT);
		case_str(FDEV_DUMP_SERV);
		case_str(FDEV_DUMP_QUEUES);
		case_str(FDEV_DUMP_CONFIG);
		case_str(FDEV_DUMP_PEERS);
		
		default:
			TRACE_DEBUG(FULL, "Unknown event : %d", event);
			return "Unknown event";
	}
}

