/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Thomas Klausner <tk@giga.or.at>								 *
*													 *
* Copyright (c) 2019, Thomas Klausner									 *
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

/* See doc/dbg_loglevel.conf.sample for more details about the features of this extension */
#include "dbg_loglevel.h"

static char *config_file = NULL;
#define MODULE_NAME "dbg_loglevel"

static void sig_hdlr(void)
{
	int old_log_level;

	old_log_level = fd_g_debug_lvl;
	if (dbg_loglevel_conf_handle(config_file) != 0) {
		fd_log_error("%s: error during config file reload, restoring previous value", MODULE_NAME);
		fd_g_debug_lvl = old_log_level;
	}
	fd_log_notice("%s: reloaded configuration, log level now %d", MODULE_NAME, fd_g_debug_lvl);
}

/* entry point */
static int dbg_loglevel_entry(char * conffile)
{
	TRACE_ENTRY("%p", conffile);

	config_file = conffile;

	/* default set by main program */
	/* fd_g_debug_lvl = FD_LOG_NOTICE; */

	/* Parse the configuration file */
	CHECK_FCT(dbg_loglevel_conf_handle(config_file));

	/* Register reload callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, MODULE_NAME, sig_hdlr));

	fd_log_notice("Extension 'Loglevel' initialized with log level %d", fd_g_debug_lvl);

	/* We're done */
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	TRACE_ENTRY();

	/* Nothing to do */
	return ;
}

EXTENSION_ENTRY(MODULE_NAME, dbg_loglevel_entry);
