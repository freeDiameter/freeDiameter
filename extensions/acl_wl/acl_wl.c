/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2019, WIDE Project and NICT								 *
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

/* 
 * Whitelist extension for freeDiameter.
 */

#include <pthread.h>
#include <signal.h>

#include "acl_wl.h"

static pthread_rwlock_t acl_wl_lock;

#define MODULE_NAME "acl_wl"

static char *acl_wl_config_file;

/* The validator function */
static int aw_validate(struct peer_info * info, int * auth, int (**cb2)(struct peer_info *))
{
	int res;
	
	TRACE_ENTRY("%p %p %p", info, auth, cb2);
	
	CHECK_PARAMS(info && auth && cb2);
	
	/* We don't use the second callback */
	*cb2 = NULL;
	
	/* Default to unknown result */
	*auth = 0;

        if (pthread_rwlock_rdlock(&acl_wl_lock) != 0) {
		fd_log_notice("%s: read-lock failed, skipping handler", MODULE_NAME);
		return 0;
	}

	/* Now search the peer in our tree */
	CHECK_FCT( aw_tree_lookup(info->pi_diamid, &res) );

        if (pthread_rwlock_unlock(&acl_wl_lock) != 0) {
		fd_log_notice("%s: read-unlock failed after aw_tree_lookup, exiting", MODULE_NAME);
		exit(1);
	}

	if (res < 0) {
		/* The peer is not whitelisted */
		return 0;
	}
	
	/* We found the peer in the tree, now check the status */
	
	/* First, if TLS is already in place, just accept */
	if (info->runtime.pir_cert_list) {
		*auth = 1;
		return 0;
	}
	
	/* Now, if we did not specify any flag, reject */
	if (res == 0) {
		TRACE_DEBUG(INFO, "Peer '%s' rejected, only TLS-protected connection is whitelisted.", info->pi_diamid);
		/* We don't actually set *auth = -1, leave space for a further extension to validate the peer */
		return 0;
	}
	
	/* Otherwise, just set the configured flags for the peer, and authorize it */
	*auth = 1;
	
	/* Save information about the security mechanism to use after CER/CEA exchange */
	if ((res & PI_SEC_NONE) && (res & PI_SEC_TLS_OLD))
		res = PI_SEC_NONE; /* If we authorized it, we must have an IPsec tunnel setup, no need for TLS in this case */
	
	info->config.pic_flags.sec = res;
	return 0;
}

static volatile int in_signal_handler = 0;

/* signal handler */
static void sig_hdlr(void)
{
	struct fd_list old_tree;

	if (in_signal_handler) {
		fd_log_error("%s: already handling a signal, ignoring new one", MODULE_NAME);
		return;
	}
	in_signal_handler = 1;

	if (pthread_rwlock_wrlock(&acl_wl_lock) != 0) {
		fd_log_error("%s: locking failed, aborting config reload", MODULE_NAME);
		return;
	}

	/* save old config in case reload goes wrong */
	old_tree = tree_root;
	fd_list_init(&tree_root, NULL);

	if (aw_conf_handle(acl_wl_config_file) != 0) {
		fd_log_error("%s: error reloading configuration, restoring previous configuration", MODULE_NAME);
		aw_tree_destroy();
		tree_root = old_tree;
	} else {
		struct fd_list new_tree;
		new_tree = tree_root;
		tree_root = old_tree;
		aw_tree_destroy();
		tree_root = new_tree;
	}

	if (pthread_rwlock_unlock(&acl_wl_lock) != 0) {
		fd_log_error("%s: unlocking failed after config reload, exiting", MODULE_NAME);
		exit(1);
	}

	fd_log_notice("%s: reloaded configuration", MODULE_NAME);

	in_signal_handler = 0;
}


/* entry point */
static int aw_entry(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	CHECK_PARAMS(conffile);

	acl_wl_config_file = conffile;

        pthread_rwlock_init(&acl_wl_lock, NULL);

        if (pthread_rwlock_wrlock(&acl_wl_lock) != 0) {
                fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
                return EDEADLK;
        }

	/* Parse configuration file */
	CHECK_FCT( aw_conf_handle(conffile) );

	TRACE_DEBUG(INFO, "Extension ACL_wl initialized with configuration: '%s'", conffile);
	if (TRACE_BOOL(ANNOYING)) {
		aw_tree_dump();
	}

	if (pthread_rwlock_unlock(&acl_wl_lock) != 0) {
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	/* Register reload callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, MODULE_NAME, sig_hdlr));

	/* Register the validator function */
	CHECK_FCT( fd_peer_validate_register ( aw_validate ) );

	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	/* Destroy the tree */
	aw_tree_destroy();
}

EXTENSION_ENTRY(MODULE_NAME, aw_entry);
