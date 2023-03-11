/* Peer Monitoring extension:
 - periodically display peers
 */

#include <freeDiameter/extension.h>
#include <signal.h>

#ifndef MONITOR_SIGNAL
#define MONITOR_SIGNAL	SIGUSR2
#endif /* MONITOR_SIGNAL */

static int 	 monitor_main(char * conffile);

EXTENSION_ENTRY("dbg_peers", monitor_main);



/* Display information about a queue */
static void display_info(char * queue_desc, char * peer, int current_count, int limit_count, int highest_count, long long total_count,
			struct timespec * total, struct timespec * blocking, struct timespec * last)
{
	long long us = (total->tv_sec * 1000000) + (total->tv_nsec / 1000);
	long double throughput = (long double)total_count * 1000000;
	throughput /= us;
	if (peer) {
		TRACE_DEBUG(INFO, "'%s'@'%s': cur:%d/%d, h:%d, T:%lld in %ld.%06lds (%.2LFitems/s), blocked:%ld.%06lds, last processing:%ld.%06lds",
			queue_desc, peer, current_count, limit_count, highest_count,
			total_count, total->tv_sec, total->tv_nsec/1000, throughput,
			blocking->tv_sec, blocking->tv_nsec/1000, last->tv_sec, last->tv_nsec/1000);
	} else {
		TRACE_DEBUG(INFO, "Global '%s': cur:%d/%d, h:%d, T:%lld in %ld.%06lds (%.2LFitems/s), blocked:%ld.%06lds, last processing:%ld.%06lds",
			queue_desc, current_count, limit_count, highest_count,
			total_count, total->tv_sec, total->tv_nsec/1000, throughput,
			blocking->tv_sec, blocking->tv_nsec/1000, last->tv_sec, last->tv_nsec/1000);
	}
}

/* Thread to display periodical debug information */
static pthread_t thr;
static void * mn_thr(void * arg)
{
#ifdef DEBUG
	int i = 0;
#endif
	fd_log_threadname("Monitor thread");
	char * buf = NULL;
	size_t len;
	
	/* Loop */
	while (1) {
		int current_count, limit_count, highest_count;
		long long total_count;
		struct timespec total, blocking, last;
		struct fd_list * li;
	
		#ifdef DEBUG
		for (i++; i % 30; i++) {
			fd_log_debug("[dbg_peers] %ih%*im%*is", i/3600, 2, (i/60) % 60 , 2, i%60); /* This makes it easier to detect inactivity periods in the log file */
			sleep(1);
		}
		#else /* DEBUG */
		sleep(3599); /* 1 hour */
		#endif /* DEBUG */
		TRACE_DEBUG(INFO, "[dbg_peers] Dumping peer statistics");
		
		/* Log to file */
		fptr = fopen("/tmp/DiamPeers.txt","w");
		if(fptr == NULL)
		{
			printf("Error loading file to write to");   
			exit(1);             
		}
		fprintf("Hello");
		fclose(fptr);

		CHECK_FCT_DO( pthread_rwlock_rdlock(&fd_g_peers_rw), /* continue */ );

		for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
			struct peer_hdr * p = (struct peer_hdr *)li->o;
			
			TRACE_DEBUG(INFO, "%s", fd_peer_dump(&buf, &len, NULL, p, 1));
			
			CHECK_FCT_DO( fd_stat_getstats(STAT_P_PSM, p, &current_count, &limit_count, &highest_count, &total_count, &total, &blocking, &last), );
			display_info("Events, incl. recept", p->info.pi_diamid, current_count, limit_count, highest_count, total_count, &total, &blocking, &last);
			
			CHECK_FCT_DO( fd_stat_getstats(STAT_P_TOSEND, p, &current_count, &limit_count, &highest_count, &total_count, &total, &blocking, &last), );
			display_info("Outgoing", p->info.pi_diamid, current_count, limit_count, highest_count, total_count, &total, &blocking, &last);
			
		}

		CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );

		sleep(1);
	}
	
	free(buf);
	return NULL;
}

/* Function called on receipt of MONITOR_SIGNAL */
static void got_sig()
{
	char * buf = NULL;
	size_t len;
	TRACE_DEBUG(INFO, "[dbg_peers] Dumping config information");
	TRACE_DEBUG(INFO, "%s", fd_conf_dump(&buf, &len, NULL));
	TRACE_DEBUG(INFO, "[dbg_peers] Dumping extensions information");
	TRACE_DEBUG(INFO, "%s", fd_ext_dump(&buf, &len, NULL));
	TRACE_DEBUG(INFO, "[dbg_peers] Dumping dictionary information");
	TRACE_DEBUG(INFO, "%s", fd_dict_dump(&buf, &len, NULL, fd_g_config->cnf_dict));
	free(buf);
}

/* Entry point */
static int monitor_main(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	
	/* Catch signal SIGUSR1 */
	CHECK_FCT( fd_event_trig_regcb(MONITOR_SIGNAL, "dbg_peers", got_sig));
	
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

