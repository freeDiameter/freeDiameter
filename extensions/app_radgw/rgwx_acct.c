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

/* RADIUS Accounting-Request messages translation plugin */

#include "rgw_common.h"

/* The state we keep for this plugin */
struct rgwp_config {
	struct {
		struct dict_object * Error_Message;		/* Error-Message */
		struct dict_object * Error_Reporting_Host;	/* Error-Reporting-Host */
		struct dict_object * Failed_AVP;		/* Failed-AVP */
		struct dict_object * Origin_Host;		/* Origin-Host */
		struct dict_object * Origin_Realm;		/* Origin-Realm */
		struct dict_object * Result_Code;		/* Result-Code */
		struct dict_object * Session_Id;		/* Session-Id */
		struct dict_object * State;			/* State */
		struct dict_object * User_Name;			/* User-Name */
	} dict; /* cache of the dictionary objects we use */
	char * confstr;
};

/* Initialize the plugin */
static int acct_conf_parse(char * conffile, struct rgwp_config ** state)
{
	struct rgwp_config * new;
	
	TRACE_ENTRY("%p %p", conffile, state);
	CHECK_PARAMS( state );
	
	CHECK_MALLOC( new = malloc(sizeof(struct rgwp_config)) );
	memset(new, 0, sizeof(struct rgwp_config));
	
	new->confstr = conffile;
	
	/* Resolve all dictionary objects we use */
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Error-Message", &new->dict.Error_Message, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Error-Reporting-Host", &new->dict.Error_Reporting_Host, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Failed-AVP", &new->dict.Failed_AVP, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &new->dict.Origin_Host, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Realm", &new->dict.Origin_Realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &new->dict.Result_Code, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Session-Id", &new->dict.Session_Id, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "State", &new->dict.State, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "User-Name", &new->dict.User_Name, ENOENT) );
	
	*state = new;
	return 0;
}

/* deinitialize */
static void acct_conf_free(struct rgwp_config * state)
{
	TRACE_ENTRY("%p", state);
	CHECK_PARAMS_DO( state, return );
	free(state);
	return;
}

/* Incoming RADIUS request */
static int acct_rad_req( struct rgwp_config * cs, struct session * session, struct radius_msg * rad_req, struct radius_msg ** rad_ans, struct msg ** diam_fw, struct rgw_client * cli )
{
	int idx;
	int got_id = 0;
	uint32_t status_type;
	
	TRACE_ENTRY("%p %p %p %p %p %p", cs, session, rad_req, rad_ans, diam_fw, cli);
	CHECK_PARAMS(rad_req && (rad_req->hdr->code == RADIUS_CODE_ACCOUNTING_REQUEST) && rad_ans && diam_fw && *diam_fw);
	
	/* Check the message contains the NAS identification */
	for (idx = 0; idx < rad_req->attr_used; idx++) {
		struct radius_attr_hdr * attr = (struct radius_attr_hdr *)(rad_req->buf + rad_req->attr_pos[idx]);
		switch (attr->type) {
			case RADIUS_ATTR_NAS_IP_ADDRESS:
			case RADIUS_ATTR_NAS_IDENTIFIER:
			case RADIUS_ATTR_NAS_IPV6_ADDRESS:
				got_id = 1;
				break;
		}
	}
	
	/* Check basic information is there */
	if (!got_id || radius_msg_get_attr_int32(rad_req, RADIUS_ATTR_ACCT_STATUS_TYPE, &status_type)) {
		TRACE_DEBUG(INFO, "[acct.rgwx] RADIUS Account-Request did not contain a NAS ip/identifier or Acct-Status-Type attribute, reject.");
		return EINVAL;
	}
	
	/* Handle the Accounting-On/Off case: nothing to do, just reply OK, since Diameter does not support this */
	if ((status_type == RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_ON) || (status_type == RADIUS_ACCT_STATUS_TYPE_ACCOUNTING_OFF)) {
		TRACE_DEBUG(FULL, "[acct.rgwx] Received Accounting-On Acct-Status-Type attribute, replying without translation to Diameter.");
		CHECK_MALLOC( *rad_ans = radius_msg_new(RADIUS_CODE_ACCOUNTING_RESPONSE, rad_req->hdr->identifier) );
		return -2;
	}
	
	/* Other cases */
	return ENOTSUP;
			/*
			      -  If the RADIUS message received is an Accounting-Request, the
        			 Acct-Status-Type attribute value must be converted to a
        			 Accounting-Record-Type AVP value.  If the Acct-Status-Type
        			 attribute value is STOP, the local server MUST issue a
        			 Session-Termination-Request message once the Diameter
        			 Accounting-Answer message has been received.
			*/

			/*
			      -  If the Accounting message contains an Acct-Termination-Cause
        			 attribute, it should be translated to the equivalent
        			 Termination-Cause AVP value.  (see below)
			*/

			/*
			      -  If the RADIUS message contains the Accounting-Input-Octets,
        			 Accounting-Input-Packets, Accounting-Output-Octets, or
        			 Accounting-Output-Packets, these attributes must be converted
        			 to the Diameter equivalents.  Further, if the Acct-Input-
        			 Gigawords or Acct-Output-Gigawords attributes are present,
        			 these must be used to properly compute the Diameter accounting
        			 AVPs.
			*/
}

static int acct_diam_ans( struct rgwp_config * cs, struct session * session, struct msg ** diam_ans, struct radius_msg ** rad_fw, struct rgw_client * cli )
{
	TRACE_ENTRY("%p %p %p %p %p", cs, session, diam_ans, rad_fw, cli);
	CHECK_PARAMS(cs);

	return ENOTSUP;
}

/* The exported symbol */
struct rgw_api rgwp_descriptor = {
	.rgwp_name       = "acct",
	.rgwp_conf_parse = acct_conf_parse,
	.rgwp_conf_free  = acct_conf_free,
	.rgwp_rad_req    = acct_rad_req,
	.rgwp_diam_ans   = acct_diam_ans
};	
