/*********************************************************************************************************
 * Software License Agreement (BSD License)                                                               *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright (c) 2013, Thomas Klausner                                                                    *
 * All rights reserved.                                                                                   *
 *                                                                                                        *
 * Written under contract by nfotex IT GmbH, http://nfotex.com/                                           *
 *                                                                                                        *
 * Redistribution and use of this software in source and binary forms, with or without modification, are  *
 * permitted provided that the following conditions are met:                                              *
 *                                                                                                        *
 * * Redistributions of source code must retain the above                                                 *
 *   copyright notice, this list of conditions and the                                                    *
 *   following disclaimer.                                                                                *
 *                                                                                                        *
 * * Redistributions in binary form must reproduce the above                                              *
 *   copyright notice, this list of conditions and the                                                    *
 *   following disclaimer in the documentation and/or other                                               *
 *   materials provided with the distribution.                                                            *
 *                                                                                                        *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED *
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A *
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR *
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT     *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF   *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
 *********************************************************************************************************/

/* 
 * Dictionary definitions for objects specified for DCCA by 3GPP.
 *
 * This extensions contains a lot of AVPs from various 3GPP standards
 * documents, and some rules for the grouped AVPs described therein.
 *
 * This extension does not contain ALL AVPs described by 3GPP, but
 * quite a big number of them.
 *
 * When extending the AVPs, please edit dict_dcca_3gpp.org instead and
 * create pastable code with contrib/tools/org_to_fd.pl.
 *
 * Some points of consideration:
 * 1. This dictionary could be split up per document.
 *
 * + pro: you can only load the AVPs/Rules you're interested in ->
 * smaller memory size
 *
 * - con: the documents use AVPs from each other A LOT, so setting the
 * dependencies correctly will be annoying
 *
 * - con: you need to load all of them as extensions
 *
 * 2. This dictionary contains ONE AVP in the "3GPP2" vendor space,
 * since I found it wasteful to write a separate dictionary just for
 * one AVP. Also, it is defined in a 3GPP document.
 *
 * 3. While there are quite a number of rules here already, many more
 * are missing. I've only added rules for those grouped AVPs or
 * commands in which I was concretely interested so far; many more
 * will need to be added to make this complete.
 *
 * That being said, I hope this will be useful for you.
 *
 */


/*
 * Some comments on the 3GPP Standards documents themselves:
 *
 * 1. It would be good if 29.061 was reviewed to check for each AVP if
 * it is Mandatory or not. The data currently in the document does not
 * match what was in the previous version of the freeDiameter
 * extension (the one that existed before I rewrote it) or what I saw
 * so far. IIRC, even the table and the document contradict each
 * other.
 *
 * 2. 29.140 has conflicting AVP names with other documents:
 *   - Sequence-Number is also in 32.329
 *   - Recipient-Address is also in 32.299
 *   - Status is also in 32.299
 *
 * 3. 29.229 has name conflict with 29.329 about User-Data (different
 * AVP code 702, instead of 606) -- the weird thing is, the latter
 * uses some AVPs from the former, but not this one.
*/
#include <freeDiameter/extension.h>


/* The content of this file follows the same structure as dict_base_proto.c */

#define CHECK_dict_new( _type, _data, _parent, _ref )	\
	CHECK_FCT(  fd_dict_new( fd_g_config->cnf_dict, (_type), (_data), (_parent), (_ref))  );

#define CHECK_dict_search( _type, _criteria, _what, _result )	\
	CHECK_FCT(  fd_dict_search( fd_g_config->cnf_dict, (_type), (_criteria), (_what), (_result), ENOENT) );

struct local_rules_definition {
	struct dict_avp_request avp_vendor_plus_name;
	enum rule_position	position;
	int 			min;
	int			max;
};

#define RULE_ORDER( _position ) ((((_position) == RULE_FIXED_HEAD) || ((_position) == RULE_FIXED_TAIL)) ? 1 : 0 )

/* Attention! This version of the macro uses AVP_BY_NAME_AND_VENDOR, in contrast to most other copies! */
#define PARSE_loc_rules( _rulearray, _parent) {								\
	int __ar;											\
	for (__ar=0; __ar < sizeof(_rulearray) / sizeof((_rulearray)[0]); __ar++) {			\
		struct dict_rule_data __data = { NULL, 							\
			(_rulearray)[__ar].position,							\
			0, 										\
			(_rulearray)[__ar].min,								\
			(_rulearray)[__ar].max};							\
		__data.rule_order = RULE_ORDER(__data.rule_position);					\
		CHECK_FCT(  fd_dict_search( 								\
			fd_g_config->cnf_dict,								\
			DICT_AVP, 									\
			AVP_BY_NAME_AND_VENDOR, 							\
			&(_rulearray)[__ar].avp_vendor_plus_name,					\
			&__data.rule_avp, 0 ) );							\
		if ( !__data.rule_avp ) {								\
			LOG_E( "AVP Not found: '%s'", (_rulearray)[__ar].avp_vendor_plus_name.avp_name);		\
			return ENOENT;									\
		}											\
		CHECK_FCT_DO( fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &__data, _parent, NULL),	\
			{							        		\
				LOG_E( "Error on rule with AVP '%s'",      			\
					    (_rulearray)[__ar].avp_vendor_plus_name.avp_name);		\
				return EINVAL;					      			\
			} );							      			\
	}									      			\
}

#define enumval_def_u32( _val_, _str_ ) \
		{ _str_, 		{ .u32 = _val_ }}

#define enumval_def_os( _len_, _val_, _str_ ) \
		{ _str_, 		{ .os = { .data = (unsigned char *)_val_, .len = _len_ }}}


static int dict_dcca_3gpp_entry(char * conffile)
{
	/*==================================================================*/
	/* Applications section                                             */
	/*==================================================================*/

	{		
                /* Create the vendors */
                {
                        struct dict_vendor_data vendor_data = { 10415, "3GPP" };
                        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_VENDOR, &vendor_data, NULL, NULL));
                }
                {
                        struct dict_vendor_data vendor_data = { 5535, "3GPP2" };
                        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_VENDOR, &vendor_data, NULL, NULL));
                }

	}
	
	/*==================================================================*/
	/* AVPs section                                                     */
	/*==================================================================*/


	/* 3GPP Experimental-Result-Code ENUMVAL Unsigned32                 */
	/* 3GPP TS 29.230 V15.7.0 (2019-12) section 8.1                     */
	{
		struct dict_object 	*type;
		struct dict_type_data 	 tdata = { AVP_TYPE_UNSIGNED32,	"Enumerated(3GPP/Experimental-Result-Code)", NULL, NULL};
		struct dict_enumval_data tvals[] = {
			enumval_def_u32(2001,	"DIAMETER_FIRST_REGISTRATION"),
			enumval_def_u32(2002,	"DIAMETER_SUBSEQUENT_REGISTRATION"),
			enumval_def_u32(2003,	"DIAMETER_UNREGISTERED_SERVICE"),
			/*
			 * 3GPP TS 29.229 V5.3.0 (2003-03) renamed 2004 from DIAMETER_SUCCESS_NOT SUPPORTED_USER_DATA.
			 */
			enumval_def_u32(2004,	"DIAMETER_SUCCESS_SERVER_NAME_NOT_STORED"),
			/*
			 * 3GPP TS 29.229 V5.3.0 (2003-03) renamed 2005 from DIAMETER_SUCCESS_SERVER_NAME_NOT_STORED.
			 * 3GPP TS 29.229 V7.2.0 (2006-06) deprecated 2005 DIAMETER_SERVER_SELECTION.
			 */
			enumval_def_u32(2005,	"DIAMETER_SERVER_SELECTION"),
			enumval_def_u32(2021,	"DIAMETER_PDP_CONTEXT_DELETION_INDICATION"),
			enumval_def_u32(2421,	"DIAMETER_QOS_FLOW_DELETION_INDICATION"),
			enumval_def_u32(4100,	"DIAMETER_USER_DATA_NOT_AVAILABLE"),
			enumval_def_u32(4101,	"DIAMETER_PRIOR_UPDATE_IN_PROGRESS"),
			enumval_def_u32(4121,	"DIAMETER_ERROR_OUT_OF_RESOURCES"),
			enumval_def_u32(4141,	"DIAMETER_PCC_BEARER_EVENT"),
			enumval_def_u32(4142,	"DIAMETER_BEARER_EVENT"),
			enumval_def_u32(4143,	"DIAMETER_AN_GW_FAILED"),
			enumval_def_u32(4144,	"DIAMETER_PENDING_TRANSACTION"),
			enumval_def_u32(4145,	"DIAMETER_UE_STATUS_SUSPEND"),
			enumval_def_u32(4181,	"DIAMETER_AUTHENTICATION_DATA_UNAVAILABLE"),
			enumval_def_u32(4182,	"DIAMETER_ERROR_CAMEL_SUBSCRIPTION_PRESENT"),
			/*
			 * DIAMETER_ERROR_ABSENT_USER name conflict between:
			 * - 3GPP TS 29.173 § 6.3.4.1 DIAMETER_ERROR_ABSENT_USER (4201). (For SLh).
			 * - 3GPP TS 29.338 § 7.3.3 DIAMETER_ERROR_ABSENT_USER (5550). (For S6c, SGd).
			 * Rename 4201 from 3GPP TS 29.173 to DIAMETER_ERROR_ABSENT_USER-29.173.
			 */
			enumval_def_u32(4201,	"DIAMETER_ERROR_ABSENT_USER-29.173"),
			enumval_def_u32(4221,	"DIAMETER_ERROR_UNREACHABLE_USER"),
			enumval_def_u32(4222,	"DIAMETER_ERROR_SUSPENDED_USER"),
			enumval_def_u32(4223,	"DIAMETER_ERROR_DETACHED_USER"),
			enumval_def_u32(4224,	"DIAMETER_ERROR_POSITIONING_DENIED"),
			enumval_def_u32(4225,	"DIAMETER_ERROR_POSITIONING_FAILED"),
			enumval_def_u32(4226,	"DIAMETER_ERROR_UNKNOWN_UNREACHABLE LCS_CLIENT"),
			enumval_def_u32(4241,	"DIAMETER_ERROR_NO_AVAILABLE_POLICY_COUNTERS"),
			enumval_def_u32(4261,	"REQUESTED_SERVICE_TEMPORARILY_NOT_AUTHORIZED"),
			enumval_def_u32(5001,	"DIAMETER_ERROR_USER_UNKNOWN"),
			enumval_def_u32(5002,	"DIAMETER_ERROR_IDENTITIES_DONT_MATCH"),
			enumval_def_u32(5003,	"DIAMETER_ERROR_IDENTITY_NOT_REGISTERED"),
			enumval_def_u32(5004,	"DIAMETER_ERROR_ROAMING_NOT_ALLOWED"),
			enumval_def_u32(5005,	"DIAMETER_ERROR_IDENTITY_ALREADY_REGISTERED"),
			enumval_def_u32(5006,	"DIAMETER_ERROR_AUTH_SCHEME_NOT_SUPPORTED"),
			enumval_def_u32(5007,	"DIAMETER_ERROR_IN_ASSIGNMENT_TYPE"),
			enumval_def_u32(5008,	"DIAMETER_ERROR_TOO_MUCH_DATA"),
			enumval_def_u32(5009,	"DIAMETER_ERROR_NOT_SUPPORTED_USER_DATA"),
			enumval_def_u32(5011,	"DIAMETER_ERROR_FEATURE_UNSUPPORTED"),
			enumval_def_u32(5012,	"DIAMETER_ERROR_SERVING_NODE_FEATURE_UNSUPPORTED"),
			enumval_def_u32(5041,	"DIAMETER_ERROR_USER_NO_WLAN_SUBSCRIPTION"),
			enumval_def_u32(5042,	"DIAMETER_ERROR_W-APN_UNUSED_BY_USER"),
			enumval_def_u32(5043,	"DIAMETER_ERROR_NO_ACCESS_INDEPENDENT_SUBSCRIPTION"),
			enumval_def_u32(5044,	"DIAMETER_ERROR_USER_NO_W-APN_SUBSCRIPTION"),
			enumval_def_u32(5045,	"DIAMETER_ERROR_UNSUITABLE_NETWORK"),
			enumval_def_u32(5061,	"INVALID_SERVICE_INFORMATION"),
			enumval_def_u32(5062,	"FILTER_RESTRICTIONS"),
			enumval_def_u32(5063,	"REQUESTED_SERVICE_NOT_AUTHORIZED"),
			enumval_def_u32(5064,	"DUPLICATED_AF_SESSION"),
			enumval_def_u32(5065,	"IP-CAN_SESSION_NOT_AVAILABLE"),
			enumval_def_u32(5066,	"UNAUTHORIZED_NON_EMERGENCY_SESSION"),
			enumval_def_u32(5067,	"UNAUTHORIZED_SPONSORED_DATA_CONNECTIVITY"),
			enumval_def_u32(5068,	"TEMPORARY_NETWORK_FAILURE"),
			enumval_def_u32(5100,	"DIAMETER_ERROR_USER_DATA_NOT_RECOGNIZED"),
			enumval_def_u32(5101,	"DIAMETER_ERROR_OPERATION_NOT_ALLOWED"),
			enumval_def_u32(5102,	"DIAMETER_ERROR_USER_DATA_CANNOT_BE_READ"),
			enumval_def_u32(5103,	"DIAMETER_ERROR_USER_DATA_CANNOT_BE_MODIFIED"),
			enumval_def_u32(5104,	"DIAMETER_ERROR_USER_DATA_CANNOT_BE_NOTIFIED"),
			enumval_def_u32(5105,	"DIAMETER_ERROR_TRANSPARENT_DATA OUT_OF_SYNC"),
			enumval_def_u32(5106,	"DIAMETER_ERROR_SUBS_DATA_ABSENT"),
			enumval_def_u32(5107,	"DIAMETER_ERROR_NO_SUBSCRIPTION_TO_DATA"),
			enumval_def_u32(5108,	"DIAMETER_ERROR_DSAI_NOT_AVAILABLE"),
			enumval_def_u32(5120,	"DIAMETER_ERROR_START_INDICATION"),
			enumval_def_u32(5121,	"DIAMETER_ERROR_STOP_INDICATION"),
			enumval_def_u32(5122,	"DIAMETER_ERROR_UNKNOWN_MBMS_BEARER_SERVICE"),
			enumval_def_u32(5123,	"DIAMETER_ERROR_SERVICE_AREA"),
			enumval_def_u32(5140,	"DIAMETER_ERROR_INITIAL_PARAMETERS"),
			enumval_def_u32(5141,	"DIAMETER_ERROR_TRIGGER_EVENT"),
			enumval_def_u32(5142,	"DIAMETER_PCC_RULE_EVENT"),
			enumval_def_u32(5143,	"DIAMETER_ERROR_BEARER_NOT_AUTHORIZED"),
			enumval_def_u32(5144,	"DIAMETER_ERROR_TRAFFIC_MAPPING_INFO_REJECTED"),
			enumval_def_u32(5145,	"DIAMETER_QOS_RULE_EVENT"),
			enumval_def_u32(5147,	"DIAMETER_ERROR_CONFLICTING_REQUEST"),
			enumval_def_u32(5148,	"DIAMETER_ADC_RULE_EVENT"),
			enumval_def_u32(5149,	"DIAMETER_ERROR_NBIFOM_NOT_AUTHORIZED"),
			enumval_def_u32(5401,	"DIAMETER_ERROR_IMPI_UNKNOWN"),
			enumval_def_u32(5402,	"DIAMETER_ERROR_NOT_AUTHORIZED"),
			enumval_def_u32(5403,	"DIAMETER_ERROR_TRANSACTION_IDENTIFIER_INVALID"),
			enumval_def_u32(5420,	"DIAMETER_ERROR_UNKNOWN_EPS_SUBSCRIPTION"),
			enumval_def_u32(5421,	"DIAMETER_ERROR_RAT_NOT_ALLOWED"),
			enumval_def_u32(5422,	"DIAMETER_ERROR_EQUIPMENT_UNKNOWN"),
			enumval_def_u32(5423,	"DIAMETER_ERROR_UNKNOWN_SERVING_NODE"),
			enumval_def_u32(5450,	"DIAMETER_ERROR_USER_NO_NON_3GPP_SUBSCRIPTION"),
			enumval_def_u32(5451,	"DIAMETER_ERROR_USER_NO_APN_SUBSCRIPTION"),
			enumval_def_u32(5452,	"DIAMETER_ERROR_RAT_TYPE_NOT_ALLOWED"),
			enumval_def_u32(5453,	"DIAMETER_ERROR_LATE_OVERLAPPING_REQUEST"),
			enumval_def_u32(5454,	"DIAMETER_ERROR_TIMED_OUT_REQUEST"),
			enumval_def_u32(5470,	"DIAMETER_ERROR_SUBSESSION"),
			enumval_def_u32(5471,	"DIAMETER_ERROR_ONGOING_SESSION_ESTABLISHMENT"),
			enumval_def_u32(5490,	"DIAMETER_ERROR_UNAUTHORIZED_REQUESTING_NETWORK"),
			enumval_def_u32(5510,	"DIAMETER_ERROR_UNAUTHORIZED_REQUESTING_ENTITY"),
			enumval_def_u32(5511,	"DIAMETER_ERROR_UNAUTHORIZED_SERVICE"),
			enumval_def_u32(5512,	"DIAMETER_ERROR_REQUESTED_RANGE_IS_NOT ALLOWED"),
			enumval_def_u32(5513,	"DIAMETER_ERROR_CONFIGURATION_EVENT_STORAGE_NOT_SUCCESSFUL"),
			enumval_def_u32(5514,	"DIAMETER_ERROR_CONFIGURATION_EVENT_NON_EXISTANT"),
			enumval_def_u32(5515,	"DIAMETER_ERROR_SCEF_REFERENCE_ID_UNKNOWN"),
			enumval_def_u32(5530,	"DIAMETER_ERROR_INVALID_SME_ADDRESS"),
			enumval_def_u32(5531,	"DIAMETER_ERROR_SC_CONGESTION"),
			enumval_def_u32(5532,	"DIAMETER_ERROR_SM_PROTOCOL"),
			enumval_def_u32(5533,	"DIAMETER_ERROR_TRIGGER_REPLACE_FAILURE"),
			enumval_def_u32(5534,	"DIAMETER_ERROR_TRIGGER_RECALL_FAILURE"),
			enumval_def_u32(5535,	"DIAMETER_ERROR_ORIGINAL_MESSAGE_NOT_PENDING"),
			enumval_def_u32(5550,	"DIAMETER_ERROR_ABSENT_USER"),
			enumval_def_u32(5551,	"DIAMETER_ERROR_USER_BUSY_FOR_MT_SMS"),
			enumval_def_u32(5552,	"DIAMETER_ERROR_FACILITY_NOT_SUPPORTED"),
			enumval_def_u32(5553,	"DIAMETER_ERROR_ILLEGAL_USER"),
			enumval_def_u32(5554,	"DIAMETER_ERROR_ILLEGAL_EQUIPMENT"),
			enumval_def_u32(5555,	"DIAMETER_ERROR_SM_DELIVERY_FAILURE"),
			enumval_def_u32(5556,	"DIAMETER_ERROR_SERVICE_NOT_SUBSCRIBED"),
			enumval_def_u32(5557,	"DIAMETER_ERROR_SERVICE_BARRED"),
			enumval_def_u32(5558,	"DIAMETER_ERROR_MWD_LIST_FULL"),
			enumval_def_u32(5570,	"DIAMETER_ERROR_UNKNOWN_POLICY_COUNTERS"),
			enumval_def_u32(5590,	"DIAMETER_ERROR_ORIGIN_ALUID_UNKNOWN"),
			enumval_def_u32(5591,	"DIAMETER_ERROR_TARGET_ALUID_UNKNOWN"),
			enumval_def_u32(5592,	"DIAMETER_ERROR_PFID_UNKNOWN"),
			enumval_def_u32(5593,	"DIAMETER_ERROR_APP_REGISTER_REJECT"),
			enumval_def_u32(5594,	"DIAMETER_ERROR_PROSE_MAP_REQUEST_DISALLOWED"),
			enumval_def_u32(5595,	"DIAMETER_ERROR_MAP_REQUEST_REJECT"),
			enumval_def_u32(5596,	"DIAMETER_ERROR_REQUESTING_RPAUID_UNKNOWN"),
			enumval_def_u32(5597,	"DIAMETER_ERROR_UNKNOWN_OR_INVALID_TARGET_SET"),
			enumval_def_u32(5598,	"DIAMETER_ERROR_MISSING_APPLICATION_DATA"),
			enumval_def_u32(5599,	"DIAMETER_ERROR_AUTHORIZATION_REJECT"),
			enumval_def_u32(5600,	"DIAMETER_ERROR_DISCOVERY_NOT_PERMITTED"),
			enumval_def_u32(5601,	"DIAMETER_ERROR_TARGET_RPAUID_UNKNOWN"),
			enumval_def_u32(5602,	"DIAMETER_ERROR_INVALID_APPLICATION_DATA"),
			enumval_def_u32(5610,	"DIAMETER_ERROR_UNKNOWN_PROSE_SUBSCRIPTION"),
			enumval_def_u32(5611,	"DIAMETER_ERROR_PROSE_NOT_ALLOWED"),
			enumval_def_u32(5612,	"DIAMETER_ERROR_UE_LOCATION_UNKNOWN"),
			enumval_def_u32(5630,	"DIAMETER_ERROR_NO_ASSOCIATED_DISCOVERY_FILTER"),
			enumval_def_u32(5631,	"DIAMETER_ERROR_ANNOUNCING_UNAUTHORIZED_IN_PLMN"),
			enumval_def_u32(5632,	"DIAMETER_ERROR_INVALID_APPLICATION_CODE"),
			enumval_def_u32(5633,	"DIAMETER_ERROR_PROXIMITY_UNAUTHORIZED"),
			enumval_def_u32(5634,	"DIAMETER_ERROR_PROXIMITY_REJECTED"),
			enumval_def_u32(5635,	"DIAMETER_ERROR_NO_PROXIMITY_REQUEST"),
			enumval_def_u32(5636,	"DIAMETER_ERROR_UNAUTHORIZED_SERVICE_IN_THIS_PLMN"),
			enumval_def_u32(5637,	"DIAMETER_ERROR_PROXIMITY_CANCELLED"),
			enumval_def_u32(5638,	"DIAMETER_ERROR_INVALID_TARGET_PDUID"),
			enumval_def_u32(5639,	"DIAMETER_ERROR_INVALID_TARGET_RPAUID"),
			enumval_def_u32(5640,	"DIAMETER_ERROR_NO_ASSOCIATED_RESTRICTED_CODE"),
			enumval_def_u32(5641,	"DIAMETER_ERROR_INVALID_DISCOVERY_TYPE"),
			enumval_def_u32(5650,	"DIAMETER_ERROR_REQUESTED_LOCATION_NOT_SERVED"),
			enumval_def_u32(5651,	"DIAMETER_ERROR_INVALID_EPS_BEARER"),
			enumval_def_u32(5652,	"DIAMETER_ERROR_NIDD_CONFIGURATION_NOT_AVAILABLE"),
			enumval_def_u32(5653,	"DIAMETER_ERROR_USER_TEMPORARILY_UNREACHABLE"),
			enumval_def_u32(5670,	"DIAMETER_ERROR_UNKNKOWN_DATA"),
			enumval_def_u32(5671,	"DIAMETER_ERROR_REQUIRED_KEY_NOT_PROVIDED"),
			enumval_def_u32(5690,	"DIAMETER_ERROR_UNKNOWN_V2X_SUBSCRIPTION"),
			enumval_def_u32(5691,	"DIAMETER_ERROR_V2X_NOT_ALLOWED"),
		};
		int i;
		/* Create the Enumerated type and enumerated values */
		CHECK_dict_new( DICT_TYPE, &tdata , NULL, &type);
		for (i = 0; i < sizeof(tvals) / sizeof(tvals[0]); i++) {
			CHECK_dict_new( DICT_ENUMVAL, &tvals[i], type, NULL);
		}
	}

	/*==================================================================*/
	/* Create AVPs via generated add_avps()                             */
	/*==================================================================*/

	extern int add_avps();
	CHECK_FCT( add_avps() );

	/*==================================================================*/
	/* Rules section                                                    */
	/*==================================================================*/

	/* 29.212 */
	
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Allocation-Retention-Priority";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Priority-Level" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Pre-emption-Capability" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Pre-emption-Vulnerability" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "QoS-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "QoS-Class-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Max-Requested-Bandwidth-UL" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Max-Requested-Bandwidth-DL" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Guaranteed-Bitrate-UL" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Guaranteed-Bitrate-DL" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Bearer-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Allocation-Retention-Priority" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "APN-Aggregate-Max-Bitrate-UL" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "APN-Aggregate-Max-Bitrate-DL" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	/* 32.299 */

	{
		/* additional allowed AVPs */
		struct dict_object *ccr;
		CHECK_dict_search(DICT_COMMAND, CMD_BY_NAME, "Credit-Control-Request", &ccr);
		struct local_rules_definition rules[] = 
			{
				{ { .avp_vendor = 10415, .avp_name = "AoC-Request-Type"}, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Information"}, RULE_OPTIONAL, -1, 1 },
			};
		PARSE_loc_rules(rules, ccr);
        }

	{
		/* additional allowed AVPs */
		struct dict_object *ccr;
		CHECK_dict_search(DICT_COMMAND, CMD_BY_NAME, "Credit-Control-Answer", &ccr);
		struct local_rules_definition rules[] = 
			{
				{ { .avp_vendor = 10415, .avp_name = "Low-Balance-Indication"}, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Remaining-Balance"}, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Information"}, RULE_OPTIONAL, -1, 1 },
			};
		PARSE_loc_rules(rules, ccr);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Address-Domain";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Domain-Name" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-IMSI-MCC-MNC" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Application-Server-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Application-Server" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Application-Provided-Called-Party-Address" }, RULE_OPTIONAL, -1, -1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Destination-Interface";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Interface-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Interface-Text" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Interface-Port" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Interface-Type" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Envelope";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Envelope-Start-Time" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Envelope-End-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 0,     .avp_name = "CC-Total-Octets" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 0,     .avp_name = "CC-Input-Octets" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 0,     .avp_name = "CC-Output-Octets" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 0,     .avp_name = "CC-Service-Specific-Units" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Event-Type";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "SIP-Method" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Event" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Expires" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "IMS-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Event-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Role-Of-Node" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Node-Functionality" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "User-Session-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Outgoing-Session-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Session-Priority" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Calling-Party-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Called-Party-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Called-Asserted-Identity" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Number-Portability-Routing-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Carrier-Select-Routing-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Alternate-Charged-Party-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Requested-Party-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Associated-URI" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Time-Stamps" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Application-Server-Information" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Inter-Operator-Identifier" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Transit-IOI-List" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IMS-Charging-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SDP-Session-Description" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "SDP-Media-Component" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Served-Party-IP-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Server-Capabilities" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Trunk-Group-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Bearer-Service" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Specific-Info" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Message-Body" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Cause-Code" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Access-Network-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Early-Media-Description" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "IMS-Communication-Service-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IMS-Application-Reference-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Online-Charging-Flag" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Real-Time-Tariff-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Account-Expiration" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Initial-IMS-Charging-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "NNI-Information" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "From-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IMS-Emergency-Indicator" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Access-Transfer-Information" }, RULE_OPTIONAL, -1, -1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Message-Class";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Class-Identifier" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Token-Text" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "MMS-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Originator-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Recipient-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Submission-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MM-Content-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Priority" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Message-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Message-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Message-Size" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Message-Class" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Delivery-Report-Requested" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Read-Reply-Report-Requested" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MMBox-Storage-Requested" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Applic-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Reply-Applic-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Aux-Applic-Info" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Content-Class" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "DRM-Content" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Adaptations" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "VASP-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "VAS-ID" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

        {
		/* Multiple-Services-Credit-Control */
		/* additional allowed AVPs */
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 0;
		vpa.avp_name = "Multiple-Services-Credit-Control";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Time-Quota-Threshold" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Volume-Quota-Threshold" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Unit-Quota-Threshold" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Quota-Holding-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Quota-Consumption-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Reporting-Reason" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Trigger" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PS-Furnish-Charging-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Refund-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "AF-Correlation-Information" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Envelope" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Envelope-Reporting" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Time-Quota-Mechanism" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Specific-Info" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "QoS-Information" }, RULE_OPTIONAL, -1, 1 },
			};
		PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Offline-Charging";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Quota-Consumption-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Time-Quota-Mechanism" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Envelope-Reporting" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 0,     .avp_name = "Multiple-Services-Credit-Control" }, RULE_OPTIONAL, -1, -1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Originator-Address";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Address-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Data" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Domain" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Originator-Interface";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Interface-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Interface-Text" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Interface-Port" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Interface-Type" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Originator-Received-Address";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Address-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Data" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Domain" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "PS-Furnish-Charging-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "3GPP-Charging-Id" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PS-Free-Format-Data" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PS-Append-Free-Format-Data" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "PS-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "3GPP-Charging-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PDN-Connection-Charging-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Node-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-PDP-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PDP-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "PDP-Address-Prefix-Length" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Dynamic-Address-Flag" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Dynamic-Address-Flag-Extension" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "QoS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SGSN-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "GGSN-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "SGW-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "CG-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Serving-Node-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SGW-Change" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-IMSI-MCC-MNC" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IMSI-Unauthenticated-Flag" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-GGSN-MCC-MNC" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-NSAPI" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 0,     .avp_name = "Called-Station-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-Session-Stop-Indicator" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-Selection-Mode" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-Charging-Characteristics" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Charging-Characteristics-Selection-Mode" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-SGSN-MCC-MNC" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-MS-TimeZone" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Charging-Rule-Base-Name" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-User-Location-Info" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "User-CSG-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 5535,  .avp_name = "3GPP2-BSID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-RAT-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PS-Furnish-Charging-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PDP-Context-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Offline-Charging" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Traffic-Data-Volumes" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Data-Container" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 0,     .avp_name = "User-Equipment-Info" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Terminal-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Start-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Stop-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Change-Condition" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Diagnostics" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Low-Priority-Indicator" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MME-Number-for-MT-SMS" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MME-Name" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MME-Realm" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Recipient-Address";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Address-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Data" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Domain" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Addressee-Type" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Recipient-Info";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Destination-Interface" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Recipient-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Recipient-Received-Address" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Recipient-SCCP-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-Protocol-ID" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Recipient-Received-Address";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Address-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Data" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Address-Domain" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "SDP-Media-Component";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "SDP-Media-Name" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SDP-Media-Description" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Local-GW-Inserted-Indication" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IP-Realm-Default-Indication" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Transcoder-Inserted-Indication" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Media-Initiator-Flag" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Media-Initiator-Party" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "3GPP-Charging-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Access-Network-Charging-Identifier-Value" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SDP-Type" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Service-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 0,     .avp_name = "Subscription-Id" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "AoC-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "WLAN-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IMS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MMS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "LCS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "PoC-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MBMS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SMS-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "MMTel-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Service-Generic-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "IM-Information" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "DCD-Information" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "SMS-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "SMS-Node" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Client-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Originator-SCCP-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SMSC-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Data-Coding-Scheme" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-Discharge-Time" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-Message-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Originator-Interface" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-Protocol-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Reply-Path-Requested" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-Status" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-User-Data-Header" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Number-Of-Messages-Sent" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Recipient-Info" }, RULE_OPTIONAL, -1, -1 },
				{ { .avp_vendor = 10415, .avp_name = "Originator-Received-Address" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SM-Service-Type" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Time-Quota-Mechanism";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Time-Quota-Type" }, RULE_REQUIRED, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Base-Time-Interval" }, RULE_REQUIRED, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Time-Stamps";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "SIP-Request-Timestamp" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SIP-Response-Timestamp" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SIP-Request-Timestamp-Fraction" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "SIP-Response-Timestamp-Fraction" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	{
		/* Used-Service-Unit */
		/* additional allowed AVPs */
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 0;
		vpa.avp_name = "Used-Service-Unit";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Reporting-Reason" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Event-Charging-TimeStamp" }, RULE_OPTIONAL, -1, -1 },
			};
		PARSE_loc_rules(rules, rule_avp);
        }

	/* OMA */
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "DCD-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Content-ID" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Content-provider-ID" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "IM-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Total-Number-Of-Messages-Sent" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Total-Number-Of-Messages-Exploded" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Number-Of-Messages-Successfully-Sent" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Number-Of-Messages-Successfully-Exploded" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 10415;
		vpa.avp_name = "Service-Generic-Information";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] =
			{
				{ { .avp_vendor = 10415, .avp_name = "Application-Server-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Application-Service-Type" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Application-Session-Id" }, RULE_OPTIONAL, -1, 1 },
				{ { .avp_vendor = 10415, .avp_name = "Delivery-Status" }, RULE_OPTIONAL, -1, 1 },
			};
			PARSE_loc_rules(rules, rule_avp);
        }

	LOG_D( "Extension 'Dictionary definitions for DCCA 3GPP' initialized");
	return 0;
}

EXTENSION_ENTRY("dict_dcca_3gpp", dict_dcca_3gpp_entry, "dict_dcca");
