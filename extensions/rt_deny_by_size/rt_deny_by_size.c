/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Thomas Klausner <tk@giga.or.at>								 *
*													 *
* Copyright (c) 2018, Thomas Klausner									 *
* All rights reserved.											 *
* 													 *
* Written under contract by Effortel Technologies SA, http://effortel.com/                               *
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

#include <signal.h>

/* See doc/rt_deny_by_size.conf.sample for more details about the features of this extension */
#include "rt_deny_by_size.h"

/* The configuration structure */
struct rt_deny_by_size_conf rt_deny_by_size_conf;

static struct fd_rt_fwd_hdl * rt__deny_by_size_hdl = NULL;
static char *config_file = NULL;
#define MODULE_NAME "rt_deny_by_size"
#define DEFAULT_MAX_SIZE 4096;

/* the routing callback that handles all the tasks of this extension */
static int rt_deny_by_size_fwd_cb(void * cbdata, struct msg ** pmsg)
{
	struct msg_hdr * hdr;

	/* Get the header of the message */
	CHECK_FCT(fd_msg_hdr(*pmsg, &hdr));

	if (hdr->msg_length > rt_deny_by_size_conf.maximum_size) {
		if (hdr->msg_flags & CMD_FLAG_REQUEST) {
			/* Create an answer */
			CHECK_FCT(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, pmsg, MSGFL_ANSW_ERROR));
			CHECK_FCT(fd_msg_rescode_set(*pmsg, "DIAMETER_UNABLE_TO_COMPLY", "[rt_deny_by_size] Message is too big", NULL, 2));
			CHECK_FCT( fd_msg_send(pmsg, NULL, NULL) );
		}
	}

	return 0;
}

static void sig_hdlr(void)
{
	int old_maximum_size;

	old_maximum_size = rt_deny_by_size_conf.maximum_size;
	if (rt_deny_by_size_conf_handle(config_file) != 0) {
		fd_log_error("%s: error during config file reload, restoring previous value", MODULE_NAME);
		rt_deny_by_size_conf.maximum_size = old_maximum_size;
	}
	fd_log_notice("%s: reloaded configuration, maximum size now %d", MODULE_NAME, rt_deny_by_size_conf.maximum_size);
}

/* entry point */
static int rt_deny_by_size_entry(char * conffile)
{
	TRACE_ENTRY("%p", conffile);

	config_file = conffile;
	/* Initialize the configuration */
	memset(&rt_deny_by_size_conf, 0, sizeof(rt_deny_by_size_conf));
	rt_deny_by_size_conf.maximum_size = DEFAULT_MAX_SIZE;

	/* Parse the configuration file */
	CHECK_FCT(rt_deny_by_size_conf_handle(config_file));

	/* Register the callback */
	CHECK_FCT(fd_rt_fwd_register(rt_deny_by_size_fwd_cb, NULL, RT_FWD_REQ, &rt__deny_by_size_hdl));

	/* Register reload callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, MODULE_NAME, sig_hdlr));

	fd_log_notice("Extension 'Deny by size' initialized with maximum size %d", rt_deny_by_size_conf.maximum_size);

	/* We're done */
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	TRACE_ENTRY();

	/* Unregister the cb */
	if (rt__deny_by_size_hdl) {
		CHECK_FCT_DO(fd_rt_fwd_unregister(rt__deny_by_size_hdl, NULL), /* continue */);
	}

	/* Done */
	return ;
}

EXTENSION_ENTRY(MODULE_NAME, rt_deny_by_size_entry);
