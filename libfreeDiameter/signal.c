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

#include "libfD.h"

#include <signal.h>

/* A structure to hold the registered signal handlers */
struct sig_hdl {
	struct fd_list	chain;		/* Link in the sig_hdl_list ordered by signal */
	int 		signal;		/* The signal this handler is registered for */
	void 		(*sig_hdl)(int);/* The callback to call when the signal is caught */
	pthread_t	sig_thr;	/* The thread that receives the signal */
	char 		*modname;	/* The name of the module who registered the signal (for debug) */
};

/* The list of sig_hdl, and the mutex to protect it */
static struct fd_list	sig_hdl_list = FD_LIST_INITIALIZER(sig_hdl_list);
static pthread_mutex_t sig_hdl_lock  = PTHREAD_MUTEX_INITIALIZER;

/* Detached thread attribute */
static pthread_attr_t detached;

/* Note if the module was initialized */
static int sig_initialized = 0;

/* signals short names list */
static int abbrevs_init(void);

/* Initialize the support for signals */
int fd_sig_init(void)
{
	sigset_t sig_all;
	
	TRACE_ENTRY("");
	
	if (sig_initialized)
		return 0;
	
	/* Initialize the list of abbreviations */
	CHECK_FCT(abbrevs_init());
	
	/* Block all signals from the current thread and all its future children, so that signals are delivered only to our sig_hdl->sig_thr */
	sigfillset(&sig_all);
	CHECK_POSIX(  pthread_sigmask(SIG_BLOCK, &sig_all, NULL)  );
	
	/* Initialize the detached attribute */
	CHECK_POSIX( pthread_attr_init(&detached) );
	CHECK_POSIX( pthread_attr_setdetachstate(&detached, PTHREAD_CREATE_DETACHED) );
	
	sig_initialized++;
	
	/* That's all we have to do here ... */
	return 0;
}


/* This thread (created detached) does call the signal handler */
static void * signal_caller(void * arg)
{
	struct sig_hdl * me = arg;
	char buf[40];
	
	/* Name this thread */
	snprintf(buf, sizeof(buf), "cb %d:%s:%s", me->signal, fd_sig_abbrev(me->signal), me->modname);
	fd_log_threadname ( buf );
	
	TRACE_ENTRY("%p", me);
	
	/* Call the signal handler */
	(*me->sig_hdl)(me->signal);
	
	TRACE_DEBUG(CALL, "Callback completed");
	
	/* Done! */
	return NULL;
}

/* This thread "really" receives the signal and starts the signal_caller thread */
static void * signal_catcher(void * arg)
{
	struct sig_hdl * me = arg;
	sigset_t ss;
	char buf[40];
	
	ASSERT(arg);
	
	/* Name this thread */
	snprintf(buf, sizeof(buf), "catch %d:%s:%s", me->signal, fd_sig_abbrev(me->signal), me->modname);
	fd_log_threadname ( buf );
	
	TRACE_ENTRY("%p", me);
	
	sigemptyset(&ss);
	sigaddset(&ss, me->signal);
	
	/* Now loop on the reception of the signal */
	while (1) {
		int sig;
		pthread_t th;
		
		/* Wait to receive the next signal */
		CHECK_POSIX_DO( sigwait(&ss, &sig), break );
		
		TRACE_DEBUG(FULL, "Signal %d caught", sig);
		
		/* Create the thread that will call the handler */
		CHECK_POSIX_DO( pthread_create( &th, &detached, signal_caller, arg ), break );
	}
	
	/* An error occurred... What should we do ? */
	TRACE_DEBUG(INFO, "!!! ERROR !!! The signal catcher for %d:%s:%s is terminating!", me->signal, fd_sig_abbrev(me->signal), me->modname);
	
	/* Better way to handle this ? */
	ASSERT(0);
	return NULL;
}

/* Register a new callback to be called on reception of a given signal (it receives the signal as parameter) */
/* EALREADY will be returned if there is already a callback registered on this signal */
/* NOTE: the signal handler will be called from a new detached thread */
int fd_sig_register(int signal, char * modname, void (*handler)(int signal))
{
	struct sig_hdl * new;
	struct fd_list * next = NULL;
	int matched = 0;
	
	TRACE_ENTRY("%d %p %p", signal, modname, handler);
	CHECK_PARAMS( signal && modname && handler && sig_initialized );
	
	/* Alloc all memory before taking the lock  */
	CHECK_MALLOC( new = malloc(sizeof(struct sig_hdl)) );
	memset(new, 0, sizeof(struct sig_hdl));
	fd_list_init(&new->chain, new);
	new->signal = signal;
	new->sig_hdl = handler;
	CHECK_MALLOC_DO( new->modname = strdup(modname), { free(new); return ENOMEM; } );
	
	/* Search in the list if we already have a handler for this signal */
	CHECK_POSIX(pthread_mutex_lock(&sig_hdl_lock));
	for (next = sig_hdl_list.next; next != &sig_hdl_list; next = next->next) {
		struct sig_hdl * nh = (struct sig_hdl *)next;
		
		if (nh->signal < signal)
			continue;
		if (nh->signal == signal) {
			matched = 1;
			TRACE_DEBUG(INFO, "Signal %d (%s) is already registered by module '%s'", signal, fd_sig_abbrev(signal), nh->modname);
		}
		break;
	}
	if (!matched)
		/* Insert the new element before the next handle */
		fd_list_insert_before(next, &new->chain);
		
	CHECK_POSIX(pthread_mutex_unlock(&sig_hdl_lock));
	
	if (matched)
		return EALREADY; /* Already registered... */
	
	/* OK, now start the thread */
	CHECK_POSIX( pthread_create( &new->sig_thr, NULL, signal_catcher, new ) );
	
	/* All good */
	return 0;
}

/* Dump list of handlers */
void fd_sig_dump(int level, int indent)
{
	struct fd_list * li = NULL;
	
	if (!TRACE_BOOL(level))
		return;
	
	
	CHECK_POSIX_DO(pthread_mutex_lock(&sig_hdl_lock), /* continue */);
	if (!FD_IS_LIST_EMPTY(&sig_hdl_list))
		fd_log_debug("%*sList of registered signal handlers:\n", indent, "");
	
	for (li = sig_hdl_list.next; li != &sig_hdl_list; li = li->next) {
		struct sig_hdl * h = (struct sig_hdl *)li;
		
		fd_log_debug("%*s  - sig %*d (%s) => %p in module %s\n", indent, "", 2, h->signal, fd_sig_abbrev(h->signal), h->sig_hdl, h->modname);
	}
	CHECK_POSIX_DO(pthread_mutex_unlock(&sig_hdl_lock), /* continue */);
	
	return;
}


/* Remove a callback */
int fd_sig_unregister(int signal)
{
	struct fd_list * li = NULL;
	struct sig_hdl * h = NULL;
	
	TRACE_ENTRY("%d", signal);
	CHECK_PARAMS( signal && sig_initialized );
	
	/* Search in the list if we already have a handler for this signal */
	CHECK_POSIX(pthread_mutex_lock(&sig_hdl_lock));
	for (li = sig_hdl_list.next; li != &sig_hdl_list; li = li->next) {
		struct sig_hdl * nh = (struct sig_hdl *)li;
		
		if (nh->signal < signal)
			continue;
		if (nh->signal == signal) {
			h = nh;
			fd_list_unlink(&h->chain);
		}
		break;
	}
	CHECK_POSIX(pthread_mutex_unlock(&sig_hdl_lock));
	
	if (!h)
		return ENOENT;
	
	/* Stop the catcher thread */
	CHECK_FCT( fd_thr_term(&h->sig_thr) );

	/* Free */
	free(h->modname);
	free(h);
	
	/* All good */
	return 0;
}

/* Terminate the signal handler thread -- must be called if fd_sig_init was called */
void fd_sig_fini(void)
{
	if (!sig_initialized)
		return;
	
	/* For each registered handler, stop the thread, free the associated memory */
	CHECK_POSIX_DO(pthread_mutex_lock(&sig_hdl_lock), /* continue */);
	while (!FD_IS_LIST_EMPTY(&sig_hdl_list)) {
		struct sig_hdl * h = (struct sig_hdl *)sig_hdl_list.next;
		
		/* Unlink */
		fd_list_unlink(&h->chain);
		
		/* Stop the catcher thread */
		CHECK_FCT_DO( fd_thr_term(&h->sig_thr), /* continue */ );
		
		/* Free */
		free(h->modname);
		free(h);
	}
	CHECK_POSIX_DO(pthread_mutex_unlock(&sig_hdl_lock), /* continue */);

	/* Destroy the thread attribute */
	CHECK_POSIX_DO( pthread_attr_destroy(&detached), /* continue */ );
	return;
}


/**************************************************************************************/

static char **abbrevs;
static size_t abbrevs_nr = 0;

/* Initialize the array of signals */
static int abbrevs_init(void)
{
	int i;
	
	#define SIGNAL_MAX_LIMIT	100 /* Do not save signals with value > this */

	/* The raw list of signals in the system -- might be incomplete, add any signal you need */
	struct sig_abb_raw {
		int	 sig;
		char 	*name;
	} abbrevs_raw[] = {
		{ 0, "[unknown signal]" }
	#define RAW_SIGNAL( _sig ) ,{ _sig, #_sig }
	#ifdef SIGBUS
		RAW_SIGNAL( SIGBUS )
	#endif /* SIGBUS */
	#ifdef SIGTTIN
		RAW_SIGNAL( SIGTTIN )
	#endif /* SIGTTIN */
	#ifdef SIGTTOU
		RAW_SIGNAL( SIGTTOU )
	#endif /* SIGTTOU */
	#ifdef SIGPROF
		RAW_SIGNAL( SIGPROF )
	#endif /* SIGPROF */
	#ifdef SIGALRM
		RAW_SIGNAL( SIGALRM )
	#endif /* SIGALRM */
	#ifdef SIGFPE
		RAW_SIGNAL( SIGFPE )
	#endif /* SIGFPE */
	#ifdef SIGSTKFLT
		RAW_SIGNAL( SIGSTKFLT )
	#endif /* SIGSTKFLT */
	#ifdef SIGSTKSZ
		RAW_SIGNAL( SIGSTKSZ )
	#endif /* SIGSTKSZ */
	#ifdef SIGUSR1
		RAW_SIGNAL( SIGUSR1 )
	#endif /* SIGUSR1 */
	#ifdef SIGURG
		RAW_SIGNAL( SIGURG )
	#endif /* SIGURG */
	#ifdef SIGIO
		RAW_SIGNAL( SIGIO )
	#endif /* SIGIO */
	#ifdef SIGQUIT
		RAW_SIGNAL( SIGQUIT )
	#endif /* SIGQUIT */
	#ifdef SIGABRT
		RAW_SIGNAL( SIGABRT )
	#endif /* SIGABRT */
	#ifdef SIGTRAP
		RAW_SIGNAL( SIGTRAP )
	#endif /* SIGTRAP */
	#ifdef SIGVTALRM
		RAW_SIGNAL( SIGVTALRM )
	#endif /* SIGVTALRM */
	#ifdef SIGSEGV
		RAW_SIGNAL( SIGSEGV )
	#endif /* SIGSEGV */
	#ifdef SIGCONT
		RAW_SIGNAL( SIGCONT )
	#endif /* SIGCONT */
	#ifdef SIGPIPE
		RAW_SIGNAL( SIGPIPE )
	#endif /* SIGPIPE */
	#ifdef SIGWINCH
		RAW_SIGNAL( SIGWINCH )
	#endif /* SIGWINCH */
	#ifdef SIGXFSZ
		RAW_SIGNAL( SIGXFSZ )
	#endif /* SIGXFSZ */
	#ifdef SIGHUP
		RAW_SIGNAL( SIGHUP )
	#endif /* SIGHUP */
	#ifdef SIGCHLD
		RAW_SIGNAL( SIGCHLD )
	#endif /* SIGCHLD */
	#ifdef SIGSYS
		RAW_SIGNAL( SIGSYS )
	#endif /* SIGSYS */
	#ifdef SIGSTOP
		RAW_SIGNAL( SIGSTOP )
	#endif /* SIGSTOP */
	#ifdef SIGUSR2
		RAW_SIGNAL( SIGUSR2 )
	#endif /* SIGUSR2 */
	#ifdef SIGTSTP
		RAW_SIGNAL( SIGTSTP )
	#endif /* SIGTSTP */
	#ifdef SIGKILL
		RAW_SIGNAL( SIGKILL )
	#endif /* SIGKILL */
	#ifdef SIGXCPU
		RAW_SIGNAL( SIGXCPU )
	#endif /* SIGXCPU */
	#ifdef SIGUNUSED
		RAW_SIGNAL( SIGUNUSED )
	#endif /* SIGUNUSED */
	#ifdef SIGPWR
		RAW_SIGNAL( SIGPWR )
	#endif /* SIGPWR */
	#ifdef SIGILL
		RAW_SIGNAL( SIGILL )
	#endif /* SIGILL */
	#ifdef SIGINT
		RAW_SIGNAL( SIGINT )
	#endif /* SIGINT */
	#ifdef SIGIOT
		RAW_SIGNAL( SIGIOT )
	#endif /* SIGIOT */
	#ifdef SIGTERM
		RAW_SIGNAL( SIGTERM )
	#endif /* SIGTERM */
	};

	TRACE_ENTRY("");
	
	/* First pass: find the highest signal number */
	for (i=0; i < sizeof(abbrevs_raw)/sizeof(abbrevs_raw[0]); i++) {
		if (abbrevs_raw[i].sig > SIGNAL_MAX_LIMIT) {
			TRACE_DEBUG(ANNOYING, "Ignoring signal %s (%d), increase SIGNAL_MAX_LIMIT if you want it included", abbrevs_raw[i].name, abbrevs_raw[i].sig);
			continue;
		}
		if (abbrevs_raw[i].sig > abbrevs_nr)
			abbrevs_nr = abbrevs_raw[i].sig;
	}
	
	/* Now, alloc the array */
	abbrevs_nr++; /* 0-based */
	CHECK_MALLOC( abbrevs = calloc( abbrevs_nr, sizeof(char *) ) );
	
	/* Second pass: add all the signals in the array */
	for (i=0; i < sizeof(abbrevs_raw)/sizeof(abbrevs_raw[0]); i++) {
		if (abbrevs_raw[i].sig > SIGNAL_MAX_LIMIT)
			continue;
		
		if (abbrevs[abbrevs_raw[i].sig] == NULL)
			abbrevs[abbrevs_raw[i].sig] = abbrevs_raw[i].name;
	}
	
	/* Third pass: Set all missing signals to the undef value */
	for (i=0; i < abbrevs_nr; i++) {
		if (abbrevs[i] == NULL)
			abbrevs[i] = abbrevs_raw[0].name;
	}
	
	/* Done! */
	return 0;
}

/* Names of signals */
const char * fd_sig_abbrev(int signal)
{
	if (signal < abbrevs_nr)
		return abbrevs[signal];
	return abbrevs[0];
}

