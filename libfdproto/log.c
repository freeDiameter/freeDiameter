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

#include "fdproto-internal.h"

#include <stdarg.h>

pthread_mutex_t fd_log_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_key_t	fd_log_thname;
int fd_g_debug_lvl = INFO;

/* These may be used to pass specific debug requests via the command-line parameters */
char * fd_debug_one_function = NULL;
char * fd_debug_one_file = NULL;

/* Useless function, only to ease setting up a breakpoint in gdb (break fd_breakhere) -- use TRACE_HERE */
int fd_breaks = 0;
int fd_breakhere(void) { return ++fd_breaks; }

/* Allow passing of the log and debug information from base stack to extensions */
void (*fd_external_logger)( const char * format, va_list *args ) = NULL;

/* Register an dexternal call back for tracing and debug */
int fd_log_handler_register( void (*logger)(const char * format, va_list *args))
{
        CHECK_PARAMS( logger );

        if ( fd_external_logger != NULL )
        {
               return EALREADY; /* only one registeration allowed */
        }
        else
        {
               fd_external_logger = logger;
        }
        return 0;
}

/* Implement a simple reset function here */
int fd_log_handler_unregister ( void )
{
        fd_external_logger = NULL;
        return 0; /* Successfull in all cases. */
}

static void fd_cleanup_mutex_silent( void * mutex )
{
	(void)pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

/* Log a debug message */
void fd_log_debug_fstr ( FILE * fstr, const char * format, ... )
{
	va_list ap;
	
	(void)pthread_mutex_lock(&fd_log_lock);
	
	pthread_cleanup_push(fd_cleanup_mutex_silent, &fd_log_lock);
	
	va_start(ap, format);
        if ( fd_external_logger != NULL )
        {
               fd_external_logger( format, &ap );
        }
        else
        {
               vfprintf( fstr ?: stdout, format, ap);
               fflush(fstr ?: stdout);
        }
	va_end(ap);

	pthread_cleanup_pop(0);
	
	(void)pthread_mutex_unlock(&fd_log_lock);
}

/* Function to set the thread's friendly name */
void fd_log_threadname ( char * name )
{
	void * val = NULL;
	
	TRACE_ENTRY("%p(%s)", name, name?:"/");
	
	/* First, check if a value is already assigned to the current thread */
	val = pthread_getspecific(fd_log_thname);
	if (TRACE_BOOL(ANNOYING)) {
		if (val) {
			fd_log_debug("(Thread '%s' renamed to '%s')\n", (char *)val, name);
		} else {
			fd_log_debug("(Thread %p named '%s')\n", pthread_self(), name?:"(nil)");
		}
	}
	if (val != NULL) {
		free(val);
	}
	
	/* Now create the new string */
	if (name == NULL) {
		CHECK_POSIX_DO( pthread_setspecific(fd_log_thname, NULL), /* continue */);
		return;
	}
	
	CHECK_MALLOC_DO( val = strdup(name), return );
	
	CHECK_POSIX_DO( pthread_setspecific(fd_log_thname, val), /* continue */);
	return;
}

/* Write time into a buffer */
char * fd_log_time ( struct timespec * ts, char * buf, size_t len )
{
	int ret;
	size_t offset = 0;
	struct timespec tp;
	struct tm tm;
	
	/* Get current time */
	if (!ts) {
		ret = clock_gettime(CLOCK_REALTIME, &tp);
		if (ret != 0) {
			snprintf(buf, len, "%s", strerror(ret));
			return buf;
		}
		ts = &tp;
	}
	
	offset += strftime(buf + offset, len - offset, "%D,%T", localtime_r( &ts->tv_sec , &tm ));
	offset += snprintf(buf + offset, len - offset, ".%6.6ld", ts->tv_nsec / 1000);

	return buf;
}
