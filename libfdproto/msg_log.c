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

/* messages logging facility */
#include "fdproto-internal.h"
#include <stdarg.h>

/* Configuration of this module */
static struct {
	struct {
		enum fd_msg_log_method 	meth;
		const char * 		metharg;
	} causes[FD_MSG_LOG_MAX + 1];
	pthread_mutex_t lock;
	int		init;
	struct dictionary *dict;	
} ml_conf = { .lock = PTHREAD_MUTEX_INITIALIZER, .init = 0 };


/* Initialize the module, with a dictionary to use to better parse the messages */
void fd_msg_log_init(struct dictionary *dict) 
{
	memset(&ml_conf.causes, 0, sizeof(ml_conf.causes));
	ml_conf.init = 1;
	ml_conf.dict = dict;
}

/* Set a configuration property */
int fd_msg_log_config(enum fd_msg_log_cause cause, enum fd_msg_log_method method, const char * arg)
{
	/* Check the parameters are valid */
	TRACE_ENTRY("%d %d %p", cause, method, arg);
	CHECK_PARAMS( (cause >= 0) && (cause <= FD_MSG_LOG_MAX) );
	CHECK_PARAMS( (method >= FD_MSG_LOGTO_DEBUGONLY) && (method <= FD_MSG_LOGTO_DIR) );
	CHECK_PARAMS( (method == FD_MSG_LOGTO_DEBUGONLY) || (arg != NULL) );
	
	/* Lock the configuration */
	CHECK_POSIX( pthread_mutex_lock(&ml_conf.lock) );
	if (!ml_conf.init) {
		ASSERT(0);
	}
	
	/* Now set the parameter */
	ml_conf.causes[cause].meth = method;
	ml_conf.causes[cause].metharg = arg;
	
	if (method) {
		TRACE_DEBUG(INFO, "Logging %s messages set to %s '%s'", 
			(cause == FD_MSG_LOG_DROPPED) ? "DROPPED" :
				(cause == FD_MSG_LOG_RECEIVED) ? "RECEIVED" :
					(cause == FD_MSG_LOG_SENT) ? "SENT" :
						"???",
			(method == FD_MSG_LOGTO_FILE) ? "file" :
				(method == FD_MSG_LOGTO_DIR) ? "directory" :
					"???",
			arg);
	}
	
	CHECK_POSIX( pthread_mutex_unlock(&ml_conf.lock) );
	
	/* Done */
	return 0;
}

/* Do not log anything within this one, since log lock is held */
static void fd_cleanup_mutex_silent( void * mutex )
{
	(void)pthread_mutex_unlock((pthread_mutex_t *)mutex);
}

/* Really log the message */
void fd_msg_log( enum fd_msg_log_cause cause, struct msg * msg, const char * prefix_format, ... )
{
	va_list ap;
	enum fd_msg_log_method 	meth;
	const char * 		metharg;
	FILE * fstr;
	
	char buftime[256];
	size_t offset = 0;
	struct timespec ts;
	struct tm tm;
	
	TRACE_ENTRY("%d %p %p", cause, msg, prefix_format);
	CHECK_PARAMS_DO( (cause >= 0) && (cause <= FD_MSG_LOG_MAX),
	{
		TRACE_DEBUG(INFO, "Invalid cause received (%d)! Message was:", cause);
		fd_msg_dump_walk(INFO, msg);
	} );
	
	/* First retrieve the config for this message */
	CHECK_POSIX_DO( pthread_mutex_lock(&ml_conf.lock), );
	if (!ml_conf.init) {
		ASSERT(0);
	}
	meth    = ml_conf.causes[cause].meth;
	metharg = ml_conf.causes[cause].metharg;
	CHECK_POSIX_DO( pthread_mutex_unlock(&ml_conf.lock), );
	
	/* Do not log if the level is not at least INFO */
	if ((meth == FD_MSG_LOGTO_DEBUGONLY) && (fd_g_debug_lvl < INFO)) {
		return;
	}
	
	/* Get current time */
	CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &ts), /* continue */);
	offset += strftime(buftime + offset, sizeof(buftime) - offset, "%D,%T", localtime_r( &ts.tv_sec , &tm ));
	offset += snprintf(buftime + offset, sizeof(buftime) - offset, ".%6.6ld", ts.tv_nsec / 1000);
	

	/* Okay, now we will create the file descriptor */
	switch (meth) {
		case FD_MSG_LOGTO_DEBUGONLY:
			fstr = fd_g_debug_fstr;
			break;
			
		case FD_MSG_LOGTO_FILE:
			TODO("Log to arg file");
			TODO("Log a note to debug stream");
			TRACE_DEBUG(INFO, "%s", metharg);
			break;
		case FD_MSG_LOGTO_DIR:
			TODO("Log to arg directory in a new file");
			TODO("Log a note to debug stream");
			break;
	}
	
	/* For file methods, let's parse the message so it looks better */
	if (((meth != FD_MSG_LOGTO_DEBUGONLY) || (fd_g_debug_lvl > FULL)) && ml_conf.dict) {
		CHECK_FCT_DO( fd_msg_parse_dict( msg, ml_conf.dict, NULL ), );
	}
	
	/* Then dump the prefix message to this stream, & to debug stream */
	(void)pthread_mutex_lock(&fd_log_lock);
	pthread_cleanup_push(fd_cleanup_mutex_silent, &fd_log_lock);
	va_start(ap, prefix_format);
	vfprintf( fstr, prefix_format, ap);
	va_end(ap);
	fflush(fstr);
	pthread_cleanup_pop(0);
	(void)pthread_mutex_unlock(&fd_log_lock);
	
	fd_log_debug_fstr(fstr, "\nLogged: %s\n\n", buftime);
	
	/* And now the message itself */
	if ((meth == FD_MSG_LOGTO_DEBUGONLY) && (fd_g_debug_lvl <= INFO)) {
		/* dump only the header */
		fd_msg_dump_fstr_one(msg, fstr);
	} else {
		/* dump the full content */
		fd_msg_dump_fstr(msg, fstr);
	}
	
	/* And finally close the stream if needed */
	switch (meth) {
		case FD_MSG_LOGTO_DEBUGONLY:
			break;
			
		case FD_MSG_LOGTO_FILE:
			TODO("close?");
			break;
		case FD_MSG_LOGTO_DIR:
			TODO("close?");
			break;
	}
}

