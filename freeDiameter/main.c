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

#include "fD.h"

#include <signal.h>
#include <getopt.h>
#include <locale.h>
#include <gcrypt.h>

/* forward declarations */
static void * sig_hdl(void * arg);
static int main_cmdline(int argc, char *argv[]);
static void main_version(void);
static void main_help( void );

/* The static configuration structure */
static struct fd_config conf;
struct fd_config * fd_g_config = &conf;

/* gcrypt functions to support posix threads */
GCRY_THREAD_OPTION_PTHREAD_IMPL;

/* freeDiameter starting point */
int main(int argc, char * argv[])
{
	int ret;
	pthread_t sig_th;
	sigset_t sig_all;
	
	memset(fd_g_config, 0, sizeof(struct fd_config));
	
	/* Block all signals */
	sigfillset(&sig_all);
	CHECK_POSIX(  pthread_sigmask(SIG_BLOCK, &sig_all, NULL)  );
	
	/* Initialize the library */
	CHECK_FCT( fd_lib_init() );
	TRACE_DEBUG(INFO, "libfreeDiameter initialized.");
	
	/* Name this thread */
	fd_log_threadname("Main");
	
	/* Initialize the config */
	CHECK_FCT( fd_conf_init() );

	/* Parse the command-line */
	CHECK_FCT(  main_cmdline(argc, argv)  );
	
	/* Initialize gcrypt and gnutls */
	(void) gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread);
	(void) gcry_control (GCRYCTL_ENABLE_QUICK_RANDOM, 0);
	CHECK_GNUTLS_DO( gnutls_global_init(), return EINVAL );
	if ( ! gnutls_check_version(GNUTLS_VERSION) ) {
		fprintf(stderr, "The GNUTLS library is too old; found '%s', need '" GNUTLS_VERSION "'\n", gnutls_check_version(NULL));
		return EINVAL;
	} else {
		TRACE_DEBUG(INFO, "GNUTLS library '%s' initialized.", gnutls_check_version(NULL));
	}
	
	/* Allow SIGINT and SIGTERM from this point */
	CHECK_POSIX(  pthread_create(&sig_th, NULL, sig_hdl, NULL)  );
	
	/* Add definitions of the base protocol */
	CHECK_FCT( fd_dict_base_protocol(fd_g_config->cnf_dict) );
	
	/* Initialize other modules */
	CHECK_FCT(  fd_queues_init()  );
	CHECK_FCT(  fd_msg_init()  );
	CHECK_FCT(  fd_p_expi_init()  );
	CHECK_FCT(  fd_rtdisp_init()  );
	
	/* Parse the configuration file */
	CHECK_FCT( fd_conf_parse() );
	
	/* Load the dynamic extensions */
	CHECK_FCT(  fd_ext_load()  );
	
	fd_conf_dump();
	
	/* Start the servers */
	CHECK_FCT( fd_servers_start() );
	
	/* Start the peer state machines */
	CHECK_FCT( fd_psm_start() );
	
	/* Now, just wait for events */
	TRACE_DEBUG(INFO, FD_PROJECT_BINARY " daemon initialized.");
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
			
			case FDEV_TERMINATE:
				ret = 0;
				goto end;
			
			default:
				TRACE_DEBUG(INFO, "Unexpected event in the daemon (%d), ignored.\n", code);
		}
	}
	
end:
	TRACE_DEBUG(INFO, FD_PROJECT_BINARY " daemon is stopping...");
	
	/* cleanups */
	CHECK_FCT_DO( fd_servers_stop(), /* Stop accepting new connections */ );
	CHECK_FCT_DO( fd_rtdisp_cleanstop(), /* Stop dispatch thread(s) after a clean loop if possible */ );
	CHECK_FCT_DO( fd_peer_fini(), /* Stop all connections */ );
	CHECK_FCT_DO( fd_rtdisp_fini(), /* Stop routing threads and destroy routing queues */ );
	
	CHECK_FCT_DO( fd_ext_fini(), /* Cleanup all extensions */ );
	CHECK_FCT_DO( fd_rtdisp_cleanup(), /* destroy remaining handlers */ );
	
	CHECK_FCT_DO( fd_thr_term(&sig_th), /* reclaim resources of the signal thread */ );
	
	gnutls_global_deinit();
	
	fd_log_debug(FD_PROJECT_BINARY " daemon is terminated.\n");
	return ret;
}

/* Parse the command-line */
static int main_cmdline(int argc, char *argv[])
{
	int c;
	int option_index = 0;
	char * locale;
	
      	struct option long_options[] = {
		{ "help",	no_argument, 		NULL, 'h' },
		{ "version",	no_argument, 		NULL, 'V' },
		{ "config",	required_argument, 	NULL, 'c' },
		{ "debug",	no_argument, 		NULL, 'd' },
		{ "quiet",	no_argument, 		NULL, 'q' },
		{ "dbglocale",	optional_argument, 	NULL, 'l' },
		{ NULL,		0, 			NULL, 0 }
	};
	
	TRACE_ENTRY("%d %p", argc, argv);
	
	/* Loop on arguments */
	while (1) {
		c = getopt_long (argc, argv, "hVc:dql:", long_options, &option_index);
		if (c == -1) 
			break;	/* Exit from the loop.  */
		
		switch (c) {
			case 'h':	/* Print help and exit.  */
				main_help();
				exit(0);

			case 'V':	/* Print version and exit.  */
				main_version();
				exit(0);

			case 'c':	/* Read configuration from this file instead of the default location..  */
				CHECK_PARAMS( optarg );
				fd_g_config->cnf_file = optarg;
				break;

			case 'l':	/* Change the locale.  */
				locale = setlocale(LC_ALL, optarg?:"");
				if (locale) {
					TRACE_DEBUG(INFO, "Locale set to: %s", optarg ?: locale);
				} else {
					TRACE_DEBUG(INFO, "Unable to set locale (%s)", optarg);
					return EINVAL;
				}
				break;

			case 'd':	/* Increase verbosity of debug messages.  */
				fd_g_debug_lvl++;
				break;
				
			case 'q':	/* Decrease verbosity then remove debug messages.  */
				fd_g_debug_lvl--;
				break;

			case '?':	/* Invalid option.  */
				/* `getopt_long' already printed an error message.  */
				TRACE_DEBUG(INFO, "getopt_long found an invalid character");
				return EINVAL;

			default:	/* bug: option not considered.  */
				TRACE_DEBUG(INFO, "A command-line option is missing in parser: %c", c);
				ASSERT(0);
				return EINVAL;
		}
	}
		
	return 0;
}

/* Display package version */
static void main_version_core(void)
{
	printf("%s, version %d.%d.%d"
#ifdef HG_VERSION
		" (r%s"
# ifdef PACKAGE_HG_REVISION
		"/%s"
# endif /* PACKAGE_HG_VERSION */
		")"
#endif /* HG_VERSION */
		"\n", 
		FD_PROJECT_NAME, FD_PROJECT_VERSION_MAJOR, FD_PROJECT_VERSION_MINOR, FD_PROJECT_VERSION_REV
#ifdef HG_VERSION
		, HG_VERSION
# ifdef PACKAGE_HG_REVISION
		, PACKAGE_HG_REVISION
# endif /* PACKAGE_HG_VERSION */
#endif /* HG_VERSION */
		);
}

/* Display package version and general info */
static void main_version(void)
{
	main_version_core();
	printf( "%s\n", FD_PROJECT_COPYRIGHT);
	printf( "\nSee " FD_PROJECT_NAME " homepage at http://aaa.koganei.wide.ad.jp/\n"
		" for information, updates and bug reports on this software.\n");
}

/* Print command-line options */
static void main_help( void )
{
	main_version_core();
	printf(	"  This daemon is an implementation of the Diameter protocol\n"
		"  used for Authentication, Authorization, and Accounting (AAA).\n");
	printf("\nUsage:  " FD_PROJECT_BINARY " [OPTIONS]...\n");
	printf( "  -h, --help             Print help and exit\n"
  		"  -V, --version          Print version and exit\n"
  		"  -c, --config=filename  Read configuration from this file instead of the \n"
		"                           default location (%s).\n", DEFAULT_CONF_FILE);
 	printf( "\nDebug:\n"
  		"  These options are mostly useful for developers\n"
  		"  -l, --dbglocale        Set the locale for error messages\n"
  		"  -d, --debug            Increase verbosity of debug messages\n"
  		"  -q, --quiet            Decrease verbosity then remove debug messages\n");
}

#ifdef HAVE_SIGNALENT_H
const char *const signalstr[] = {
# include "signalent.h"
};
const int nsignalstr = sizeof signalstr / sizeof signalstr[0];
# define SIGNALSTR(sig) (((sig) < nsignalstr) ? signalstr[(sig)] : "unknown")
#else /* HAVE_SIGNALENT_H */
# define SIGNALSTR(sig) ("")
#endif /* HAVE_SIGNALENT_H */

/* signal handler */
static void * sig_hdl(void * arg)
{
	sigset_t sig_main;
	int sig = 0;
	
	TRACE_ENTRY();
	fd_log_threadname("Main signal handler");
	
	sigemptyset(&sig_main);
	sigaddset(&sig_main, SIGINT);
	sigaddset(&sig_main, SIGTERM);
	
	CHECK_SYS_DO(  sigwait(&sig_main, &sig), TRACE_DEBUG(INFO, "Error in sigwait function") );
	
	TRACE_DEBUG(INFO, "Received signal %s (%d), exiting", SIGNALSTR(sig), sig);
	CHECK_FCT_DO( fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), exit(2) );
	return NULL;
}
	
