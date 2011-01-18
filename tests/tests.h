/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
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

/* This file contains the definition of our test harness.
 * The harness is very simple yet.
 * It may be interessant to go to dejagnu later...
 *
 */
#ifndef _TESTS_H
#define _TESTS_H

#include "fdproto-internal.h"
#include "fdcore-internal.h"

#include <pthread.h>
#include <errno.h>
#include <gcrypt.h>

/* Test timeout duration, unless -n is passed on the command line */
#ifndef TEST_TIMEOUT
#define TEST_TIMEOUT	30	/* in seconds */
#endif /* TEST_TIMEOUT */

/* Standard includes */
#include <getopt.h>
#include <time.h>
#include <libgen.h>
#include <signal.h>

/* Define the return code values */
#define PASS	0
#define FAIL	1

/* Define the macro to fail a test with a message */
#define FAILTEST( message... ){				\
	fprintf(stderr, ## message);			\
	TRACE_DEBUG(INFO, "Test failed");		\
	(void)fd_core_shutdown();			\
	(void)fd_core_wait_shutdown_complete();		\
	exit(FAIL);					\
}

/* Define the macro to pass a test */
#define PASSTEST( ){					\
	fprintf(stderr, "Test %s passed\n", __FILE__);	\
	TRACE_DEBUG(INFO, "Test passed");		\
	(void)fd_core_shutdown();			\
	(void)fd_core_wait_shutdown_complete();		\
	exit(PASS);					\
}

static int test_verbo = 0;
static struct fd_config conf;
struct fd_config * fd_g_config = &conf;

/* Define the standard check routines */
#define CHECK( _val, _assert ){				\
	if (test_verbo > 0) {				\
		fprintf(stderr,				\
			"%s:%-4d: CHECK( " #_assert " == "\
				#_val " )\n",		\
			__FILE__, 			\
			__LINE__);			\
	}{						\
	__typeof__ (_val) __ret = (_assert);		\
	if (__ret != (_val)) {				\
		FAILTEST( "%s:%d: CHECK FAILED : %s == %lx != %lx\n",	\
			__FILE__,			\
			__LINE__,			\
			#_assert,			\
			(unsigned long)__ret,		\
			(unsigned long)(_val));		\
	}}						\
}

static pthread_t timeout_thr;
static void * timeout_catch(void * arg)
{
	int sig;
	sigset_t ss;
	fd_log_threadname ( "timeout catcher" );
	
	sigemptyset(&ss);
	sigaddset(&ss, SIGALRM);
	
	CHECK_POSIX_DO( sigwait(&ss, &sig),  );
	
	FAILTEST("The timeout (" _stringize(TEST_TIMEOUT) " sec) was reached. Use -n or change TEST_TIMEOUT if the test needs more time to execute.");
	
	return NULL;
}
	

GCRY_THREAD_OPTION_PTHREAD_IMPL;

static inline void parse_cmdline(int argc, char * argv[]) {
	int c;
	int no_timeout = 0;
	while ((c = getopt (argc, argv, "dqnf:F:")) != -1) {
		switch (c) {
			case 'd':	/* Increase verbosity of debug messages.  */
				test_verbo++;
				break;
				
			case 'q':	/* Decrease verbosity.  */
				test_verbo--;
				break;
			
			case 'n':	/* Disable the timeout of the test.  */
				no_timeout = 1;
				break;
			
			case 'f':	/* Full debug for the function with this name.  */
				#ifdef DEBUG
				fd_debug_one_function = optarg;
				#else /* DEBUG */
				TRACE_DEBUG(INFO, "Error: must compile with DEBUG support to use this feature");
				#endif /* DEBUG */
				break;
				
			case 'F':	/* Full debug for the functions in file with this name.  */
				#ifdef DEBUG
				fd_debug_one_file = optarg;
				#else /* DEBUG */
				TRACE_DEBUG(INFO, "Error: must compile with DEBUG support to use this feature");
				#endif /* DEBUG */
				break;
				
			default:	/* bug: option not considered.  */
				return;
		}
	}
	fd_g_debug_lvl = (test_verbo > 0) ? (test_verbo - 1) : 0;
	if (!no_timeout) {
		alarm(TEST_TIMEOUT);
		CHECK( 0, pthread_create(&timeout_thr, NULL, timeout_catch, NULL) );
	}
}
 
static inline void test_init(int argc, char * argv[])
{
	memset(fd_g_config, 0, sizeof(struct fd_config));
	
	CHECK( 0, fd_libproto_init() );
	
	fd_log_threadname(basename(__FILE__));
	
	/* Initialize gcrypt and gnutls */
	(void) gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	(void) gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
	CHECK( 0, gnutls_global_init());
	
	/* Initialize the config */
	CHECK( 0, fd_conf_init() );

	/* Add definitions of the base protocol */
	CHECK( 0, fd_dict_base_protocol(fd_g_config->cnf_dict) );
	
	/* Initialize only the sessions */
	CHECK( 0, fd_sess_start()  );
	
	/* Parse the command line */
	parse_cmdline(argc, argv);
	
	return;
}
#define INIT_FD()  test_init(argc, argv);

#endif /* _TESTS_H */
