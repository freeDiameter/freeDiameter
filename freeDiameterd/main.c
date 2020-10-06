/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2020, WIDE Project and NICT								 *
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

#if defined(__GLIBC__)
#define _BSD_SOURCE /* for vsyslog */
#endif

#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>

#include <signal.h>
#include <getopt.h>
#include <locale.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* forward declarations */
static int main_cmdline(int argc, char *argv[]);
static void * catch_signals(void * arg);
static pthread_t signals_thr;

static char *conffile = NULL;
static int daemon_mode = 0;
static int gnutls_debug = 0;
static char *pidfile = NULL;

/* gnutls debug */
static void fd_gnutls_debug(int level, const char * str) {
	fd_log_debug(" [gnutls:%d] %s", level, str);
}

static void pidfile_cleanup(void)
{
	if (pidfile != NULL) {
		LOG_I("Removing pidfile '%s'", pidfile);
		CHECK_SYS_DO( unlink(pidfile), /* ignore */ );
		pidfile = NULL;
	}
}

static int pidfile_create(void)
{
	if (pidfile == NULL) {
		return 0;
	}

	/* Create pidfile */
	FILE * fp = fopen(pidfile, "w");
	if (fp == NULL) {
		int ret = errno;
		LOG_F("Unable to write pidfile '%s'; Error: %s",
			pidfile, strerror(ret));
		pidfile = NULL;	/* disable pidfile_cleanup() */
		return ret;
	}

	/* Cleanup pidfile on exit */
	if (atexit(pidfile_cleanup) != 0) {
		LOG_F("Unable to setup pidfile cleanup");
		CHECK_SYS( fclose(fp) );
		pidfile_cleanup();
		return EINVAL;
	}

	/* Write the pid and close pidfile */
	fprintf(fp, "%d\n", getpid());
	CHECK_SYS_DO( fclose(fp), { pidfile_cleanup(); return __ret__; } );

	LOG_I("Created pidfile '%s'", pidfile);
	return 0;
}


static void syslog_logger(int loglevel, const char * format, va_list args)
{
	if (loglevel < fd_g_debug_lvl) {
		return;
	}

	int level;

	switch (loglevel) {
	case FD_LOG_INFO:
		level = LOG_INFO;
		break;
	case FD_LOG_NOTICE:
		level = LOG_NOTICE;
		break;
	case FD_LOG_ERROR:
		level = LOG_ERR;
		break;
	case FD_LOG_FATAL:
		level = LOG_CRIT;
		break;
	default:
		/* fallthrough */
	case FD_LOG_ANNOYING:
	case FD_LOG_DEBUG:
		level = LOG_DEBUG;
		break;
	}

	vsyslog(level | LOG_DAEMON, format, args);
}


/* freeDiameter starting point */
int main(int argc, char * argv[])
{
	int ret;
	sigset_t sig_all;

	/* Block all signals from the current thread and all its future children -- we will catch everything in catch_signals */
	sigfillset(&sig_all);
	ret = pthread_sigmask(SIG_BLOCK, &sig_all, NULL);
	ASSERT(ret == 0);

	/* Parse the command-line */
	ret = main_cmdline(argc, argv);
	if (ret != 0) {
		return EXIT_FAILURE;
	}

	if (daemon_mode) {
		TRACE_DEBUG(INFO, "entering background mode");
		CHECK_SYS_DO( daemon(1, 0), return EXIT_FAILURE );
	}

	CHECK_FCT_DO( pidfile_create(), return EXIT_FAILURE );

	/* Initialize the core library */
	ret = fd_core_initialize();
	if (ret != 0) {
		fprintf(stderr, "An error occurred during freeDiameter core library initialization.\n");
		return EXIT_FAILURE;
	}

	/* Set gnutls debug level ? */
	if (gnutls_debug) {
		gnutls_global_set_log_function((gnutls_log_func)fd_gnutls_debug);
		gnutls_global_set_log_level (gnutls_debug);
		TRACE_DEBUG(INFO, "Enabled GNUTLS debug at level %d", gnutls_debug);
	}

	/* Parse the configuration file */
	CHECK_FCT_DO( fd_core_parseconf(conffile), goto error );

	/* Start the servers */
	CHECK_FCT_DO( fd_core_start(), goto error );

	/* Allow SIGINT and SIGTERM from this point to terminate the application */
	CHECK_POSIX_DO( pthread_create(&signals_thr, NULL, catch_signals, NULL), goto error );

	TRACE_DEBUG(INFO, FD_PROJECT_BINARY " daemon initialized.");

	/* Now, just wait for termination */
	CHECK_FCT_DO( fd_core_wait_shutdown_complete(), return EXIT_FAILURE );

	/* Just in case it was not the result of a signal, we cancel signals_thr */
	fd_thr_term(&signals_thr);

	return EXIT_SUCCESS;
error:
	CHECK_FCT_DO( fd_core_shutdown(),  );
	CHECK_FCT_DO( fd_core_wait_shutdown_complete(), return EXIT_FAILURE );
	fd_thr_term(&signals_thr);
	return EXIT_FAILURE;
}


/* Display package version */
static void main_version_core(void)
{
	printf("%s, version %s\n", FD_PROJECT_NAME, fd_core_version);
}

/* Display package version and general info */
static void main_version(void)
{
	main_version_core();
	printf( "%s\n", FD_PROJECT_COPYRIGHT);
	printf( "\nSee " FD_PROJECT_NAME " homepage at http://www.freediameter.net/\n"
		" for information, updates and bug reports on this software.\n");
}

/* Print command-line options */
static void main_help( void )
{
	main_version_core();
	printf(	"  This daemon is an implementation of the Diameter protocol\n"
		"  used for Authentication, Authorization, and Accounting (AAA).\n");
	printf("\nUsage:  " FD_PROJECT_BINARY " [OPTIONS]...\n");
	printf( "  -h, --help              Print help and exit\n"
  		"  -V, --version           Print version and exit\n"
  		"  -c, --config=filename   Read configuration from this file instead of the \n"
		"                          default location (" DEFAULT_CONF_PATH "/" FD_DEFAULT_CONF_FILENAME ")\n"
		"  -D, --daemon            Start program in background\n"
		"  -p, --pidfile=filename  Write PID to filename\n"
		"  -s, --syslog            Write log output to syslog (instead of stdout)\n");
 	printf( "\nDebug:\n"
  		"  These options are mostly useful for developers\n"
		"  -d, --debug             Increase verbosity of log messages\n"
  		"  -f, --dbg_func <func>   Enable all traces within the function <func>\n"
  		"  -F, --dbg_file <file.c> Enable all traces within the file <file.c> (basename match)\n"
		"  -g, --dbg_gnutls <int>  Enable GNU TLS debug at level <int>\n"
  		"  -l, --dbglocale         Set the locale for error messages\n"
		"  -q, --quiet             Decrease verbosity of log messages\n"
	);
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
		{ "syslog",     no_argument,            NULL, 's' },
		{ "daemon",	no_argument, 		NULL, 'D' },
		{ "pidfile",	required_argument,	NULL, 'p' },
		{ "debug",	no_argument, 		NULL, 'd' },
		{ "quiet",	no_argument, 		NULL, 'q' },
		{ "dbglocale",	optional_argument, 	NULL, 'l' },
		{ "dbg_func",	required_argument, 	NULL, 'f' },
		{ "dbg_file",	required_argument, 	NULL, 'F' },
		{ "dbg_gnutls",	required_argument, 	NULL, 'g' },
		{ NULL,		0, 			NULL, 0 }
	};

	/* Loop on arguments */
	while (1) {
		c = getopt_long (argc, argv, "hVc:Dp:dql:f:F:g:s", long_options, &option_index);
		if (c == -1)
			break;	/* Exit from the loop.  */

		switch (c) {
			case 'h':	/* Print help and exit.  */
				main_help();
				exit(EXIT_SUCCESS);

			case 'V':	/* Print version and exit.  */
				main_version();
				exit(EXIT_SUCCESS);

			case 'c':	/* Read configuration from this file instead of the default location..  */
				if (optarg == NULL ) {
					fprintf(stderr, "Missing argument with --config directive\n");
					return EINVAL;
				}
				conffile = optarg;
				break;

			case 'D':
				daemon_mode = 1;
				break;

			case 'p':	/* Write pidfile */
				if (optarg == NULL ) {
					fprintf(stderr, "Missing argument with --pidfile directive\n");
					return EINVAL;
				}
				pidfile = optarg;
				break;

			case 'l':	/* Change the locale.  */
				locale = setlocale(LC_ALL, optarg?:"");
				if (!locale) {
					fprintf(stderr, "Unable to set locale (%s)\n", optarg);
					return EINVAL;
				}
				break;

			case 'd':	/* Increase verbosity of debug messages.  */
				fd_g_debug_lvl--;
				break;

			case 'f':	/* Full debug for the function with this name.  */
				#ifdef DEBUG
				fd_debug_one_function = optarg;
				fd_g_debug_lvl = FD_LOG_DEBUG;
				#else /* DEBUG */
				fprintf(stderr, "Error: must compile with DEBUG support to use --dbg_func feature!\n");
				return EINVAL;
				#endif /* DEBUG */
				break;

			case 'F':	/* Full debug for the file with this name.  */
				#ifdef DEBUG
				fd_debug_one_file = basename(optarg);
				fd_g_debug_lvl = FD_LOG_DEBUG;
				#else /* DEBUG */
				fprintf(stderr, "Error: must compile with DEBUG support to use --dbg_file feature!\n");
				return EINVAL;
				#endif /* DEBUG */
				break;

			case 'g':	/* Set a debug level and function for GNU TLS calls.  */
				gnutls_debug = (int)atoi(optarg);
				break;

			case 'q':	/* Decrease verbosity then remove debug messages.  */
				fd_g_debug_lvl++;
				break;

			case 's':	/* Write log data using syslog(3) */
				if (fd_log_handler_register(syslog_logger) != 0) {
					fprintf(stderr, "Cannot initialize syslog logger\n");
					return EINVAL;
				}
				break;

			case '?':	/* Invalid option.  */
				/* `getopt_long' already printed an error message.  */
				fprintf(stderr, "getopt_long found an invalid character\n");
				return EINVAL;

			default:	/* bug: option not considered.  */
				fprintf(stderr, "A command-line option is missing in parser: %c\n", c);
				ASSERT(0);
				return EINVAL;
		}
	}

	return 0;
}

/* Handle some signals */
static void * catch_signals(void * arg)
{
	sigset_t ss;
	fd_log_threadname ( "signals catcher" );

	sigemptyset(&ss);

	/* Signals that terminate the daemon */
	sigaddset(&ss, SIGTERM);
	sigaddset(&ss, SIGINT);

	/* Signals that send an event */
	sigaddset(&ss, SIGUSR1);
	sigaddset(&ss, SIGUSR2);

	/* We unblock all other signals, so that their default handler is used (such as SIGTSTP) */
	CHECK_SYS_DO( pthread_sigmask( SIG_SETMASK, &ss, NULL ), goto out );

	/* Now loop on the reception of the signal */
	while (1) {
		int sig, *ps;

		/* Wait to receive the next signal */
		CHECK_POSIX_DO( sigwait(&ss, &sig), break );

		TRACE_DEBUG(FULL, "Signal %d caught", sig);

		switch (sig) {
			case SIGUSR1:
			case SIGUSR2:
				CHECK_MALLOC_DO( ps = malloc(sizeof(int)), goto out);
				*ps = sig;
				CHECK_FCT_DO( fd_event_send(fd_g_config->cnf_main_ev, FDEV_TRIGGER, sizeof(int), ps), goto out );
				break;

			case SIGINT:
			case SIGTERM:
				CHECK_FCT_DO( fd_core_shutdown(), goto out );

		}
	}
out:
	/* Better way to handle this ? */
	ASSERT(0);
	return NULL;
}
