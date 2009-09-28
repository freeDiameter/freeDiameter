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

const char *peer_state_str[] = { "<error>"
	, "STATE_DISABLED"
	, "STATE_OPEN"
	, "STATE_CLOSED"
	, "STATE_CLOSING"
	, "STATE_WAITCNXACK"
	, "STATE_WAITCNXACK_ELEC"
	, "STATE_WAITCEA"
	, "STATE_SUSPECT"
	, "STATE_REOPEN"
	};

struct fd_list   fd_g_peers;
pthread_rwlock_t fd_g_peers_rw;

static int started = 0;
static pthread_mutex_t  started_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   started_cnd = PTHREAD_COND_INITIALIZER;

/* Wait for start signal */
int fd_peer_waitstart()
{
	CHECK_POSIX( pthread_mutex_lock(&started_mtx) );
awake:	
	if (! started) {
		pthread_cleanup_push( fd_cleanup_mutex, &started_mtx );
		CHECK_POSIX( pthread_cond_wait(&started_cnd, &started_mtx) );
		pthread_cleanup_pop( 0 );
		goto awake;
	}
	CHECK_POSIX( pthread_mutex_unlock(&started_mtx) );
	return 0;
}

/* Allow the state machines to start */
int fd_peer_start()
{
	CHECK_POSIX( pthread_mutex_lock(&started_mtx) );
	started = 1;
	CHECK_POSIX( pthread_cond_broadcast(&started_cnd) );
	CHECK_POSIX( pthread_mutex_unlock(&started_mtx) );
	return 0;
}

/* Initialize the peers list */
int fd_peer_init()
{
	TRACE_ENTRY();
	
	fd_list_init(&fd_g_peers, NULL);
	CHECK_POSIX( pthread_rwlock_init(&fd_g_peers_rw, NULL) );
	
	return 0;
}

/* Dump the list of peers */
void fd_peer_dump_list(int details)
{
	struct fd_list * li;
	
	fd_log_debug("Dumping list of peers :\n");
	CHECK_FCT_DO( pthread_rwlock_rdlock(&fd_g_peers_rw), /* continue */ );
	
	for (li = fd_g_peers.next; li != &fd_g_peers; li = li->next) {
		struct fd_peer * np = (struct fd_peer *)li;
		if (np->p_eyec != EYEC_PEER) {
			fd_log_debug("  Invalid entry @%p !\n", li);
			continue;
		}
		
		fd_log_debug("   %s\t%s", STATE_STR(np->p_hdr.info.pi_state), np->p_hdr.info.pi_diamid);
		if (details > INFO) {
			fd_log_debug("\t(rlm:%s)", np->p_hdr.info.pi_realm);
			if (np->p_hdr.info.pi_prodname)
				fd_log_debug("\t['%s' %u]", np->p_hdr.info.pi_prodname, np->p_hdr.info.pi_firmrev);
			fd_log_debug("\t(from %s)", np->p_dbgorig);
		}
		fd_log_debug("\n");
	}
	
	CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_peers_rw), /* continue */ );
}

/* Add a new peer entry */
int fd_peer_add ( struct peer_info * info, char * orig_dbg, void (*cb)(struct peer_info *, void *), void * cb_data )
{
	return ENOTSUP;
}
