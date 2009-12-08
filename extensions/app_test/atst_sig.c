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

/* Install the signal handler */

#include "app_test.h"

static pthread_t th = (pthread_t) NULL;

static void * atst_sig_th(void * arg)
{
	int sig;
	int ret;
	sigset_t ss;
	void (*cb)(void) = arg;
	
	fd_log_threadname ( "app_test signal handler" );
	
	sigemptyset(&ss);
	sigaddset(&ss, atst_conf->signal);
	
	while (1) {
		ret = sigwait(&ss, &sig);
		if (ret != 0) {
			fd_log_debug("sigwait failed in the app_test extension: %s", strerror(errno));
			break;
		}
		
		TRACE_DEBUG(FULL, "Signal caught, calling function...");
		
		/* Call the cb function */
		(*cb)();
		
	}
	
	return NULL;
}

int atst_sig_init(void (*cb)(void))
{
	return pthread_create( &th, NULL, atst_sig_th, (void *)cb );
}

void atst_sig_fini(void)
{
	void * th_ret = NULL;
	
	if (th != (pthread_t) NULL) {
		(void) pthread_cancel(th);
		(void) pthread_join(th, &th_ret);
	}
	
	return;
}
