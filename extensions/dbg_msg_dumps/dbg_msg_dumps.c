/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2013, WIDE Project and NICT								 *
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

/* This extension uses the hooks mechanism to display the full content of received and sent messages, for
   learning & debugging purpose.
   Do NOT use this extension in production environment because it will slow down all operation. */

#include <freeDiameter/extension.h>

static struct fd_hook_hdl *md_hdl = NULL;

/* The callback called when messages are received and sent */
static void md_hook_cb(enum fd_hook_type type, struct msg * msg, struct peer_hdr * peer, void * other, struct fd_hook_permsgdata *pmd, void * regdata)
{
	char * buf = NULL;
	size_t len;
	
	CHECK_MALLOC_DO( fd_msg_dump_treeview(&buf, &len, NULL, msg, fd_g_config->cnf_dict, 1, 1), 
		{ LOG_E("Error while dumping a message"); return; } );
	
	LOG_N("%s %s: %s",
		(type == HOOK_MESSAGE_RECEIVED) ? "RCV FROM" : "SENT TO",
		peer ? peer->info.pi_diamid:"<unknown>", 
		buf);

	free(buf);
}

/* Entry point */
static int md_main(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	
	CHECK_FCT( fd_hook_register( HOOK_MASK( HOOK_MESSAGE_RECEIVED, HOOK_MESSAGE_SENT ), 
					md_hook_cb, NULL, NULL, &md_hdl) );
	
	return 0;
}

/* Cleanup */
void fd_ext_fini(void)
{
	TRACE_ENTRY();
	CHECK_FCT_DO( fd_hook_unregister( md_hdl ), );
	return ;
}

EXTENSION_ENTRY("dbg_msg_dumps", md_main);
