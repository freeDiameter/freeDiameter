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

/* Install the dictionary objects */

#include "app_test.h"

struct dict_object * atst_vendor = NULL;
struct dict_object * atst_appli = NULL;
struct dict_object * atst_cmd_r = NULL;
struct dict_object * atst_cmd_a = NULL;
struct dict_object * atst_avp = NULL;

struct dict_object * atst_sess_id = NULL;
struct dict_object * atst_origin_host = NULL;
struct dict_object * atst_origin_realm = NULL;
struct dict_object * atst_dest_host = NULL;
struct dict_object * atst_dest_realm = NULL;
struct dict_object * atst_res_code = NULL;

int atst_dict_init(void)
{
	TRACE_DEBUG(FULL, "Initializing the dictionary for test");
	
	/* Create the Test Vendor */
	{
		struct dict_vendor_data data;
		data.vendor_id = atst_conf->vendor_id;
		data.vendor_name = "app_test vendor";
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_VENDOR, &data, NULL, &atst_vendor));
	}
	
	/* Create the Test Application */
	{
		struct dict_application_data data;
		data.application_id = atst_conf->appli_id;
		data.application_name = "app_test application";
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_APPLICATION, &data, atst_vendor, &atst_appli));
	}
	
	/* Create the Test-Request & Test-Answer commands */
	{
		struct dict_cmd_data data;
		data.cmd_code = atst_conf->cmd_id;
		data.cmd_name = "Test-Request";
		data.cmd_flag_mask = CMD_FLAG_PROXIABLE | CMD_FLAG_REQUEST;
		data.cmd_flag_val  = CMD_FLAG_PROXIABLE | CMD_FLAG_REQUEST;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_COMMAND, &data, atst_appli, &atst_cmd_r));
		data.cmd_name = "Test-Answer";
		data.cmd_flag_val  = CMD_FLAG_PROXIABLE;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_COMMAND, &data, atst_appli, &atst_cmd_a));
	}
	
	/* Create the Test AVP */
	{
		struct dict_avp_data data;
		data.avp_code = atst_conf->avp_id;
		data.avp_vendor = atst_conf->vendor_id;
		data.avp_name = "Test-AVP";
		data.avp_flag_mask = AVP_FLAG_VENDOR;
		data.avp_flag_val = AVP_FLAG_VENDOR;
		data.avp_basetype = AVP_TYPE_INTEGER32;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_AVP, &data, NULL, &atst_avp));
	}
	
	/* Now resolve some other useful AVPs */
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Session-Id", &atst_sess_id, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &atst_origin_host, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Realm", &atst_origin_realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Host", &atst_dest_host, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Realm", &atst_dest_realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &atst_res_code, ENOENT) );
	
	/* Create the rules for Test-Request and Test-Answer */
	{
		struct dict_rule_data data;
		data.rule_min = 1;
		data.rule_max = 1;
		
		/* Session-Id is in first position */
		data.rule_avp = atst_sess_id;
		data.rule_position = RULE_FIXED_HEAD;
		data.rule_order = 1;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_r, NULL));
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_a, NULL));
		
		data.rule_position = RULE_REQUIRED;
		/* Test-AVP is mandatory */
		data.rule_avp = atst_avp;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_r, NULL));
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_a, NULL));
		
		/* idem for Origin Host and Realm */
		data.rule_avp = atst_origin_host;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_r, NULL));
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_a, NULL));
		
		data.rule_avp = atst_origin_realm;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_r, NULL));
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_a, NULL));
		
		/* And Result-Code is mandatory for answers only */
		data.rule_avp = atst_res_code;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_a, NULL));
		
		/* And Destination-Realm for requests only */
		data.rule_avp = atst_dest_realm;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_r, NULL));
		
		/* And Destination-Host optional for requests only */
		data.rule_position = RULE_OPTIONAL;
		data.rule_min = 0;
		data.rule_avp = atst_dest_host;
		CHECK_FCT(fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &data, atst_cmd_r, NULL));
		
	}
	
	return 0;
}
