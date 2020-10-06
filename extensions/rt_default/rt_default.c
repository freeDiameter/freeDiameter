/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2019, WIDE Project and NICT								 *
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

/* 
 * Configurable routing of messages for freeDiameter.
 */

#include <signal.h>

#include "rt_default.h"

#define MODULE_NAME "rt_default"

#include <pthread.h>

static pthread_rwlock_t rtd_lock;

static char *rtd_config_file;

/* The callback called on new messages */
static int rtd_out(void * cbdata, struct msg ** pmsg, struct fd_list * candidates)
{
	struct msg * msg = *pmsg;
	TRACE_ENTRY("%p %p %p", cbdata, msg, candidates);
	int ret;
	
	CHECK_PARAMS(msg && candidates);

	if (pthread_rwlock_rdlock(&rtd_lock) != 0) {
		fd_log_notice("%s: read-lock failed, skipping handler", MODULE_NAME);
		return 0;
	}
	/* Simply pass it to the appropriate function */
	if (FD_IS_LIST_EMPTY(candidates)) {
		ret = 0;
	} else {
		ret = rtd_process( msg, candidates );
	}
	if (pthread_rwlock_unlock(&rtd_lock) != 0) {
		fd_log_notice("%s: read-unlock failed after rtd_out, exiting", MODULE_NAME);
		exit(1);
	}
	return ret;
}

/* handler */
static struct fd_rt_out_hdl * rtd_hdl = NULL;

static volatile int in_signal_handler = 0;

/* signal handler */
static void sig_hdlr(void)
{
	if (in_signal_handler) {
		fd_log_error("%s: already handling a signal, ignoring new one", MODULE_NAME);
		return;
	}
	in_signal_handler = 1;

	if (pthread_rwlock_wrlock(&rtd_lock) != 0) {
		fd_log_error("%s: locking failed, aborting config reload", MODULE_NAME);
		return;
	}
	rtd_conf_reload(rtd_config_file);
	if (pthread_rwlock_unlock(&rtd_lock) != 0) {
		fd_log_error("%s: unlocking failed after config reload, exiting", MODULE_NAME);
		exit(1);
	}		

	fd_log_notice("%s: reloaded configuration", MODULE_NAME);

	in_signal_handler = 0;
}


/* entry point */
static int rtd_entry(char * conffile)
{
	TRACE_ENTRY("%p", conffile);

	rtd_config_file = conffile;
	pthread_rwlock_init(&rtd_lock, NULL);

	if (pthread_rwlock_wrlock(&rtd_lock) != 0) {
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Initialize the repo */
	CHECK_FCT( rtd_init() );
	
	/* Parse the configuration file */
	CHECK_FCT( rtd_conf_handle(conffile) );

	if (pthread_rwlock_unlock(&rtd_lock) != 0) {
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Register reload callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, MODULE_NAME, sig_hdlr));

#if 0
	/* Dump the rules */
	rtd_dump();
#endif /* 0 */
	
	/* Register the callback */
	CHECK_FCT( fd_rt_out_register( rtd_out, NULL, 5, &rtd_hdl ) );
	
	/* We're done */
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	TRACE_ENTRY();
	
	/* Unregister the cb */
	CHECK_FCT_DO( fd_rt_out_unregister ( rtd_hdl, NULL ), /* continue */ );
	
	/* Destroy the data */
	rtd_fini();

	pthread_rwlock_destroy(&rtd_lock);

	/* Done */
	return ;
}

EXTENSION_ENTRY(MODULE_NAME, rtd_entry);
