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

enum { MAX_PEERS = 64,
       MAX_NAME_STR = 128 };

typedef struct {
	char peer_name[MAX_NAME_STR];
	char counter_name[MAX_NAME_STR];
	prom_counter_t *counter;
	bool connected;
} peer_t;

static struct MHD_Daemon *prom_daemon = NULL;

/* The metric name specifies the general feature of a
 * system that is measured (e.g. http_requests_total - 
 * the total number of HTTP requests received). It may
 * contain ASCII letters and digits, as well as underscores
 * and colons. It must match the regex [a-zA-Z_:][a-zA-Z0-9_:]*
 * https://prometheus.io/docs/concepts/data_model/#metric-names-and-labels 
 * 
 * Assuming name parameter begins with character that matches regex [a-zA-Z_:] */
void sanitise_counter_name(char * name) {
	int i = 0;

	if (NULL == name) {
		return;
	}

	while (('\0' != name[i]) &&
	       (i < MAX_NAME_STR)) {
		bool valid_char = false;
		char c = name[i];

		if ((('a' <= c) && (c <= 'z')) ||
			(('A' <= c) && (c <= 'Z')) ||
			(('0' <= c) && (c <= '9')) ||
			('_' == c)                 ||
			(':' == c)) {
			valid_char = true;
		}

		if (!valid_char) {
			name[i] = '_';
		}

		++i;
	}
}

bool add_peer(peer_t* peers, char* peer_name) {
	if ((NULL == peers) ||
	    (NULL == peer_name)) {
		return false;
	}

	for (int i = 0; i < MAX_PEERS; ++i) {
		/* Find unused peer element */
		if (NULL == peers[i].counter) {
			fd_log_notice("Adding counter for '%s'", peer_name);

			strncpy(peers[i].peer_name, peer_name, MAX_NAME_STR - 1);
			strcpy(peers[i].counter_name, "fd_peer_");
			strncat(peers[i].counter_name, peer_name, MAX_NAME_STR - strlen(peers[i].counter_name));
			sanitise_counter_name(peers[i].counter_name);
			fd_log_notice("Counter name is '%s'", peers[i].counter_name);

			peers[i].counter = prom_counter_new(peers[i].counter_name, "A counter for a freeDiameter peer, a value of 1 signifies that this peer is connected", 0, NULL);
			assert(peers[i].counter == prom_collector_registry_must_register_metric(peers[i].counter));
			
			prom_counter_inc(peers[i].counter, NULL);
			peers[i].connected = true;

			return true;
		}
	}

	return false;
}

bool clear_disconnected_peers(peer_t* peers) {
	if (NULL == peers) {
		return false;
	}

	for (int i = 0; i < MAX_PEERS; ++i) {
		if ((false == peers[i].connected) &&
		    (NULL != peers[i].counter)) {
			/* Clear the peer element */
			fd_log_notice("Removing counter for '%s'", peers[i].peer_name);
			
			strcpy(peers[i].peer_name, "");
			strcpy(peers[i].counter_name, "");

			prom_collector_registry_unregister_metric(peers[i].counter);
			peers[i].counter = NULL;
		}
	}
	
	return true;
}

bool step_start(peer_t* peers) {
	if (NULL == peers) {
		return false;
	}

	for (int i = 0; i < MAX_PEERS; ++i) {
		peers[i].connected = false;
	}

	return true;
}

bool refresh_or_add_peer(peer_t* peers, char* peer_name) {
	if ((NULL == peers) || 
	    (NULL == peer_name)) {
		return false;
	}

	for (int i = 0; i < MAX_PEERS; ++i) {
		if (!strcmp(peer_name, peers[i].peer_name)) {
			/* Refresh peer */
			fd_log_notice("Peer with metric name '%s' is still active", peers[i].counter_name);
			peers[i].connected = true;
			return true;
		}
	}

	return add_peer(peers, peer_name);
}

/* Thread to display periodical debug information */
static pthread_t thr;
static void * mn_thr(void* seconds)
{
	fd_log_threadname("Monitor Diameter Peer thread");
	
	/* Init all of prometheus */
    assert(0 == prom_collector_registry_default_init());
    
    /* Set the active registry for the HTTP handler */
    promhttp_set_active_collector_registry(NULL);

	peer_t peers[MAX_PEERS] = {0};

    prom_daemon = promhttp_start_daemon(MHD_USE_SELECT_INTERNALLY, 8000, NULL, NULL);
    if (prom_daemon == NULL) {
		fd_log_error("Failed to start prom_daemon");
        return NULL;
    }

	while (true) {
		step_start(peers);

		for (struct fd_list *li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
			struct peer_hdr * p = (struct peer_hdr *)li->o;
			refresh_or_add_peer(peers, p->info.pi_diamid);
		}

		clear_disconnected_peers(peers);

		sleep(1);
	}
	
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

    prom_collector_registry_destroy(PROM_COLLECTOR_REGISTRY_DEFAULT);
    MHD_stop_daemon(prom_daemon);

	return ;
}

