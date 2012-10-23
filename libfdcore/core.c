/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2012, WIDE Project and NICT								 *
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

#include <gcrypt.h>

/* The static configuration structure */
static struct fd_config g_conf;
struct fd_config * fd_g_config = NULL;

/* gcrypt functions to support posix threads */
#ifndef GNUTLS_VERSION_210
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#endif /* GNUTLS_VERSION_210 */

/* Signal extensions when the framework is completely initialized (they are waiting in fd_core_waitstartcomplete()) */
static int             is_ready = 0;
static pthread_mutex_t is_ready_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  is_ready_cnd = PTHREAD_COND_INITIALIZER;

static int signal_framework_ready(void)
{
	TRACE_ENTRY("");
	CHECK_POSIX( pthread_mutex_lock( &is_ready_mtx ) );
	is_ready = 1;
	CHECK_POSIX( pthread_cond_broadcast( &is_ready_cnd ) );
	CHECK_POSIX( pthread_mutex_unlock( &is_ready_mtx ) );
	return 0;
}

/* Thread that process incoming events on the main queue -- and terminates the framework when requested */
static pthread_t core_runner = (pthread_t)NULL;

/* How the thread is terminated */
enum core_mode {
	CORE_MODE_EVENTS,
	CORE_MODE_IMMEDIATE
};

static void * core_runner_thread(void * arg) 
{
	if (arg && (*(int *)arg == CORE_MODE_IMMEDIATE))
		goto end;
	
	fd_log_threadname("fD Core Runner");
	
	/* Handle events incoming on the main event queue */
	while (1) {
		int code; size_t sz; void * data;
		CHECK_FCT_DO(  fd_event_get(fd_g_config->cnf_main_ev, &code, &sz, &data),  break  );
		switch (code) {
			case FDEV_DUMP_DICT:
				fd_dict_dump(fd_g_config->cnf_dict);
				break;
			
			case FDEV_DUMP_EXT:
				fd_ext_dump();
				break;
			
			case FDEV_DUMP_SERV:
				fd_servers_dump();
				break;
			
			case FDEV_DUMP_QUEUES:
				fd_fifo_dump(0, "Incoming messages", fd_g_incoming, fd_msg_dump_walk);
				fd_fifo_dump(0, "Outgoing messages", fd_g_outgoing, fd_msg_dump_walk);
				fd_fifo_dump(0, "Local messages",    fd_g_local,    fd_msg_dump_walk);
				break;
			
			case FDEV_DUMP_CONFIG:
				fd_conf_dump();
				break;
			
			case FDEV_DUMP_PEERS:
				fd_peer_dump_list(FULL);
				break;
			
			case FDEV_TRIGGER:
				{
					int tv, *p;
					CHECK_PARAMS_DO( sz == sizeof(int), 
						{
							TRACE_DEBUG(NONE, "Internal error: got FDEV_TRIGGER without trigger value!");
							ASSERT(0);
							goto end;
						} );
					p = data;
					tv = *p;
					free(p);
					CHECK_FCT_DO( fd_event_trig_call_cb(tv), goto end );
				}
				break;
			
			case FDEV_TERMINATE:
				goto end;
			
			default:
				TRACE_DEBUG(INFO, "Unexpected event in the main event queue (%d), ignored.\n", code);
		}
	}
	
end:
	TRACE_DEBUG(INFO, FD_PROJECT_BINARY " framework is stopping...");
	fd_log_threadname("fD Core Shutdown");
	
	/* cleanups */
	CHECK_FCT_DO( fd_servers_stop(), /* Stop accepting new connections */ );
	CHECK_FCT_DO( fd_rtdisp_cleanstop(), /* Stop dispatch thread(s) after a clean loop if possible */ );
	CHECK_FCT_DO( fd_peer_fini(), /* Stop all connections */ );
	CHECK_FCT_DO( fd_rtdisp_fini(), /* Stop routing threads and destroy routing queues */ );
	
	CHECK_FCT_DO( fd_ext_term(), /* Cleanup all extensions */ );
	CHECK_FCT_DO( fd_rtdisp_cleanup(), /* destroy remaining handlers */ );
	
	GNUTLS_TRACE( gnutls_global_deinit() );
	
	CHECK_FCT_DO( fd_conf_deinit(), );
	
	CHECK_FCT_DO( fd_event_trig_fini(), );
	
	fd_log_debug(FD_PROJECT_BINARY " framework is terminated.\n");
	
	fd_libproto_fini();
	
	return NULL;
}

/*********************************/
/* Public interfaces */

/* Return a string describing the version of the library */
const char *fd_core_version(void)
{
	return _stringize(FD_PROJECT_VERSION_MAJOR) "." _stringize(FD_PROJECT_VERSION_MINOR) "." _stringize(FD_PROJECT_VERSION_REV);
}

/* Initialize the libfdcore internals. This also initializes libfdproto */
int fd_core_initialize(void)
{
	int ret;
	
	/* Initialize the library -- must come first since it initializes the debug facility */
	ret = fd_libproto_init();
	if (ret != 0) {
		fprintf(stderr, "Unable to initialize libfdproto: %s\n", strerror(ret));
		return ret;
	}
	
	TRACE_DEBUG(INFO, "libfdproto initialized.");
	
	/* Name this thread */
	fd_log_threadname("Main");
	
	/* Initialize gcrypt and gnutls */
	#ifndef GNUTLS_VERSION_210
	GNUTLS_TRACE( (void) gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread) );
	GNUTLS_TRACE( (void) gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0) );
	#endif /* GNUTLS_VERSION_210 */
	CHECK_GNUTLS_DO( gnutls_global_init(), return EINVAL );
	if ( ! gnutls_check_version(GNUTLS_VERSION) ) {
		fprintf(stderr, "The GNUTLS library is too old; found '%s', need '" GNUTLS_VERSION "'\n", gnutls_check_version(NULL));
		return EINVAL;
	} else {
	#ifdef GNUTLS_VERSION_210
		TRACE_DEBUG(INFO, "libgnutls '%s' initialized.", gnutls_check_version(NULL) );
	#else /* GNUTLS_VERSION_210 */
		TRACE_DEBUG(INFO, "libgnutls '%s', libgcrypt '%s', initialized.", gnutls_check_version(NULL), gcry_check_version(NULL) );
	#endif /* GNUTLS_VERSION_210 */
	}
	
	/* Initialize the config with default values */
	memset(&g_conf, 0, sizeof(struct fd_config));
	fd_g_config = &g_conf;
	CHECK_FCT( fd_conf_init() );
	
	/* Initialize the message logging facility */
	fd_msg_log_init(fd_g_config->cnf_dict);

	/* Add definitions of the base protocol */
	CHECK_FCT( fd_dict_base_protocol(fd_g_config->cnf_dict) );
	
	/* Initialize some modules */
	CHECK_FCT( fd_queues_init() );
	CHECK_FCT( fd_msg_init()    );
	CHECK_FCT( fd_sess_start()  );
	CHECK_FCT( fd_p_expi_init() );
	
	/* Next thing is to parse the config, leave this for a different function */
	return 0;
}

/* Parse the freeDiameter.conf configuration file, load the extensions */
int fd_core_parseconf(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	
	/* Conf file */
	if (conffile)
		fd_g_config->cnf_file = conffile; /* otherwise, we use the default name */
	
	CHECK_FCT( fd_conf_parse() );
	
	/* The following module use data from the configuration */
	CHECK_FCT( fd_rtdisp_init() );
	
	/* Now, load all dynamic extensions */
	CHECK_FCT(  fd_ext_load()  );
	
	/* Display configuration */
	fd_conf_dump();
	
	/* Display registered triggers for FDEV_TRIGGER */
	fd_event_trig_dump();
	
	return 0;
}

/* For threads that would need to wait complete start of the framework (ex: in extensions) */
int fd_core_waitstartcomplete(void)
{
	int ret = 0;
	
	TRACE_ENTRY("");
	
	CHECK_POSIX( pthread_mutex_lock( &is_ready_mtx ) );
	pthread_cleanup_push( fd_cleanup_mutex, &is_ready_mtx );
	
	while (!ret && !is_ready) {
		CHECK_POSIX_DO( ret = pthread_cond_wait( &is_ready_cnd, &is_ready_mtx ),  );
	}
	
	pthread_cleanup_pop( 0 );
	CHECK_POSIX( pthread_mutex_unlock( &is_ready_mtx ) );
	
	return ret;
}

/* Start the server & client threads */
int fd_core_start(void)
{
	/* Start server threads */ 
	CHECK_FCT( fd_servers_start() );
	
	/* Start the peer state machines */
	CHECK_FCT( fd_psm_start() );
	
	/* Start the core runner thread that handles main events (until shutdown) */
	CHECK_POSIX( pthread_create(&core_runner, NULL, core_runner_thread, NULL) );

	/* Unlock threads waiting into fd_core_waitstartcomplete */
	CHECK_FCT( signal_framework_ready() );
	
	/* Ok, everything is running now... */
	return 0;
}


/* Initialize shutdown of the framework. This is not blocking. */
int fd_core_shutdown(void)
{
	if (core_runner != (pthread_t)NULL) {
		/* Signal the framework to terminate */
		CHECK_FCT( fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL) );
	} else {
		/* The framework was maybe not fully initialized (ex: tests) */
		enum core_mode arg = CORE_MODE_IMMEDIATE;
		(void) core_runner_thread(&arg);
	}
	
	return 0;
}


/* Wait for the shutdown to be complete -- this must always be called after fd_core_shutdown to relaim some resources. */
int fd_core_wait_shutdown_complete(void)
{
	int ret;
	void * th_ret = NULL;
	
	if (core_runner != (pthread_t)NULL) {
		/* Just wait for core_runner_thread to complete and return gracefully */
		ret = pthread_join(core_runner, &th_ret);
		if (ret != 0) {
			fprintf(stderr, "Unable to wait for main framework thread termination: %s\n", strerror(ret));
			return ret;
		}
	}
	
	return 0;
}




