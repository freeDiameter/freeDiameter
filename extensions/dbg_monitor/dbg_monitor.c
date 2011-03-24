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

/* Monitoring extension:
 - periodically display queues and peers information
 - upon SIGUSR2, display additional debug information
 */

#include <freeDiameter/extension.h>
#include <signal.h>

#ifndef MONITOR_SIGNAL
#define MONITOR_SIGNAL	SIGUSR2
#endif /* MONITOR_SIGNAL */

static int 	 monitor_main(char * conffile);

EXTENSION_ENTRY("dbg_monitor", monitor_main);

/* Thread to display periodical debug information */
static pthread_t thr;
static void * mn_thr(void * arg)
{
	int i = 0;
	fd_log_threadname("Monitor thread");
	
	/* Loop */
	while (1) {
		#ifdef DEBUG
		for (i++; i % 30; i++) {
			fd_log_debug("[dbg_monitor] %ih%*im%*is\n", i/3600, 2, (i/60) % 60 , 2, i%60); /* This makes it easier to detect inactivity periods in the log file */
			sleep(1);
		}
		#else /* DEBUG */
		sleep(3600); /* 1 hour */
		#endif /* DEBUG */
		fd_log_debug("[dbg_monitor] Dumping current information\n");
		CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_QUEUES, 0, NULL), /* continue */);
		CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_SERV, 0, NULL), /* continue */);
		CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_PEERS, 0, NULL), /* continue */);
		sleep(1);
	}
	
	return NULL;
}

/* Function called on receipt of MONITOR_SIGNAL */
static void got_sig()
{
	fd_log_debug("[dbg_monitor] Dumping extra information\n");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_DICT, 0, NULL), /* continue */);
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_CONFIG, 0, NULL), /* continue */);
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_EXT, 0, NULL), /* continue */);
}

/* Entry point */
static int monitor_main(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	
	/* Catch signal SIGUSR1 */
	CHECK_FCT( fd_event_trig_regcb(MONITOR_SIGNAL, "dbg_monitor", got_sig));
	
	CHECK_POSIX( pthread_create( &thr, NULL, mn_thr, NULL ) );
	return 0;
}

/* Cleanup */
void fd_ext_fini(void)
{
	TRACE_ENTRY();
	CHECK_FCT_DO( fd_thr_term(&thr), /* continue */ );
	return ;
}

