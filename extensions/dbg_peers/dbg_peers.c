/* Peer Monitoring extension:
 - periodically display peers
 */

#include <freeDiameter/extension.h>
#include <signal.h>
#include "prom/prom.h"
#include <stdio.h>
#include "microhttpd.h"
#include "prom/promhttp.h"
#include <stdbool.h>
#include <string.h>

static int monitor_peers_main(char * statefile);

EXTENSION_ENTRY("dbg_peers", monitor_peers_main);

static struct MHD_Daemon *prom_daemon = NULL;
static unsigned int poll_period_sec = 1;
static unsigned short metric_export_port = 8000;
static pthread_t thr;

/* Thread to display periodical debug information */
static void * mn_thr(void* nothing)
{
	fd_log_threadname("Monitor Diameter Peer thread");
	
	/* Init all of prometheus */
    assert(0 == prom_collector_registry_default_init());
    
    /* Set the active registry for the HTTP handler */
    promhttp_set_active_collector_registry(NULL);

    prom_daemon = promhttp_start_daemon(MHD_USE_SELECT_INTERNALLY, metric_export_port, NULL, NULL);
    if (prom_daemon == NULL) {
		fd_log_error("Failed to start prom_daemon on port %u", metric_export_port);
        return NULL;
    }

	char *lable = "endpoint";
	prom_gauge_t *peer_gauge = prom_gauge_new("prom_diam_connected_peers", "Connected Diameter Peer Count", 1, &lable);
	prom_metric_t *peer_gauge_metric = prom_collector_registry_must_register_metric(peer_gauge);

	while (true) {
		for (struct fd_list *li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
			struct peer_hdr * p = (struct peer_hdr *)li->o;
			double val = 0.0;

			if (STATE_OPEN == fd_peer_get_state(p)) {
				val = 1.0;
			}

			prom_gauge_set(peer_gauge_metric, val, &p->info.pi_diamid);
		}

		sleep(poll_period_sec);
	}
	
	return NULL;
}

/* Entry point */
static int monitor_peers_main(char * statefile)
{
	TRACE_DEBUG(INFO, "[dbg_peers] Writing Diameter peers to %s", statefile);
	
	//todo load the config

	CHECK_POSIX( pthread_create( &thr, NULL, mn_thr, statefile ) );

	return 0;
}

/* Cleanup */
void fd_ext_fini(void)
{
	CHECK_FCT_DO( fd_thr_term(&thr), /* continue */ );
	TRACE_DEBUG(INFO, "[dbg_peers] Cleaning up files");

    prom_collector_registry_destroy(PROM_COLLECTOR_REGISTRY_DEFAULT);
    MHD_stop_daemon(prom_daemon);

	return ;
}

