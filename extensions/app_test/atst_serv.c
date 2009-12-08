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

/* Install the dispatch callbacks */

#include "app_test.h"

static struct disp_hdl * atst_hdl_fb = NULL; /* handler for fallback cb */
static struct disp_hdl * atst_hdl_tr = NULL; /* handler for Test-Request req cb */

/* Default callback for the application. */
static int atst_fb_cb( struct msg ** msg, struct avp * avp, struct session * sess, enum disp_action * act)
{
	/* This CB should never be called */
	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);
	
	fd_log_debug("Unexpected message received!\n");
	
	return ENOTSUP;
}

/* Callback for incoming Test-Request messages */
static int atst_tr_cb( struct msg ** msg, struct avp * avp, struct session * sess, enum disp_action * act)
{
	struct msg *ans, *qry;
	union avp_value val;
	
	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);
	
	if (msg == NULL)
		return EINVAL;
	
	/* Create answer header */
	qry = *msg;
	CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, msg, 0 ) );
	ans = *msg;
	
	/* Set the Test-AVP AVP */
	{
		struct avp * src = NULL;
		struct avp_hdr * hdr = NULL;
		
		CHECK_FCT( fd_msg_search_avp ( qry, atst_avp, &src) );
		CHECK_FCT( fd_msg_avp_hdr( src, &hdr )  );
		
		CHECK_FCT( fd_msg_avp_new ( atst_avp, 0, &avp ) );
		CHECK_FCT( fd_msg_avp_setvalue( avp, hdr->avp_value ) );
		CHECK_FCT( fd_msg_avp_add( ans, MSG_BRW_LAST_CHILD, avp ) );
	}
	
	/* Set the Origin-Host, Origin-Realm, Result-Code AVPs */
	CHECK_FCT( fd_msg_rescode_set( ans, "DIAMETER_SUCCESS", NULL, NULL, 1 ) );
	
	/* Send the answer */
	CHECK_FCT( fd_msg_send( msg, NULL, NULL ) );
	
	return 0;
}

int atst_serv_init(void)
{
	struct disp_when data;
	
	TRACE_DEBUG(FULL, "Initializing dispatch callbacks for test");
	
	memset(&data, 0, sizeof(data));
	data.app = atst_appli;
	data.command = atst_cmd_r;
	
	/* fallback CB if command != Test-Request received */
	CHECK_FCT( fd_disp_register( atst_fb_cb, DISP_HOW_APPID, &data, &atst_hdl_fb ) );
	
	/* Now specific handler for Test-Request */
	CHECK_FCT( fd_disp_register( atst_tr_cb, DISP_HOW_CC, &data, &atst_hdl_tr ) );
	
	return 0;
}

void atst_serv_fini(void)
{
	if (atst_hdl_fb) {
		(void) fd_disp_unregister(&atst_hdl_fb);
	}
	if (atst_hdl_tr) {
		(void) fd_disp_unregister(&atst_hdl_tr);
	}
	
	return;
}
