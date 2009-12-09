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

/* Function called on receipt of SIGUSR1 */
static void got_sig(int signal)
{
	fd_log_debug("[dbg_monitor] Dumping extra information\n");
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_DICT, 0, NULL), /* continue */);
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_CONFIG, 0, NULL), /* continue */);
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_EXT, 0, NULL), /* continue */);
}
/* Thread to display periodical debug information */
static pthread_t thr;
static void * mn_thr(void * arg)
{
	sigset_t sig;
	struct sigaction act;
	fd_log_threadname("Monitor thread");
	
	/* Catch signal SIGUSR1 */
	memset(&act, 0, sizeof(act));
	act.sa_handler = got_sig;
	CHECK_SYS_DO( sigaction(MONITOR_SIGNAL, &act, NULL), /* conitnue */ );
	sigemptyset(&sig);
	sigaddset(&sig, MONITOR_SIGNAL);
	CHECK_POSIX_DO(  pthread_sigmask(SIG_UNBLOCK, &sig, NULL), /* conitnue */  );
	
	/* Loop */
	while (1) {
		#ifdef DEBUG
		sleep(30);
		#else /* DEBUG */
		sleep(3600); /* 1 hour */
		#endif /* DEBUG */
		fd_log_debug("[dbg_monitor] Dumping current information\n");
		CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_QUEUES, 0, NULL), /* continue */);
		CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_SERV, 0, NULL), /* continue */);
		CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_DUMP_PEERS, 0, NULL), /* continue */);
		pthread_testcancel();
	}
	
	return NULL;
}

static int monitor_main(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	CHECK_POSIX( pthread_create( &thr, NULL, mn_thr, NULL ) );
	return 0;
}

void fd_ext_fini(void)
{
	TRACE_ENTRY();
	CHECK_FCT_DO( fd_thr_term(&thr), /* continue */ );
	return ;
}

