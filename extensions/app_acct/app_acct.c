/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@nict.go.jp>							 *
*													 *
* Copyright (c) 2010, WIDE Project and NICT								 *
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

/* The simple Accounting server for freeDiameter */

#include "app_acct.h"

/* Default callback for the Accounting application. */
static int acct_fallback( struct msg ** msg, struct avp * avp, struct session * sess, enum disp_action * act)
{
	/* This CB should never be called */
	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);
	
	fd_log_debug("Unexpected message received!\n");
	
	return ENOTSUP;
}


/* Callback for incoming Base Accounting Accounting-Request messages */
static int acct_cb( struct msg ** msg, struct avp * avp, struct session * sess, enum disp_action * act)
{
	struct msg_hdr *hdr = NULL;
	struct msg *ans, *qry;
	struct avp * a = NULL;
	struct avp_hdr * h = NULL;
	char * s;
	
	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);
	if (msg == NULL)
		return EINVAL;
	
	qry = *msg;
	/* Create the answer message, including the Session-Id AVP */
	CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, msg, 0 ) );
	ans = *msg;

	/* Set the Origin-Host, Origin-Realm, Result-Code AVPs */
	CHECK_FCT( fd_msg_rescode_set( ans, "DIAMETER_SUCCESS", NULL, NULL, 1 ) );

	fd_log_debug("--------------Received the following Accounting message:--------------\n");

	CHECK_FCT( fd_sess_getsid ( sess, &s ) );
	fd_log_debug("Session: %s\n", s);

	/* We may also dump other data from the message, such as Accounting session Id, number of packets, ...  */

	fd_log_debug("----------------------------------------------------------------------\n");

	/* Send the answer */
	CHECK_FCT( fd_msg_send( msg, NULL, NULL ) );
		
	return 0;
}
	

/* entry point */
static int acct_entry(char * conffile)
{
	struct disp_when data;
	
	TRACE_ENTRY("%p", conffile);
	
	/* Initialize the configuration and parse the file */
	CHECK_FCT( acct_conf_init() );
	CHECK_FCT( acct_conf_parse(conffile) );
	CHECK_FCT( acct_conf_check(conffile) );
	
	/* Now initialize the database module */
	CHECK_FCT( acct_db_init() );
	
	/* Register the dispatch callbacks */
	memset(&data, 0, sizeof(data));
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Base Accounting", &data.app, ENOENT) );
	CHECK_FCT( fd_disp_register( acct_fallback, DISP_HOW_APPID, &data, NULL ) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Accounting-Request", &data.command, ENOENT) );
	CHECK_FCT( fd_disp_register( acct_cb, DISP_HOW_CC, &data, NULL ) );
	
	/* Advertise the support for the Diameter Base Accounting application in the peer */
	CHECK_FCT( fd_disp_app_support ( data.app, NULL, 0, 1 ) );
	
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	/* Close the db connection */
	acct_db_free();
	
	/* Destroy the configuration */
	acct_conf_free();
}

EXTENSION_ENTRY("app_acct", acct_entry);
