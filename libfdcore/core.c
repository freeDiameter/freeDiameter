/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2023, WIDE Project and NICT								 *
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

GCC_DIAG_OFF("-Wdeprecated-declarations")
#include <gcrypt.h>
GCC_DIAG_ON("-Wdeprecated-declarations")

/* The static configuration structure */
static struct fd_config g_conf;
struct fd_config * fd_g_config = NULL;

/* gcrypt functions to support posix threads */

/* Thread that process incoming events on the main queue -- and terminates the framework when requested */
static pthread_t core_runner = (pthread_t)NULL;

/* Signal extensions when the framework is completely initialized (they are waiting in fd_core_waitstartcomplete()) */
static enum core_state {
	CORE_NOT_INIT,	/* initial state */
	CORE_LIBS_INIT,	/* fd_core_initialize was called */
	CORE_CONF_READY,/* Configuration was parsed, extensions are loaded */
	CORE_RUNNING,	/* Servers and clients are started, core_runner thread is running */
	CORE_SHUTDOWN,	/* The framework is terminating all objects */
	CORE_TERM	/* Shutdown complete. */	
} core_state = CORE_NOT_INIT;
static pthread_mutex_t core_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  core_cnd = PTHREAD_COND_INITIALIZER;

static enum core_state core_state_get(void)
{
	enum core_state cur_state;
	CHECK_POSIX_DO( pthread_mutex_lock( &core_mtx ), );
	cur_state = core_state;
	CHECK_POSIX_DO( pthread_mutex_unlock( &core_mtx ), );
	return cur_state;
}

static void core_state_set(enum core_state newstate)
{
	CHECK_POSIX_DO( pthread_mutex_lock( &core_mtx ), );
	LOG_D("Core state: %d -> %d", core_state, newstate);
	core_state = newstate;
	CHECK_POSIX_DO( pthread_cond_broadcast( &core_cnd ), );
	CHECK_POSIX_DO( pthread_mutex_unlock( &core_mtx ), );
}

static int core_state_wait(enum core_state waitstate)
{
	int ret = 0;
	CHECK_POSIX( pthread_mutex_lock( &core_mtx ));
	pthread_cleanup_push( fd_cleanup_mutex, &core_mtx );
	while (waitstate > core_state) {
		CHECK_POSIX_DO(ret = pthread_cond_wait(&core_cnd, &core_mtx), break);
	}
	pthread_cleanup_pop( 0 );
	CHECK_POSIX( pthread_mutex_unlock( &core_mtx ));
	return ret;
}

#define SHUTDOWN_TIMEOUT 15
#define SHUTDOWN_DRAIN_TIMEOUT 10
#define SHUTDOWN_QUIET_TIME 3
#define SHUTDOWN_FINAL_DRAIN 2

static pthread_t watchdog = (pthread_t)NULL;
static struct timespec watchdog_deadline;
static char watchdog_message[128];
static int shutdown_requested;

static void *shutdown_timeout_report(void *arg)
{
	const char *message = arg;
	(void)write(STDERR_FILENO, message, strlen(message));
	return NULL;
}

static void *shutdown_watchdog(void *arg)
{
	struct timespec deadline = *(struct timespec *)arg;
	struct timespec now;
	const char *message;
	pthread_t reporter;
	static const char clock_error[] =
		"ERROR: Cannot read shutdown watchdog clock; forcing process exit\n";

	for (;;) {
		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
			message = clock_error;
			break;
		}
		if (!TS_IS_INFERIOR(&now, &deadline)) {
			message = watchdog_message;
			break;
		}
		usleep(100000);
	}
	/* Neither logging locks nor a blocked stderr may prevent forced termination. */
	if (pthread_create(&reporter, NULL, shutdown_timeout_report, (void *)message) == 0)
		usleep(100000);
	_exit(EXIT_FAILURE);
}

static int shutdown_drain(const struct timespec *deadline, int peers, int quiet)
{
	struct timespec now, idle_since = { 0, 0 };
	unsigned long generation, previous = 0, pending, after, after_pending;
	long exchanges;

	for (;;) {
		CHECK_SYS(clock_gettime(CLOCK_MONOTONIC, &now));
		if (!TS_IS_INFERIOR(&now, deadline))
			return ETIMEDOUT;

		fd_work_snapshot(&generation, &pending);
		exchanges = 0;
		if (peers)
			CHECK_FCT(fd_peer_pending(&exchanges));
		fd_work_snapshot(&after, &after_pending);

		if (!pending && !after_pending && !exchanges && generation == after) {
			if (!idle_since.tv_sec || previous != generation) {
				idle_since = now;
				previous = generation;
			}
			if (now.tv_sec - idle_since.tv_sec >= quiet &&
			    (now.tv_sec - idle_since.tv_sec > quiet || now.tv_nsec >= idle_since.tv_nsec)) {
				if (peers || fd_work_stop_if_idle(generation))
					return 0;
			}
		} else {
			idle_since.tv_sec = 0;
		}
		usleep(100000);
	}
}

static void core_shutdown(void)
{
	struct timespec deadline, drain_deadline;
	int running = core_state_get() == CORE_SHUTDOWN;
	int ret;

	LOG_N( FD_PROJECT_BINARY " framework is stopping...");
	fd_log_threadname("fD Core Shutdown");

	fd_work_quiesce();
	CHECK_FCT_DO( fd_servers_stop(), /* Stop accepting new connections */ );
	CHECK_FCT_DO(fd_p_expi_fini(), );
	if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
		LOG_E("Cannot read shutdown clock: %s", strerror(errno));
		memset(&deadline, 0, sizeof(deadline));
	}
	drain_deadline = deadline;
	deadline.tv_sec += SHUTDOWN_TIMEOUT;
	drain_deadline.tv_sec += SHUTDOWN_DRAIN_TIMEOUT;
	if (running) {
		ret = shutdown_drain(&drain_deadline, 1, SHUTDOWN_QUIET_TIME);
		if (ret)
			LOG_E("Shutdown drain did not complete: %s", strerror(ret));
	}
	CHECK_FCT_DO(fd_peer_stop(&deadline), );
	if (clock_gettime(CLOCK_MONOTONIC, &drain_deadline) != 0) {
		ret = errno;
	} else {
		drain_deadline.tv_sec += SHUTDOWN_FINAL_DRAIN;
		ret = shutdown_drain(&drain_deadline, 0, 0);
	}
	if (ret) {
		LOG_E("Final shutdown drain did not complete: %s", strerror(ret));
		fd_work_stop();
	}
	CHECK_FCT_DO(fd_rtdisp_cleanstop(), );
	CHECK_FCT_DO(fd_rtdisp_fini(), );
	CHECK_FCT_DO(fd_peer_fini(), );
	
	CHECK_FCT_DO( fd_ext_term(), /* Cleanup all extensions */ );
	CHECK_FCT_DO( fd_rtdisp_cleanup(), /* destroy remaining handlers */ );
	
	GNUTLS_TRACE( gnutls_global_deinit() );
	
	CHECK_FCT_DO( fd_conf_deinit(), );
	
	CHECK_FCT_DO( fd_event_trig_fini(), );
	
	fd_log_debug(FD_PROJECT_BINARY " framework is terminated.");
	
	fd_libproto_fini();
	CHECK_FCT_DO(fd_thr_term(&watchdog), _exit(EXIT_FAILURE));
	
}	


static void * core_runner_thread(void * arg) 
{
	fd_log_threadname("fD Core Runner");
	
	core_state_wait(CORE_RUNNING);
	
	/* Handle events incoming on the main event queue */
	while (1) {
		int code; size_t sz; void * data;
		CHECK_FCT_DO(  fd_event_get(fd_g_config->cnf_main_ev, &code, &sz, &data),  break  );
		switch (code) {
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
			
			case FDEV_TERMINATE_INT:
				goto end;
			
			default:
				TRACE_DEBUG(INFO, "Unexpected event in the main event queue (%d), ignored.", code);
		}
	}
	
end:
	core_shutdown();
	
	return NULL;
}

/*********************************/
/* Public interfaces */

/* Initialize the libfdcore internals. This also initializes libfdproto */
int fd_core_initialize(void)
{
	int ret;
	
	if (core_state_get() != CORE_NOT_INIT) {
		fprintf(stderr, "fd_core_initialize() called more than once!\n");
		return EINVAL;
	}
	
	/* Initialize the library -- must come first since it initializes the debug facility */
	ret = fd_libproto_init();
	if (ret != 0) {
		fprintf(stderr, "Unable to initialize libfdproto: %s\n", strerror(ret));
		return ret;
	}
	
	/* Name this thread */
	fd_log_threadname("Main");
	
	LOG_N("libfdproto '%s' initialized.", fd_libproto_version);
	
	/* Initialize gcrypt and gnutls */
	CHECK_GNUTLS_DO( gnutls_global_init(), return EINVAL );
	if ( ! gnutls_check_version(GNUTLS_VERSION) ) {
		TRACE_ERROR( "The GNUTLS library is too old; found '%s', need '" GNUTLS_VERSION "'", gnutls_check_version(NULL));
		return EINVAL;
	} else {
		TRACE_DEBUG(INFO, "libgnutls '%s' initialized.", gnutls_check_version(NULL) );
	}
	
	/* Initialize the config with default values */
	memset(&g_conf, 0, sizeof(struct fd_config));
	fd_g_config = &g_conf;
	CHECK_FCT( fd_conf_init() );
	
	/* Add definitions of the base protocol */
	CHECK_FCT( fd_dict_base_protocol(fd_g_config->cnf_dict) );
	
	/* Initialize some modules */
	CHECK_FCT( fd_hooks_init()  );
	CHECK_FCT( fd_queues_init() );
	CHECK_FCT( fd_sess_start()  );
	CHECK_FCT( fd_p_expi_init() );
	
	core_state_set(CORE_LIBS_INIT);
	
	LOG_N("libfdcore '%s' initialized.", fd_core_version);
	
	
	/* Next thing is to parse the config, leave this for a different function */
	return 0;
}

static pthread_mutex_t core_lock = PTHREAD_MUTEX_INITIALIZER;

/* Parse the freeDiameter.conf configuration file, load the extensions */
static int fd_core_parseconf_int(const char * conffile)
{
	char * buf = NULL, *b;
	size_t len = 0, offset=0;
	
	
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
	b = fd_conf_dump(&buf, &len, NULL);
	LOG_SPLIT(FD_LOG_NOTICE, NULL, b ?: "<Error during configuration dump...>", NULL);
	
	/* Display extensions status */
	b = fd_ext_dump(&buf, &len, NULL);
	LOG_SPLIT(FD_LOG_NOTICE, "Loaded extensions: ", b?:"<Error during extensions dump...>", NULL);
	
	/* Display registered triggers for FDEV_TRIGGER */
	b = fd_event_trig_dump(&buf, &len, &offset);
	if (!b || offset) {
		LOG_N("%s", b ?: "Error during triggers dump...");
	}
	
	free(buf);	
		
	/* Since some extensions might have modified the definitions from the dict_base_protocol, we only load the objects now */
	CHECK_FCT( fd_msg_init()    );
	
	/* Ok, ready for next step */
	core_state_set(CORE_CONF_READY);
	
	return 0;
}

int fd_core_parseconf(const char * conffile)
{
	int ret;
	CHECK_POSIX( pthread_mutex_lock(&core_lock) );
	ret = fd_core_parseconf_int(conffile);
	CHECK_POSIX( pthread_mutex_unlock(&core_lock) );
	return ret;
}


/* For threads that would need to wait complete start of the framework (ex: in extensions) */
int fd_core_waitstartcomplete(void)
{
	TRACE_ENTRY("");
	
	return core_state_wait(CORE_RUNNING);
}

/* Start the server & client threads */
static int fd_core_start_int(void)
{
	/* Start server threads */ 
	CHECK_FCT( fd_servers_start() );
	
	/* Start the peer state machines */
	CHECK_FCT( fd_psm_start() );
	
	/* Start the core runner thread that handles main events (until shutdown) */
	CHECK_POSIX( pthread_create(&core_runner, NULL, core_runner_thread, NULL) );
	
	/* Unlock threads waiting into fd_core_waitstartcomplete */
	core_state_set(CORE_RUNNING);
	
	/* Ok, everything is running now... */
	return 0;
}

int fd_core_start(void)
{
	int ret;
	CHECK_FCT( fd_queues_init_after_conf() );

	CHECK_POSIX( pthread_mutex_lock(&core_lock) );
	ret = fd_core_start_int();
	CHECK_POSIX( pthread_mutex_unlock(&core_lock) );
	return ret;
}

/* Initialize shutdown of the framework. This is not blocking. */
int fd_core_shutdown_timeout(int timeout)
{
	enum core_state cur_state;
	int ret = 0;

	CHECK_PARAMS(timeout >= -1);
	CHECK_POSIX(pthread_mutex_lock(&core_mtx));
	cur_state = core_state;
	if (shutdown_requested || cur_state >= CORE_SHUTDOWN) {
		CHECK_POSIX(pthread_mutex_unlock(&core_mtx));
		return 0;
	}
	if (timeout >= 0) {
		if (clock_gettime(CLOCK_MONOTONIC, &watchdog_deadline) != 0) {
			ret = errno;
		} else {
			watchdog_deadline.tv_sec += timeout;
			snprintf(watchdog_message, sizeof(watchdog_message),
				 "ERROR: freeDiameter shutdown exceeded %d seconds; forcing process exit\n",
				 timeout);
			ret = pthread_create(&watchdog, NULL, shutdown_watchdog, &watchdog_deadline);
		}
	}
	if (!ret)
		shutdown_requested = 1;
	CHECK_POSIX(pthread_mutex_unlock(&core_mtx));
	CHECK_FCT(ret);
	
	LOG_N("Initiating freeDiameter shutdown sequence (%d)", cur_state);
		
	if (cur_state < CORE_RUNNING) {
		/* Calling application must make sure the initialization is not ongoing in a separate thread... */
		if (pthread_mutex_lock(&core_lock) != 0) {
			/* This function must not be called asynchronously from fd_core_parseconf / fd_core_start ! Please review your main app design */
			ASSERT(0);
			return EINVAL;
		}
		core_shutdown();
		core_state_set(CORE_TERM);
		pthread_mutex_unlock(&core_lock);
	} else if (cur_state == CORE_RUNNING) {
		fd_work_quiesce();
		core_state_set(CORE_SHUTDOWN);
		CHECK_FCT( fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE_INT, 0, NULL) );
	}
	
	/* Other case, the framework is already shutting down */
	
	return 0;
}

int fd_core_shutdown(void)
{
	return fd_core_shutdown_timeout(-1);
}


/* Wait for the shutdown to be complete -- this must be called after fd_core_shutdown to reclaim some resources. */
int fd_core_wait_shutdown_complete(void)
{
	enum core_state cur_state = core_state_get();
	void * th_ret = NULL;
	
	CHECK_FCT(core_state_wait(CORE_SHUTDOWN));
	
	if (cur_state == CORE_TERM)
		return 0;
	
	/* Just wait for core_runner_thread to complete and return gracefully */
	CHECK_POSIX(pthread_join(core_runner, &th_ret));

	core_state_set(CORE_TERM);
	fd_log_threadname(NULL);
	return 0;
}

/* Forcibly shut down any Diameter connections */
int fd_connections_shutdown(void)
{
	LOG_N( FD_PROJECT_BINARY " Closing Diameter connections");
	return fd_peer_fini_force();
}
