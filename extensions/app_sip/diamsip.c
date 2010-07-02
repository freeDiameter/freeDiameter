/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Alexandre Westfahl <awestfahl@freediameter.net>						 *
*													 *
* Copyright (c) 2010, Alexandre Westfahl, Teraoka Laboratory (Keio University), and the WIDE Project. 	 *		
*													 *
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
* * Neither the name of the Teraoka Laboratory nor the 							 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission of Teraoka Laboratory 						 *
*   													 *
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
#include "diamsip.h"

static struct disp_hdl * diamsip_MAR_hdl=NULL;
static struct disp_hdl * diamsip_default_hdl=NULL;

int diamsip_default_cb( struct msg ** msg, struct avp * avp, struct session * sess, enum disp_action * act)
{

	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);

	
	return 0;
}




/* entry point */
static int ds_entry()
{
	struct dict_object * app=NULL;
	struct disp_when data;
	
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Session Initiation Protocol (SIP) Application", &app, ENOENT) );
	CHECK_FCT( fd_disp_app_support ( app, NULL, 1, 0 ) );
	
	
	
	//We set usefull AVPs 
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Auth-Session-State", &sip_dict.Auth_Session_State, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Auth-Application-Id", &sip_dict.Auth_Application_Id, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Auth-Data-Item", &sip_dict.SIP_Auth_Data_Item, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authorization", &sip_dict.SIP_Authorization, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authenticate", &sip_dict.SIP_Authenticate, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Number-Auth-Items", &sip_dict.SIP_Number_Auth_Items, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authentication-Scheme", &sip_dict.SIP_Authentication_Scheme, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authentication-Info", &sip_dict.SIP_Authentication_Info, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Server-URI", &sip_dict.SIP_Server_URI, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Method", &sip_dict.SIP_Method, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-AOR", &sip_dict.SIP_AOR, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Realm", &sip_dict.Digest_Realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-URI", &sip_dict.Digest_URI, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Nonce", &sip_dict.Digest_Nonce, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-CNonce", &sip_dict.Digest_CNonce, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Method", &sip_dict.Digest_Method, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Nonce-Count", &sip_dict.Digest_Nonce_Count, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Realm", &sip_dict.Digest_Realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Response", &sip_dict.Digest_Response, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Response-Auth", &sip_dict.Digest_Response_Auth, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Username", &sip_dict.Digest_Username, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Algorithm", &sip_dict.Digest_Algorithm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-QoP", &sip_dict.Digest_QOP, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "User-Name", &sip_dict.User_Name, ENOENT) );
	
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-HA1", &sip_dict.Digest_HA1, ENOENT) );
	
	
	//Register Application
	memset(&data, 0, sizeof(data));
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Session Initiation Protocol (SIP) Application", &data.app, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Multimedia-Auth-Request", &data.command, ENOENT) );
	
	//Callback for unexpected messages
	CHECK_FCT( fd_disp_register( diamsip_MAR_cb, DISP_HOW_APPID, &data, &diamsip_default_hdl ) );
	
	//**Command Codes
	//MAR
	CHECK_FCT( fd_disp_register( diamsip_MAR_cb, DISP_HOW_CC, &data, &diamsip_MAR_hdl ) );
	
	//TRACE_DEBUG(INFO,"*%s*%s*%s*%s*",DB_SERVER,DB_USERNAME, DB_PASSWORD, DB_DATABASE);
	//We start database connection
	if(start_mysql_connection(DB_SERVER,DB_USERNAME, DB_PASSWORD, DB_DATABASE))
		return 1;
	
	CHECK_FCT(fd_sess_handler_create(&ds_sess_hdl, free));
	

	//listnonce=NULL;
	return 0;
}

//Cleanup callback
void fd_ext_fini(void)
{
	
	if (diamsip_MAR_cb) {
		(void) fd_disp_unregister(&diamsip_MAR_hdl);
		CHECK_FCT_DO( fd_sess_handler_destroy(&ds_sess_hdl),return);
	}
	
	//We close database connection
	close_mysql_connection();
	
	//We delete the chained list of nonces
	//nonce_deletelistnonce();
	//TODO:NONCE
	
	TRACE_ENTRY();
	return ;
}

EXTENSION_ENTRY("diam_sip", ds_entry);


/*






test set for digest calculate

TRACE_DEBUG(FULL,"TEST");
									DigestCalcHA1("MD5", "12345678", "example.com", "secret", "3bada1a0","56593a80", HA1);
									TRACE_DEBUG(FULL,"TEST->HA1 done: *%s*",HA1);
      									DigestCalcResponse(HA1, "3bada1a0", "00000001", "56593a80", "auth","INVITE", "sip:97226491335@example.com", HA2, response);
      									DigestCalcResponseAuth(HA1, "3bada1a0", "00000001", "56593a80", "auth","INVITE", "sip:97226491335@example.com", HA2, responseauth);
	
	
	
	
	
	
old digest reponse check

						struct avp_hdr * tempavphdr=NULL;
						
						
						CHECK_FCT(fd_msg_browse ( avp, MSG_BRW_WALK, &tempavp, NULL) );
						
						while(tempavp)
						{
							CHECK_FCT( fd_msg_avp_hdr( tempavp, &tempavphdr )  );
							
							if(tempavphdr->avp_code==380)
							{
								found_response=0;
								//We have not found it but we finished looking in this Auth-Data-Item
								tempavp=NULL; 
							}
							else if(tempavphdr->avp_code==103)
							{
								found_response=1;
								//We found it, we can leave the loop
								tempavp=NULL; 
							}
							else
							{
								CHECK_FCT(fd_msg_browse ( tempavp, MSG_BRW_WALK, &tempavp, NULL) );
							}
						}
*/
