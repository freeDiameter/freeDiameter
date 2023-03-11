/* Peer Monitoring extension:
 - periodically display peers
 */

#include <freeDiameter/extension.h>
#include <signal.h>

static int 	 monitor_peers_main(char * statefile);

EXTENSION_ENTRY("dbg_peers", monitor_peers_main);

/* Thread to display periodical debug information */
static pthread_t thr;
static void * mn_thr(char * statefile)
{
	fd_log_threadname("Monitor Diameter Peer thread");
	char * buf = NULL;
	size_t len;
	
	/* Loop */
	while (1) {
		int current_count, limit_count, highest_count;
		long long total_count;
		struct timespec total, blocking, last;
		struct fd_list * li;
	
		TRACE_DEBUG(INFO, "[dbg_peers] Dumping peer statistics to file %s", statefile);
		
		/* Log to file */
		FILE * fptr;
		fptr = fopen(statefile,"w");
		if(fptr == NULL)
		{
			TRACE_DEBUG(INFO, "[dbg_peers] Error loading file to write to at %s", statefile);
			exit(1);             
		}
		fprintf(fptr, "%s", "Start of File\n");

		CHECK_FCT_DO( pthread_rwlock_rdlock(&fd_g_peers_rw), /* continue */ );

		for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
			struct peer_hdr * p = (struct peer_hdr *)li->o;
			
			TRACE_DEBUG(INFO, "[dbg_peers] %s", fd_peer_dump(&buf, &len, NULL, p, 1));
			fprintf(fptr, "%s %s", fd_peer_dump(&buf, &len, NULL, p, 1), "\n");
		}

		fprintf(fptr, "%s", "End of File\n");
		fclose(fptr);

		CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );

		sleep(10); /* Sleep 10 Seconds */
	}
	
	free(buf);
	return NULL;
}

/* Entry point */
static int monitor_peers_main(char * statefile)
{
	TRACE_DEBUG(INFO, "[dbg_peers] Writing Diameter peers to %s", statefile);
	
	CHECK_POSIX( pthread_create( &thr, NULL, mn_thr, statefile ) );

	return 0;
}

/* Cleanup */
void fd_ext_fini(void)
{
	CHECK_FCT_DO( fd_thr_term(&thr), /* continue */ );
	TRACE_DEBUG(INFO, "[dbg_peers] Cleaning up files");
	int ret;
	ret = remove("/tmp/DiamPeers.txt");
	if(ret == 0) {
		printf("Diameter Peer state file deleted successfully");
	} else {
		printf("Error: unable to delete the file");
	}	
	return ;
}

