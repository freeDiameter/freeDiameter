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
			TRACE_DEBUG(INFO, "AVP Not found: '%s'", (_rulearray)[__ar].avp_vendor_plus_name.avp_name);		\
			return ENOENT;									\
		}											\
		CHECK_FCT_DO( fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &__data, _parent, NULL),	\
			{							        		\
				TRACE_DEBUG(INFO, "Error on rule with AVP '%s'",      			\
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
	/* Types section                                                    */
	/*==================================================================*/

	struct dict_object * Address_type;
	struct dict_object * DiameterIdentity_type;
	struct dict_object * DiameterURI_type;
	struct dict_object * IPFilterRule_type;
	struct dict_object * Time_type;
	struct dict_object * UTF8String_type;

	CHECK_dict_search( DICT_TYPE, TYPE_BY_NAME, "Address", &Address_type);
	CHECK_dict_search( DICT_TYPE, TYPE_BY_NAME, "DiameterIdentity", &DiameterIdentity_type);
	CHECK_dict_search( DICT_TYPE, TYPE_BY_NAME, "DiameterURI", &DiameterURI_type);
	CHECK_dict_search( DICT_TYPE, TYPE_BY_NAME, "IPFilterRule", &IPFilterRule_type);
	CHECK_dict_search( DICT_TYPE, TYPE_BY_NAME, "Time", &Time_type);
	CHECK_dict_search( DICT_TYPE, TYPE_BY_NAME, "UTF8String", &UTF8String_type);


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
	/* Start of generated data.                                         */
	/*                                                                  */
	/* The following is created automatically with:                     */
	/*     csv_to_fd -p fdc dict_dcca_3gpp.csv                          */
	/* Changes will be lost during the next update.                     */
	/* Do not modify; modify the source .csv file instead.              */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.061 V15.5.0 (2018-12)                                 */
	/* From 3GPP 29061-f50.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.061 Table 9a: Gi/Sgi specific AVPs                    */

	/* Note: 3GPP TS 29.061 is unclear and inconsistent about M.        */
	/* For this reason, other sources are assumed more trustworthy.     */
	/*==================================================================*/

	/* Note: 3GPP-IMSI (1) changed to Must M.                           */
	/* 3GPP-IMSI, UTF8String, code 1, section 16.4.7                    */
	{
		struct dict_avp_data data = {
			1,	/* Code */
			10415,	/* Vendor */
			"3GPP-IMSI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP-Charging-Id (2) type changed from OctetString to Unsigned32, */
	/* per 3GPP TS 29.061 clause 16.4.7.2/2 and packet dumps.           */
	/* 3GPP-Charging-Id, Unsigned32, code 2, section 16.4.7             */
	{
		struct dict_avp_data data = {
			2,	/* Code */
			10415,	/* Vendor */
			"3GPP-Charging-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-PDP-Type (3) changed to Must M.                       */
	/* 3GPP-PDP-Type, Enumerated, code 3, section 16.4.7                */
	{
		struct dict_avp_data data = {
			3,	/* Code */
			10415,	/* Vendor */
			"3GPP-PDP-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/3GPP-PDP-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* 3GPP-CG-Address, OctetString, code 4, section 16.4.7             */
	{
		struct dict_avp_data data = {
			4,	/* Code */
			10415,	/* Vendor */
			"3GPP-CG-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-GPRS-Negotiated-QoS-Profile (5) changed to Must M per contrib/3gpp. */
	/* 3GPP-GPRS-Negotiated-QoS-Profile, UTF8String, code 5, section 16.4.7 */
	{
		struct dict_avp_data data = {
			5,	/* Code */
			10415,	/* Vendor */
			"3GPP-GPRS-Negotiated-QoS-Profile",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* 3GPP-SGSN-Address, OctetString, code 6, section 16.4.7           */
	{
		struct dict_avp_data data = {
			6,	/* Code */
			10415,	/* Vendor */
			"3GPP-SGSN-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-GGSN-Address, OctetString, code 7, section 16.4.7           */
	{
		struct dict_avp_data data = {
			7,	/* Code */
			10415,	/* Vendor */
			"3GPP-GGSN-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-IMSI-MCC-MNC (8) changed to Must M per packet dumps.  */
	/* 3GPP-IMSI-MCC-MNC, UTF8String, code 8, section 16.4.7            */
	{
		struct dict_avp_data data = {
			8,	/* Code */
			10415,	/* Vendor */
			"3GPP-IMSI-MCC-MNC",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP-GGSN-MCC-MNC (9) changed to Must M per contrib/3gpp.  */
	/* 3GPP-GGSN-MCC-MNC, UTF8String, code 9, section 16.4.7            */
	{
		struct dict_avp_data data = {
			9,	/* Code */
			10415,	/* Vendor */
			"3GPP-GGSN-MCC-MNC",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP-NSAPI (10) changed to Must M per contrib/3gpp.        */
	/* 3GPP-NSAPI, OctetString, code 10, section 16.4.7                 */
	{
		struct dict_avp_data data = {
			10,	/* Code */
			10415,	/* Vendor */
			"3GPP-NSAPI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 table 9a does not define                    */
	/* 3GPP-Session-Stop-Indicator (11) even when it is referenced      */
	/* by other 3GPP Diameter AVPs including PS-Information (874).      */
	/* 3GPP-Session-Stop-Indicator, OctetString, code 11, section 16.4.7 */
	{
		struct dict_avp_data data = {
			11,	/* Code */
			10415,	/* Vendor */
			"3GPP-Session-Stop-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-Selection-Mode (12) changed to Must M per contrib/3gpp. */
	/* 3GPP-Selection-Mode, UTF8String, code 12, section 16.4.7         */
	{
		struct dict_avp_data data = {
			12,	/* Code */
			10415,	/* Vendor */
			"3GPP-Selection-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP-Charging-Characteristics (13) changed to Must M per contrib/3gpp. */
	/* 3GPP-Charging-Characteristics, UTF8String, code 13, section 16.4.7 */
	{
		struct dict_avp_data data = {
			13,	/* Code */
			10415,	/* Vendor */
			"3GPP-Charging-Characteristics",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* 3GPP-CG-IPv6-Address (14) to 3GPP-CG-Ipv6-Address.               */
	/* This rename was not applied.                                     */
	/* 3GPP-CG-IPv6-Address (14) changed to Must M per contrib/3gpp.    */
	/* 3GPP-CG-IPv6-Address, OctetString, code 14, section 16.4.7       */
	{
		struct dict_avp_data data = {
			14,	/* Code */
			10415,	/* Vendor */
			"3GPP-CG-IPv6-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* 3GPP-SGSN-IPv6-Address (15) to 3GPP-SGSN-Ipv6-Address.           */
	/* This rename was not applied.                                     */
	/* 3GPP-SGSN-IPv6-Address (15) changed to Must M per contrib/3gpp.  */
	/* 3GPP-SGSN-IPv6-Address, OctetString, code 15, section 16.4.7     */
	{
		struct dict_avp_data data = {
			15,	/* Code */
			10415,	/* Vendor */
			"3GPP-SGSN-IPv6-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* 3GPP-GGSN-IPv6-Address (16) to 3GPP-GGSN-Ipv6-Address.           */
	/* This rename was not applied.                                     */
	/* 3GPP-GGSN-IPv6-Address (16) changed to Must M per contrib/3gpp.  */
	/* 3GPP-GGSN-IPv6-Address, OctetString, code 16, section 16.4.7     */
	{
		struct dict_avp_data data = {
			16,	/* Code */
			10415,	/* Vendor */
			"3GPP-GGSN-IPv6-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* 3GPP-IPv6-DNS-Servers (17) to 3GPP-Ipv6-DNS-Servers.             */
	/* This rename was not applied.                                     */
	/* 3GPP-IPv6-DNS-Servers (17) changed to Must M per contrib/3gpp.   */
	/* 3GPP-IPv6-DNS-Servers, OctetString, code 17, section 16.4.7      */
	{
		struct dict_avp_data data = {
			17,	/* Code */
			10415,	/* Vendor */
			"3GPP-IPv6-DNS-Servers",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-SGSN-MCC-MNC (18) changed to Must M per contrib/3gpp. */
	/* 3GPP-SGSN-MCC-MNC, UTF8String, code 18, section 16.4.7           */
	{
		struct dict_avp_data data = {
			18,	/* Code */
			10415,	/* Vendor */
			"3GPP-SGSN-MCC-MNC",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP-IMEISV (20) changed to Must M.                        */
	/* 3GPP-IMEISV, OctetString, code 20, section 16.4.7                */
	{
		struct dict_avp_data data = {
			20,	/* Code */
			10415,	/* Vendor */
			"3GPP-IMEISV",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-RAT-Type (21) changed to Must M.                      */
	/* 3GPP-RAT-Type, OctetString, code 21, section 16.4.7              */
	{
		struct dict_avp_data data = {
			21,	/* Code */
			10415,	/* Vendor */
			"3GPP-RAT-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-User-Location-Info (22) changed to Must M.            */
	/* 3GPP-User-Location-Info, OctetString, code 22, section 16.4.7    */
	{
		struct dict_avp_data data = {
			22,	/* Code */
			10415,	/* Vendor */
			"3GPP-User-Location-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP-MS-TimeZone (23) changed to Must M.                   */
	/* 3GPP-MS-TimeZone, OctetString, code 23, section 16.4.7           */
	{
		struct dict_avp_data data = {
			23,	/* Code */
			10415,	/* Vendor */
			"3GPP-MS-TimeZone",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-CAMEL-Charging-Info, OctetString, code 24, section 16.4.7   */
	{
		struct dict_avp_data data = {
			24,	/* Code */
			10415,	/* Vendor */
			"3GPP-CAMEL-Charging-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-Packet-Filter, OctetString, code 25, section 16.4.7         */
	{
		struct dict_avp_data data = {
			25,	/* Code */
			10415,	/* Vendor */
			"3GPP-Packet-Filter",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-Negotiated-DSCP, OctetString, code 26, section 16.4.7       */
	{
		struct dict_avp_data data = {
			26,	/* Code */
			10415,	/* Vendor */
			"3GPP-Negotiated-DSCP",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-Allocate-IP-Type, OctetString, code 27, section 16.4.7      */
	{
		struct dict_avp_data data = {
			27,	/* Code */
			10415,	/* Vendor */
			"3GPP-Allocate-IP-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 table 9a does not define                    */
	/* External-Identifier (28) for Diameter.                           */
	/* 3GPP TS 29.061 table 9b refers to 3GPP TS 29.336                 */
	/* for External-Identifier (3111).                                  */
	/*                                                                  */
	/* External-Identifier (28) OctetString replaced by                 */
	/* External-Identifier (3111) UTF8String.                           */


	/* TWAN-Identifier, OctetString, code 29, section 16.4.7            */
	{
		struct dict_avp_data data = {
			29,	/* Code */
			10415,	/* Vendor */
			"TWAN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-User-Location-Info-Time, OctetString, code 30, section 16.4.7 */
	{
		struct dict_avp_data data = {
			30,	/* Code */
			10415,	/* Vendor */
			"3GPP-User-Location-Info-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-Secondary-RAT-Usage, OctetString, code 31, section 16.4.7   */
	{
		struct dict_avp_data data = {
			31,	/* Code */
			10415,	/* Vendor */
			"3GPP-Secondary-RAT-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.061 Table 10: Gmb specific AVPs                       */
	/*==================================================================*/

	/* TMGI, OctetString, code 900, section 17.7.2                      */
	{
		struct dict_avp_data data = {
			900,	/* Code */
			10415,	/* Vendor */
			"TMGI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Required-MBMS-Bearer-Capabilities, UTF8String, code 901, section 17.7.3 */
	{
		struct dict_avp_data data = {
			901,	/* Code */
			10415,	/* Vendor */
			"Required-MBMS-Bearer-Capabilities",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* MBMS-StartStop-Indication, Enumerated, code 902, section 17.7.5  */
	{
		struct dict_avp_data data = {
			902,	/* Code */
			10415,	/* Vendor */
			"MBMS-StartStop-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-StartStop-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-Service-Area, OctetString, code 903, section 17.7.6         */
	{
		struct dict_avp_data data = {
			903,	/* Code */
			10415,	/* Vendor */
			"MBMS-Service-Area",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Session-Duration, OctetString, code 904, section 17.7.7     */
	{
		struct dict_avp_data data = {
			904,	/* Code */
			10415,	/* Vendor */
			"MBMS-Session-Duration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Alternative-APN, UTF8String, code 905, section 17.7.8            */
	{
		struct dict_avp_data data = {
			905,	/* Code */
			10415,	/* Vendor */
			"Alternative-APN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* MBMS-Service-Type, Enumerated, code 906, section 17.7.9          */
	{
		struct dict_avp_data data = {
			906,	/* Code */
			10415,	/* Vendor */
			"MBMS-Service-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-Service-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-2G-3G-Indicator, Enumerated, code 907, section 17.7.10      */
	{
		struct dict_avp_data data = {
			907,	/* Code */
			10415,	/* Vendor */
			"MBMS-2G-3G-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-2G-3G-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-Session-Identity, OctetString, code 908, section 17.7.11    */
	{
		struct dict_avp_data data = {
			908,	/* Code */
			10415,	/* Vendor */
			"MBMS-Session-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RAI, UTF8String, code 909, section 17.7.12                       */
	{
		struct dict_avp_data data = {
			909,	/* Code */
			10415,	/* Vendor */
			"RAI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Additional-MBMS-Trace-Info, OctetString, code 910, section 17.7.13 */
	{
		struct dict_avp_data data = {
			910,	/* Code */
			10415,	/* Vendor */
			"Additional-MBMS-Trace-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Time-To-Data-Transfer, OctetString, code 911, section 17.7.14 */
	{
		struct dict_avp_data data = {
			911,	/* Code */
			10415,	/* Vendor */
			"MBMS-Time-To-Data-Transfer",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Session-Repetition-Number, OctetString, code 912, section 17.7.15 */
	{
		struct dict_avp_data data = {
			912,	/* Code */
			10415,	/* Vendor */
			"MBMS-Session-Repetition-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Required-QoS, UTF8String, code 913, section 17.7.16         */
	{
		struct dict_avp_data data = {
			913,	/* Code */
			10415,	/* Vendor */
			"MBMS-Required-QoS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* MBMS-Counting-Information, Enumerated, code 914, section 17.7.17 */
	{
		struct dict_avp_data data = {
			914,	/* Code */
			10415,	/* Vendor */
			"MBMS-Counting-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-Counting-Information)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-User-Data-Mode-Indication, Enumerated, code 915, section 17.7.18 */
	{
		struct dict_avp_data data = {
			915,	/* Code */
			10415,	/* Vendor */
			"MBMS-User-Data-Mode-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-User-Data-Mode-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-GGSN-Address, OctetString, code 916, section 17.7.19        */
	{
		struct dict_avp_data data = {
			916,	/* Code */
			10415,	/* Vendor */
			"MBMS-GGSN-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* MBMS-GGSN-IPv6-Address (917) to MBMS-GGSN-Ipv6-Address.          */
	/* This rename was not applied.                                     */
	/* MBMS-GGSN-IPv6-Address, OctetString, code 917, section 17.7.20   */
	{
		struct dict_avp_data data = {
			917,	/* Code */
			10415,	/* Vendor */
			"MBMS-GGSN-IPv6-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-BMSC-SSM-IP-Address, OctetString, code 918, section 17.7.21 */
	{
		struct dict_avp_data data = {
			918,	/* Code */
			10415,	/* Vendor */
			"MBMS-BMSC-SSM-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* MBMS-BMSC-SSM-IPv6-Address (919) to MBMS-BMSC-SSM-Ipv6-Address.  */
	/* This rename was not applied.                                     */
	/* MBMS-BMSC-SSM-IPv6-Address, OctetString, code 919, section 17.7.22 */
	{
		struct dict_avp_data data = {
			919,	/* Code */
			10415,	/* Vendor */
			"MBMS-BMSC-SSM-IPv6-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Flow-Identifier, OctetString, code 920, section 17.7.23     */
	{
		struct dict_avp_data data = {
			920,	/* Code */
			10415,	/* Vendor */
			"MBMS-Flow-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CN-IP-Multicast-Distribution, Enumerated, code 921, section 17.7.24 */
	{
		struct dict_avp_data data = {
			921,	/* Code */
			10415,	/* Vendor */
			"CN-IP-Multicast-Distribution",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CN-IP-Multicast-Distribution)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-HC-Indicator, Enumerated, code 922, section 17.7.25         */
	{
		struct dict_avp_data data = {
			922,	/* Code */
			10415,	/* Vendor */
			"MBMS-HC-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-HC-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.061 Table 20.5a.1: SGmb specific AVPs                 */
	/*==================================================================*/

	/* MBMS-Access-Indicator, Enumerated, code 923, section 20.5a.1     */
	{
		struct dict_avp_data data = {
			923,	/* Code */
			10415,	/* Vendor */
			"MBMS-Access-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-Access-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-GW-SSM-IP-Address, OctetString, code 924, section 20.5a.2   */
	{
		struct dict_avp_data data = {
			924,	/* Code */
			10415,	/* Vendor */
			"MBMS-GW-SSM-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.061 V12.6.0 (2014-06) renamed                   */
	/* MBMS-GW-SSM-IPv6-Address (925) to MBMS-GW-SSM-Ipv6-Address.      */
	/* This rename was not applied.                                     */
	/* MBMS-GW-SSM-IPv6-Address, OctetString, code 925, section 20.5a.3 */
	{
		struct dict_avp_data data = {
			925,	/* Code */
			10415,	/* Vendor */
			"MBMS-GW-SSM-IPv6-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-BMSC-SSM-UDP-Port, OctetString, code 926, section 20.5a.4   */
	{
		struct dict_avp_data data = {
			926,	/* Code */
			10415,	/* Vendor */
			"MBMS-BMSC-SSM-UDP-Port",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-GW-UDP-Port, OctetString, code 927, section 20.5a.5         */
	{
		struct dict_avp_data data = {
			927,	/* Code */
			10415,	/* Vendor */
			"MBMS-GW-UDP-Port",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-GW-UDP-Port-Indicator, Enumerated, code 928, section 20.5a.6 */
	{
		struct dict_avp_data data = {
			928,	/* Code */
			10415,	/* Vendor */
			"MBMS-GW-UDP-Port-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-GW-UDP-Port-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-Data-Transfer-Start, Unsigned64, code 929, section 20.5a.7  */
	{
		struct dict_avp_data data = {
			929,	/* Code */
			10415,	/* Vendor */
			"MBMS-Data-Transfer-Start",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Data-Transfer-Stop, Unsigned64, code 930, section 20.5a.8   */
	{
		struct dict_avp_data data = {
			930,	/* Code */
			10415,	/* Vendor */
			"MBMS-Data-Transfer-Stop",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Flag, Unsigned32, code 931, section 20.5a.9                 */
	{
		struct dict_avp_data data = {
			931,	/* Code */
			10415,	/* Vendor */
			"MBMS-Flag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Restart-Counter, Unsigned32, code 932, section 20.5a.10          */
	{
		struct dict_avp_data data = {
			932,	/* Code */
			10415,	/* Vendor */
			"Restart-Counter",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Diagnostic-Info, Unsigned32, code 933, section 20.5a.11          */
	{
		struct dict_avp_data data = {
			933,	/* Code */
			10415,	/* Vendor */
			"Diagnostic-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-Cell-List, OctetString, code 934, section 20.5a.12          */
	{
		struct dict_avp_data data = {
			934,	/* Code */
			10415,	/* Vendor */
			"MBMS-Cell-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.128 V15.6.0 (2019-09)                                 */
	/* From 3GPP 29128-f60.docx                                         */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.128 Table 6.4.1-1: T6a/T6b specific Diameter AVPs     */
	/*==================================================================*/

	/* Communication-Failure-Information, Grouped, code 4300, section 6.4.4 */
	{
		struct dict_avp_data data = {
			4300,	/* Code */
			10415,	/* Vendor */
			"Communication-Failure-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Cause-Type, Unsigned32, code 4301, section 6.4.5                 */
	{
		struct dict_avp_data data = {
			4301,	/* Code */
			10415,	/* Vendor */
			"Cause-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* S1AP-Cause, Unsigned32, code 4302, section 6.4.6                 */
	{
		struct dict_avp_data data = {
			4302,	/* Code */
			10415,	/* Vendor */
			"S1AP-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RANAP-Cause, Unsigned32, code 4303, section 6.4.7                */
	{
		struct dict_avp_data data = {
			4303,	/* Code */
			10415,	/* Vendor */
			"RANAP-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* BSSGP-Cause, Unsigned32, code 4309, section 6.4.8                */
	{
		struct dict_avp_data data = {
			4309,	/* Code */
			10415,	/* Vendor */
			"BSSGP-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GMM-Cause, Unsigned32, code 4304, section 6.4.9                  */
	{
		struct dict_avp_data data = {
			4304,	/* Code */
			10415,	/* Vendor */
			"GMM-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Cause, Unsigned32, code 4305, section 6.4.10                  */
	{
		struct dict_avp_data data = {
			4305,	/* Code */
			10415,	/* Vendor */
			"SM-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-UE-Per-Location-Configuration, Grouped, code 4306, section 6.4.11 */
	{
		struct dict_avp_data data = {
			4306,	/* Code */
			10415,	/* Vendor */
			"Number-Of-UE-Per-Location-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-UE-Per-Location-Report, Grouped, code 4307, section 6.4.12 */
	{
		struct dict_avp_data data = {
			4307,	/* Code */
			10415,	/* Vendor */
			"Number-Of-UE-Per-Location-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UE-Count, Unsigned32, code 4308, section 6.4.13                  */
	{
		struct dict_avp_data data = {
			4308,	/* Code */
			10415,	/* Vendor */
			"UE-Count",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Connection-Action, Unsigned32, code 4314, section 6.4.18         */
	{
		struct dict_avp_data data = {
			4314,	/* Code */
			10415,	/* Vendor */
			"Connection-Action",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.128 table 6.4.1-1 row Non-IP-Data (4315)        */
	/* has type "Octetstring" instead of "OctetString".                 */
	/* Non-IP-Data, OctetString, code 4315, section 6.4.19              */
	{
		struct dict_avp_data data = {
			4315,	/* Code */
			10415,	/* Vendor */
			"Non-IP-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Serving-PLMN-Rate-Control, Grouped, code 4310, section 6.4.21    */
	{
		struct dict_avp_data data = {
			4310,	/* Code */
			10415,	/* Vendor */
			"Serving-PLMN-Rate-Control",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Uplink-Rate-Limit, Unsigned32, code 4311, section 6.4.23         */
	{
		struct dict_avp_data data = {
			4311,	/* Code */
			10415,	/* Vendor */
			"Uplink-Rate-Limit",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Downlink-Rate-Limit, Unsigned32, code 4312, section 6.4.22       */
	{
		struct dict_avp_data data = {
			4312,	/* Code */
			10415,	/* Vendor */
			"Downlink-Rate-Limit",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-PCO, OctetString, code 4313, section 6.4.26             */
	{
		struct dict_avp_data data = {
			4313,	/* Code */
			10415,	/* Vendor */
			"Extended-PCO",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SCEF-Wait-Time, Time, code 4316, section 6.4.24                  */
	{
		struct dict_avp_data data = {
			4316,	/* Code */
			10415,	/* Vendor */
			"SCEF-Wait-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* CMR-Flags, Unsigned32, code 4317, section 6.4.25                 */
	{
		struct dict_avp_data data = {
			4317,	/* Code */
			10415,	/* Vendor */
			"CMR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RRC-Cause-Counter, Grouped, code 4318, section 6.4.27            */
	{
		struct dict_avp_data data = {
			4318,	/* Code */
			10415,	/* Vendor */
			"RRC-Cause-Counter",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Counter-Value, Unsigned32, code 4319, section 6.4.28             */
	{
		struct dict_avp_data data = {
			4319,	/* Code */
			10415,	/* Vendor */
			"Counter-Value",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RRC-Counter-Timestamp, Time, code 4320, section 6.4.29           */
	{
		struct dict_avp_data data = {
			4320,	/* Code */
			10415,	/* Vendor */
			"RRC-Counter-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* TDA-Flags, Unsigned32, code 4321, section 6.4.31                 */
	{
		struct dict_avp_data data = {
			4321,	/* Code */
			10415,	/* Vendor */
			"TDA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Idle-Status-Indication, Grouped, code 4322, section 6.4.32       */
	{
		struct dict_avp_data data = {
			4322,	/* Code */
			10415,	/* Vendor */
			"Idle-Status-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Idle-Status-Timestamp, Time, code 4323, section 6.4.33           */
	{
		struct dict_avp_data data = {
			4323,	/* Code */
			10415,	/* Vendor */
			"Idle-Status-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Active-Time, Unsigned32, code 4324, section 6.4.34               */
	{
		struct dict_avp_data data = {
			4324,	/* Code */
			10415,	/* Vendor */
			"Active-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.140 V7.0.0 (2007-06)                                  */
	/* From 3GPP 29140-700.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.140 Table 6.3.1: Diameter MM10 Application AVPs       */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* Served-User-Identity, Grouped, code 1100, section 6.3.1          */
	{
		struct dict_avp_data data = {
			1100,	/* Code */
			10415,	/* Vendor */
			"Served-User-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* VASP-ID, UTF8String, code 1101, section 6.3.3                    */
	{
		struct dict_avp_data data = {
			1101,	/* Code */
			10415,	/* Vendor */
			"VASP-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* VAS-ID, UTF8String, code 1102, section 6.3.4                     */
	{
		struct dict_avp_data data = {
			1102,	/* Code */
			10415,	/* Vendor */
			"VAS-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Trigger-Event, Enumerated, code 1103, section 6.3.5              */
	{
		struct dict_avp_data data = {
			1103,	/* Code */
			10415,	/* Vendor */
			"Trigger-Event",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Trigger-Event)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Sender-Address, UTF8String, code 1104, section 6.3.7             */
	{
		struct dict_avp_data data = {
			1104,	/* Code */
			10415,	/* Vendor */
			"Sender-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Initial-Recipient-Address, Grouped, code 1105, section 6.3.8     */
	{
		struct dict_avp_data data = {
			1105,	/* Code */
			10415,	/* Vendor */
			"Initial-Recipient-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Result-Recipient-Address, Grouped, code 1106, section 6.3.9      */
	{
		struct dict_avp_data data = {
			1106,	/* Code */
			10415,	/* Vendor */
			"Result-Recipient-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: Name conflict with 3GPP TS 32.329 Sequence-Number (716).   */
	/* Sequence-Number (1107) renamed to Sequence-Number-29.140 (1107). */
	/* Sequence-Number-29.140, Unsigned32, code 1107, section 6.3.10    */
	{
		struct dict_avp_data data = {
			1107,	/* Code */
			10415,	/* Vendor */
			"Sequence-Number-29.140",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: Name conflict with 3GPP TS 32.299 Recipient-Address (1201). */
	/* Recipient-Address (1108) renamed to Recipient-Address-29.140 (1108). */
	/* Recipient-Address-29.140, UTF8String, code 1108, section 6.3.11  */
	{
		struct dict_avp_data data = {
			1108,	/* Code */
			10415,	/* Vendor */
			"Recipient-Address-29.140",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Routeing-Address, UTF8String, code 1109, section 6.3.12          */
	{
		struct dict_avp_data data = {
			1109,	/* Code */
			10415,	/* Vendor */
			"Routeing-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Originating-Interface, Enumerated, code 1110, section 6.3.13     */
	{
		struct dict_avp_data data = {
			1110,	/* Code */
			10415,	/* Vendor */
			"Originating-Interface",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Originating-Interface)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Delivery-Report, Enumerated, code 1111, section 6.3.14           */
	{
		struct dict_avp_data data = {
			1111,	/* Code */
			10415,	/* Vendor */
			"Delivery-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Delivery-Report)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Read-Reply, Enumerated, code 1112, section 6.3.15                */
	{
		struct dict_avp_data data = {
			1112,	/* Code */
			10415,	/* Vendor */
			"Read-Reply",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Read-Reply)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Sender-Visibility, Enumerated, code 1113, section 6.3.16         */
	{
		struct dict_avp_data data = {
			1113,	/* Code */
			10415,	/* Vendor */
			"Sender-Visibility",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Sender-Visibility)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Service-Key, UTF8String, code 1114, section 6.3.17               */
	{
		struct dict_avp_data data = {
			1114,	/* Code */
			10415,	/* Vendor */
			"Service-Key",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Billing-Information, UTF8String, code 1115, section 6.3.18       */
	{
		struct dict_avp_data data = {
			1115,	/* Code */
			10415,	/* Vendor */
			"Billing-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: Name conflict with 3GPP TS 32.299 Status (2702).           */
	/* Status (1116) renamed to Status-29.140 (1116).                   */
	/* Status-29.140, Grouped, code 1116, section 6.3.19                */
	{
		struct dict_avp_data data = {
			1116,	/* Code */
			10415,	/* Vendor */
			"Status-29.140",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Status-Code, UTF8String, code 1117, section 6.3.20               */
	{
		struct dict_avp_data data = {
			1117,	/* Code */
			10415,	/* Vendor */
			"Status-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Status-Text, UTF8String, code 1118, section 6.3.21               */
	{
		struct dict_avp_data data = {
			1118,	/* Code */
			10415,	/* Vendor */
			"Status-Text",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Routeing-Address-Resolution, Enumerated, code 1119, section 6.3.22 */
	{
		struct dict_avp_data data = {
			1119,	/* Code */
			10415,	/* Vendor */
			"Routeing-Address-Resolution",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Routeing-Address-Resolution)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.172 V15.2.0 (2019-12)                                 */
	/* From 3GPP 29172-f20.docx                                         */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.172 Table 7.4.1-1: Diameter ELP Application AVPs      */
	/*==================================================================*/

	/* SLg-Location-Type, Enumerated, code 2500, section 7.4.2          */
	{
		struct dict_avp_data data = {
			2500,	/* Code */
			10415,	/* Vendor */
			"SLg-Location-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SLg-Location-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* LCS-EPS-Client-Name, Grouped, code 2501, section 7.4.3           */
	{
		struct dict_avp_data data = {
			2501,	/* Code */
			10415,	/* Vendor */
			"LCS-EPS-Client-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Requestor-Name, Grouped, code 2502, section 7.4.4            */
	{
		struct dict_avp_data data = {
			2502,	/* Code */
			10415,	/* Vendor */
			"LCS-Requestor-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Priority, Unsigned32, code 2503, section 7.4.5               */
	{
		struct dict_avp_data data = {
			2503,	/* Code */
			10415,	/* Vendor */
			"LCS-Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-QoS, Grouped, code 2504, section 7.4.6                       */
	{
		struct dict_avp_data data = {
			2504,	/* Code */
			10415,	/* Vendor */
			"LCS-QoS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Horizontal-Accuracy, Unsigned32, code 2505, section 7.4.7        */
	{
		struct dict_avp_data data = {
			2505,	/* Code */
			10415,	/* Vendor */
			"Horizontal-Accuracy",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Vertical-Accuracy, Unsigned32, code 2506, section 7.4.8          */
	{
		struct dict_avp_data data = {
			2506,	/* Code */
			10415,	/* Vendor */
			"Vertical-Accuracy",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Vertical-Requested, Enumerated, code 2507, section 7.4.9         */
	{
		struct dict_avp_data data = {
			2507,	/* Code */
			10415,	/* Vendor */
			"Vertical-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Vertical-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Velocity-Requested, Enumerated, code 2508, section 7.4.10        */
	{
		struct dict_avp_data data = {
			2508,	/* Code */
			10415,	/* Vendor */
			"Velocity-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Velocity-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Response-Time, Enumerated, code 2509, section 7.4.11             */
	{
		struct dict_avp_data data = {
			2509,	/* Code */
			10415,	/* Vendor */
			"Response-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Response-Time)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Supported-GAD-Shapes, Unsigned32, code 2510, section 7.4.12      */
	{
		struct dict_avp_data data = {
			2510,	/* Code */
			10415,	/* Vendor */
			"Supported-GAD-Shapes",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Codeword, UTF8String, code 2511, section 7.4.13              */
	{
		struct dict_avp_data data = {
			2511,	/* Code */
			10415,	/* Vendor */
			"LCS-Codeword",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* LCS-Privacy-Check, Enumerated, code 2512, section 7.4.14         */
	{
		struct dict_avp_data data = {
			2512,	/* Code */
			10415,	/* Vendor */
			"LCS-Privacy-Check",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/LCS-Privacy-Check)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Accuracy-Fulfilment-Indicator, Enumerated, code 2513, section 7.4.15 */
	{
		struct dict_avp_data data = {
			2513,	/* Code */
			10415,	/* Vendor */
			"Accuracy-Fulfilment-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Accuracy-Fulfilment-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Age-Of-Location-Estimate, Unsigned32, code 2514, section 7.4.16  */
	{
		struct dict_avp_data data = {
			2514,	/* Code */
			10415,	/* Vendor */
			"Age-Of-Location-Estimate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Velocity-Estimate, OctetString, code 2515, section 7.4.17        */
	{
		struct dict_avp_data data = {
			2515,	/* Code */
			10415,	/* Vendor */
			"Velocity-Estimate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* EUTRAN-Positioning-Data, OctetString, code 2516, section 7.4.18  */
	{
		struct dict_avp_data data = {
			2516,	/* Code */
			10415,	/* Vendor */
			"EUTRAN-Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ECGI, OctetString, code 2517, section 7.4.19                     */
	{
		struct dict_avp_data data = {
			2517,	/* Code */
			10415,	/* Vendor */
			"ECGI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Location-Event, Enumerated, code 2518, section 7.4.20            */
	{
		struct dict_avp_data data = {
			2518,	/* Code */
			10415,	/* Vendor */
			"Location-Event",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Location-Event)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Pseudonym-Indicator, Enumerated, code 2519, section 7.4.21       */
	{
		struct dict_avp_data data = {
			2519,	/* Code */
			10415,	/* Vendor */
			"Pseudonym-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Pseudonym-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* LCS-Service-Type-ID, Unsigned32, code 2520, section 7.4.22       */
	{
		struct dict_avp_data data = {
			2520,	/* Code */
			10415,	/* Vendor */
			"LCS-Service-Type-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Privacy-Check-Non-Session, Grouped, code 2521, section 7.4.23 */
	{
		struct dict_avp_data data = {
			2521,	/* Code */
			10415,	/* Vendor */
			"LCS-Privacy-Check-Non-Session",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Privacy-Check-Session, Grouped, code 2522, section 7.4.24    */
	{
		struct dict_avp_data data = {
			2522,	/* Code */
			10415,	/* Vendor */
			"LCS-Privacy-Check-Session",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-QoS-Class, Enumerated, code 2523, section 7.4.27             */
	{
		struct dict_avp_data data = {
			2523,	/* Code */
			10415,	/* Vendor */
			"LCS-QoS-Class",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/LCS-QoS-Class)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* GERAN-Positioning-Info, Grouped, code 2524, section 7.4.29       */
	{
		struct dict_avp_data data = {
			2524,	/* Code */
			10415,	/* Vendor */
			"GERAN-Positioning-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GERAN-Positioning-Data, OctetString, code 2525, section 7.4.30   */
	{
		struct dict_avp_data data = {
			2525,	/* Code */
			10415,	/* Vendor */
			"GERAN-Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GERAN-GANSS-Positioning-Data, OctetString, code 2526, section 7.4.31 */
	{
		struct dict_avp_data data = {
			2526,	/* Code */
			10415,	/* Vendor */
			"GERAN-GANSS-Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UTRAN-Positioning-Info, Grouped, code 2527, section 7.4.32       */
	{
		struct dict_avp_data data = {
			2527,	/* Code */
			10415,	/* Vendor */
			"UTRAN-Positioning-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UTRAN-Positioning-Data, OctetString, code 2528, section 7.4.33   */
	{
		struct dict_avp_data data = {
			2528,	/* Code */
			10415,	/* Vendor */
			"UTRAN-Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UTRAN-GANSS-Positioning-Data, OctetString, code 2529, section 7.4.34 */
	{
		struct dict_avp_data data = {
			2529,	/* Code */
			10415,	/* Vendor */
			"UTRAN-GANSS-Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LRR-Flags, Unsigned32, code 2530, section 7.4.35                 */
	{
		struct dict_avp_data data = {
			2530,	/* Code */
			10415,	/* Vendor */
			"LRR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Reference-Number, OctetString, code 2531, section 7.4.37     */
	{
		struct dict_avp_data data = {
			2531,	/* Code */
			10415,	/* Vendor */
			"LCS-Reference-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Deferred-Location-Type, Unsigned32, code 2532, section 7.4.36    */
	{
		struct dict_avp_data data = {
			2532,	/* Code */
			10415,	/* Vendor */
			"Deferred-Location-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Area-Event-Info, Grouped, code 2533, section 7.4.38              */
	{
		struct dict_avp_data data = {
			2533,	/* Code */
			10415,	/* Vendor */
			"Area-Event-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Area-Definition, Grouped, code 2534, section 7.4.39              */
	{
		struct dict_avp_data data = {
			2534,	/* Code */
			10415,	/* Vendor */
			"Area-Definition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Area, Grouped, code 2535, section 7.4.40                         */
	{
		struct dict_avp_data data = {
			2535,	/* Code */
			10415,	/* Vendor */
			"Area",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Area-Type, Unsigned32, code 2536, section 7.4.41                 */
	{
		struct dict_avp_data data = {
			2536,	/* Code */
			10415,	/* Vendor */
			"Area-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Area-Identification, Grouped, code 2537, section 7.4.42          */
	{
		struct dict_avp_data data = {
			2537,	/* Code */
			10415,	/* Vendor */
			"Area-Identification",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Occurrence-Info, Enumerated, code 2538, section 7.4.43           */
	{
		struct dict_avp_data data = {
			2538,	/* Code */
			10415,	/* Vendor */
			"Occurrence-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Occurrence-Info)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Interval-Time, Unsigned32, code 2539, section 7.4.44             */
	{
		struct dict_avp_data data = {
			2539,	/* Code */
			10415,	/* Vendor */
			"Interval-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Periodic-LDR-Information, Grouped, code 2540, section 7.4.45     */
	{
		struct dict_avp_data data = {
			2540,	/* Code */
			10415,	/* Vendor */
			"Periodic-LDR-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-Amount, Unsigned32, code 2541, section 7.4.46          */
	{
		struct dict_avp_data data = {
			2541,	/* Code */
			10415,	/* Vendor */
			"Reporting-Amount",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-Interval, Unsigned32, code 2542, section 7.4.47        */
	{
		struct dict_avp_data data = {
			2542,	/* Code */
			10415,	/* Vendor */
			"Reporting-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-PLMN-List, Grouped, code 2543, section 7.4.48          */
	{
		struct dict_avp_data data = {
			2543,	/* Code */
			10415,	/* Vendor */
			"Reporting-PLMN-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PLMN-ID-List, Grouped, code 2544, section 7.4.49                 */
	{
		struct dict_avp_data data = {
			2544,	/* Code */
			10415,	/* Vendor */
			"PLMN-ID-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PLR-Flags, Unsigned32, code 2545, section 7.4.52                 */
	{
		struct dict_avp_data data = {
			2545,	/* Code */
			10415,	/* Vendor */
			"PLR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PLA-Flags, Unsigned32, code 2546, section 7.4.53                 */
	{
		struct dict_avp_data data = {
			2546,	/* Code */
			10415,	/* Vendor */
			"PLA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Deferred-MT-LR-Data, Grouped, code 2547, section 7.4.54          */
	{
		struct dict_avp_data data = {
			2547,	/* Code */
			10415,	/* Vendor */
			"Deferred-MT-LR-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Termination-Cause, Unsigned32, code 2548, section 7.4.55         */
	{
		struct dict_avp_data data = {
			2548,	/* Code */
			10415,	/* Vendor */
			"Termination-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LRA-Flags, Unsigned32, code 2549, section 7.4.56                 */
	{
		struct dict_avp_data data = {
			2549,	/* Code */
			10415,	/* Vendor */
			"LRA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Periodic-Location-Support-Indicator, Enumerated, code 2550, section 7.4.50 */
	{
		struct dict_avp_data data = {
			2550,	/* Code */
			10415,	/* Vendor */
			"Periodic-Location-Support-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Periodic-Location-Support-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Prioritized-List-Indicator, Enumerated, code 2551, section 7.4.51 */
	{
		struct dict_avp_data data = {
			2551,	/* Code */
			10415,	/* Vendor */
			"Prioritized-List-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Prioritized-List-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ESMLC-Cell-Info, Grouped, code 2552, section 7.4.57              */
	{
		struct dict_avp_data data = {
			2552,	/* Code */
			10415,	/* Vendor */
			"ESMLC-Cell-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Cell-Portion-ID, Unsigned32, code 2553, section 7.4.58           */
	{
		struct dict_avp_data data = {
			2553,	/* Code */
			10415,	/* Vendor */
			"Cell-Portion-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 1xRTT-RCID, OctetString, code 2554, section 7.4.59               */
	{
		struct dict_avp_data data = {
			2554,	/* Code */
			10415,	/* Vendor */
			"1xRTT-RCID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Delayed-Location-Reporting-Data, Grouped, code 2555, section 7.4.60 */
	{
		struct dict_avp_data data = {
			2555,	/* Code */
			10415,	/* Vendor */
			"Delayed-Location-Reporting-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Civic-Address, UTF8String, code 2556, section 7.4.61             */
	{
		struct dict_avp_data data = {
			2556,	/* Code */
			10415,	/* Vendor */
			"Civic-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Barometric-Pressure, Unsigned32, code 2557, section 7.4.62       */
	{
		struct dict_avp_data data = {
			2557,	/* Code */
			10415,	/* Vendor */
			"Barometric-Pressure",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UTRAN-Additional-Positioning-Data, OctetString, code 2558, section 7.4.63 */
	{
		struct dict_avp_data data = {
			2558,	/* Code */
			10415,	/* Vendor */
			"UTRAN-Additional-Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Motion-Event-Info, Grouped, code 2559, section 7.4.64            */
	{
		struct dict_avp_data data = {
			2559,	/* Code */
			10415,	/* Vendor */
			"Motion-Event-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Linear-Distance, Unsigned32, code 2560, section 7.4.65           */
	{
		struct dict_avp_data data = {
			2560,	/* Code */
			10415,	/* Vendor */
			"Linear-Distance",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Maximum-Interval, Unsigned32, code 2561, section 7.4.66          */
	{
		struct dict_avp_data data = {
			2561,	/* Code */
			10415,	/* Vendor */
			"Maximum-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Sampling-Interval, Unsigned32, code 2562, section 7.4.67         */
	{
		struct dict_avp_data data = {
			2562,	/* Code */
			10415,	/* Vendor */
			"Sampling-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-Duration, Unsigned32, code 2563, section 7.4.68        */
	{
		struct dict_avp_data data = {
			2563,	/* Code */
			10415,	/* Vendor */
			"Reporting-Duration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-Location-Requirements, Unsigned32, code 2564, section 7.4.69 */
	{
		struct dict_avp_data data = {
			2564,	/* Code */
			10415,	/* Vendor */
			"Reporting-Location-Requirements",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Additional-Area, Grouped, code 2565, section 7.4.70              */
	{
		struct dict_avp_data data = {
			2565,	/* Code */
			10415,	/* Vendor */
			"Additional-Area",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.173 V15.0.0 (2018-06)                                 */
	/* From 3GPP 29173-f00.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.173 Table 6.4.1/1: SLh specific Diameter AVPs         */
	/*==================================================================*/

	/* LMSI, OctetString, code 2400, section 6.4.2                      */
	{
		struct dict_avp_data data = {
			2400,	/* Code */
			10415,	/* Vendor */
			"LMSI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Serving-Node, Grouped, code 2401, section 6.4.3                  */
	{
		struct dict_avp_data data = {
			2401,	/* Code */
			10415,	/* Vendor */
			"Serving-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MME-Name, DiameterIdentity, code 2402, section 6.4.4             */
	{
		struct dict_avp_data data = {
			2402,	/* Code */
			10415,	/* Vendor */
			"MME-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* MSC-Number, OctetString, code 2403, section 6.4.5                */
	{
		struct dict_avp_data data = {
			2403,	/* Code */
			10415,	/* Vendor */
			"MSC-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Capabilities-Sets, Unsigned32, code 2404, section 6.4.6      */
	{
		struct dict_avp_data data = {
			2404,	/* Code */
			10415,	/* Vendor */
			"LCS-Capabilities-Sets",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GMLC-Address, Address, code 2405, section 6.4.7                  */
	{
		struct dict_avp_data data = {
			2405,	/* Code */
			10415,	/* Vendor */
			"GMLC-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Additional-Serving-Node, Grouped, code 2406, section 6.4.8       */
	{
		struct dict_avp_data data = {
			2406,	/* Code */
			10415,	/* Vendor */
			"Additional-Serving-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PPR-Address, Address, code 2407, section 6.4.9                   */
	{
		struct dict_avp_data data = {
			2407,	/* Code */
			10415,	/* Vendor */
			"PPR-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* MME-Realm, DiameterIdentity, code 2408, section 6.4.12           */
	{
		struct dict_avp_data data = {
			2408,	/* Code */
			10415,	/* Vendor */
			"MME-Realm",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* SGSN-Name, DiameterIdentity, code 2409, section 6.4.13           */
	{
		struct dict_avp_data data = {
			2409,	/* Code */
			10415,	/* Vendor */
			"SGSN-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* SGSN-Realm, DiameterIdentity, code 2410, section 6.4.14          */
	{
		struct dict_avp_data data = {
			2410,	/* Code */
			10415,	/* Vendor */
			"SGSN-Realm",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* RIA-Flags, Unsigned32, code 2411, section 6.4.15                 */
	{
		struct dict_avp_data data = {
			2411,	/* Code */
			10415,	/* Vendor */
			"RIA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.210 V6.7.0 (2006-12)                                  */
	/* From 3GPP 29210-670.doc                                          */
	/*                                                                  */
	/* Replaced by 3GPP TS 29.212.                                      */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.210 Table 5.2: Gx specific Diameter AVPs              */
	/* AVPs not provided by 3GPP TS 29.212.                             */
	/*==================================================================*/

	/* PDP-Session-Operation, Enumerated, code 1015, section 5.2.21     */
	{
		struct dict_avp_data data = {
			1015,	/* Code */
			10415,	/* Vendor */
			"PDP-Session-Operation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PDP-Session-Operation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.212 V15.9.0 (2019-12)                                 */
	/* From 3GPP 29212-f90.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.212 Table 5.3.0.1: Gx specific Diameter AVPs          */
	/*==================================================================*/

	/* Note: 3GPP TS 29.212 V12.1.0 (2013-06) removed ADC-Revalidation-Time (2801). */
	/* ADC-Revalidation-Time, Time, code 2801, section 5.3.93           */
	{
		struct dict_avp_data data = {
			2801,	/* Code */
			10415,	/* Vendor */
			"ADC-Revalidation-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* 3GPP-PS-Data-Off-Status, Enumerated, code 2847, section 5.3.133  */
	{
		struct dict_avp_data data = {
			2847,	/* Code */
			10415,	/* Vendor */
			"3GPP-PS-Data-Off-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/3GPP-PS-Data-Off-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Access-Availability-Change-Reason, Unsigned32, code 2833, section 5.3.121 */
	{
		struct dict_avp_data data = {
			2833,	/* Code */
			10415,	/* Vendor */
			"Access-Availability-Change-Reason",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Network-Charging-Identifier-Gx, Grouped, code 1022, section 5.3.22 */
	{
		struct dict_avp_data data = {
			1022,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Charging-Identifier-Gx",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Allocation-Retention-Priority, Grouped, code 1034, section 5.3.32 */
	{
		struct dict_avp_data data = {
			1034,	/* Code */
			10415,	/* Vendor */
			"Allocation-Retention-Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AN-GW-Address, Address, code 1050, section 5.3.49                */
	{
		struct dict_avp_data data = {
			1050,	/* Code */
			10415,	/* Vendor */
			"AN-GW-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* AN-GW-Status, Enumerated, code 2811, section 5.3.100             */
	{
		struct dict_avp_data data = {
			2811,	/* Code */
			10415,	/* Vendor */
			"AN-GW-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AN-GW-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* APN-Aggregate-Max-Bitrate-DL, Unsigned32, code 1040, section 5.3.39 */
	{
		struct dict_avp_data data = {
			1040,	/* Code */
			10415,	/* Vendor */
			"APN-Aggregate-Max-Bitrate-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-Aggregate-Max-Bitrate-UL, Unsigned32, code 1041, section 5.3.40 */
	{
		struct dict_avp_data data = {
			1041,	/* Code */
			10415,	/* Vendor */
			"APN-Aggregate-Max-Bitrate-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Application-Detection-Information, Grouped, code 1098, section 5.3.91 */
	{
		struct dict_avp_data data = {
			1098,	/* Code */
			10415,	/* Vendor */
			"Application-Detection-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Bearer-Control-Mode, Enumerated, code 1023, section 5.3.23       */
	{
		struct dict_avp_data data = {
			1023,	/* Code */
			10415,	/* Vendor */
			"Bearer-Control-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Bearer-Control-Mode)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Bearer-Identifier, OctetString, code 1020, section 5.3.20        */
	{
		struct dict_avp_data data = {
			1020,	/* Code */
			10415,	/* Vendor */
			"Bearer-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Bearer-Operation, Enumerated, code 1021, section 5.3.21          */
	{
		struct dict_avp_data data = {
			1021,	/* Code */
			10415,	/* Vendor */
			"Bearer-Operation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Bearer-Operation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Bearer-Usage, Enumerated, code 1000, section 5.3.1               */
	{
		struct dict_avp_data data = {
			1000,	/* Code */
			10415,	/* Vendor */
			"Bearer-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Bearer-Usage)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Charging-Correlation-Indicator, Enumerated, code 1073, section 5.3.67 */
	{
		struct dict_avp_data data = {
			1073,	/* Code */
			10415,	/* Vendor */
			"Charging-Correlation-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Charging-Correlation-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Charging-Rule-Base-Name, UTF8String, code 1004, section 5.3.5    */
	{
		struct dict_avp_data data = {
			1004,	/* Code */
			10415,	/* Vendor */
			"Charging-Rule-Base-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Charging-Rule-Definition, Grouped, code 1003, section 5.3.4      */
	{
		struct dict_avp_data data = {
			1003,	/* Code */
			10415,	/* Vendor */
			"Charging-Rule-Definition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Charging-Rule-Install, Grouped, code 1001, section 5.3.2         */
	{
		struct dict_avp_data data = {
			1001,	/* Code */
			10415,	/* Vendor */
			"Charging-Rule-Install",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Charging-Rule-Name, OctetString, code 1005, section 5.3.6        */
	{
		struct dict_avp_data data = {
			1005,	/* Code */
			10415,	/* Vendor */
			"Charging-Rule-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Charging-Rule-Remove, Grouped, code 1002, section 5.3.3          */
	{
		struct dict_avp_data data = {
			1002,	/* Code */
			10415,	/* Vendor */
			"Charging-Rule-Remove",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Charging-Rule-Report, Grouped, code 1018, section 5.3.18         */
	{
		struct dict_avp_data data = {
			1018,	/* Code */
			10415,	/* Vendor */
			"Charging-Rule-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CoA-Information, Grouped, code 1039, section 5.3.37              */
	{
		struct dict_avp_data data = {
			1039,	/* Code */
			10415,	/* Vendor */
			"CoA-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CoA-IP-Address, Address, code 1035, section 5.3.33               */
	{
		struct dict_avp_data data = {
			1035,	/* Code */
			10415,	/* Vendor */
			"CoA-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Conditional-APN-Aggregate-Max-Bitrate, Grouped, code 2818, section 5.3.105 */
	{
		struct dict_avp_data data = {
			2818,	/* Code */
			10415,	/* Vendor */
			"Conditional-APN-Aggregate-Max-Bitrate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Conditional-Policy-Information, Grouped, code 2840, section 5.3.128 */
	{
		struct dict_avp_data data = {
			2840,	/* Code */
			10415,	/* Vendor */
			"Conditional-Policy-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Credit-Management-Status, Unsigned32, code 1082, section 5.3.102 */
	{
		struct dict_avp_data data = {
			1082,	/* Code */
			10415,	/* Vendor */
			"Credit-Management-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CSG-Information-Reporting, Enumerated, code 1071, section 5.3.64 */
	{
		struct dict_avp_data data = {
			1071,	/* Code */
			10415,	/* Vendor */
			"CSG-Information-Reporting",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CSG-Information-Reporting)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Default-Access, Enumerated, code 2829, section 5.3.120           */
	{
		struct dict_avp_data data = {
			2829,	/* Code */
			10415,	/* Vendor */
			"Default-Access",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Default-Access)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Default-Bearer-Indication, Enumerated, code 2844, section 5.3.132 */
	{
		struct dict_avp_data data = {
			2844,	/* Code */
			10415,	/* Vendor */
			"Default-Bearer-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Default-Bearer-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Default-EPS-Bearer-QoS, Grouped, code 1049, section 5.3.48       */
	{
		struct dict_avp_data data = {
			1049,	/* Code */
			10415,	/* Vendor */
			"Default-EPS-Bearer-QoS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Default-QoS-Information, Grouped, code 2816, section 5.3.103     */
	{
		struct dict_avp_data data = {
			2816,	/* Code */
			10415,	/* Vendor */
			"Default-QoS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Default-QoS-Name, UTF8String, code 2817, section 5.3.104         */
	{
		struct dict_avp_data data = {
			2817,	/* Code */
			10415,	/* Vendor */
			"Default-QoS-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Event-Report-Indication, Grouped, code 1033, section 5.3.30      */
	{
		struct dict_avp_data data = {
			1033,	/* Code */
			10415,	/* Vendor */
			"Event-Report-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Event-Trigger, Enumerated, code 1006, section 5.3.7              */
	{
		struct dict_avp_data data = {
			1006,	/* Code */
			10415,	/* Vendor */
			"Event-Trigger",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Event-Trigger)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Execution-Time, Time, code 2839, section 5.3.127                 */
	{
		struct dict_avp_data data = {
			2839,	/* Code */
			10415,	/* Vendor */
			"Execution-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Extended-APN-AMBR-DL, Unsigned32, code 2848, section 5.3.134     */
	{
		struct dict_avp_data data = {
			2848,	/* Code */
			10415,	/* Vendor */
			"Extended-APN-AMBR-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-APN-AMBR-UL, Unsigned32, code 2849, section 5.3.135     */
	{
		struct dict_avp_data data = {
			2849,	/* Code */
			10415,	/* Vendor */
			"Extended-APN-AMBR-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-GBR-DL, Unsigned32, code 2850, section 5.3.136          */
	{
		struct dict_avp_data data = {
			2850,	/* Code */
			10415,	/* Vendor */
			"Extended-GBR-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-GBR-UL, Unsigned32, code 2851, section 5.3.137          */
	{
		struct dict_avp_data data = {
			2851,	/* Code */
			10415,	/* Vendor */
			"Extended-GBR-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Flow-Direction, Enumerated, code 1080, section 5.3.65            */
	{
		struct dict_avp_data data = {
			1080,	/* Code */
			10415,	/* Vendor */
			"Flow-Direction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Flow-Direction)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Flow-Information, Grouped, code 1058, section 5.3.53             */
	{
		struct dict_avp_data data = {
			1058,	/* Code */
			10415,	/* Vendor */
			"Flow-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Flow-Label, OctetString, code 1057, section 5.3.52               */
	{
		struct dict_avp_data data = {
			1057,	/* Code */
			10415,	/* Vendor */
			"Flow-Label",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Fixed-User-Location-Info, Grouped, code 2825, section 5.3.112    */
	{
		struct dict_avp_data data = {
			2825,	/* Code */
			10415,	/* Vendor */
			"Fixed-User-Location-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Guaranteed-Bitrate-DL, Unsigned32, code 1025, section 5.3.25     */
	{
		struct dict_avp_data data = {
			1025,	/* Code */
			10415,	/* Vendor */
			"Guaranteed-Bitrate-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Guaranteed-Bitrate-UL, Unsigned32, code 1026, section 5.3.26     */
	{
		struct dict_avp_data data = {
			1026,	/* Code */
			10415,	/* Vendor */
			"Guaranteed-Bitrate-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.212 V11.8.0 (2013-03) removed HeNB-BBF-FQDN (2803). */
	/* HeNB-BBF-FQDN, UTF8String, code 2803, section 5.3.94             */
	{
		struct dict_avp_data data = {
			2803,	/* Code */
			10415,	/* Vendor */
			"HeNB-BBF-FQDN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* HeNB-Local-IP-Address, Address, code 2804, section 5.3.95        */
	{
		struct dict_avp_data data = {
			2804,	/* Code */
			10415,	/* Vendor */
			"HeNB-Local-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* IP-CAN-Session-Charging-Scope, Enumerated, code 2827, section 5.3.114 */
	{
		struct dict_avp_data data = {
			2827,	/* Code */
			10415,	/* Vendor */
			"IP-CAN-Session-Charging-Scope",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IP-CAN-Session-Charging-Scope)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* IP-CAN-Type, Enumerated, code 1027, section 5.3.27               */
	{
		struct dict_avp_data data = {
			1027,	/* Code */
			10415,	/* Vendor */
			"IP-CAN-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IP-CAN-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Max-PLR-DL, Float32, code 2852, section 5.3.138                  */
	{
		struct dict_avp_data data = {
			2852,	/* Code */
			10415,	/* Vendor */
			"Max-PLR-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_FLOAT32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Max-PLR-UL, Float32, code 2853, section 5.3.139                  */
	{
		struct dict_avp_data data = {
			2853,	/* Code */
			10415,	/* Vendor */
			"Max-PLR-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_FLOAT32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Metering-Method, Enumerated, code 1007, section 5.3.8            */
	{
		struct dict_avp_data data = {
			1007,	/* Code */
			10415,	/* Vendor */
			"Metering-Method",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Metering-Method)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Monitoring-Flags, Unsigned32, code 2828, section 5.3.115         */
	{
		struct dict_avp_data data = {
			2828,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Key, OctetString, code 1066, section 5.3.59           */
	{
		struct dict_avp_data data = {
			1066,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Key",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Mute-Notification, Enumerated, code 2809, section 5.3.98         */
	{
		struct dict_avp_data data = {
			2809,	/* Code */
			10415,	/* Vendor */
			"Mute-Notification",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Mute-Notification)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Monitoring-Time, Time, code 2810, section 5.3.99                 */
	{
		struct dict_avp_data data = {
			2810,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* NBIFOM-Mode, Enumerated, code 2830, section 5.3.117              */
	{
		struct dict_avp_data data = {
			2830,	/* Code */
			10415,	/* Vendor */
			"NBIFOM-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/NBIFOM-Mode)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* NBIFOM-Support, Enumerated, code 2831, section 5.3.116           */
	{
		struct dict_avp_data data = {
			2831,	/* Code */
			10415,	/* Vendor */
			"NBIFOM-Support",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/NBIFOM-Support)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* NetLoc-Access-Support, Unsigned32, code 2824, section 5.3.111    */
	{
		struct dict_avp_data data = {
			2824,	/* Code */
			10415,	/* Vendor */
			"NetLoc-Access-Support",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Network-Request-Support, Enumerated, code 1024, section 5.3.24   */
	{
		struct dict_avp_data data = {
			1024,	/* Code */
			10415,	/* Vendor */
			"Network-Request-Support",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Network-Request-Support)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Offline, Enumerated, code 1008, section 5.3.9                    */
	{
		struct dict_avp_data data = {
			1008,	/* Code */
			10415,	/* Vendor */
			"Offline",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Offline)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Online, Enumerated, code 1009, section 5.3.10                    */
	{
		struct dict_avp_data data = {
			1009,	/* Code */
			10415,	/* Vendor */
			"Online",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Online)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Packet-Filter-Content, IPFilterRule, code 1059, section 5.3.54   */
	{
		struct dict_avp_data data = {
			1059,	/* Code */
			10415,	/* Vendor */
			"Packet-Filter-Content",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, IPFilterRule_type, NULL);
	};

	/* Packet-Filter-Identifier, OctetString, code 1060, section 5.3.55 */
	{
		struct dict_avp_data data = {
			1060,	/* Code */
			10415,	/* Vendor */
			"Packet-Filter-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Packet-Filter-Information, Grouped, code 1061, section 5.3.56    */
	{
		struct dict_avp_data data = {
			1061,	/* Code */
			10415,	/* Vendor */
			"Packet-Filter-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Packet-Filter-Operation, Enumerated, code 1062, section 5.3.57   */
	{
		struct dict_avp_data data = {
			1062,	/* Code */
			10415,	/* Vendor */
			"Packet-Filter-Operation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Packet-Filter-Operation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Packet-Filter-Usage, Enumerated, code 1072, section 5.3.66       */
	{
		struct dict_avp_data data = {
			1072,	/* Code */
			10415,	/* Vendor */
			"Packet-Filter-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Packet-Filter-Usage)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PCC-Rule-Status, Enumerated, code 1019, section 5.3.19           */
	{
		struct dict_avp_data data = {
			1019,	/* Code */
			10415,	/* Vendor */
			"PCC-Rule-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PCC-Rule-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: 3GPP TS 29.212 table 5.3.0.1 row PDN-Connection-ID (1065)  */
	/* missing M.                                                       */
	/* PDN-Connection-ID, OctetString, code 1065, section 5.3.58        */
	{
		struct dict_avp_data data = {
			1065,	/* Code */
			10415,	/* Vendor */
			"PDN-Connection-ID",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PRA-Install, Grouped, code 2845, section 5.3.130                 */
	{
		struct dict_avp_data data = {
			2845,	/* Code */
			10415,	/* Vendor */
			"PRA-Install",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PRA-Remove, Grouped, code 2846, section 5.3.131                  */
	{
		struct dict_avp_data data = {
			2846,	/* Code */
			10415,	/* Vendor */
			"PRA-Remove",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Precedence, Unsigned32, code 1010, section 5.3.11                */
	{
		struct dict_avp_data data = {
			1010,	/* Code */
			10415,	/* Vendor */
			"Precedence",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Pre-emption-Capability, Enumerated, code 1047, section 5.3.46    */
	{
		struct dict_avp_data data = {
			1047,	/* Code */
			10415,	/* Vendor */
			"Pre-emption-Capability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Pre-emption-Capability)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Pre-emption-Vulnerability, Enumerated, code 1048, section 5.3.47 */
	{
		struct dict_avp_data data = {
			1048,	/* Code */
			10415,	/* Vendor */
			"Pre-emption-Vulnerability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Pre-emption-Vulnerability)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Presence-Reporting-Area-Elements-List, OctetString, code 2820, section 5.3.107 */
	{
		struct dict_avp_data data = {
			2820,	/* Code */
			10415,	/* Vendor */
			"Presence-Reporting-Area-Elements-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Presence-Reporting-Area-Identifier, OctetString, code 2821, section 5.3.108 */
	{
		struct dict_avp_data data = {
			2821,	/* Code */
			10415,	/* Vendor */
			"Presence-Reporting-Area-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Presence-Reporting-Area-Information, Grouped, code 2822, section 5.3.109 */
	{
		struct dict_avp_data data = {
			2822,	/* Code */
			10415,	/* Vendor */
			"Presence-Reporting-Area-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Presence-Reporting-Area-Node, Unsigned32, code 2855, section 5.3.141 */
	{
		struct dict_avp_data data = {
			2855,	/* Code */
			10415,	/* Vendor */
			"Presence-Reporting-Area-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Presence-Reporting-Area-Status, Unsigned32, code 2823, section 5.3.110 */
	{
		struct dict_avp_data data = {
			2823,	/* Code */
			10415,	/* Vendor */
			"Presence-Reporting-Area-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Priority-Level, Unsigned32, code 1046, section 5.3.45            */
	{
		struct dict_avp_data data = {
			1046,	/* Code */
			10415,	/* Vendor */
			"Priority-Level",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.212 table 5.3.0.1 row PS-to-CS-Session-Continuity (1099) */
	/* missing M.                                                       */
	/* PS-to-CS-Session-Continuity, Enumerated, code 1099, section 5.3.84 */
	{
		struct dict_avp_data data = {
			1099,	/* Code */
			10415,	/* Vendor */
			"PS-to-CS-Session-Continuity",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PS-to-CS-Session-Continuity)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* QoS-Class-Identifier, Enumerated, code 1028, section 5.3.17      */
	{
		struct dict_avp_data data = {
			1028,	/* Code */
			10415,	/* Vendor */
			"QoS-Class-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/QoS-Class-Identifier)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* QoS-Information, Grouped, code 1016, section 5.3.16              */
	{
		struct dict_avp_data data = {
			1016,	/* Code */
			10415,	/* Vendor */
			"QoS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* QoS-Negotiation, Enumerated, code 1029, section 5.3.28           */
	{
		struct dict_avp_data data = {
			1029,	/* Code */
			10415,	/* Vendor */
			"QoS-Negotiation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/QoS-Negotiation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* QoS-Upgrade, Enumerated, code 1030, section 5.3.29               */
	{
		struct dict_avp_data data = {
			1030,	/* Code */
			10415,	/* Vendor */
			"QoS-Upgrade",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/QoS-Upgrade)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* RAN-NAS-Release-Cause, OctetString, code 2819, section 5.3.106   */
	{
		struct dict_avp_data data = {
			2819,	/* Code */
			10415,	/* Vendor */
			"RAN-NAS-Release-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RAN-Rule-Support, Unsigned32, code 2832, section 5.3.122         */
	{
		struct dict_avp_data data = {
			2832,	/* Code */
			10415,	/* Vendor */
			"RAN-Rule-Support",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RAT-Type, Enumerated, code 1032, section 5.3.31                  */
	{
		struct dict_avp_data data = {
			1032,	/* Code */
			10415,	/* Vendor */
			"RAT-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/RAT-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Redirect-Information, Grouped, code 1085, section 5.3.82         */
	{
		struct dict_avp_data data = {
			1085,	/* Code */
			10415,	/* Vendor */
			"Redirect-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Redirect-Support, Enumerated, code 1086, section 5.3.83          */
	{
		struct dict_avp_data data = {
			1086,	/* Code */
			10415,	/* Vendor */
			"Redirect-Support",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Redirect-Support)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Removal-Of-Access, Enumerated, code 2842, section 5.3.126        */
	{
		struct dict_avp_data data = {
			2842,	/* Code */
			10415,	/* Vendor */
			"Removal-Of-Access",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Removal-Of-Access)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Reporting-Level, Enumerated, code 1011, section 5.3.12           */
	{
		struct dict_avp_data data = {
			1011,	/* Code */
			10415,	/* Vendor */
			"Reporting-Level",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Reporting-Level)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Resource-Allocation-Notification, Enumerated, code 1063, section 5.3.50 */
	{
		struct dict_avp_data data = {
			1063,	/* Code */
			10415,	/* Vendor */
			"Resource-Allocation-Notification",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Resource-Allocation-Notification)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Resource-Release-Notification, Enumerated, code 2841, section 5.3.125 */
	{
		struct dict_avp_data data = {
			2841,	/* Code */
			10415,	/* Vendor */
			"Resource-Release-Notification",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Resource-Release-Notification)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Revalidation-Time, Time, code 1042, section 5.3.41               */
	{
		struct dict_avp_data data = {
			1042,	/* Code */
			10415,	/* Vendor */
			"Revalidation-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Routing-Filter, Grouped, code 1078, section 5.3.72               */
	{
		struct dict_avp_data data = {
			1078,	/* Code */
			10415,	/* Vendor */
			"Routing-Filter",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-IP-Address, Address, code 1079, section 5.3.73           */
	{
		struct dict_avp_data data = {
			1079,	/* Code */
			10415,	/* Vendor */
			"Routing-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Routing-Rule-Definition, Grouped, code 1076, section 5.3.70      */
	{
		struct dict_avp_data data = {
			1076,	/* Code */
			10415,	/* Vendor */
			"Routing-Rule-Definition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-Rule-Identifier, OctetString, code 1077, section 5.3.71  */
	{
		struct dict_avp_data data = {
			1077,	/* Code */
			10415,	/* Vendor */
			"Routing-Rule-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-Rule-Install, Grouped, code 1081, section 5.3.68         */
	{
		struct dict_avp_data data = {
			1081,	/* Code */
			10415,	/* Vendor */
			"Routing-Rule-Install",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-Rule-Remove, Grouped, code 1075, section 5.3.69          */
	{
		struct dict_avp_data data = {
			1075,	/* Code */
			10415,	/* Vendor */
			"Routing-Rule-Remove",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-Rule-Failure-Code, Unsigned32, code 2834, section 5.3.119 */
	{
		struct dict_avp_data data = {
			2834,	/* Code */
			10415,	/* Vendor */
			"Routing-Rule-Failure-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-Rule-Report, Grouped, code 2835, section 5.3.118         */
	{
		struct dict_avp_data data = {
			2835,	/* Code */
			10415,	/* Vendor */
			"Routing-Rule-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Rule-Activation-Time, Time, code 1043, section 5.3.42            */
	{
		struct dict_avp_data data = {
			1043,	/* Code */
			10415,	/* Vendor */
			"Rule-Activation-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Rule-Deactivation-Time, Time, code 1044, section 5.3.43          */
	{
		struct dict_avp_data data = {
			1044,	/* Code */
			10415,	/* Vendor */
			"Rule-Deactivation-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Rule-Failure-Code, Enumerated, code 1031, section 5.3.38         */
	{
		struct dict_avp_data data = {
			1031,	/* Code */
			10415,	/* Vendor */
			"Rule-Failure-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Rule-Failure-Code)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Security-Parameter-Index, OctetString, code 1056, section 5.3.51 */
	{
		struct dict_avp_data data = {
			1056,	/* Code */
			10415,	/* Vendor */
			"Security-Parameter-Index",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Session-Release-Cause, Enumerated, code 1045, section 5.3.44     */
	{
		struct dict_avp_data data = {
			1045,	/* Code */
			10415,	/* Vendor */
			"Session-Release-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Session-Release-Cause)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* TCP-Source-Port, Unsigned32, code 2843, section 5.3.129          */
	{
		struct dict_avp_data data = {
			2843,	/* Code */
			10415,	/* Vendor */
			"TCP-Source-Port",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TDF-Information, Grouped, code 1087, section 5.3.78              */
	{
		struct dict_avp_data data = {
			1087,	/* Code */
			10415,	/* Vendor */
			"TDF-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TDF-Application-Identifier, OctetString, code 1088, section 5.3.77 */
	{
		struct dict_avp_data data = {
			1088,	/* Code */
			10415,	/* Vendor */
			"TDF-Application-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TDF-Application-Instance-Identifier, OctetString, code 2802, section 5.3.92 */
	{
		struct dict_avp_data data = {
			2802,	/* Code */
			10415,	/* Vendor */
			"TDF-Application-Instance-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TDF-Destination-Host, DiameterIdentity, code 1089, section 5.3.80 */
	{
		struct dict_avp_data data = {
			1089,	/* Code */
			10415,	/* Vendor */
			"TDF-Destination-Host",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* TDF-Destination-Realm, DiameterIdentity, code 1090, section 5.3.79 */
	{
		struct dict_avp_data data = {
			1090,	/* Code */
			10415,	/* Vendor */
			"TDF-Destination-Realm",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* TDF-IP-Address, Address, code 1091, section 5.3.81               */
	{
		struct dict_avp_data data = {
			1091,	/* Code */
			10415,	/* Vendor */
			"TDF-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* TFT-Filter, IPFilterRule, code 1012, section 5.3.13              */
	{
		struct dict_avp_data data = {
			1012,	/* Code */
			10415,	/* Vendor */
			"TFT-Filter",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, IPFilterRule_type, NULL);
	};

	/* TFT-Packet-Filter-Information, Grouped, code 1013, section 5.3.14 */
	{
		struct dict_avp_data data = {
			1013,	/* Code */
			10415,	/* Vendor */
			"TFT-Packet-Filter-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Traffic-Steering-Policy-Identifier-DL, OctetString, code 2836, section 5.3.123 */
	{
		struct dict_avp_data data = {
			2836,	/* Code */
			10415,	/* Vendor */
			"Traffic-Steering-Policy-Identifier-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Traffic-Steering-Policy-Identifier-UL, OctetString, code 2837, section 5.3.124 */
	{
		struct dict_avp_data data = {
			2837,	/* Code */
			10415,	/* Vendor */
			"Traffic-Steering-Policy-Identifier-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ToS-Traffic-Class, OctetString, code 1014, section 5.3.15        */
	{
		struct dict_avp_data data = {
			1014,	/* Code */
			10415,	/* Vendor */
			"ToS-Traffic-Class",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Tunnel-Header-Filter, IPFilterRule, code 1036, section 5.3.34    */
	{
		struct dict_avp_data data = {
			1036,	/* Code */
			10415,	/* Vendor */
			"Tunnel-Header-Filter",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, IPFilterRule_type, NULL);
	};

	/* Tunnel-Header-Length, Unsigned32, code 1037, section 5.3.35      */
	{
		struct dict_avp_data data = {
			1037,	/* Code */
			10415,	/* Vendor */
			"Tunnel-Header-Length",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Tunnel-Information, Grouped, code 1038, section 5.3.36           */
	{
		struct dict_avp_data data = {
			1038,	/* Code */
			10415,	/* Vendor */
			"Tunnel-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UDP-Source-Port, Unsigned32, code 2806, section 5.3.97           */
	{
		struct dict_avp_data data = {
			2806,	/* Code */
			10415,	/* Vendor */
			"UDP-Source-Port",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UE-Local-IP-Address, Address, code 2805, section 5.3.96          */
	{
		struct dict_avp_data data = {
			2805,	/* Code */
			10415,	/* Vendor */
			"UE-Local-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* UE-Status, Unsigned32, code 2854, section 5.3.140                */
	{
		struct dict_avp_data data = {
			2854,	/* Code */
			10415,	/* Vendor */
			"UE-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Usage-Monitoring-Information, Grouped, code 1067, section 5.3.60 */
	{
		struct dict_avp_data data = {
			1067,	/* Code */
			10415,	/* Vendor */
			"Usage-Monitoring-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Usage-Monitoring-Level, Enumerated, code 1068, section 5.3.61    */
	{
		struct dict_avp_data data = {
			1068,	/* Code */
			10415,	/* Vendor */
			"Usage-Monitoring-Level",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Usage-Monitoring-Level)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Usage-Monitoring-Report, Enumerated, code 1069, section 5.3.62   */
	{
		struct dict_avp_data data = {
			1069,	/* Code */
			10415,	/* Vendor */
			"Usage-Monitoring-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Usage-Monitoring-Report)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Usage-Monitoring-Support, Enumerated, code 1070, section 5.3.63  */
	{
		struct dict_avp_data data = {
			1070,	/* Code */
			10415,	/* Vendor */
			"Usage-Monitoring-Support",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Usage-Monitoring-Support)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* User-Location-Info-Time, Time, code 2812, section 5.3.101        */
	{
		struct dict_avp_data data = {
			2812,	/* Code */
			10415,	/* Vendor */
			"User-Location-Info-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* PCSCF-Restoration-Indication, Unsigned32, code 2826, section 5.3.113 */
	{
		struct dict_avp_data data = {
			2826,	/* Code */
			10415,	/* Vendor */
			"PCSCF-Restoration-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.212 Table 5a.3.0.1: Gxx specific Diameter AVPs        */
	/*==================================================================*/

	/* QoS-Rule-Install, Grouped, code 1051, section 5a.3.1             */
	{
		struct dict_avp_data data = {
			1051,	/* Code */
			10415,	/* Vendor */
			"QoS-Rule-Install",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* QoS-Rule-Remove, Grouped, code 1052, section 5a.3.2              */
	{
		struct dict_avp_data data = {
			1052,	/* Code */
			10415,	/* Vendor */
			"QoS-Rule-Remove",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* QoS-Rule-Definition, Grouped, code 1053, section 5a.3.3          */
	{
		struct dict_avp_data data = {
			1053,	/* Code */
			10415,	/* Vendor */
			"QoS-Rule-Definition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* QoS-Rule-Name, OctetString, code 1054, section 5a.3.4            */
	{
		struct dict_avp_data data = {
			1054,	/* Code */
			10415,	/* Vendor */
			"QoS-Rule-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* QoS-Rule-Base-Name, UTF8String, code 1074, section 5a.3.7        */
	{
		struct dict_avp_data data = {
			1074,	/* Code */
			10415,	/* Vendor */
			"QoS-Rule-Base-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* QoS-Rule-Report, Grouped, code 1055, section 5a.3.5              */
	{
		struct dict_avp_data data = {
			1055,	/* Code */
			10415,	/* Vendor */
			"QoS-Rule-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Session-Linking-Indicator, Enumerated, code 1064, section 5a.3.6 */
	{
		struct dict_avp_data data = {
			1064,	/* Code */
			10415,	/* Vendor */
			"Session-Linking-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Session-Linking-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.212 Table 5b.3.0.1: Sd specific Diameter AVPs         */
	/*==================================================================*/

	/* ADC-Rule-Base-Name, UTF8String, code 1095, section 5b.3.4        */
	{
		struct dict_avp_data data = {
			1095,	/* Code */
			10415,	/* Vendor */
			"ADC-Rule-Base-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* ADC-Rule-Definition, Grouped, code 1094, section 5b.3.3          */
	{
		struct dict_avp_data data = {
			1094,	/* Code */
			10415,	/* Vendor */
			"ADC-Rule-Definition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ADC-Rule-Install, Grouped, code 1092, section 5b.3.1             */
	{
		struct dict_avp_data data = {
			1092,	/* Code */
			10415,	/* Vendor */
			"ADC-Rule-Install",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ADC-Rule-Name, OctetString, code 1096, section 5b.3.5            */
	{
		struct dict_avp_data data = {
			1096,	/* Code */
			10415,	/* Vendor */
			"ADC-Rule-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ADC-Rule-Remove, Grouped, code 1093, section 5b.3.2              */
	{
		struct dict_avp_data data = {
			1093,	/* Code */
			10415,	/* Vendor */
			"ADC-Rule-Remove",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ADC-Rule-Report, Grouped, code 1097, section 5b.3.6              */
	{
		struct dict_avp_data data = {
			1097,	/* Code */
			10415,	/* Vendor */
			"ADC-Rule-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.212 Table 5c.3.1.1: St specific Diameter AVPs         */
	/*==================================================================*/

	/* Request-Type, Unsigned32, code 2838, section 5c.3.2              */
	{
		struct dict_avp_data data = {
			2838,	/* Code */
			10415,	/* Vendor */
			"Request-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.212 Table E.6.3.1.1: S15 specific Diameter AVPs       */
	/*==================================================================*/

	/* CS-Service-Qos-Request-Identifier, OctetString, code 2807, section E.6.3.2 */
	{
		struct dict_avp_data data = {
			2807,	/* Code */
			10415,	/* Vendor */
			"CS-Service-Qos-Request-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CS-Service-QoS-Request-Operation, Enumerated, code 2808, section E.6.3.3 */
	{
		struct dict_avp_data data = {
			2808,	/* Code */
			10415,	/* Vendor */
			"CS-Service-QoS-Request-Operation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CS-Service-QoS-Request-Operation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* CS-Service-Resource-Report, Grouped, code 2813, section E.6.3.6  */
	{
		struct dict_avp_data data = {
			2813,	/* Code */
			10415,	/* Vendor */
			"CS-Service-Resource-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CS-Service-Resource-Failure-Cause, Enumerated, code 2814, section E.6.3.5 */
	{
		struct dict_avp_data data = {
			2814,	/* Code */
			10415,	/* Vendor */
			"CS-Service-Resource-Failure-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CS-Service-Resource-Failure-Cause)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* CS-Service-Resource-Result-Operation, Enumerated, code 2815, section E.6.3.4 */
	{
		struct dict_avp_data data = {
			2815,	/* Code */
			10415,	/* Vendor */
			"CS-Service-Resource-Result-Operation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CS-Service-Resource-Result-Operation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.214 V15.7.0 (2019-09)                                 */
	/* From 3GPP 29214-f70.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.214 Table 5.3.0.1: Rx specific Diameter AVPs          */
	/*==================================================================*/

	/* Abort-Cause, Enumerated, code 500, section 5.3.1                 */
	{
		struct dict_avp_data data = {
			500,	/* Code */
			10415,	/* Vendor */
			"Abort-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Abort-Cause)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Access-Network-Charging-Address, Address, code 501, section 5.3.2 */
	{
		struct dict_avp_data data = {
			501,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Charging-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Access-Network-Charging-Identifier, Grouped, code 502, section 5.3.3 */
	{
		struct dict_avp_data data = {
			502,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Charging-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Network-Charging-Identifier-Value, OctetString, code 503, section 5.3.4 */
	{
		struct dict_avp_data data = {
			503,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Charging-Identifier-Value",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Acceptable-Service-Info, Grouped, code 526, section 5.3.24       */
	{
		struct dict_avp_data data = {
			526,	/* Code */
			10415,	/* Vendor */
			"Acceptable-Service-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AF-Application-Identifier, OctetString, code 504, section 5.3.5  */
	{
		struct dict_avp_data data = {
			504,	/* Code */
			10415,	/* Vendor */
			"AF-Application-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AF-Charging-Identifier, OctetString, code 505, section 5.3.6     */
	{
		struct dict_avp_data data = {
			505,	/* Code */
			10415,	/* Vendor */
			"AF-Charging-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AF-Requested-Data, Unsigned32, code 551, section 5.3.50          */
	{
		struct dict_avp_data data = {
			551,	/* Code */
			10415,	/* Vendor */
			"AF-Requested-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AF-Signalling-Protocol, Enumerated, code 529, section 5.3.26     */
	{
		struct dict_avp_data data = {
			529,	/* Code */
			10415,	/* Vendor */
			"AF-Signalling-Protocol",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AF-Signalling-Protocol)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Application-Service-Provider-Identity, UTF8String, code 532, section 5.3.29 */
	{
		struct dict_avp_data data = {
			532,	/* Code */
			10415,	/* Vendor */
			"Application-Service-Provider-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Codec-Data, OctetString, code 524, section 5.3.7                 */
	{
		struct dict_avp_data data = {
			524,	/* Code */
			10415,	/* Vendor */
			"Codec-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Content-Version, Unsigned64, code 552, section 5.3.49            */
	{
		struct dict_avp_data data = {
			552,	/* Code */
			10415,	/* Vendor */
			"Content-Version",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Max-Requested-BW-DL, Unsigned32, code 554, section 5.3.52 */
	{
		struct dict_avp_data data = {
			554,	/* Code */
			10415,	/* Vendor */
			"Extended-Max-Requested-BW-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Max-Requested-BW-UL, Unsigned32, code 555, section 5.3.53 */
	{
		struct dict_avp_data data = {
			555,	/* Code */
			10415,	/* Vendor */
			"Extended-Max-Requested-BW-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Max-Supported-BW-DL, Unsigned32, code 556, section 5.3.54 */
	{
		struct dict_avp_data data = {
			556,	/* Code */
			10415,	/* Vendor */
			"Extended-Max-Supported-BW-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Max-Supported-BW-UL, Unsigned32, code 557, section 5.3.55 */
	{
		struct dict_avp_data data = {
			557,	/* Code */
			10415,	/* Vendor */
			"Extended-Max-Supported-BW-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Min-Desired-BW-DL, Unsigned32, code 558, section 5.3.56 */
	{
		struct dict_avp_data data = {
			558,	/* Code */
			10415,	/* Vendor */
			"Extended-Min-Desired-BW-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Min-Desired-BW-UL, Unsigned32, code 559, section 5.3.57 */
	{
		struct dict_avp_data data = {
			559,	/* Code */
			10415,	/* Vendor */
			"Extended-Min-Desired-BW-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Min-Requested-BW-DL, Unsigned32, code 560, section 5.3.58 */
	{
		struct dict_avp_data data = {
			560,	/* Code */
			10415,	/* Vendor */
			"Extended-Min-Requested-BW-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Extended-Min-Requested-BW-UL, Unsigned32, code 561, section 5.3.59 */
	{
		struct dict_avp_data data = {
			561,	/* Code */
			10415,	/* Vendor */
			"Extended-Min-Requested-BW-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Flow-Description, IPFilterRule, code 507, section 5.3.8          */
	{
		struct dict_avp_data data = {
			507,	/* Code */
			10415,	/* Vendor */
			"Flow-Description",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, IPFilterRule_type, NULL);
	};

	/* Flow-Number, Unsigned32, code 509, section 5.3.9                 */
	{
		struct dict_avp_data data = {
			509,	/* Code */
			10415,	/* Vendor */
			"Flow-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Flows, Grouped, code 510, section 5.3.10                         */
	{
		struct dict_avp_data data = {
			510,	/* Code */
			10415,	/* Vendor */
			"Flows",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Flow-Status, Enumerated, code 511, section 5.3.11                */
	{
		struct dict_avp_data data = {
			511,	/* Code */
			10415,	/* Vendor */
			"Flow-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Flow-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Flow-Usage, Enumerated, code 512, section 5.3.12                 */
	{
		struct dict_avp_data data = {
			512,	/* Code */
			10415,	/* Vendor */
			"Flow-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Flow-Usage)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* GCS-Identifier, OctetString, code 538, section 5.3.36            */
	{
		struct dict_avp_data data = {
			538,	/* Code */
			10415,	/* Vendor */
			"GCS-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.214 table 5.3.0.1 row IMS-Content-Identifier (563) */
	/* missing M, assume MUST NOT.                                      */
	/* IMS-Content-Identifier, OctetString, code 563, section 5.3.60    */
	{
		struct dict_avp_data data = {
			563,	/* Code */
			10415,	/* Vendor */
			"IMS-Content-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.214 table 5.3.0.1 row IMS-Content (564)         */
	/* missing M, assume MUST NOT.                                      */
	/* IMS-Content-Type, Enumerated, code 564, section 5.3.61           */
	{
		struct dict_avp_data data = {
			564,	/* Code */
			10415,	/* Vendor */
			"IMS-Content-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IMS-Content-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* IP-Domain-Id, OctetString, code 537, section 5.3.35              */
	{
		struct dict_avp_data data = {
			537,	/* Code */
			10415,	/* Vendor */
			"IP-Domain-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Max-Requested-Bandwidth-DL, Unsigned32, code 515, section 5.3.14 */
	{
		struct dict_avp_data data = {
			515,	/* Code */
			10415,	/* Vendor */
			"Max-Requested-Bandwidth-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Max-Requested-Bandwidth-UL, Unsigned32, code 516, section 5.3.15 */
	{
		struct dict_avp_data data = {
			516,	/* Code */
			10415,	/* Vendor */
			"Max-Requested-Bandwidth-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Max-Supported-Bandwidth-DL, Unsigned32, code 543, section 5.3.41 */
	{
		struct dict_avp_data data = {
			543,	/* Code */
			10415,	/* Vendor */
			"Max-Supported-Bandwidth-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Max-Supported-Bandwidth-UL, Unsigned32, code 544, section 5.3.42 */
	{
		struct dict_avp_data data = {
			544,	/* Code */
			10415,	/* Vendor */
			"Max-Supported-Bandwidth-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MCPTT-Identifier, OctetString, code 547, section 5.3.45          */
	{
		struct dict_avp_data data = {
			547,	/* Code */
			10415,	/* Vendor */
			"MCPTT-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MCVideo-Identifier, OctetString, code 562, section 5.3.45a       */
	{
		struct dict_avp_data data = {
			562,	/* Code */
			10415,	/* Vendor */
			"MCVideo-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Media-Component-Description, Grouped, code 517, section 5.3.16   */
	{
		struct dict_avp_data data = {
			517,	/* Code */
			10415,	/* Vendor */
			"Media-Component-Description",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Media-Component-Number, Unsigned32, code 518, section 5.3.17     */
	{
		struct dict_avp_data data = {
			518,	/* Code */
			10415,	/* Vendor */
			"Media-Component-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Media-Component-Status, Unsigned32, code 549, section 5.3.48     */
	{
		struct dict_avp_data data = {
			549,	/* Code */
			10415,	/* Vendor */
			"Media-Component-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Media-Sub-Component, Grouped, code 519, section 5.3.18           */
	{
		struct dict_avp_data data = {
			519,	/* Code */
			10415,	/* Vendor */
			"Media-Sub-Component",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Media-Type, Enumerated, code 520, section 5.3.19                 */
	{
		struct dict_avp_data data = {
			520,	/* Code */
			10415,	/* Vendor */
			"Media-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Media-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MPS-Identifier, OctetString, code 528, section 5.3.30            */
	{
		struct dict_avp_data data = {
			528,	/* Code */
			10415,	/* Vendor */
			"MPS-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Min-Desired-Bandwidth-DL, Unsigned32, code 545, section 5.3.43   */
	{
		struct dict_avp_data data = {
			545,	/* Code */
			10415,	/* Vendor */
			"Min-Desired-Bandwidth-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Min-Desired-Bandwidth-UL, Unsigned32, code 546, section 5.3.44   */
	{
		struct dict_avp_data data = {
			546,	/* Code */
			10415,	/* Vendor */
			"Min-Desired-Bandwidth-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Min-Requested-Bandwidth-DL, Unsigned32, code 534, section 5.3.32 */
	{
		struct dict_avp_data data = {
			534,	/* Code */
			10415,	/* Vendor */
			"Min-Requested-Bandwidth-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Min-Requested-Bandwidth-UL, Unsigned32, code 535, section 5.3.33 */
	{
		struct dict_avp_data data = {
			535,	/* Code */
			10415,	/* Vendor */
			"Min-Requested-Bandwidth-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Priority-Sharing-Indicator, Enumerated, code 550, section 5.3.47 */
	{
		struct dict_avp_data data = {
			550,	/* Code */
			10415,	/* Vendor */
			"Priority-Sharing-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Priority-Sharing-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Pre-emption-Control-Info, Unsigned32, code 553, section 5.3.51   */
	{
		struct dict_avp_data data = {
			553,	/* Code */
			10415,	/* Vendor */
			"Pre-emption-Control-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Required-Access-Info, Enumerated, code 536, section 5.3.34       */
	{
		struct dict_avp_data data = {
			536,	/* Code */
			10415,	/* Vendor */
			"Required-Access-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Required-Access-Info)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Retry-Interval, Unsigned32, code 541, section 5.3.39             */
	{
		struct dict_avp_data data = {
			541,	/* Code */
			10415,	/* Vendor */
			"Retry-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Rx-Request-Type, Enumerated, code 533, section 5.3.31            */
	{
		struct dict_avp_data data = {
			533,	/* Code */
			10415,	/* Vendor */
			"Rx-Request-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Rx-Request-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* RR-Bandwidth, Unsigned32, code 521, section 5.3.20               */
	{
		struct dict_avp_data data = {
			521,	/* Code */
			10415,	/* Vendor */
			"RR-Bandwidth",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RS-Bandwidth, Unsigned32, code 522, section 5.3.21               */
	{
		struct dict_avp_data data = {
			522,	/* Code */
			10415,	/* Vendor */
			"RS-Bandwidth",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Authorization-Info, Unsigned32, code 548, section 5.3.46 */
	{
		struct dict_avp_data data = {
			548,	/* Code */
			10415,	/* Vendor */
			"Service-Authorization-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-URN, OctetString, code 525, section 5.3.23               */
	{
		struct dict_avp_data data = {
			525,	/* Code */
			10415,	/* Vendor */
			"Service-URN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Info-Status, Enumerated, code 527, section 5.3.25        */
	{
		struct dict_avp_data data = {
			527,	/* Code */
			10415,	/* Vendor */
			"Service-Info-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Service-Info-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Sharing-Key-DL, Unsigned32, code 539, section 5.3.37             */
	{
		struct dict_avp_data data = {
			539,	/* Code */
			10415,	/* Vendor */
			"Sharing-Key-DL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Sharing-Key-UL, Unsigned32, code 540, section 5.3.38             */
	{
		struct dict_avp_data data = {
			540,	/* Code */
			10415,	/* Vendor */
			"Sharing-Key-UL",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Specific-Action, Enumerated, code 513, section 5.3.13            */
	{
		struct dict_avp_data data = {
			513,	/* Code */
			10415,	/* Vendor */
			"Specific-Action",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Specific-Action)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SIP-Forking-Indication, Enumerated, code 523, section 5.3.22     */
	{
		struct dict_avp_data data = {
			523,	/* Code */
			10415,	/* Vendor */
			"SIP-Forking-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SIP-Forking-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Sponsor-Identity, UTF8String, code 531, section 5.3.28           */
	{
		struct dict_avp_data data = {
			531,	/* Code */
			10415,	/* Vendor */
			"Sponsor-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Sponsored-Connectivity-Data, Grouped, code 530, section 5.3.27   */
	{
		struct dict_avp_data data = {
			530,	/* Code */
			10415,	/* Vendor */
			"Sponsored-Connectivity-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Sponsoring-Action, Enumerated, code 542, section 5.3.40          */
	{
		struct dict_avp_data data = {
			542,	/* Code */
			10415,	/* Vendor */
			"Sponsoring-Action",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Sponsoring-Action)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.229 V15.2.0 (2019-09)                                 */
	/* From 3GPP 29229-f20.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.229 Table 6.3.1: Diameter Multimedia Application AVPs */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* Visited-Network-Identifier, OctetString, code 600, section 6.3.1 */
	{
		struct dict_avp_data data = {
			600,	/* Code */
			10415,	/* Vendor */
			"Visited-Network-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Public-Identity, UTF8String, code 601, section 6.3.2             */
	{
		struct dict_avp_data data = {
			601,	/* Code */
			10415,	/* Vendor */
			"Public-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Server-Name, UTF8String, code 602, section 6.3.3                 */
	{
		struct dict_avp_data data = {
			602,	/* Code */
			10415,	/* Vendor */
			"Server-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Server-Capabilities, Grouped, code 603, section 6.3.4            */
	{
		struct dict_avp_data data = {
			603,	/* Code */
			10415,	/* Vendor */
			"Server-Capabilities",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Mandatory-Capability, Unsigned32, code 604, section 6.3.5        */
	{
		struct dict_avp_data data = {
			604,	/* Code */
			10415,	/* Vendor */
			"Mandatory-Capability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Optional-Capability, Unsigned32, code 605, section 6.3.6         */
	{
		struct dict_avp_data data = {
			605,	/* Code */
			10415,	/* Vendor */
			"Optional-Capability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* User-Data, OctetString, code 606, section 6.3.7                  */
	{
		struct dict_avp_data data = {
			606,	/* Code */
			10415,	/* Vendor */
			"User-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Number-Auth-Items, Unsigned32, code 607, section 6.3.8       */
	{
		struct dict_avp_data data = {
			607,	/* Code */
			10415,	/* Vendor */
			"SIP-Number-Auth-Items",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Authentication-Scheme, UTF8String, code 608, section 6.3.9   */
	{
		struct dict_avp_data data = {
			608,	/* Code */
			10415,	/* Vendor */
			"SIP-Authentication-Scheme",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SIP-Authenticate, OctetString, code 609, section 6.3.10          */
	{
		struct dict_avp_data data = {
			609,	/* Code */
			10415,	/* Vendor */
			"SIP-Authenticate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Authorization, OctetString, code 610, section 6.3.11         */
	{
		struct dict_avp_data data = {
			610,	/* Code */
			10415,	/* Vendor */
			"SIP-Authorization",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Authentication-Context, OctetString, code 611, section 6.3.12 */
	{
		struct dict_avp_data data = {
			611,	/* Code */
			10415,	/* Vendor */
			"SIP-Authentication-Context",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Auth-Data-Item, Grouped, code 612, section 6.3.13            */
	{
		struct dict_avp_data data = {
			612,	/* Code */
			10415,	/* Vendor */
			"SIP-Auth-Data-Item",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Item-Number, Unsigned32, code 613, section 6.3.14            */
	{
		struct dict_avp_data data = {
			613,	/* Code */
			10415,	/* Vendor */
			"SIP-Item-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Server-Assignment-Type, Enumerated, code 614, section 6.3.15     */
	{
		struct dict_avp_data data = {
			614,	/* Code */
			10415,	/* Vendor */
			"Server-Assignment-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Server-Assignment-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Deregistration-Reason, Grouped, code 615, section 6.3.16         */
	{
		struct dict_avp_data data = {
			615,	/* Code */
			10415,	/* Vendor */
			"Deregistration-Reason",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reason-Code, Enumerated, code 616, section 6.3.17                */
	{
		struct dict_avp_data data = {
			616,	/* Code */
			10415,	/* Vendor */
			"Reason-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Reason-Code)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Reason-Info, UTF8String, code 617, section 6.3.18                */
	{
		struct dict_avp_data data = {
			617,	/* Code */
			10415,	/* Vendor */
			"Reason-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Charging-Information, Grouped, code 618, section 6.3.19          */
	{
		struct dict_avp_data data = {
			618,	/* Code */
			10415,	/* Vendor */
			"Charging-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Primary-Event-Charging-Function-Name, DiameterURI, code 619, section 6.3.20 */
	{
		struct dict_avp_data data = {
			619,	/* Code */
			10415,	/* Vendor */
			"Primary-Event-Charging-Function-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterURI_type, NULL);
	};

	/* Secondary-Event-Charging-Function-Name, DiameterURI, code 620, section 6.3.21 */
	{
		struct dict_avp_data data = {
			620,	/* Code */
			10415,	/* Vendor */
			"Secondary-Event-Charging-Function-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterURI_type, NULL);
	};

	/* Primary-Charging-Collection-Function-Name, DiameterURI, code 621, section 6.3.22 */
	{
		struct dict_avp_data data = {
			621,	/* Code */
			10415,	/* Vendor */
			"Primary-Charging-Collection-Function-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterURI_type, NULL);
	};

	/* Secondary-Charging-Collection-Function-Name, DiameterURI, code 622, section 6.3.23 */
	{
		struct dict_avp_data data = {
			622,	/* Code */
			10415,	/* Vendor */
			"Secondary-Charging-Collection-Function-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterURI_type, NULL);
	};

	/* User-Authorization-Type, Enumerated, code 623, section 6.3.24    */
	{
		struct dict_avp_data data = {
			623,	/* Code */
			10415,	/* Vendor */
			"User-Authorization-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/User-Authorization-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* User-Data-Already-Available, Enumerated, code 624, section 6.3.26 */
	{
		struct dict_avp_data data = {
			624,	/* Code */
			10415,	/* Vendor */
			"User-Data-Already-Available",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/User-Data-Already-Available)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Confidentiality-Key, OctetString, code 625, section 6.3.27       */
	{
		struct dict_avp_data data = {
			625,	/* Code */
			10415,	/* Vendor */
			"Confidentiality-Key",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Integrity-Key, OctetString, code 626, section 6.3.28             */
	{
		struct dict_avp_data data = {
			626,	/* Code */
			10415,	/* Vendor */
			"Integrity-Key",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.229 V6.5.0 (2005-06) removed User-Data-Request-Type (627). */
	/* User-Data-Request-Type, Enumerated, code 627, section 6.3.25     */
	{
		struct dict_avp_data data = {
			627,	/* Code */
			10415,	/* Vendor */
			"User-Data-Request-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/User-Data-Request-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Supported-Features, Grouped, code 628, section 6.3.29            */
	{
		struct dict_avp_data data = {
			628,	/* Code */
			10415,	/* Vendor */
			"Supported-Features",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Feature-List-ID, Unsigned32, code 629, section 6.3.30            */
	{
		struct dict_avp_data data = {
			629,	/* Code */
			10415,	/* Vendor */
			"Feature-List-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Feature-List, Unsigned32, code 630, section 6.3.31               */
	{
		struct dict_avp_data data = {
			630,	/* Code */
			10415,	/* Vendor */
			"Feature-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Supported-Applications, Grouped, code 631, section 6.3.32        */
	{
		struct dict_avp_data data = {
			631,	/* Code */
			10415,	/* Vendor */
			"Supported-Applications",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Associated-Identities, Grouped, code 632, section 6.3.33         */
	{
		struct dict_avp_data data = {
			632,	/* Code */
			10415,	/* Vendor */
			"Associated-Identities",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Originating-Request, Enumerated, code 633, section 6.3.34        */
	{
		struct dict_avp_data data = {
			633,	/* Code */
			10415,	/* Vendor */
			"Originating-Request",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Originating-Request)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Wildcarded-Public-Identity, UTF8String, code 634, section 6.3.35 */
	{
		struct dict_avp_data data = {
			634,	/* Code */
			10415,	/* Vendor */
			"Wildcarded-Public-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SIP-Digest-Authenticate, Grouped, code 635, section 6.3.36       */
	{
		struct dict_avp_data data = {
			635,	/* Code */
			10415,	/* Vendor */
			"SIP-Digest-Authenticate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Wildcarded-IMPU, UTF8String, code 636, section 6.3.43            */
	{
		struct dict_avp_data data = {
			636,	/* Code */
			10415,	/* Vendor */
			"Wildcarded-IMPU",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* UAR-Flags, Unsigned32, code 637, section 6.3.44                  */
	{
		struct dict_avp_data data = {
			637,	/* Code */
			10415,	/* Vendor */
			"UAR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Loose-Route-Indication, Enumerated, code 638, section 6.3.45     */
	{
		struct dict_avp_data data = {
			638,	/* Code */
			10415,	/* Vendor */
			"Loose-Route-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Loose-Route-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SCSCF-Restoration-Info, Grouped, code 639, section 6.3.46        */
	{
		struct dict_avp_data data = {
			639,	/* Code */
			10415,	/* Vendor */
			"SCSCF-Restoration-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Path, OctetString, code 640, section 6.3.47                      */
	{
		struct dict_avp_data data = {
			640,	/* Code */
			10415,	/* Vendor */
			"Path",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Contact, OctetString, code 641, section 6.3.48                   */
	{
		struct dict_avp_data data = {
			641,	/* Code */
			10415,	/* Vendor */
			"Contact",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Subscription-Info, Grouped, code 642, section 6.3.49             */
	{
		struct dict_avp_data data = {
			642,	/* Code */
			10415,	/* Vendor */
			"Subscription-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Call-ID-SIP-Header, OctetString, code 643, section 6.3.49.1      */
	{
		struct dict_avp_data data = {
			643,	/* Code */
			10415,	/* Vendor */
			"Call-ID-SIP-Header",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* From-SIP-Header, OctetString, code 644, section 6.3.49.2         */
	{
		struct dict_avp_data data = {
			644,	/* Code */
			10415,	/* Vendor */
			"From-SIP-Header",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* To-SIP-Header, OctetString, code 645, section 6.3.49.3           */
	{
		struct dict_avp_data data = {
			645,	/* Code */
			10415,	/* Vendor */
			"To-SIP-Header",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Record-Route, OctetString, code 646, section 6.3.49.4            */
	{
		struct dict_avp_data data = {
			646,	/* Code */
			10415,	/* Vendor */
			"Record-Route",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Associated-Registered-Identities, Grouped, code 647, section 6.3.50 */
	{
		struct dict_avp_data data = {
			647,	/* Code */
			10415,	/* Vendor */
			"Associated-Registered-Identities",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Multiple-Registration-Indication, Enumerated, code 648, section 6.3.51 */
	{
		struct dict_avp_data data = {
			648,	/* Code */
			10415,	/* Vendor */
			"Multiple-Registration-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Multiple-Registration-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Restoration-Info, Grouped, code 649, section 6.3.52              */
	{
		struct dict_avp_data data = {
			649,	/* Code */
			10415,	/* Vendor */
			"Restoration-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Session-Priority, Enumerated, code 650, section 6.3.56           */
	{
		struct dict_avp_data data = {
			650,	/* Code */
			10415,	/* Vendor */
			"Session-Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Session-Priority)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Identity-with-Emergency-Registration, Grouped, code 651, section 6.3.57 */
	{
		struct dict_avp_data data = {
			651,	/* Code */
			10415,	/* Vendor */
			"Identity-with-Emergency-Registration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Priviledged-Sender-Indication, Enumerated, code 652, section 6.3.58 */
	{
		struct dict_avp_data data = {
			652,	/* Code */
			10415,	/* Vendor */
			"Priviledged-Sender-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Priviledged-Sender-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* LIA-Flags, Unsigned32, code 653, section 6.3.59                  */
	{
		struct dict_avp_data data = {
			653,	/* Code */
			10415,	/* Vendor */
			"LIA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Initial-CSeq-Sequence-Number, Unsigned32, code 654, section 6.3.62 */
	{
		struct dict_avp_data data = {
			654,	/* Code */
			10415,	/* Vendor */
			"Initial-CSeq-Sequence-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SAR-Flags, Unsigned32, code 655, section 6.3.63                  */
	{
		struct dict_avp_data data = {
			655,	/* Code */
			10415,	/* Vendor */
			"SAR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Allowed-WAF-WWSF-Identities, Grouped, code 656, section 6.3.64   */
	{
		struct dict_avp_data data = {
			656,	/* Code */
			10415,	/* Vendor */
			"Allowed-WAF-WWSF-Identities",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WebRTC-Authentication-Function-Name, UTF8String, code 657, section 6.3.65 */
	{
		struct dict_avp_data data = {
			657,	/* Code */
			10415,	/* Vendor */
			"WebRTC-Authentication-Function-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* WebRTC-Web-Server-Function-Name, UTF8String, code 658, section 6.3.66 */
	{
		struct dict_avp_data data = {
			658,	/* Code */
			10415,	/* Vendor */
			"WebRTC-Web-Server-Function-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* RTR-Flags, Unsigned32, code 659, section 6.3.69                  */
	{
		struct dict_avp_data data = {
			659,	/* Code */
			10415,	/* Vendor */
			"RTR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.272 V15.10.0 (2019-12)                                */
	/* From 3GPP 29272-fa0.docx                                         */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.272 Table 7.3.1/1: S6a/S6d, S7a/S7d and S13/S13' specific DiameterAVPs */
	/*==================================================================*/

	/* Subscription-Data, Grouped, code 1400, section 7.3.2             */
	{
		struct dict_avp_data data = {
			1400,	/* Code */
			10415,	/* Vendor */
			"Subscription-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Terminal-Information, Grouped, code 1401, section 7.3.3          */
	{
		struct dict_avp_data data = {
			1401,	/* Code */
			10415,	/* Vendor */
			"Terminal-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IMEI, UTF8String, code 1402, section 7.3.4                       */
	{
		struct dict_avp_data data = {
			1402,	/* Code */
			10415,	/* Vendor */
			"IMEI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Software-Version, UTF8String, code 1403, section 7.3.5           */
	{
		struct dict_avp_data data = {
			1403,	/* Code */
			10415,	/* Vendor */
			"Software-Version",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* QoS-Subscribed, OctetString, code 1404, section 7.3.77           */
	{
		struct dict_avp_data data = {
			1404,	/* Code */
			10415,	/* Vendor */
			"QoS-Subscribed",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ULR-Flags, Unsigned32, code 1405, section 7.3.7                  */
	{
		struct dict_avp_data data = {
			1405,	/* Code */
			10415,	/* Vendor */
			"ULR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ULA-Flags, Unsigned32, code 1406, section 7.3.8                  */
	{
		struct dict_avp_data data = {
			1406,	/* Code */
			10415,	/* Vendor */
			"ULA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Visited-PLMN-Id, OctetString, code 1407, section 7.3.9           */
	{
		struct dict_avp_data data = {
			1407,	/* Code */
			10415,	/* Vendor */
			"Visited-PLMN-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Requested-EUTRAN-Authentication-Info, Grouped, code 1408, section 7.3.11 */
	{
		struct dict_avp_data data = {
			1408,	/* Code */
			10415,	/* Vendor */
			"Requested-EUTRAN-Authentication-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Requested-UTRAN-GERAN-Authentication-Info, Grouped, code 1409, section 7.3.12 */
	{
		struct dict_avp_data data = {
			1409,	/* Code */
			10415,	/* Vendor */
			"Requested-UTRAN-GERAN-Authentication-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Requested-Vectors, Unsigned32, code 1410, section 7.3.14 */
	{
		struct dict_avp_data data = {
			1410,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Requested-Vectors",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Re-Synchronization-Info, OctetString, code 1411, section 7.3.15  */
	{
		struct dict_avp_data data = {
			1411,	/* Code */
			10415,	/* Vendor */
			"Re-Synchronization-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Immediate-Response-Preferred, Unsigned32, code 1412, section 7.3.16 */
	{
		struct dict_avp_data data = {
			1412,	/* Code */
			10415,	/* Vendor */
			"Immediate-Response-Preferred",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Authentication-Info, Grouped, code 1413, section 7.3.17          */
	{
		struct dict_avp_data data = {
			1413,	/* Code */
			10415,	/* Vendor */
			"Authentication-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* E-UTRAN-Vector, Grouped, code 1414, section 7.3.18               */
	{
		struct dict_avp_data data = {
			1414,	/* Code */
			10415,	/* Vendor */
			"E-UTRAN-Vector",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UTRAN-Vector, Grouped, code 1415, section 7.3.19                 */
	{
		struct dict_avp_data data = {
			1415,	/* Code */
			10415,	/* Vendor */
			"UTRAN-Vector",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GERAN-Vector, Grouped, code 1416, section 7.3.20                 */
	{
		struct dict_avp_data data = {
			1416,	/* Code */
			10415,	/* Vendor */
			"GERAN-Vector",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Network-Access-Mode, Enumerated, code 1417, section 7.3.21       */
	{
		struct dict_avp_data data = {
			1417,	/* Code */
			10415,	/* Vendor */
			"Network-Access-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Network-Access-Mode)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* HPLMN-ODB, Unsigned32, code 1418, section 7.3.22                 */
	{
		struct dict_avp_data data = {
			1418,	/* Code */
			10415,	/* Vendor */
			"HPLMN-ODB",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Item-Number, Unsigned32, code 1419, section 7.3.23               */
	{
		struct dict_avp_data data = {
			1419,	/* Code */
			10415,	/* Vendor */
			"Item-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Cancellation-Type, Enumerated, code 1420, section 7.3.24         */
	{
		struct dict_avp_data data = {
			1420,	/* Code */
			10415,	/* Vendor */
			"Cancellation-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Cancellation-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* DSR-Flags, Unsigned32, code 1421, section 7.3.25                 */
	{
		struct dict_avp_data data = {
			1421,	/* Code */
			10415,	/* Vendor */
			"DSR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* DSA-Flags, Unsigned32, code 1422, section 7.3.26                 */
	{
		struct dict_avp_data data = {
			1422,	/* Code */
			10415,	/* Vendor */
			"DSA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Context-Identifier, Unsigned32, code 1423, section 7.3.27        */
	{
		struct dict_avp_data data = {
			1423,	/* Code */
			10415,	/* Vendor */
			"Context-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Subscriber-Status, Enumerated, code 1424, section 7.3.29         */
	{
		struct dict_avp_data data = {
			1424,	/* Code */
			10415,	/* Vendor */
			"Subscriber-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Subscriber-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Operator-Determined-Barring, Unsigned32, code 1425, section 7.3.30 */
	{
		struct dict_avp_data data = {
			1425,	/* Code */
			10415,	/* Vendor */
			"Operator-Determined-Barring",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Restriction-Data, Unsigned32, code 1426, section 7.3.31   */
	{
		struct dict_avp_data data = {
			1426,	/* Code */
			10415,	/* Vendor */
			"Access-Restriction-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-OI-Replacement, UTF8String, code 1427, section 7.3.32        */
	{
		struct dict_avp_data data = {
			1427,	/* Code */
			10415,	/* Vendor */
			"APN-OI-Replacement",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* All-APN-Configurations-Included-Indicator, Enumerated, code 1428, section 7.3.33 */
	{
		struct dict_avp_data data = {
			1428,	/* Code */
			10415,	/* Vendor */
			"All-APN-Configurations-Included-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/All-APN-Configurations-Included-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* APN-Configuration-Profile, Grouped, code 1429, section 7.3.34    */
	{
		struct dict_avp_data data = {
			1429,	/* Code */
			10415,	/* Vendor */
			"APN-Configuration-Profile",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-Configuration, Grouped, code 1430, section 7.3.35            */
	{
		struct dict_avp_data data = {
			1430,	/* Code */
			10415,	/* Vendor */
			"APN-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* EPS-Subscribed-QoS-Profile, Grouped, code 1431, section 7.3.37   */
	{
		struct dict_avp_data data = {
			1431,	/* Code */
			10415,	/* Vendor */
			"EPS-Subscribed-QoS-Profile",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* VPLMN-Dynamic-Address-Allowed, Enumerated, code 1432, section 7.3.38 */
	{
		struct dict_avp_data data = {
			1432,	/* Code */
			10415,	/* Vendor */
			"VPLMN-Dynamic-Address-Allowed",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/VPLMN-Dynamic-Address-Allowed)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* STN-SR, OctetString, code 1433, section 7.3.39                   */
	{
		struct dict_avp_data data = {
			1433,	/* Code */
			10415,	/* Vendor */
			"STN-SR",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Alert-Reason, Enumerated, code 1434, section 7.3.83              */
	{
		struct dict_avp_data data = {
			1434,	/* Code */
			10415,	/* Vendor */
			"Alert-Reason",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Alert-Reason)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AMBR, Grouped, code 1435, section 7.3.41                         */
	{
		struct dict_avp_data data = {
			1435,	/* Code */
			10415,	/* Vendor */
			"AMBR",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CSG-Subscription-Data, Grouped, code 1436, section 7.3.78        */
	{
		struct dict_avp_data data = {
			1436,	/* Code */
			10415,	/* Vendor */
			"CSG-Subscription-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CSG-Id, Unsigned32, code 1437, section 7.3.79                    */
	{
		struct dict_avp_data data = {
			1437,	/* Code */
			10415,	/* Vendor */
			"CSG-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PDN-GW-Allocation-Type, Enumerated, code 1438, section 7.3.44    */
	{
		struct dict_avp_data data = {
			1438,	/* Code */
			10415,	/* Vendor */
			"PDN-GW-Allocation-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PDN-GW-Allocation-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Expiration-Date, Time, code 1439, section 7.3.80                 */
	{
		struct dict_avp_data data = {
			1439,	/* Code */
			10415,	/* Vendor */
			"Expiration-Date",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* RAT-Frequency-Selection-Priority-ID, Unsigned32, code 1440, section 7.3.46 */
	{
		struct dict_avp_data data = {
			1440,	/* Code */
			10415,	/* Vendor */
			"RAT-Frequency-Selection-Priority-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IDA-Flags, Unsigned32, code 1441, section 7.3.47                 */
	{
		struct dict_avp_data data = {
			1441,	/* Code */
			10415,	/* Vendor */
			"IDA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PUA-Flags, Unsigned32, code 1442, section 7.3.48                 */
	{
		struct dict_avp_data data = {
			1442,	/* Code */
			10415,	/* Vendor */
			"PUA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NOR-Flags, Unsigned32, code 1443, section 7.3.49                 */
	{
		struct dict_avp_data data = {
			1443,	/* Code */
			10415,	/* Vendor */
			"NOR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* User-Id, UTF8String, code 1444, section 7.3.50                   */
	{
		struct dict_avp_data data = {
			1444,	/* Code */
			10415,	/* Vendor */
			"User-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Equipment-Status, Enumerated, code 1445, section 7.3.51          */
	{
		struct dict_avp_data data = {
			1445,	/* Code */
			10415,	/* Vendor */
			"Equipment-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Equipment-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Regional-Subscription-Zone-Code, OctetString, code 1446, section 7.3.52 */
	{
		struct dict_avp_data data = {
			1446,	/* Code */
			10415,	/* Vendor */
			"Regional-Subscription-Zone-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RAND, OctetString, code 1447, section 7.3.53                     */
	{
		struct dict_avp_data data = {
			1447,	/* Code */
			10415,	/* Vendor */
			"RAND",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* XRES, OctetString, code 1448, section 7.3.54                     */
	{
		struct dict_avp_data data = {
			1448,	/* Code */
			10415,	/* Vendor */
			"XRES",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AUTN, OctetString, code 1449, section 7.3.55                     */
	{
		struct dict_avp_data data = {
			1449,	/* Code */
			10415,	/* Vendor */
			"AUTN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* KASME, OctetString, code 1450, section 7.3.56                    */
	{
		struct dict_avp_data data = {
			1450,	/* Code */
			10415,	/* Vendor */
			"KASME",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trace-Collection-Entity, Address, code 1452, section 7.3.98      */
	{
		struct dict_avp_data data = {
			1452,	/* Code */
			10415,	/* Vendor */
			"Trace-Collection-Entity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Kc, OctetString, code 1453, section 7.3.59                       */
	{
		struct dict_avp_data data = {
			1453,	/* Code */
			10415,	/* Vendor */
			"Kc",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SRES, OctetString, code 1454, section 7.3.60                     */
	{
		struct dict_avp_data data = {
			1454,	/* Code */
			10415,	/* Vendor */
			"SRES",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PDN-Type, Enumerated, code 1456, section 7.3.62                  */
	{
		struct dict_avp_data data = {
			1456,	/* Code */
			10415,	/* Vendor */
			"PDN-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PDN-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Roaming-Restricted-Due-To-Unsupported-Feature, Enumerated, code 1457, section 7.3.81 */
	{
		struct dict_avp_data data = {
			1457,	/* Code */
			10415,	/* Vendor */
			"Roaming-Restricted-Due-To-Unsupported-Feature",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Roaming-Restricted-Due-To-Unsupported-Feature)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Trace-Data, Grouped, code 1458, section 7.3.63                   */
	{
		struct dict_avp_data data = {
			1458,	/* Code */
			10415,	/* Vendor */
			"Trace-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trace-Reference, OctetString, code 1459, section 7.3.64          */
	{
		struct dict_avp_data data = {
			1459,	/* Code */
			10415,	/* Vendor */
			"Trace-Reference",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trace-Depth, Enumerated, code 1462, section 7.3.67               */
	{
		struct dict_avp_data data = {
			1462,	/* Code */
			10415,	/* Vendor */
			"Trace-Depth",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Trace-Depth)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Trace-NE-Type-List, OctetString, code 1463, section 7.3.68       */
	{
		struct dict_avp_data data = {
			1463,	/* Code */
			10415,	/* Vendor */
			"Trace-NE-Type-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trace-Interface-List, OctetString, code 1464, section 7.3.69     */
	{
		struct dict_avp_data data = {
			1464,	/* Code */
			10415,	/* Vendor */
			"Trace-Interface-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trace-Event-List, OctetString, code 1465, section 7.3.70         */
	{
		struct dict_avp_data data = {
			1465,	/* Code */
			10415,	/* Vendor */
			"Trace-Event-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* OMC-Id, OctetString, code 1466, section 7.3.71                   */
	{
		struct dict_avp_data data = {
			1466,	/* Code */
			10415,	/* Vendor */
			"OMC-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GPRS-Subscription-Data, Grouped, code 1467, section 7.3.72       */
	{
		struct dict_avp_data data = {
			1467,	/* Code */
			10415,	/* Vendor */
			"GPRS-Subscription-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Complete-Data-List-Included-Indicator, Enumerated, code 1468, section 7.3.73 */
	{
		struct dict_avp_data data = {
			1468,	/* Code */
			10415,	/* Vendor */
			"Complete-Data-List-Included-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Complete-Data-List-Included-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PDP-Context, Grouped, code 1469, section 7.3.74                  */
	{
		struct dict_avp_data data = {
			1469,	/* Code */
			10415,	/* Vendor */
			"PDP-Context",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PDP-Type, OctetString, code 1470, section 7.3.75                 */
	{
		struct dict_avp_data data = {
			1470,	/* Code */
			10415,	/* Vendor */
			"PDP-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP2-MEID, OctetString, code 1471, section 7.3.6                */
	{
		struct dict_avp_data data = {
			1471,	/* Code */
			10415,	/* Vendor */
			"3GPP2-MEID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Specific-APN-Info, Grouped, code 1472, section 7.3.82            */
	{
		struct dict_avp_data data = {
			1472,	/* Code */
			10415,	/* Vendor */
			"Specific-APN-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Info, Grouped, code 1473, section 7.3.84                     */
	{
		struct dict_avp_data data = {
			1473,	/* Code */
			10415,	/* Vendor */
			"LCS-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GMLC-Number, OctetString, code 1474, section 7.3.85              */
	{
		struct dict_avp_data data = {
			1474,	/* Code */
			10415,	/* Vendor */
			"GMLC-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-PrivacyException, Grouped, code 1475, section 7.3.86         */
	{
		struct dict_avp_data data = {
			1475,	/* Code */
			10415,	/* Vendor */
			"LCS-PrivacyException",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SS-Code, OctetString, code 1476, section 7.3.87                  */
	{
		struct dict_avp_data data = {
			1476,	/* Code */
			10415,	/* Vendor */
			"SS-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.272 V12.5.0 (2014-06) table 7.3.1/1             */
	/* changed row SS-Status (1477) to be OctetString instead of Grouped. */
	/* Clause 7.3.88 already described SS-Status (1147) as OctetString. */
	/* SS-Status, OctetString, code 1477, section 7.3.88                */
	{
		struct dict_avp_data data = {
			1477,	/* Code */
			10415,	/* Vendor */
			"SS-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Notification-To-UE-User, Enumerated, code 1478, section 7.3.89   */
	{
		struct dict_avp_data data = {
			1478,	/* Code */
			10415,	/* Vendor */
			"Notification-To-UE-User",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Notification-To-UE-User)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* External-Client, Grouped, code 1479, section 7.3.90              */
	{
		struct dict_avp_data data = {
			1479,	/* Code */
			10415,	/* Vendor */
			"External-Client",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Client-Identity, OctetString, code 1480, section 7.3.91          */
	{
		struct dict_avp_data data = {
			1480,	/* Code */
			10415,	/* Vendor */
			"Client-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* GMLC-Restriction, Enumerated, code 1481, section 7.3.92          */
	{
		struct dict_avp_data data = {
			1481,	/* Code */
			10415,	/* Vendor */
			"GMLC-Restriction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/GMLC-Restriction)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PLMN-Client, Enumerated, code 1482, section 7.3.93               */
	{
		struct dict_avp_data data = {
			1482,	/* Code */
			10415,	/* Vendor */
			"PLMN-Client",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PLMN-Client)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Service-Type, Grouped, code 1483, section 7.3.94                 */
	{
		struct dict_avp_data data = {
			1483,	/* Code */
			10415,	/* Vendor */
			"Service-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ServiceTypeIdentity, Unsigned32, code 1484, section 7.3.95       */
	{
		struct dict_avp_data data = {
			1484,	/* Code */
			10415,	/* Vendor */
			"ServiceTypeIdentity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MO-LR, Grouped, code 1485, section 7.3.96                        */
	{
		struct dict_avp_data data = {
			1485,	/* Code */
			10415,	/* Vendor */
			"MO-LR",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Teleservice-List, Grouped, code 1486, section 7.3.99             */
	{
		struct dict_avp_data data = {
			1486,	/* Code */
			10415,	/* Vendor */
			"Teleservice-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TS-Code, OctetString, code 1487, section 7.3.100                 */
	{
		struct dict_avp_data data = {
			1487,	/* Code */
			10415,	/* Vendor */
			"TS-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.272 V12.4.0 (2014-03) renamed                   */
	/* Call-Barring-Infor-List (1488) to Call-Barring-Info (1488).      */
	/* Call-Barring-Info, Grouped, code 1488, section 7.3.101           */
	{
		struct dict_avp_data data = {
			1488,	/* Code */
			10415,	/* Vendor */
			"Call-Barring-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SGSN-Number, OctetString, code 1489, section 7.3.102             */
	{
		struct dict_avp_data data = {
			1489,	/* Code */
			10415,	/* Vendor */
			"SGSN-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IDR-Flags, Unsigned32, code 1490, section 7.3.103                */
	{
		struct dict_avp_data data = {
			1490,	/* Code */
			10415,	/* Vendor */
			"IDR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ICS-Indicator, Enumerated, code 1491, section 7.3.104            */
	{
		struct dict_avp_data data = {
			1491,	/* Code */
			10415,	/* Vendor */
			"ICS-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ICS-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* IMS-Voice-Over-PS-Sessions-Supported, Enumerated, code 1492, section 7.3.106 */
	{
		struct dict_avp_data data = {
			1492,	/* Code */
			10415,	/* Vendor */
			"IMS-Voice-Over-PS-Sessions-Supported",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IMS-Voice-Over-PS-Sessions-Supported)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Homogeneous-Support-of-IMS-Voice-Over-PS-Sessions, Enumerated, code 1493, section 7.3.107 */
	{
		struct dict_avp_data data = {
			1493,	/* Code */
			10415,	/* Vendor */
			"Homogeneous-Support-of-IMS-Voice-Over-PS-Sessions",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Homogeneous-Support-of-IMS-Voice-Over-PS-Sessions)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Last-UE-Activity-Time, Time, code 1494, section 7.3.108          */
	{
		struct dict_avp_data data = {
			1494,	/* Code */
			10415,	/* Vendor */
			"Last-UE-Activity-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* EPS-User-State, Grouped, code 1495, section 7.3.110              */
	{
		struct dict_avp_data data = {
			1495,	/* Code */
			10415,	/* Vendor */
			"EPS-User-State",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* EPS-Location-Information, Grouped, code 1496, section 7.3.111    */
	{
		struct dict_avp_data data = {
			1496,	/* Code */
			10415,	/* Vendor */
			"EPS-Location-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MME-User-State, Grouped, code 1497, section 7.3.112              */
	{
		struct dict_avp_data data = {
			1497,	/* Code */
			10415,	/* Vendor */
			"MME-User-State",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SGSN-User-State, Grouped, code 1498, section 7.3.113             */
	{
		struct dict_avp_data data = {
			1498,	/* Code */
			10415,	/* Vendor */
			"SGSN-User-State",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* User-State, Enumerated, code 1499, section 7.3.114               */
	{
		struct dict_avp_data data = {
			1499,	/* Code */
			10415,	/* Vendor */
			"User-State",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/User-State)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: 3GPP TS 29.272 table 7.3.1/1 row MME-Location-Information (1600) */
	/* incorrectly has a space instead of hyphen in the name.           */
	/* Generated name renamed from MME-LocationInformation (1600).      */
	/* MME-Location-Information, Grouped, code 1600, section 7.3.115    */
	{
		struct dict_avp_data data = {
			1600,	/* Code */
			10415,	/* Vendor */
			"MME-Location-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SGSN-Location-Information, Grouped, code 1601, section 7.3.116   */
	{
		struct dict_avp_data data = {
			1601,	/* Code */
			10415,	/* Vendor */
			"SGSN-Location-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* E-UTRAN-Cell-Global-Identity, OctetString, code 1602, section 7.3.117 */
	{
		struct dict_avp_data data = {
			1602,	/* Code */
			10415,	/* Vendor */
			"E-UTRAN-Cell-Global-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Tracking-Area-Identity, OctetString, code 1603, section 7.3.118  */
	{
		struct dict_avp_data data = {
			1603,	/* Code */
			10415,	/* Vendor */
			"Tracking-Area-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Cell-Global-Identity, OctetString, code 1604, section 7.3.119    */
	{
		struct dict_avp_data data = {
			1604,	/* Code */
			10415,	/* Vendor */
			"Cell-Global-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Routing-Area-Identity, OctetString, code 1605, section 7.3.120   */
	{
		struct dict_avp_data data = {
			1605,	/* Code */
			10415,	/* Vendor */
			"Routing-Area-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Location-Area-Identity, OctetString, code 1606, section 7.3.121  */
	{
		struct dict_avp_data data = {
			1606,	/* Code */
			10415,	/* Vendor */
			"Location-Area-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Area-Identity, OctetString, code 1607, section 7.3.122   */
	{
		struct dict_avp_data data = {
			1607,	/* Code */
			10415,	/* Vendor */
			"Service-Area-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Geographical-Information, OctetString, code 1608, section 7.3.123 */
	{
		struct dict_avp_data data = {
			1608,	/* Code */
			10415,	/* Vendor */
			"Geographical-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Geodetic-Information, OctetString, code 1609, section 7.3.124    */
	{
		struct dict_avp_data data = {
			1609,	/* Code */
			10415,	/* Vendor */
			"Geodetic-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Current-Location-Retrieved, Enumerated, code 1610, section 7.3.125 */
	{
		struct dict_avp_data data = {
			1610,	/* Code */
			10415,	/* Vendor */
			"Current-Location-Retrieved",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Current-Location-Retrieved)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Age-Of-Location-Information, Unsigned32, code 1611, section 7.3.126 */
	{
		struct dict_avp_data data = {
			1611,	/* Code */
			10415,	/* Vendor */
			"Age-Of-Location-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Active-APN, Grouped, code 1612, section 7.3.127                  */
	{
		struct dict_avp_data data = {
			1612,	/* Code */
			10415,	/* Vendor */
			"Active-APN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Error-Diagnostic, Enumerated, code 1614, section 7.3.128         */
	{
		struct dict_avp_data data = {
			1614,	/* Code */
			10415,	/* Vendor */
			"Error-Diagnostic",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Error-Diagnostic)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Ext-PDP-Address, Address, code 1621, section 7.3.129             */
	{
		struct dict_avp_data data = {
			1621,	/* Code */
			10415,	/* Vendor */
			"Ext-PDP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* UE-SRVCC-Capability, Enumerated, code 1615, section 7.3.130      */
	{
		struct dict_avp_data data = {
			1615,	/* Code */
			10415,	/* Vendor */
			"UE-SRVCC-Capability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/UE-SRVCC-Capability)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MPS-Priority, Unsigned32, code 1616, section 7.3.131             */
	{
		struct dict_avp_data data = {
			1616,	/* Code */
			10415,	/* Vendor */
			"MPS-Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* VPLMN-LIPA-Allowed, Enumerated, code 1617, section 7.3.132       */
	{
		struct dict_avp_data data = {
			1617,	/* Code */
			10415,	/* Vendor */
			"VPLMN-LIPA-Allowed",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/VPLMN-LIPA-Allowed)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* LIPA-Permission, Enumerated, code 1618, section 7.3.133          */
	{
		struct dict_avp_data data = {
			1618,	/* Code */
			10415,	/* Vendor */
			"LIPA-Permission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/LIPA-Permission)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Subscribed-Periodic-RAU-TAU-Timer, Unsigned32, code 1619, section 7.3.134 */
	{
		struct dict_avp_data data = {
			1619,	/* Code */
			10415,	/* Vendor */
			"Subscribed-Periodic-RAU-TAU-Timer",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Ext-PDP-Type, OctetString, code 1620, section 7.3.75A            */
	{
		struct dict_avp_data data = {
			1620,	/* Code */
			10415,	/* Vendor */
			"Ext-PDP-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIPTO-Permission, Enumerated, code 1613, section 7.3.135         */
	{
		struct dict_avp_data data = {
			1613,	/* Code */
			10415,	/* Vendor */
			"SIPTO-Permission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SIPTO-Permission)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MDT-Configuration, Grouped, code 1622, section 7.3.136           */
	{
		struct dict_avp_data data = {
			1622,	/* Code */
			10415,	/* Vendor */
			"MDT-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Job-Type, Enumerated, code 1623, section 7.3.137                 */
	{
		struct dict_avp_data data = {
			1623,	/* Code */
			10415,	/* Vendor */
			"Job-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Job-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Area-Scope, Grouped, code 1624, section 7.3.138                  */
	{
		struct dict_avp_data data = {
			1624,	/* Code */
			10415,	/* Vendor */
			"Area-Scope",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* List-Of-Measurements, Unsigned32, code 1625, section 7.3.139     */
	{
		struct dict_avp_data data = {
			1625,	/* Code */
			10415,	/* Vendor */
			"List-Of-Measurements",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-Trigger, Unsigned32, code 1626, section 7.3.140        */
	{
		struct dict_avp_data data = {
			1626,	/* Code */
			10415,	/* Vendor */
			"Reporting-Trigger",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Report-Interval, Enumerated, code 1627, section 7.3.141          */
	{
		struct dict_avp_data data = {
			1627,	/* Code */
			10415,	/* Vendor */
			"Report-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Report-Interval)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Report-Amount, Enumerated, code 1628, section 7.3.142            */
	{
		struct dict_avp_data data = {
			1628,	/* Code */
			10415,	/* Vendor */
			"Report-Amount",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Report-Amount)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Event-Threshold-RSRP, Unsigned32, code 1629, section 7.3.143     */
	{
		struct dict_avp_data data = {
			1629,	/* Code */
			10415,	/* Vendor */
			"Event-Threshold-RSRP",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Event-Threshold-RSRQ, Unsigned32, code 1630, section 7.3.144     */
	{
		struct dict_avp_data data = {
			1630,	/* Code */
			10415,	/* Vendor */
			"Event-Threshold-RSRQ",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Logging-Interval, Enumerated, code 1631, section 7.3.145         */
	{
		struct dict_avp_data data = {
			1631,	/* Code */
			10415,	/* Vendor */
			"Logging-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Logging-Interval)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Logging-Duration, Enumerated, code 1632, section 7.3.146         */
	{
		struct dict_avp_data data = {
			1632,	/* Code */
			10415,	/* Vendor */
			"Logging-Duration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Logging-Duration)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Relay-Node-Indicator, Enumerated, code 1633, section 7.3.147     */
	{
		struct dict_avp_data data = {
			1633,	/* Code */
			10415,	/* Vendor */
			"Relay-Node-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Relay-Node-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MDT-User-Consent, Enumerated, code 1634, section 7.3.148         */
	{
		struct dict_avp_data data = {
			1634,	/* Code */
			10415,	/* Vendor */
			"MDT-User-Consent",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MDT-User-Consent)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PUR-Flags, Unsigned32, code 1635, section 7.3.149                */
	{
		struct dict_avp_data data = {
			1635,	/* Code */
			10415,	/* Vendor */
			"PUR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Subscribed-VSRVCC, Enumerated, code 1636, section 7.3.150        */
	{
		struct dict_avp_data data = {
			1636,	/* Code */
			10415,	/* Vendor */
			"Subscribed-VSRVCC",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Subscribed-VSRVCC)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Equivalent-PLMN-List, Grouped, code 1637, section 7.3.151        */
	{
		struct dict_avp_data data = {
			1637,	/* Code */
			10415,	/* Vendor */
			"Equivalent-PLMN-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CLR-Flags, Unsigned32, code 1638, section 7.3.152                */
	{
		struct dict_avp_data data = {
			1638,	/* Code */
			10415,	/* Vendor */
			"CLR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UVR-Flags, Unsigned32, code 1639, section 7.3.153                */
	{
		struct dict_avp_data data = {
			1639,	/* Code */
			10415,	/* Vendor */
			"UVR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UVA-Flags, Unsigned32, code 1640, section 7.3.154                */
	{
		struct dict_avp_data data = {
			1640,	/* Code */
			10415,	/* Vendor */
			"UVA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* VPLMN-CSG-Subscription-Data, Grouped, code 1641, section 7.3.155 */
	{
		struct dict_avp_data data = {
			1641,	/* Code */
			10415,	/* Vendor */
			"VPLMN-CSG-Subscription-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Time-Zone, UTF8String, code 1642, section 7.3.163                */
	{
		struct dict_avp_data data = {
			1642,	/* Code */
			10415,	/* Vendor */
			"Time-Zone",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* A-MSISDN, OctetString, code 1643, section 7.3.157                */
	{
		struct dict_avp_data data = {
			1643,	/* Code */
			10415,	/* Vendor */
			"A-MSISDN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MME-Number-for-MT-SMS, OctetString, code 1645, section 7.3.159   */
	{
		struct dict_avp_data data = {
			1645,	/* Code */
			10415,	/* Vendor */
			"MME-Number-for-MT-SMS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SMS-Register-Request, Enumerated, code 1648, section 7.3.162     */
	{
		struct dict_avp_data data = {
			1648,	/* Code */
			10415,	/* Vendor */
			"SMS-Register-Request",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SMS-Register-Request)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Local-Time-Zone, Grouped, code 1649, section 7.3.156             */
	{
		struct dict_avp_data data = {
			1649,	/* Code */
			10415,	/* Vendor */
			"Local-Time-Zone",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Daylight-Saving-Time, Enumerated, code 1650, section 7.3.164     */
	{
		struct dict_avp_data data = {
			1650,	/* Code */
			10415,	/* Vendor */
			"Daylight-Saving-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Daylight-Saving-Time)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Subscription-Data-Flags, Unsigned32, code 1654, section 7.3.165  */
	{
		struct dict_avp_data data = {
			1654,	/* Code */
			10415,	/* Vendor */
			"Subscription-Data-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.272 V12.9.0 (2015-12) changed AVP code of       */
	/* Measurement-Period-LTE (1655) from 1656.                         */
	/* Measurement-Period-LTE, Enumerated, code 1655, section 7.3.166   */
	{
		struct dict_avp_data data = {
			1655,	/* Code */
			10415,	/* Vendor */
			"Measurement-Period-LTE",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Measurement-Period-LTE)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: 3GPP TS 29.272 V12.9.0 (2015-12) changed AVP code of       */
	/* Measurement-Period-UMTS (1656) from 1655.                        */
	/* Measurement-Period-UMTS, Enumerated, code 1656, section 7.3.167  */
	{
		struct dict_avp_data data = {
			1656,	/* Code */
			10415,	/* Vendor */
			"Measurement-Period-UMTS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Measurement-Period-UMTS)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Collection-Period-RRM-LTE, Enumerated, code 1657, section 7.3.168 */
	{
		struct dict_avp_data data = {
			1657,	/* Code */
			10415,	/* Vendor */
			"Collection-Period-RRM-LTE",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Collection-Period-RRM-LTE)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Collection-Period-RRM-UMTS, Enumerated, code 1658, section 7.3.169 */
	{
		struct dict_avp_data data = {
			1658,	/* Code */
			10415,	/* Vendor */
			"Collection-Period-RRM-UMTS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Collection-Period-RRM-UMTS)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Positioning-Method, OctetString, code 1659, section 7.3.170      */
	{
		struct dict_avp_data data = {
			1659,	/* Code */
			10415,	/* Vendor */
			"Positioning-Method",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Measurement-Quantity, OctetString, code 1660, section 7.3.171    */
	{
		struct dict_avp_data data = {
			1660,	/* Code */
			10415,	/* Vendor */
			"Measurement-Quantity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Event-Threshold-Event-1F, Integer32, code 1661, section 7.3.172  */
	{
		struct dict_avp_data data = {
			1661,	/* Code */
			10415,	/* Vendor */
			"Event-Threshold-Event-1F",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Event-Threshold-Event-1I, Integer32, code 1662, section 7.3.173  */
	{
		struct dict_avp_data data = {
			1662,	/* Code */
			10415,	/* Vendor */
			"Event-Threshold-Event-1I",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Restoration-Priority, Unsigned32, code 1663, section 7.3.174     */
	{
		struct dict_avp_data data = {
			1663,	/* Code */
			10415,	/* Vendor */
			"Restoration-Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SGs-MME-Identity, UTF8String, code 1664, section 7.3.175         */
	{
		struct dict_avp_data data = {
			1664,	/* Code */
			10415,	/* Vendor */
			"SGs-MME-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SIPTO-Local-Network-Permission, Unsigned32, code 1665, section 7.3.176 */
	{
		struct dict_avp_data data = {
			1665,	/* Code */
			10415,	/* Vendor */
			"SIPTO-Local-Network-Permission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Coupled-Node-Diameter-ID, DiameterIdentity, code 1666, section 7.3.177 */
	{
		struct dict_avp_data data = {
			1666,	/* Code */
			10415,	/* Vendor */
			"Coupled-Node-Diameter-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* WLAN-offloadability, Grouped, code 1667, section 7.3.181         */
	{
		struct dict_avp_data data = {
			1667,	/* Code */
			10415,	/* Vendor */
			"WLAN-offloadability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WLAN-offloadability-EUTRAN, Unsigned32, code 1668, section 7.3.182 */
	{
		struct dict_avp_data data = {
			1668,	/* Code */
			10415,	/* Vendor */
			"WLAN-offloadability-EUTRAN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WLAN-offloadability-UTRAN, Unsigned32, code 1669, section 7.3.183 */
	{
		struct dict_avp_data data = {
			1669,	/* Code */
			10415,	/* Vendor */
			"WLAN-offloadability-UTRAN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reset-ID, OctetString, code 1670, section 7.3.184                */
	{
		struct dict_avp_data data = {
			1670,	/* Code */
			10415,	/* Vendor */
			"Reset-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MDT-Allowed-PLMN-Id, OctetString, code 1671, section 7.3.185     */
	{
		struct dict_avp_data data = {
			1671,	/* Code */
			10415,	/* Vendor */
			"MDT-Allowed-PLMN-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Adjacent-PLMNs, Grouped, code 1672, section 7.3.186              */
	{
		struct dict_avp_data data = {
			1672,	/* Code */
			10415,	/* Vendor */
			"Adjacent-PLMNs",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Adjacent-Access-Restriction-Data, Grouped, code 1673, section 7.3.187 */
	{
		struct dict_avp_data data = {
			1673,	/* Code */
			10415,	/* Vendor */
			"Adjacent-Access-Restriction-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* DL-Buffering-Suggested-Packet-Count, Integer32, code 1674, section 7.3.188 */
	{
		struct dict_avp_data data = {
			1674,	/* Code */
			10415,	/* Vendor */
			"DL-Buffering-Suggested-Packet-Count",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IMSI-Group-Id, Grouped, code 1675, section 7.3.189               */
	{
		struct dict_avp_data data = {
			1675,	/* Code */
			10415,	/* Vendor */
			"IMSI-Group-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Group-Service-Id, Unsigned32, code 1676, section 7.3.190         */
	{
		struct dict_avp_data data = {
			1676,	/* Code */
			10415,	/* Vendor */
			"Group-Service-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Group-PLMN-Id, OctetString, code 1677, section 7.3.191           */
	{
		struct dict_avp_data data = {
			1677,	/* Code */
			10415,	/* Vendor */
			"Group-PLMN-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Local-Group-Id, OctetString, code 1678, section 7.3.192          */
	{
		struct dict_avp_data data = {
			1678,	/* Code */
			10415,	/* Vendor */
			"Local-Group-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AIR-Flags, Unsigned32, code 1679, section 7.3.201                */
	{
		struct dict_avp_data data = {
			1679,	/* Code */
			10415,	/* Vendor */
			"AIR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UE-Usage-Type, Unsigned32, code 1680, section 7.3.202            */
	{
		struct dict_avp_data data = {
			1680,	/* Code */
			10415,	/* Vendor */
			"UE-Usage-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Non-IP-PDN-Type-Indicator, Enumerated, code 1681, section 7.3.204 */
	{
		struct dict_avp_data data = {
			1681,	/* Code */
			10415,	/* Vendor */
			"Non-IP-PDN-Type-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Non-IP-PDN-Type-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Non-IP-Data-Delivery-Mechanism, Unsigned32, code 1682, section 7.3.205 */
	{
		struct dict_avp_data data = {
			1682,	/* Code */
			10415,	/* Vendor */
			"Non-IP-Data-Delivery-Mechanism",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Additional-Context-ID, Unsigned32, code 1683, section 7.3.206    */
	{
		struct dict_avp_data data = {
			1683,	/* Code */
			10415,	/* Vendor */
			"Additional-Context-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SCEF-Realm, DiameterIdentity, code 1684, section 7.3.207         */
	{
		struct dict_avp_data data = {
			1684,	/* Code */
			10415,	/* Vendor */
			"SCEF-Realm",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* Subscription-Data-Deletion, Grouped, code 1685, section 7.3.208  */
	{
		struct dict_avp_data data = {
			1685,	/* Code */
			10415,	/* Vendor */
			"Subscription-Data-Deletion",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 29.272 table 7.3.1/1 row Preferred-Data-Mode (1686) */
	/* incorrectly has value type Grouped instead of Unsigned32,        */
	/* conflicting with clause 7.3.209.                                 */
	/* Preferred-Data-Mode, Unsigned32, code 1686, section 7.3.209      */
	{
		struct dict_avp_data data = {
			1686,	/* Code */
			10415,	/* Vendor */
			"Preferred-Data-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Emergency-Info, Grouped, code 1687, section 7.3.210              */
	{
		struct dict_avp_data data = {
			1687,	/* Code */
			10415,	/* Vendor */
			"Emergency-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* V2X-Subscription-Data, Grouped, code 1688, section 7.3.212       */
	{
		struct dict_avp_data data = {
			1688,	/* Code */
			10415,	/* Vendor */
			"V2X-Subscription-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* V2X-Permission, Unsigned32, code 1689, section 7.3.213           */
	{
		struct dict_avp_data data = {
			1689,	/* Code */
			10415,	/* Vendor */
			"V2X-Permission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PDN-Connection-Continuity, Unsigned32, code 1690, section 7.3.214 */
	{
		struct dict_avp_data data = {
			1690,	/* Code */
			10415,	/* Vendor */
			"PDN-Connection-Continuity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* eDRX-Cycle-Length, Grouped, code 1691, section 7.3.215           */
	{
		struct dict_avp_data data = {
			1691,	/* Code */
			10415,	/* Vendor */
			"eDRX-Cycle-Length",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* eDRX-Cycle-Length-Value, OctetString, code 1692, section 7.3.216 */
	{
		struct dict_avp_data data = {
			1692,	/* Code */
			10415,	/* Vendor */
			"eDRX-Cycle-Length-Value",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UE-PC5-AMBR, Unsigned32, code 1693, section 7.3.217              */
	{
		struct dict_avp_data data = {
			1693,	/* Code */
			10415,	/* Vendor */
			"UE-PC5-AMBR",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBSFN-Area, Grouped, code 1694, section 7.3.219                  */
	{
		struct dict_avp_data data = {
			1694,	/* Code */
			10415,	/* Vendor */
			"MBSFN-Area",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBSFN-Area-ID, Unsigned32, code 1695, section 7.3.220            */
	{
		struct dict_avp_data data = {
			1695,	/* Code */
			10415,	/* Vendor */
			"MBSFN-Area-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Carrier-Frequency, Unsigned32, code 1696, section 7.3.221        */
	{
		struct dict_avp_data data = {
			1696,	/* Code */
			10415,	/* Vendor */
			"Carrier-Frequency",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RDS-Indicator, Enumerated, code 1697, section 7.3.222            */
	{
		struct dict_avp_data data = {
			1697,	/* Code */
			10415,	/* Vendor */
			"RDS-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/RDS-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Service-Gap-Time, Unsigned32, code 1698, section 7.3.223         */
	{
		struct dict_avp_data data = {
			1698,	/* Code */
			10415,	/* Vendor */
			"Service-Gap-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Aerial-UE-Subscription-Information, Unsigned32, code 1699, section 7.3.224 */
	{
		struct dict_avp_data data = {
			1699,	/* Code */
			10415,	/* Vendor */
			"Aerial-UE-Subscription-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Broadcast-Location-Assistance-Data-Types, Unsigned64, code 1700, section 7.3.225 */
	{
		struct dict_avp_data data = {
			1700,	/* Code */
			10415,	/* Vendor */
			"Broadcast-Location-Assistance-Data-Types",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Paging-Time-Window, Grouped, code 1701, section 7.3.226          */
	{
		struct dict_avp_data data = {
			1701,	/* Code */
			10415,	/* Vendor */
			"Paging-Time-Window",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Operation-Mode, Unsigned32, code 1702, section 7.3.227           */
	{
		struct dict_avp_data data = {
			1702,	/* Code */
			10415,	/* Vendor */
			"Operation-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Paging-Time-Window-Length, OctetString, code 1703, section 7.3.228 */
	{
		struct dict_avp_data data = {
			1703,	/* Code */
			10415,	/* Vendor */
			"Paging-Time-Window-Length",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Core-Network-Restrictions, Unsigned32, code 1704, section 7.3.230 */
	{
		struct dict_avp_data data = {
			1704,	/* Code */
			10415,	/* Vendor */
			"Core-Network-Restrictions",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* eDRX-Related-RAT, Grouped, code 1705, section 7.3.229            */
	{
		struct dict_avp_data data = {
			1705,	/* Code */
			10415,	/* Vendor */
			"eDRX-Related-RAT",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Interworking-5GS-Indicator, Enumerated, code 1706, section 7.3.231 */
	{
		struct dict_avp_data data = {
			1706,	/* Code */
			10415,	/* Vendor */
			"Interworking-5GS-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Interworking-5GS-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.273 V15.4.0 (2019-09)                                 */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.273 Table 5.2.3.1/1: Diameter STa AVPs                */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* ANID, UTF8String, code 1504, section 5.2.3.7                     */
	{
		struct dict_avp_data data = {
			1504,	/* Code */
			10415,	/* Vendor */
			"ANID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* AN-Trusted, Enumerated, code 1503, section 5.2.3.9               */
	{
		struct dict_avp_data data = {
			1503,	/* Code */
			10415,	/* Vendor */
			"AN-Trusted",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AN-Trusted)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MIP-FA-RK, OctetString, code 1506, section 5.2.3.12              */
	{
		struct dict_avp_data data = {
			1506,	/* Code */
			10415,	/* Vendor */
			"MIP-FA-RK",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MIP-FA-RK-SPI, Unsigned32, code 1507, section 5.2.3.13           */
	{
		struct dict_avp_data data = {
			1507,	/* Code */
			10415,	/* Vendor */
			"MIP-FA-RK-SPI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Full-Network-Name, OctetString, code 1516, section 5.2.3.14      */
	{
		struct dict_avp_data data = {
			1516,	/* Code */
			10415,	/* Vendor */
			"Full-Network-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Short-Network-Name, OctetString, code 1517, section 5.2.3.15     */
	{
		struct dict_avp_data data = {
			1517,	/* Code */
			10415,	/* Vendor */
			"Short-Network-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WLAN-Identifier, Grouped, code 1509, section 5.2.3.18            */
	{
		struct dict_avp_data data = {
			1509,	/* Code */
			10415,	/* Vendor */
			"WLAN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AAA-Failure-Indication, Unsigned32, code 1518, section 8.2.3.21  */
	{
		struct dict_avp_data data = {
			1518,	/* Code */
			10415,	/* Vendor */
			"AAA-Failure-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Transport-Access-Type, Enumerated, code 1519, section 5.2.3.19   */
	{
		struct dict_avp_data data = {
			1519,	/* Code */
			10415,	/* Vendor */
			"Transport-Access-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Transport-Access-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* DER-Flags, Unsigned32, code 1520, section 5.2.3.20               */
	{
		struct dict_avp_data data = {
			1520,	/* Code */
			10415,	/* Vendor */
			"DER-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* DEA-Flags, Unsigned32, code 1521, section 5.2.3.21               */
	{
		struct dict_avp_data data = {
			1521,	/* Code */
			10415,	/* Vendor */
			"DEA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SSID, UTF8String, code 1524, section 5.2.3.22                    */
	{
		struct dict_avp_data data = {
			1524,	/* Code */
			10415,	/* Vendor */
			"SSID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* HESSID, UTF8String, code 1525, section 5.2.3.23                  */
	{
		struct dict_avp_data data = {
			1525,	/* Code */
			10415,	/* Vendor */
			"HESSID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Access-Network-Info, Grouped, code 1526, section 5.2.3.24        */
	{
		struct dict_avp_data data = {
			1526,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAN-Connection-Mode, Unsigned32, code 1527, section 5.2.3.25    */
	{
		struct dict_avp_data data = {
			1527,	/* Code */
			10415,	/* Vendor */
			"TWAN-Connection-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAN-Connectivity-Parameters, Grouped, code 1528, section 5.2.3.26 */
	{
		struct dict_avp_data data = {
			1528,	/* Code */
			10415,	/* Vendor */
			"TWAN-Connectivity-Parameters",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Connectivity-Flags, Unsigned32, code 1529, section 5.2.3.27      */
	{
		struct dict_avp_data data = {
			1529,	/* Code */
			10415,	/* Vendor */
			"Connectivity-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAN-PCO, OctetString, code 1530, section 5.2.3.28               */
	{
		struct dict_avp_data data = {
			1530,	/* Code */
			10415,	/* Vendor */
			"TWAN-PCO",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAG-CP-Address, Address, code 1531, section 5.2.3.29            */
	{
		struct dict_avp_data data = {
			1531,	/* Code */
			10415,	/* Vendor */
			"TWAG-CP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* TWAG-UP-Address, UTF8String, code 1532, section 5.2.3.30         */
	{
		struct dict_avp_data data = {
			1532,	/* Code */
			10415,	/* Vendor */
			"TWAG-UP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* TWAN-S2a-Failure-Cause, Unsigned32, code 1533, section 5.2.3.31  */
	{
		struct dict_avp_data data = {
			1533,	/* Code */
			10415,	/* Vendor */
			"TWAN-S2a-Failure-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Back-Off-Timer, Unsigned32, code 1534, section 5.2.3.32       */
	{
		struct dict_avp_data data = {
			1534,	/* Code */
			10415,	/* Vendor */
			"SM-Back-Off-Timer",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WLCP-Key, OctetString, code 1535, section 5.2.3.33               */
	{
		struct dict_avp_data data = {
			1535,	/* Code */
			10415,	/* Vendor */
			"WLCP-Key",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Emergency-Services, Unsigned32, code 1538, section 7.2.3.4       */
	{
		struct dict_avp_data data = {
			1538,	/* Code */
			10415,	/* Vendor */
			"Emergency-Services",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IMEI-Check-In-VPLMN-Result, Unsigned32, code 1540, section 5.2.3.35 */
	{
		struct dict_avp_data data = {
			1540,	/* Code */
			10415,	/* Vendor */
			"IMEI-Check-In-VPLMN-Result",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};


	/*==================================================================*/
	/* 3GPP TS 29.273 Table 6.2.3.1/1: Diameter SWd AVPs                */
	/*                                                                  */
	/* Note: No AVPs are first defined in this table.                   */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.273 Table 7.2.3.1/1: Diameter SWm AVPs                */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* Trace-Info, Grouped, code 1505, section 8.2.3.1.3                */
	{
		struct dict_avp_data data = {
			1505,	/* Code */
			10415,	/* Vendor */
			"Trace-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AAR-Flags, Unsigned32, code 1539, section 7.2.3.5                */
	{
		struct dict_avp_data data = {
			1539,	/* Code */
			10415,	/* Vendor */
			"AAR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};


	/*==================================================================*/
	/* 3GPP TS 29.273 Table 8.2.3.0/1: Diameter SWx AVPs                */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* Non-3GPP-User-Data, Grouped, code 1500, section 8.2.3.1          */
	{
		struct dict_avp_data data = {
			1500,	/* Code */
			10415,	/* Vendor */
			"Non-3GPP-User-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Non-3GPP-IP-Access, Enumerated, code 1501, section 8.2.3.3       */
	{
		struct dict_avp_data data = {
			1501,	/* Code */
			10415,	/* Vendor */
			"Non-3GPP-IP-Access",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Non-3GPP-IP-Access)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Non-3GPP-IP-Access-APN, Enumerated, code 1502, section 8.2.3.4   */
	{
		struct dict_avp_data data = {
			1502,	/* Code */
			10415,	/* Vendor */
			"Non-3GPP-IP-Access-APN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Non-3GPP-IP-Access-APN)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PPR-Flags, Unsigned32, code 1508, section 8.2.3.17               */
	{
		struct dict_avp_data data = {
			1508,	/* Code */
			10415,	/* Vendor */
			"PPR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAN-Default-APN-Context-Id, Unsigned32, code 1512, section 8.2.3.18 */
	{
		struct dict_avp_data data = {
			1512,	/* Code */
			10415,	/* Vendor */
			"TWAN-Default-APN-Context-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAN-Access-Info, Grouped, code 1510, section 8.2.3.19           */
	{
		struct dict_avp_data data = {
			1510,	/* Code */
			10415,	/* Vendor */
			"TWAN-Access-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Authorization-Flags, Unsigned32, code 1511, section 8.2.3.20 */
	{
		struct dict_avp_data data = {
			1511,	/* Code */
			10415,	/* Vendor */
			"Access-Authorization-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-AAA-Server-Name, DiameterIdentity, code 318, section 8.2.3.24 */
	{
		struct dict_avp_data data = {
			318,	/* Code */
			10415,	/* Vendor */
			"3GPP-AAA-Server-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* ERP-Authorization, Unsigned32, code 1541, section 8.2.3.27       */
	{
		struct dict_avp_data data = {
			1541,	/* Code */
			10415,	/* Vendor */
			"ERP-Authorization",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};


	/*==================================================================*/
	/* 3GPP TS 29.273 Table 9.2.3.1.1/1: Diameter S6b AVPs for DSMIPv6  */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* RAR-Flags, Unsigned32, code 1522, section 9.2.3.1.5              */
	{
		struct dict_avp_data data = {
			1522,	/* Code */
			10415,	/* Vendor */
			"RAR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: Trust-Relationship-Update (1515) only present in           */
	/* 3GPP TS 29.273 V11.3.0 (2012-09) before being deprecated.        */
	/* Trust-Relationship-Update (1515) not provided.                   */

	/* DER-S6b-Flags, Unsigned32, code 1523, section 9.2.3.7            */
	{
		struct dict_avp_data data = {
			1523,	/* Code */
			10415,	/* Vendor */
			"DER-S6b-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};


	/*==================================================================*/
	/* 3GPP TS 29.273 Table 9.2.3.2.1/1: Diameter S6b AVPs for PMIPv6 or GTPv2 */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* Origination-Time-Stamp, Unsigned64, code 1536, section 9.2.3.2.6 */
	{
		struct dict_avp_data data = {
			1536,	/* Code */
			10415,	/* Vendor */
			"Origination-Time-Stamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Maximum-Wait-Time, Unsigned32, code 1537, section 9.2.3.2.7      */
	{
		struct dict_avp_data data = {
			1537,	/* Code */
			10415,	/* Vendor */
			"Maximum-Wait-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};


	/*==================================================================*/
	/* 3GPP TS 29.273 Table 9.2.3.6.1/1: Diameter S6b AVPs for MIPv4 FACoA */
	/*                                                                  */
	/* Note: No AVPs are first defined in this table.                   */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.329 V15.2.0 (2019-09)                                 */
	/* From 3GPP 29329-f20.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.329 Table 6.3.1: Table 6.3.1: Diameter Multimedia Application AVPs */
	/*                                                                  */
	/* Note: AVPs referenced from other sources are not added here.     */
	/*==================================================================*/

	/* User-Identity, Grouped, code 700, section 6.3.1                  */
	{
		struct dict_avp_data data = {
			700,	/* Code */
			10415,	/* Vendor */
			"User-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MSISDN, OctetString, code 701, section 6.3.2                     */
	{
		struct dict_avp_data data = {
			701,	/* Code */
			10415,	/* Vendor */
			"MSISDN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: Name conflict with 3GPP TS 29.229 User-Data (606).         */
	/* User-Data (702) renamed to User-Data-29.329 (702).               */
	/* User-Data-29.329, OctetString, code 702, section 6.3.3           */
	{
		struct dict_avp_data data = {
			702,	/* Code */
			10415,	/* Vendor */
			"User-Data-29.329",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Data-Reference, Enumerated, code 703, section 6.3.4              */
	{
		struct dict_avp_data data = {
			703,	/* Code */
			10415,	/* Vendor */
			"Data-Reference",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Data-Reference)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Service-Indication, OctetString, code 704, section 6.3.5         */
	{
		struct dict_avp_data data = {
			704,	/* Code */
			10415,	/* Vendor */
			"Service-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Subs-Req-Type, Enumerated, code 705, section 6.3.6               */
	{
		struct dict_avp_data data = {
			705,	/* Code */
			10415,	/* Vendor */
			"Subs-Req-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Subs-Req-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Requested-Domain, Enumerated, code 706, section 6.3.7            */
	{
		struct dict_avp_data data = {
			706,	/* Code */
			10415,	/* Vendor */
			"Requested-Domain",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Requested-Domain)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Current-Location, Enumerated, code 707, section 6.3.8            */
	{
		struct dict_avp_data data = {
			707,	/* Code */
			10415,	/* Vendor */
			"Current-Location",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Current-Location)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Identity-Set, Enumerated, code 708, section 6.3.10               */
	{
		struct dict_avp_data data = {
			708,	/* Code */
			10415,	/* Vendor */
			"Identity-Set",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Identity-Set)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Expiry-Time, Time, code 709, section 6.3.16                      */
	{
		struct dict_avp_data data = {
			709,	/* Code */
			10415,	/* Vendor */
			"Expiry-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Send-Data-Indication, Enumerated, code 710, section 6.3.17       */
	{
		struct dict_avp_data data = {
			710,	/* Code */
			10415,	/* Vendor */
			"Send-Data-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Send-Data-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* DSAI-Tag, OctetString, code 711, section 6.3.18                  */
	{
		struct dict_avp_data data = {
			711,	/* Code */
			10415,	/* Vendor */
			"DSAI-Tag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* One-Time-Notification, Enumerated, code 712, section 6.3.22      */
	{
		struct dict_avp_data data = {
			712,	/* Code */
			10415,	/* Vendor */
			"One-Time-Notification",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/One-Time-Notification)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Requested-Nodes, Unsigned32, code 713, section 6.3.7A            */
	{
		struct dict_avp_data data = {
			713,	/* Code */
			10415,	/* Vendor */
			"Requested-Nodes",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Serving-Node-Indication, Enumerated, code 714, section 6.3.23    */
	{
		struct dict_avp_data data = {
			714,	/* Code */
			10415,	/* Vendor */
			"Serving-Node-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Serving-Node-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Repository-Data-ID, Grouped, code 715, section 6.3.24            */
	{
		struct dict_avp_data data = {
			715,	/* Code */
			10415,	/* Vendor */
			"Repository-Data-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Sequence-Number, Unsigned32, code 716, section 6.3.25            */
	{
		struct dict_avp_data data = {
			716,	/* Code */
			10415,	/* Vendor */
			"Sequence-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Pre-paging-Supported, Enumerated, code 717, section 6.3.26       */
	{
		struct dict_avp_data data = {
			717,	/* Code */
			10415,	/* Vendor */
			"Pre-paging-Supported",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Pre-paging-Supported)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Local-Time-Zone-Indication, Enumerated, code 718, section 6.3.27 */
	{
		struct dict_avp_data data = {
			718,	/* Code */
			10415,	/* Vendor */
			"Local-Time-Zone-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Local-Time-Zone-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* UDR-Flags, Unsigned32, code 719, section 6.3.28                  */
	{
		struct dict_avp_data data = {
			719,	/* Code */
			10415,	/* Vendor */
			"UDR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Call-Reference-Info, Grouped, code 720, section 6.3.29           */
	{
		struct dict_avp_data data = {
			720,	/* Code */
			10415,	/* Vendor */
			"Call-Reference-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Call-Reference-Number, OctetString, code 721, section 6.3.30     */
	{
		struct dict_avp_data data = {
			721,	/* Code */
			10415,	/* Vendor */
			"Call-Reference-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AS-Number, OctetString, code 722, section 6.3.31                 */
	{
		struct dict_avp_data data = {
			722,	/* Code */
			10415,	/* Vendor */
			"AS-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.336 V15.7.0 (2019-09)                                 */
	/* From 3GPP 29336-f70.docx                                         */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.336 Table 6.4.1/1: S6m/S6n specific Diameter AVPs     */
	/*==================================================================*/

	/* IP-SM-GW-Number, OctetString, code 3100, section 6.4.14          */
	{
		struct dict_avp_data data = {
			3100,	/* Code */
			10415,	/* Vendor */
			"IP-SM-GW-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IP-SM-GW-Name, DiameterIdentity, code 3101, section 6.4.15       */
	{
		struct dict_avp_data data = {
			3101,	/* Code */
			10415,	/* Vendor */
			"IP-SM-GW-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* User-Identifier, Grouped, code 3102, section 6.4.2               */
	{
		struct dict_avp_data data = {
			3102,	/* Code */
			10415,	/* Vendor */
			"User-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-ID, Enumerated, code 3103, section 6.4.3                 */
	{
		struct dict_avp_data data = {
			3103,	/* Code */
			10415,	/* Vendor */
			"Service-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Service-ID)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SCS-Identity, OctetString, code 3104, section 6.4.4              */
	{
		struct dict_avp_data data = {
			3104,	/* Code */
			10415,	/* Vendor */
			"SCS-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Parameters, Grouped, code 3105, section 6.4.5            */
	{
		struct dict_avp_data data = {
			3105,	/* Code */
			10415,	/* Vendor */
			"Service-Parameters",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* T4-Parameters, Grouped, code 3106, section 6.4.6                 */
	{
		struct dict_avp_data data = {
			3106,	/* Code */
			10415,	/* Vendor */
			"T4-Parameters",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Data, Grouped, code 3107, section 6.4.7                  */
	{
		struct dict_avp_data data = {
			3107,	/* Code */
			10415,	/* Vendor */
			"Service-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* T4-Data, Grouped, code 3108, section 6.4.8                       */
	{
		struct dict_avp_data data = {
			3108,	/* Code */
			10415,	/* Vendor */
			"T4-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* HSS-Cause, Unsigned32, code 3109, section 6.4.9                  */
	{
		struct dict_avp_data data = {
			3109,	/* Code */
			10415,	/* Vendor */
			"HSS-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIR-Flags, Unsigned32, code 3110, section 6.4.10                 */
	{
		struct dict_avp_data data = {
			3110,	/* Code */
			10415,	/* Vendor */
			"SIR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* External-Identifier, UTF8String, code 3111, section 6.4.11       */
	{
		struct dict_avp_data data = {
			3111,	/* Code */
			10415,	/* Vendor */
			"External-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* IP-SM-GW-Realm, DiameterIdentity, code 3112, section 6.4.18      */
	{
		struct dict_avp_data data = {
			3112,	/* Code */
			10415,	/* Vendor */
			"IP-SM-GW-Realm",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.336 Table 8.4.1-1: S6t specific Diameter AVPs         */
	/*==================================================================*/

	/* AESE-Communication-Pattern, Grouped, code 3113, section 8.4.25   */
	{
		struct dict_avp_data data = {
			3113,	/* Code */
			10415,	/* Vendor */
			"AESE-Communication-Pattern",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Communication-Pattern-Set, Grouped, code 3114, section 8.4.26    */
	{
		struct dict_avp_data data = {
			3114,	/* Code */
			10415,	/* Vendor */
			"Communication-Pattern-Set",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Periodic-Communication-Indicator, Unsigned32, code 3115, section 8.4.27 */
	{
		struct dict_avp_data data = {
			3115,	/* Code */
			10415,	/* Vendor */
			"Periodic-Communication-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Communication-Duration-Time, Unsigned32, code 3116, section 8.4.28 */
	{
		struct dict_avp_data data = {
			3116,	/* Code */
			10415,	/* Vendor */
			"Communication-Duration-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Periodic-time, Unsigned32, code 3117, section 8.4.29             */
	{
		struct dict_avp_data data = {
			3117,	/* Code */
			10415,	/* Vendor */
			"Periodic-time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Scheduled-Communication-Time, Grouped, code 3118, section 8.4.30 */
	{
		struct dict_avp_data data = {
			3118,	/* Code */
			10415,	/* Vendor */
			"Scheduled-Communication-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Stationary-Indication, Unsigned32, code 3119, section 8.4.31     */
	{
		struct dict_avp_data data = {
			3119,	/* Code */
			10415,	/* Vendor */
			"Stationary-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AESE-Communication-Pattern-Config-Status, Grouped, code 3120, section 8.4.32 */
	{
		struct dict_avp_data data = {
			3120,	/* Code */
			10415,	/* Vendor */
			"AESE-Communication-Pattern-Config-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AESE-Error-Report, Grouped, code 3121, section 8.4.33            */
	{
		struct dict_avp_data data = {
			3121,	/* Code */
			10415,	/* Vendor */
			"AESE-Error-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Configuration, Grouped, code 3122, section 8.4.2 */
	{
		struct dict_avp_data data = {
			3122,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Report, Grouped, code 3123, section 8.4.3       */
	{
		struct dict_avp_data data = {
			3123,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SCEF-Reference-ID, Unsigned32, code 3124, section 8.4.4          */
	{
		struct dict_avp_data data = {
			3124,	/* Code */
			10415,	/* Vendor */
			"SCEF-Reference-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SCEF-ID, DiameterIdentity, code 3125, section 8.4.5              */
	{
		struct dict_avp_data data = {
			3125,	/* Code */
			10415,	/* Vendor */
			"SCEF-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* SCEF-Reference-ID-for-Deletion, Unsigned32, code 3126, section 8.4.6 */
	{
		struct dict_avp_data data = {
			3126,	/* Code */
			10415,	/* Vendor */
			"SCEF-Reference-ID-for-Deletion",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Type, Unsigned32, code 3127, section 8.4.7            */
	{
		struct dict_avp_data data = {
			3127,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Maximum-Number-of-Reports, Unsigned32, code 3128, section 8.4.8  */
	{
		struct dict_avp_data data = {
			3128,	/* Code */
			10415,	/* Vendor */
			"Maximum-Number-of-Reports",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UE-Reachability-Configuration, Grouped, code 3129, section 8.4.9 */
	{
		struct dict_avp_data data = {
			3129,	/* Code */
			10415,	/* Vendor */
			"UE-Reachability-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Duration, Time, code 3130, section 8.4.10             */
	{
		struct dict_avp_data data = {
			3130,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Duration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Maximum-Detection-Time, Unsigned32, code 3131, section 8.4.11    */
	{
		struct dict_avp_data data = {
			3131,	/* Code */
			10415,	/* Vendor */
			"Maximum-Detection-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reachability-Type, Unsigned32, code 3132, section 8.4.12         */
	{
		struct dict_avp_data data = {
			3132,	/* Code */
			10415,	/* Vendor */
			"Reachability-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Maximum-Latency, Unsigned32, code 3133, section 8.4.13           */
	{
		struct dict_avp_data data = {
			3133,	/* Code */
			10415,	/* Vendor */
			"Maximum-Latency",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Maximum-Response-Time, Unsigned32, code 3134, section 8.4.14     */
	{
		struct dict_avp_data data = {
			3134,	/* Code */
			10415,	/* Vendor */
			"Maximum-Response-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Location-Information-Configuration, Grouped, code 3135, section 8.4.15 */
	{
		struct dict_avp_data data = {
			3135,	/* Code */
			10415,	/* Vendor */
			"Location-Information-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MONTE-Location-Type, Unsigned32, code 3136, section 8.4.16       */
	{
		struct dict_avp_data data = {
			3136,	/* Code */
			10415,	/* Vendor */
			"MONTE-Location-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Accuracy, Unsigned32, code 3137, section 8.4.17                  */
	{
		struct dict_avp_data data = {
			3137,	/* Code */
			10415,	/* Vendor */
			"Accuracy",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Association-Type, Unsigned32, code 3138, section 8.4.18          */
	{
		struct dict_avp_data data = {
			3138,	/* Code */
			10415,	/* Vendor */
			"Association-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Roaming-Information, Unsigned32, code 3139, section 8.4.19       */
	{
		struct dict_avp_data data = {
			3139,	/* Code */
			10415,	/* Vendor */
			"Roaming-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reachability-Information, Unsigned32, code 3140, section 8.4.20  */
	{
		struct dict_avp_data data = {
			3140,	/* Code */
			10415,	/* Vendor */
			"Reachability-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IMEI-Change, Unsigned32, code 3141, section 8.4.22               */
	{
		struct dict_avp_data data = {
			3141,	/* Code */
			10415,	/* Vendor */
			"IMEI-Change",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Config-Status, Grouped, code 3142, section 8.4.24 */
	{
		struct dict_avp_data data = {
			3142,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Config-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Supported-Services, Grouped, code 3143, section 8.4.40           */
	{
		struct dict_avp_data data = {
			3143,	/* Code */
			10415,	/* Vendor */
			"Supported-Services",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Supported-Monitoring-Events, Unsigned64, code 3144, section 8.4.41 */
	{
		struct dict_avp_data data = {
			3144,	/* Code */
			10415,	/* Vendor */
			"Supported-Monitoring-Events",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CIR-Flags, Unsigned32, code 3145, section 8.4.39                 */
	{
		struct dict_avp_data data = {
			3145,	/* Code */
			10415,	/* Vendor */
			"CIR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Result, Grouped, code 3146, section 8.4.37               */
	{
		struct dict_avp_data data = {
			3146,	/* Code */
			10415,	/* Vendor */
			"Service-Result",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Result-Code, Unsigned32, code 3147, section 8.4.38       */
	{
		struct dict_avp_data data = {
			3147,	/* Code */
			10415,	/* Vendor */
			"Service-Result-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reference-ID-Validity-Time, Time, code 3148, section 8.4.42      */
	{
		struct dict_avp_data data = {
			3148,	/* Code */
			10415,	/* Vendor */
			"Reference-ID-Validity-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Event-Handling, Unsigned32, code 3149, section 8.4.43            */
	{
		struct dict_avp_data data = {
			3149,	/* Code */
			10415,	/* Vendor */
			"Event-Handling",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NIDD-Authorization-Request, Grouped, code 3150, section 8.4.44   */
	{
		struct dict_avp_data data = {
			3150,	/* Code */
			10415,	/* Vendor */
			"NIDD-Authorization-Request",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NIDD-Authorization-Response, Grouped, code 3151, section 8.4.45  */
	{
		struct dict_avp_data data = {
			3151,	/* Code */
			10415,	/* Vendor */
			"NIDD-Authorization-Response",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Report, Grouped, code 3152, section 8.4.47               */
	{
		struct dict_avp_data data = {
			3152,	/* Code */
			10415,	/* Vendor */
			"Service-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Node-Type, Unsigned32, code 3153, section 8.4.48                 */
	{
		struct dict_avp_data data = {
			3153,	/* Code */
			10415,	/* Vendor */
			"Node-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* S6t-HSS-Cause, Unsigned32, code 3154, section 8.4.50             */
	{
		struct dict_avp_data data = {
			3154,	/* Code */
			10415,	/* Vendor */
			"S6t-HSS-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Enhanced-Coverage-Restriction, Grouped, code 3155, section 8.4.51 */
	{
		struct dict_avp_data data = {
			3155,	/* Code */
			10415,	/* Vendor */
			"Enhanced-Coverage-Restriction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Enhanced-Coverage-Restriction-Data, Grouped, code 3156, section 8.4.52 */
	{
		struct dict_avp_data data = {
			3156,	/* Code */
			10415,	/* Vendor */
			"Enhanced-Coverage-Restriction-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Restricted-PLMN-List, Grouped, code 3157, section 8.4.53         */
	{
		struct dict_avp_data data = {
			3157,	/* Code */
			10415,	/* Vendor */
			"Restricted-PLMN-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Allowed-PLMN-List, Grouped, code 3158, section 8.4.54            */
	{
		struct dict_avp_data data = {
			3158,	/* Code */
			10415,	/* Vendor */
			"Allowed-PLMN-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Requested-Validity-Time, Time, code 3159, section 8.4.55         */
	{
		struct dict_avp_data data = {
			3159,	/* Code */
			10415,	/* Vendor */
			"Requested-Validity-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Granted-Validity-Time, Time, code 3160, section 8.4.56           */
	{
		struct dict_avp_data data = {
			3160,	/* Code */
			10415,	/* Vendor */
			"Granted-Validity-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* NIDD-Authorization-Update, Grouped, code 3161, section 8.4.57    */
	{
		struct dict_avp_data data = {
			3161,	/* Code */
			10415,	/* Vendor */
			"NIDD-Authorization-Update",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Loss-Of-Connectivity-Reason, Unsigned32, code 3162, section 8.4.58 */
	{
		struct dict_avp_data data = {
			3162,	/* Code */
			10415,	/* Vendor */
			"Loss-Of-Connectivity-Reason",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Group-Reporting-Guard-Timer, Unsigned32, code 3163, section 8.4.59 */
	{
		struct dict_avp_data data = {
			3163,	/* Code */
			10415,	/* Vendor */
			"Group-Reporting-Guard-Timer",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CIA-Flags, Unsigned32, code 3164, section 8.4.60                 */
	{
		struct dict_avp_data data = {
			3164,	/* Code */
			10415,	/* Vendor */
			"CIA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Group-Monitoring-Event-Report, Grouped, code 3165, section 8.4.61 */
	{
		struct dict_avp_data data = {
			3165,	/* Code */
			10415,	/* Vendor */
			"Group-Monitoring-Event-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Group-Monitoring-Event-Report-Item, Grouped, code 3166, section 8.4.62 */
	{
		struct dict_avp_data data = {
			3166,	/* Code */
			10415,	/* Vendor */
			"Group-Monitoring-Event-Report-Item",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RIR-Flags, Unsigned32, code 3167, section 8.4.63                 */
	{
		struct dict_avp_data data = {
			3167,	/* Code */
			10415,	/* Vendor */
			"RIR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Type-Of-External-Identifier, Unsigned32, code 3168, section 8.4.64 */
	{
		struct dict_avp_data data = {
			3168,	/* Code */
			10415,	/* Vendor */
			"Type-Of-External-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-Validity-Time, Grouped, code 3169, section 8.4.65            */
	{
		struct dict_avp_data data = {
			3169,	/* Code */
			10415,	/* Vendor */
			"APN-Validity-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Suggested-Network-Configuration, Grouped, code 3170, section 8.4.66 */
	{
		struct dict_avp_data data = {
			3170,	/* Code */
			10415,	/* Vendor */
			"Suggested-Network-Configuration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Report-Status, Grouped, code 3171, section 8.4.67 */
	{
		struct dict_avp_data data = {
			3171,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Report-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PLMN-ID-Requested, Enumerated, code 3172, section 8.4.68         */
	{
		struct dict_avp_data data = {
			3172,	/* Code */
			10415,	/* Vendor */
			"PLMN-ID-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PLMN-ID-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AdditionalIdentifiers, Grouped, code 3173, section 8.4.69        */
	{
		struct dict_avp_data data = {
			3173,	/* Code */
			10415,	/* Vendor */
			"AdditionalIdentifiers",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NIR-Flags, Unsigned32, code 3174, section 8.4.70                 */
	{
		struct dict_avp_data data = {
			3174,	/* Code */
			10415,	/* Vendor */
			"NIR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reporting-Time-Stamp, Time, code 3175, section 8.4.71            */
	{
		struct dict_avp_data data = {
			3175,	/* Code */
			10415,	/* Vendor */
			"Reporting-Time-Stamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* NIA-Flags, Unsigned32, code 3176, section 8.4.72                 */
	{
		struct dict_avp_data data = {
			3176,	/* Code */
			10415,	/* Vendor */
			"NIA-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Group-User-Identifier, Grouped, code 3177, section 8.4.73        */
	{
		struct dict_avp_data data = {
			3177,	/* Code */
			10415,	/* Vendor */
			"Group-User-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MTC-Provider-Info, Grouped, code 3178, section 8.4.74            */
	{
		struct dict_avp_data data = {
			3178,	/* Code */
			10415,	/* Vendor */
			"MTC-Provider-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MTC-Provider-ID, UTF8String, code 3179, section 8.4.75           */
	{
		struct dict_avp_data data = {
			3179,	/* Code */
			10415,	/* Vendor */
			"MTC-Provider-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.338 V15.1.0 (2018-12)                                 */
	/* From 3GPP 29338-f10.docx                                         */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.338 Table 5.3.3.1/1: S6c specific Diameter AVPs       */
	/*==================================================================*/

	/* SM-RP-MTI, Enumerated, code 3308, section 5.3.3.2                */
	{
		struct dict_avp_data data = {
			3308,	/* Code */
			10415,	/* Vendor */
			"SM-RP-MTI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SM-RP-MTI)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SM-RP-SMEA, OctetString, code 3309, section 5.3.3.3              */
	{
		struct dict_avp_data data = {
			3309,	/* Code */
			10415,	/* Vendor */
			"SM-RP-SMEA",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SRR-Flags, Unsigned32, code 3310, section 5.3.3.4                */
	{
		struct dict_avp_data data = {
			3310,	/* Code */
			10415,	/* Vendor */
			"SRR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Delivery-Not-Intended, Enumerated, code 3311, section 5.3.3.5 */
	{
		struct dict_avp_data data = {
			3311,	/* Code */
			10415,	/* Vendor */
			"SM-Delivery-Not-Intended",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SM-Delivery-Not-Intended)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MWD-Status, Unsigned32, code 3312, section 5.3.3.8               */
	{
		struct dict_avp_data data = {
			3312,	/* Code */
			10415,	/* Vendor */
			"MWD-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MME-Absent-User-Diagnostic-SM, Unsigned32, code 3313, section 5.3.3.9 */
	{
		struct dict_avp_data data = {
			3313,	/* Code */
			10415,	/* Vendor */
			"MME-Absent-User-Diagnostic-SM",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MSC-Absent-User-Diagnostic-SM, Unsigned32, code 3314, section 5.3.3.10 */
	{
		struct dict_avp_data data = {
			3314,	/* Code */
			10415,	/* Vendor */
			"MSC-Absent-User-Diagnostic-SM",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SGSN-Absent-User-Diagnostic-SM, Unsigned32, code 3315, section 5.3.3.11 */
	{
		struct dict_avp_data data = {
			3315,	/* Code */
			10415,	/* Vendor */
			"SGSN-Absent-User-Diagnostic-SM",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Delivery-Outcome, Grouped, code 3316, section 5.3.3.14        */
	{
		struct dict_avp_data data = {
			3316,	/* Code */
			10415,	/* Vendor */
			"SM-Delivery-Outcome",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MME-SM-Delivery-Outcome, Grouped, code 3317, section 5.3.3.15    */
	{
		struct dict_avp_data data = {
			3317,	/* Code */
			10415,	/* Vendor */
			"MME-SM-Delivery-Outcome",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MSC-SM-Delivery-Outcome, Grouped, code 3318, section 5.3.3.16    */
	{
		struct dict_avp_data data = {
			3318,	/* Code */
			10415,	/* Vendor */
			"MSC-SM-Delivery-Outcome",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SGSN-SM-Delivery-Outcome, Grouped, code 3319, section 5.3.3.17   */
	{
		struct dict_avp_data data = {
			3319,	/* Code */
			10415,	/* Vendor */
			"SGSN-SM-Delivery-Outcome",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IP-SM-GW-SM-Delivery-Outcome, Grouped, code 3320, section 5.3.3.18 */
	{
		struct dict_avp_data data = {
			3320,	/* Code */
			10415,	/* Vendor */
			"IP-SM-GW-SM-Delivery-Outcome",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Delivery-Cause, Enumerated, code 3321, section 5.3.3.19       */
	{
		struct dict_avp_data data = {
			3321,	/* Code */
			10415,	/* Vendor */
			"SM-Delivery-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SM-Delivery-Cause)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Absent-User-Diagnostic-SM, Unsigned32, code 3322, section 5.3.3.20 */
	{
		struct dict_avp_data data = {
			3322,	/* Code */
			10415,	/* Vendor */
			"Absent-User-Diagnostic-SM",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RDR-Flags, Unsigned32, code 3323, section 5.3.3.21               */
	{
		struct dict_avp_data data = {
			3323,	/* Code */
			10415,	/* Vendor */
			"RDR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Maximum-UE-Availability-Time, Time, code 3329, section 5.3.3.22  */
	{
		struct dict_avp_data data = {
			3329,	/* Code */
			10415,	/* Vendor */
			"Maximum-UE-Availability-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SMS-GMSC-Alert-Event, Unsigned32, code 3333, section 5.3.3.23    */
	{
		struct dict_avp_data data = {
			3333,	/* Code */
			10415,	/* Vendor */
			"SMS-GMSC-Alert-Event",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 29.344 V15.1.0 (2019-09)                                 */
	/* From 3GPP 29344-f10.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 29.344 Table 6.3.1-1: PC4a specific Diameter AVPs        */
	/*==================================================================*/

	/* ProSe-Subscription-Data, Grouped, code 3701, section 6.3.2       */
	{
		struct dict_avp_data data = {
			3701,	/* Code */
			10415,	/* Vendor */
			"ProSe-Subscription-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Permission, Unsigned32, code 3702, section 6.3.3           */
	{
		struct dict_avp_data data = {
			3702,	/* Code */
			10415,	/* Vendor */
			"ProSe-Permission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Allowed-PLMN, Grouped, code 3703, section 6.3.4            */
	{
		struct dict_avp_data data = {
			3703,	/* Code */
			10415,	/* Vendor */
			"ProSe-Allowed-PLMN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Direct-Allowed, Unsigned32, code 3704, section 6.3.5       */
	{
		struct dict_avp_data data = {
			3704,	/* Code */
			10415,	/* Vendor */
			"ProSe-Direct-Allowed",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* UPR-Flags, Unsigned32, code 3705, section 6.3.6                  */
	{
		struct dict_avp_data data = {
			3705,	/* Code */
			10415,	/* Vendor */
			"UPR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PNR-Flags, Unsigned32, code 3706, section 6.3.7                  */
	{
		struct dict_avp_data data = {
			3706,	/* Code */
			10415,	/* Vendor */
			"PNR-Flags",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Initial-Location-Information, Grouped, code 3707, section 6.3.9 */
	{
		struct dict_avp_data data = {
			3707,	/* Code */
			10415,	/* Vendor */
			"ProSe-Initial-Location-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP TS 32.299 V15.7.0 (2019-06)                                 */
	/* From 3GPP 32299-f70.doc                                          */
	/*==================================================================*/

	/*==================================================================*/
	/* 3GPP TS 32.299 Table 7.2.0.1: 3GPP specific AVPs                 */
	/*==================================================================*/

	/* 3GPP-OC-Specific-Reduction, Grouped, code 1320                   */
	{
		struct dict_avp_data data = {
			1320,	/* Code */
			10415,	/* Vendor */
			"3GPP-OC-Specific-Reduction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-OC-Rating-Group, Unsigned32, code 1321                      */
	{
		struct dict_avp_data data = {
			1321,	/* Code */
			10415,	/* Vendor */
			"3GPP-OC-Rating-Group",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* 3GPP-OC-Request-Type, Enumerated, code 1322                      */
	{
		struct dict_avp_data data = {
			1322,	/* Code */
			10415,	/* Vendor */
			"3GPP-OC-Request-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/3GPP-OC-Request-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: Name conflict with 3GPP TS 29.212 3GPP-PS-Data-Off-Status (2847). */
	/* 3GPP-PS-Data-Off-Status (4406) renamed to 3GPP-PS-Data-Off-Status-32.299 (4406). */
	/* 3GPP-PS-Data-Off-Status-32.299, Enumerated, code 4406            */
	{
		struct dict_avp_data data = {
			4406,	/* Code */
			10415,	/* Vendor */
			"3GPP-PS-Data-Off-Status-32.299",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/3GPP-PS-Data-Off-Status-32.299)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Access-Network-Info-Change, Grouped, code 4401                   */
	{
		struct dict_avp_data data = {
			4401,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Info-Change",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Network-Information, OctetString, code 1263               */
	{
		struct dict_avp_data data = {
			1263,	/* Code */
			10415,	/* Vendor */
			"Access-Network-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Transfer-Information, Grouped, code 2709                  */
	{
		struct dict_avp_data data = {
			2709,	/* Code */
			10415,	/* Vendor */
			"Access-Transfer-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Access-Transfer-Type, Enumerated, code 2710                      */
	{
		struct dict_avp_data data = {
			2710,	/* Code */
			10415,	/* Vendor */
			"Access-Transfer-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Access-Transfer-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Account-Expiration, Time, code 2309                              */
	{
		struct dict_avp_data data = {
			2309,	/* Code */
			10415,	/* Vendor */
			"Account-Expiration",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Accumulated-Cost, Grouped, code 2052                             */
	{
		struct dict_avp_data data = {
			2052,	/* Code */
			10415,	/* Vendor */
			"Accumulated-Cost",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Adaptations, Enumerated, code 1217                               */
	{
		struct dict_avp_data data = {
			1217,	/* Code */
			10415,	/* Vendor */
			"Adaptations",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Adaptations)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Additional-Content-Information, Grouped, code 1207               */
	{
		struct dict_avp_data data = {
			1207,	/* Code */
			10415,	/* Vendor */
			"Additional-Content-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Additional-Exception-Reports, Enumerated, code 3936              */
	{
		struct dict_avp_data data = {
			3936,	/* Code */
			10415,	/* Vendor */
			"Additional-Exception-Reports",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Additional-Exception-Reports)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Additional-Type-Information, UTF8String, code 1205               */
	{
		struct dict_avp_data data = {
			1205,	/* Code */
			10415,	/* Vendor */
			"Additional-Type-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Address-Data, UTF8String, code 897                               */
	{
		struct dict_avp_data data = {
			897,	/* Code */
			10415,	/* Vendor */
			"Address-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Address-Domain, Grouped, code 898                                */
	{
		struct dict_avp_data data = {
			898,	/* Code */
			10415,	/* Vendor */
			"Address-Domain",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Addressee-Type, Enumerated, code 1208                            */
	{
		struct dict_avp_data data = {
			1208,	/* Code */
			10415,	/* Vendor */
			"Addressee-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Addressee-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Address-Type, Enumerated, code 899                               */
	{
		struct dict_avp_data data = {
			899,	/* Code */
			10415,	/* Vendor */
			"Address-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Address-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AF-Correlation-Information, Grouped, code 1276                   */
	{
		struct dict_avp_data data = {
			1276,	/* Code */
			10415,	/* Vendor */
			"AF-Correlation-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Alternate-Charged-Party-Address, UTF8String, code 1280           */
	{
		struct dict_avp_data data = {
			1280,	/* Code */
			10415,	/* Vendor */
			"Alternate-Charged-Party-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Announcement-Identifier, Unsigned32, code 3905                   */
	{
		struct dict_avp_data data = {
			3905,	/* Code */
			10415,	/* Vendor */
			"Announcement-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Announcement-Information, Grouped, code 3904                     */
	{
		struct dict_avp_data data = {
			3904,	/* Code */
			10415,	/* Vendor */
			"Announcement-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Announcement-Order, Unsigned32, code 3906                        */
	{
		struct dict_avp_data data = {
			3906,	/* Code */
			10415,	/* Vendor */
			"Announcement-Order",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Announcing-PLMN-ID, UTF8String, code 4408                        */
	{
		struct dict_avp_data data = {
			4408,	/* Code */
			10415,	/* Vendor */
			"Announcing-PLMN-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Announcing-UE-HPLMN-Identifier, UTF8String, code 3426            */
	{
		struct dict_avp_data data = {
			3426,	/* Code */
			10415,	/* Vendor */
			"Announcing-UE-HPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Announcing-UE-VPLMN-Identifier, UTF8String, code 3427            */
	{
		struct dict_avp_data data = {
			3427,	/* Code */
			10415,	/* Vendor */
			"Announcing-UE-VPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* AoC-Cost-Information, Grouped, code 2053                         */
	{
		struct dict_avp_data data = {
			2053,	/* Code */
			10415,	/* Vendor */
			"AoC-Cost-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AoC-Format, Enumerated, code 2310                                */
	{
		struct dict_avp_data data = {
			2310,	/* Code */
			10415,	/* Vendor */
			"AoC-Format",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AoC-Format)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AoC-Information, Grouped, code 2054                              */
	{
		struct dict_avp_data data = {
			2054,	/* Code */
			10415,	/* Vendor */
			"AoC-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AoC-Request-Type, Enumerated, code 2055                          */
	{
		struct dict_avp_data data = {
			2055,	/* Code */
			10415,	/* Vendor */
			"AoC-Request-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AoC-Request-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AoC-Service, Grouped, code 2311                                  */
	{
		struct dict_avp_data data = {
			2311,	/* Code */
			10415,	/* Vendor */
			"AoC-Service",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* AoC-Service-Obligatory-Type, Enumerated, code 2312               */
	{
		struct dict_avp_data data = {
			2312,	/* Code */
			10415,	/* Vendor */
			"AoC-Service-Obligatory-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AoC-Service-Obligatory-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AoC-Service-Type, Enumerated, code 2313                          */
	{
		struct dict_avp_data data = {
			2313,	/* Code */
			10415,	/* Vendor */
			"AoC-Service-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/AoC-Service-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* AoC-Subscription-Information, Grouped, code 2314                 */
	{
		struct dict_avp_data data = {
			2314,	/* Code */
			10415,	/* Vendor */
			"AoC-Subscription-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* API-Content, UTF8String, code 1309                               */
	{
		struct dict_avp_data data = {
			1309,	/* Code */
			10415,	/* Vendor */
			"API-Content",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* API-Direction, Enumerated, code 1310                             */
	{
		struct dict_avp_data data = {
			1310,	/* Code */
			10415,	/* Vendor */
			"API-Direction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/API-Direction)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* API-Identifier, OctetString, code 1311                           */
	{
		struct dict_avp_data data = {
			1311,	/* Code */
			10415,	/* Vendor */
			"API-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* API-Invocation-Timestamp, Time, code 1312                        */
	{
		struct dict_avp_data data = {
			1312,	/* Code */
			10415,	/* Vendor */
			"API-Invocation-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* API-Network-Service-Node, Enumerated, code 1315                  */
	{
		struct dict_avp_data data = {
			1315,	/* Code */
			10415,	/* Vendor */
			"API-Network-Service-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/API-Network-Service-Node)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* API-Result-Code, Unsigned32, code 1313                           */
	{
		struct dict_avp_data data = {
			1313,	/* Code */
			10415,	/* Vendor */
			"API-Result-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* API-Size, Unsigned64, code 1314                                  */
	{
		struct dict_avp_data data = {
			1314,	/* Code */
			10415,	/* Vendor */
			"API-Size",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-Rate-Control, Grouped, code 3933                             */
	{
		struct dict_avp_data data = {
			3933,	/* Code */
			10415,	/* Vendor */
			"APN-Rate-Control",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-Rate-Control-Downlink, Grouped, code 3934                    */
	{
		struct dict_avp_data data = {
			3934,	/* Code */
			10415,	/* Vendor */
			"APN-Rate-Control-Downlink",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* APN-Rate-Control-Uplink, Grouped, code 3935                      */
	{
		struct dict_avp_data data = {
			3935,	/* Code */
			10415,	/* Vendor */
			"APN-Rate-Control-Uplink",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Applic-ID, UTF8String, code 1218                                 */
	{
		struct dict_avp_data data = {
			1218,	/* Code */
			10415,	/* Vendor */
			"Applic-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Application-Provided-Called-Party-Address, UTF8String, code 837  */
	{
		struct dict_avp_data data = {
			837,	/* Code */
			10415,	/* Vendor */
			"Application-Provided-Called-Party-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Application-Server, UTF8String, code 836                         */
	{
		struct dict_avp_data data = {
			836,	/* Code */
			10415,	/* Vendor */
			"Application-Server",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Application-Server-Information, Grouped, code 850                */
	{
		struct dict_avp_data data = {
			850,	/* Code */
			10415,	/* Vendor */
			"Application-Server-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Application-Specific-Data, OctetString, code 3458                */
	{
		struct dict_avp_data data = {
			3458,	/* Code */
			10415,	/* Vendor */
			"Application-Specific-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Associated-Party-Address, UTF8String, code 2035                  */
	{
		struct dict_avp_data data = {
			2035,	/* Code */
			10415,	/* Vendor */
			"Associated-Party-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Associated-URI, UTF8String, code 856                             */
	{
		struct dict_avp_data data = {
			856,	/* Code */
			10415,	/* Vendor */
			"Associated-URI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Authorised-QoS, UTF8String, code 849                             */
	{
		struct dict_avp_data data = {
			849,	/* Code */
			10415,	/* Vendor */
			"Authorised-QoS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Aux-Applic-Info, UTF8String, code 1219                           */
	{
		struct dict_avp_data data = {
			1219,	/* Code */
			10415,	/* Vendor */
			"Aux-Applic-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Base-Time-Interval, Unsigned32, code 1265                        */
	{
		struct dict_avp_data data = {
			1265,	/* Code */
			10415,	/* Vendor */
			"Base-Time-Interval",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Basic-Service-Code, Grouped, code 3411                           */
	{
		struct dict_avp_data data = {
			3411,	/* Code */
			10415,	/* Vendor */
			"Basic-Service-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Bearer-Capability, OctetString, code 3412                        */
	{
		struct dict_avp_data data = {
			3412,	/* Code */
			10415,	/* Vendor */
			"Bearer-Capability",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Bearer-Service, OctetString, code 854                            */
	{
		struct dict_avp_data data = {
			854,	/* Code */
			10415,	/* Vendor */
			"Bearer-Service",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* BSSID, UTF8String, code 2716                                     */
	{
		struct dict_avp_data data = {
			2716,	/* Code */
			10415,	/* Vendor */
			"BSSID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Called-Asserted-Identity, UTF8String, code 1250                  */
	{
		struct dict_avp_data data = {
			1250,	/* Code */
			10415,	/* Vendor */
			"Called-Asserted-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Called-Identity, UTF8String, code 3916                           */
	{
		struct dict_avp_data data = {
			3916,	/* Code */
			10415,	/* Vendor */
			"Called-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Called-Identity-Change, Grouped, code 3917                       */
	{
		struct dict_avp_data data = {
			3917,	/* Code */
			10415,	/* Vendor */
			"Called-Identity-Change",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Called-Party-Address, UTF8String, code 832                       */
	{
		struct dict_avp_data data = {
			832,	/* Code */
			10415,	/* Vendor */
			"Called-Party-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Calling-Party-Address, UTF8String, code 831                      */
	{
		struct dict_avp_data data = {
			831,	/* Code */
			10415,	/* Vendor */
			"Calling-Party-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Carrier-Select-Routing-Information, UTF8String, code 2023        */
	{
		struct dict_avp_data data = {
			2023,	/* Code */
			10415,	/* Vendor */
			"Carrier-Select-Routing-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Cause-Code, Integer32, code 861                                  */
	{
		struct dict_avp_data data = {
			861,	/* Code */
			10415,	/* Vendor */
			"Cause-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Cellular-Network-Information, OctetString, code 3924             */
	{
		struct dict_avp_data data = {
			3924,	/* Code */
			10415,	/* Vendor */
			"Cellular-Network-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CG-Address, Address, code 846                                    */
	{
		struct dict_avp_data data = {
			846,	/* Code */
			10415,	/* Vendor */
			"CG-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Change-Condition, Integer32, code 2037                           */
	{
		struct dict_avp_data data = {
			2037,	/* Code */
			10415,	/* Vendor */
			"Change-Condition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Change-Time, Time, code 2038                                     */
	{
		struct dict_avp_data data = {
			2038,	/* Code */
			10415,	/* Vendor */
			"Change-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Charge-Reason-Code, Enumerated, code 2118                        */
	{
		struct dict_avp_data data = {
			2118,	/* Code */
			10415,	/* Vendor */
			"Charge-Reason-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Charge-Reason-Code)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Charged-Party, UTF8String, code 857                              */
	{
		struct dict_avp_data data = {
			857,	/* Code */
			10415,	/* Vendor */
			"Charged-Party",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Charging-Characteristics-Selection-Mode, Enumerated, code 2066   */
	{
		struct dict_avp_data data = {
			2066,	/* Code */
			10415,	/* Vendor */
			"Charging-Characteristics-Selection-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Charging-Characteristics-Selection-Mode)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Charging-Per-IP-CAN-Session-Indicator, Enumerated, code 4400     */
	{
		struct dict_avp_data data = {
			4400,	/* Code */
			10415,	/* Vendor */
			"Charging-Per-IP-CAN-Session-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Charging-Per-IP-CAN-Session-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Civic-Address-Information, UTF8String, code 1305                 */
	{
		struct dict_avp_data data = {
			1305,	/* Code */
			10415,	/* Vendor */
			"Civic-Address-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Class-Identifier, Enumerated, code 1214                          */
	{
		struct dict_avp_data data = {
			1214,	/* Code */
			10415,	/* Vendor */
			"Class-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Class-Identifier)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Client-Address, Address, code 2018                               */
	{
		struct dict_avp_data data = {
			2018,	/* Code */
			10415,	/* Vendor */
			"Client-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* CN-Operator-Selection-Entity, Enumerated, code 3421              */
	{
		struct dict_avp_data data = {
			3421,	/* Code */
			10415,	/* Vendor */
			"CN-Operator-Selection-Entity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CN-Operator-Selection-Entity)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Content-Class, Enumerated, code 1220                             */
	{
		struct dict_avp_data data = {
			1220,	/* Code */
			10415,	/* Vendor */
			"Content-Class",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Content-Class)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Content-Disposition, UTF8String, code 828                        */
	{
		struct dict_avp_data data = {
			828,	/* Code */
			10415,	/* Vendor */
			"Content-Disposition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Content-Length, Unsigned32, code 827                             */
	{
		struct dict_avp_data data = {
			827,	/* Code */
			10415,	/* Vendor */
			"Content-Length",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Content-Size, Unsigned32, code 1206                              */
	{
		struct dict_avp_data data = {
			1206,	/* Code */
			10415,	/* Vendor */
			"Content-Size",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Content-Type, UTF8String, code 826                               */
	{
		struct dict_avp_data data = {
			826,	/* Code */
			10415,	/* Vendor */
			"Content-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Coverage-Info, Grouped, code 3459                                */
	{
		struct dict_avp_data data = {
			3459,	/* Code */
			10415,	/* Vendor */
			"Coverage-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Coverage-Status, Enumerated, code 3428                           */
	{
		struct dict_avp_data data = {
			3428,	/* Code */
			10415,	/* Vendor */
			"Coverage-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Coverage-Status)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* CP-CIoT-EPS-Optimisation-Indicator, Enumerated, code 3930        */
	{
		struct dict_avp_data data = {
			3930,	/* Code */
			10415,	/* Vendor */
			"CP-CIoT-EPS-Optimisation-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CP-CIoT-EPS-Optimisation-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* CPDT-Information, Grouped, code 3927                             */
	{
		struct dict_avp_data data = {
			3927,	/* Code */
			10415,	/* Vendor */
			"CPDT-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* CSG-Access-Mode, Enumerated, code 2317                           */
	{
		struct dict_avp_data data = {
			2317,	/* Code */
			10415,	/* Vendor */
			"CSG-Access-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CSG-Access-Mode)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* CSG-Membership-Indication, Enumerated, code 2318                 */
	{
		struct dict_avp_data data = {
			2318,	/* Code */
			10415,	/* Vendor */
			"CSG-Membership-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/CSG-Membership-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* CUG-Information, OctetString, code 2304                          */
	{
		struct dict_avp_data data = {
			2304,	/* Code */
			10415,	/* Vendor */
			"CUG-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Current-Tariff, Grouped, code 2056                               */
	{
		struct dict_avp_data data = {
			2056,	/* Code */
			10415,	/* Vendor */
			"Current-Tariff",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Data-Coding-Scheme, Integer32, code 2001                         */
	{
		struct dict_avp_data data = {
			2001,	/* Code */
			10415,	/* Vendor */
			"Data-Coding-Scheme",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Deferred-Location-Event-Type, UTF8String, code 1230              */
	{
		struct dict_avp_data data = {
			1230,	/* Code */
			10415,	/* Vendor */
			"Deferred-Location-Event-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Delivery-Report-Requested, Enumerated, code 1216                 */
	{
		struct dict_avp_data data = {
			1216,	/* Code */
			10415,	/* Vendor */
			"Delivery-Report-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Delivery-Report-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Destination-Interface, Grouped, code 2002                        */
	{
		struct dict_avp_data data = {
			2002,	/* Code */
			10415,	/* Vendor */
			"Destination-Interface",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Diagnostics, Integer32, code 2039                                */
	{
		struct dict_avp_data data = {
			2039,	/* Code */
			10415,	/* Vendor */
			"Diagnostics",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Discoveree-UE-HPLMN-Identifier, UTF8String, code 4402            */
	{
		struct dict_avp_data data = {
			4402,	/* Code */
			10415,	/* Vendor */
			"Discoveree-UE-HPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Discoveree-UE-VPLMN-Identifier, UTF8String, code 4403            */
	{
		struct dict_avp_data data = {
			4403,	/* Code */
			10415,	/* Vendor */
			"Discoveree-UE-VPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Discoverer-UE-HPLMN-Identifier, UTF8String, code 4404            */
	{
		struct dict_avp_data data = {
			4404,	/* Code */
			10415,	/* Vendor */
			"Discoverer-UE-HPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Discoverer-UE-VPLMN-Identifier, UTF8String, code 4405            */
	{
		struct dict_avp_data data = {
			4405,	/* Code */
			10415,	/* Vendor */
			"Discoverer-UE-VPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Domain-Name, UTF8String, code 1200                               */
	{
		struct dict_avp_data data = {
			1200,	/* Code */
			10415,	/* Vendor */
			"Domain-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* DRM-Content, Enumerated, code 1221                               */
	{
		struct dict_avp_data data = {
			1221,	/* Code */
			10415,	/* Vendor */
			"DRM-Content",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/DRM-Content)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Dynamic-Address-Flag, Enumerated, code 2051                      */
	{
		struct dict_avp_data data = {
			2051,	/* Code */
			10415,	/* Vendor */
			"Dynamic-Address-Flag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Dynamic-Address-Flag)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Dynamic-Address-Flag-Extension, Enumerated, code 2068            */
	{
		struct dict_avp_data data = {
			2068,	/* Code */
			10415,	/* Vendor */
			"Dynamic-Address-Flag-Extension",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Dynamic-Address-Flag-Extension)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Early-Media-Description, Grouped, code 1272                      */
	{
		struct dict_avp_data data = {
			1272,	/* Code */
			10415,	/* Vendor */
			"Early-Media-Description",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Enhanced-Diagnostics, Grouped, code 3901                         */
	{
		struct dict_avp_data data = {
			3901,	/* Code */
			10415,	/* Vendor */
			"Enhanced-Diagnostics",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Envelope, Grouped, code 1266                                     */
	{
		struct dict_avp_data data = {
			1266,	/* Code */
			10415,	/* Vendor */
			"Envelope",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Envelope-End-Time, Time, code 1267                               */
	{
		struct dict_avp_data data = {
			1267,	/* Code */
			10415,	/* Vendor */
			"Envelope-End-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Envelope-Reporting, Enumerated, code 1268                        */
	{
		struct dict_avp_data data = {
			1268,	/* Code */
			10415,	/* Vendor */
			"Envelope-Reporting",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Envelope-Reporting)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Envelope-Start-Time, Time, code 1269                             */
	{
		struct dict_avp_data data = {
			1269,	/* Code */
			10415,	/* Vendor */
			"Envelope-Start-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* EPDG-Address, Address, code 3425                                 */
	{
		struct dict_avp_data data = {
			3425,	/* Code */
			10415,	/* Vendor */
			"EPDG-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Event, UTF8String, code 825                                      */
	{
		struct dict_avp_data data = {
			825,	/* Code */
			10415,	/* Vendor */
			"Event",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Event-Charging-TimeStamp, Time, code 1258                        */
	{
		struct dict_avp_data data = {
			1258,	/* Code */
			10415,	/* Vendor */
			"Event-Charging-TimeStamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Event-Type, Grouped, code 823                                    */
	{
		struct dict_avp_data data = {
			823,	/* Code */
			10415,	/* Vendor */
			"Event-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Expires, Unsigned32, code 888                                    */
	{
		struct dict_avp_data data = {
			888,	/* Code */
			10415,	/* Vendor */
			"Expires",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Exposure-Function-API-Information, Grouped, code 1316            */
	{
		struct dict_avp_data data = {
			1316,	/* Code */
			10415,	/* Vendor */
			"Exposure-Function-API-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* FE-Identifier-List, UTF8String, code 4413                        */
	{
		struct dict_avp_data data = {
			4413,	/* Code */
			10415,	/* Vendor */
			"FE-Identifier-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* File-Repair-Supported, Enumerated, code 1224                     */
	{
		struct dict_avp_data data = {
			1224,	/* Code */
			10415,	/* Vendor */
			"File-Repair-Supported",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/File-Repair-Supported)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Forwarding-Pending, Enumerated, code 3415                        */
	{
		struct dict_avp_data data = {
			3415,	/* Code */
			10415,	/* Vendor */
			"Forwarding-Pending",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Forwarding-Pending)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* From-Address, UTF8String, code 2708                              */
	{
		struct dict_avp_data data = {
			2708,	/* Code */
			10415,	/* Vendor */
			"From-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* GGSN-Address, Address, code 847                                  */
	{
		struct dict_avp_data data = {
			847,	/* Code */
			10415,	/* Vendor */
			"GGSN-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* IMS-Application-Reference-Identifier, UTF8String, code 2601      */
	{
		struct dict_avp_data data = {
			2601,	/* Code */
			10415,	/* Vendor */
			"IMS-Application-Reference-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* IMS-Charging-Identifier, UTF8String, code 841                    */
	{
		struct dict_avp_data data = {
			841,	/* Code */
			10415,	/* Vendor */
			"IMS-Charging-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* IMS-Communication-Service-Identifier, UTF8String, code 1281      */
	{
		struct dict_avp_data data = {
			1281,	/* Code */
			10415,	/* Vendor */
			"IMS-Communication-Service-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* IMS-Emergency-Indicator, Enumerated, code 2322                   */
	{
		struct dict_avp_data data = {
			2322,	/* Code */
			10415,	/* Vendor */
			"IMS-Emergency-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IMS-Emergency-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* IMS-Information, Grouped, code 876                               */
	{
		struct dict_avp_data data = {
			876,	/* Code */
			10415,	/* Vendor */
			"IMS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* IMS-Visited-Network-Identifier, UTF8String, code 2713            */
	{
		struct dict_avp_data data = {
			2713,	/* Code */
			10415,	/* Vendor */
			"IMS-Visited-Network-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* IMSI-Unauthenticated-Flag, Enumerated, code 2308                 */
	{
		struct dict_avp_data data = {
			2308,	/* Code */
			10415,	/* Vendor */
			"IMSI-Unauthenticated-Flag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IMSI-Unauthenticated-Flag)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Incoming-Trunk-Group-Id, UTF8String, code 852                    */
	{
		struct dict_avp_data data = {
			852,	/* Code */
			10415,	/* Vendor */
			"Incoming-Trunk-Group-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Incremental-Cost, Grouped, code 2062                             */
	{
		struct dict_avp_data data = {
			2062,	/* Code */
			10415,	/* Vendor */
			"Incremental-Cost",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Initial-IMS-Charging-Identifier, UTF8String, code 2321           */
	{
		struct dict_avp_data data = {
			2321,	/* Code */
			10415,	/* Vendor */
			"Initial-IMS-Charging-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Instance-Id, UTF8String, code 3402                               */
	{
		struct dict_avp_data data = {
			3402,	/* Code */
			10415,	/* Vendor */
			"Instance-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Interface-Id, UTF8String, code 2003                              */
	{
		struct dict_avp_data data = {
			2003,	/* Code */
			10415,	/* Vendor */
			"Interface-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Interface-Port, UTF8String, code 2004                            */
	{
		struct dict_avp_data data = {
			2004,	/* Code */
			10415,	/* Vendor */
			"Interface-Port",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Interface-Text, UTF8String, code 2005                            */
	{
		struct dict_avp_data data = {
			2005,	/* Code */
			10415,	/* Vendor */
			"Interface-Text",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Interface-Type, Enumerated, code 2006                            */
	{
		struct dict_avp_data data = {
			2006,	/* Code */
			10415,	/* Vendor */
			"Interface-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Interface-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Inter-Operator-Identifier, Grouped, code 838                     */
	{
		struct dict_avp_data data = {
			838,	/* Code */
			10415,	/* Vendor */
			"Inter-Operator-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Inter-UE-Transfer, Enumerated, code 3902                         */
	{
		struct dict_avp_data data = {
			3902,	/* Code */
			10415,	/* Vendor */
			"Inter-UE-Transfer",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Inter-UE-Transfer)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* IP-Realm-Default-Indication, Enumerated, code 2603               */
	{
		struct dict_avp_data data = {
			2603,	/* Code */
			10415,	/* Vendor */
			"IP-Realm-Default-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/IP-Realm-Default-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ISUP-Cause, Grouped, code 3416                                   */
	{
		struct dict_avp_data data = {
			3416,	/* Code */
			10415,	/* Vendor */
			"ISUP-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ISUP-Cause-Diagnostics, OctetString, code 3422                   */
	{
		struct dict_avp_data data = {
			3422,	/* Code */
			10415,	/* Vendor */
			"ISUP-Cause-Diagnostics",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ISUP-Cause-Location, Unsigned32, code 3423                       */
	{
		struct dict_avp_data data = {
			3423,	/* Code */
			10415,	/* Vendor */
			"ISUP-Cause-Location",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ISUP-Cause-Value, Unsigned32, code 3424                          */
	{
		struct dict_avp_data data = {
			3424,	/* Code */
			10415,	/* Vendor */
			"ISUP-Cause-Value",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ISUP-Location-Number, OctetString, code 3414                     */
	{
		struct dict_avp_data data = {
			3414,	/* Code */
			10415,	/* Vendor */
			"ISUP-Location-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Language, UTF8String, code 3914                                  */
	{
		struct dict_avp_data data = {
			3914,	/* Code */
			10415,	/* Vendor */
			"Language",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Layer-2-Group-ID, OctetString, code 3429                         */
	{
		struct dict_avp_data data = {
			3429,	/* Code */
			10415,	/* Vendor */
			"Layer-2-Group-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-APN, UTF8String, code 1231                                   */
	{
		struct dict_avp_data data = {
			1231,	/* Code */
			10415,	/* Vendor */
			"LCS-APN",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* LCS-Client-Dialed-By-MS, UTF8String, code 1233                   */
	{
		struct dict_avp_data data = {
			1233,	/* Code */
			10415,	/* Vendor */
			"LCS-Client-Dialed-By-MS",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* LCS-Client-External-ID, UTF8String, code 1234                    */
	{
		struct dict_avp_data data = {
			1234,	/* Code */
			10415,	/* Vendor */
			"LCS-Client-External-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* LCS-Client-Id, Grouped, code 1232                                */
	{
		struct dict_avp_data data = {
			1232,	/* Code */
			10415,	/* Vendor */
			"LCS-Client-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Client-Name, Grouped, code 1235                              */
	{
		struct dict_avp_data data = {
			1235,	/* Code */
			10415,	/* Vendor */
			"LCS-Client-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Client-Type, Enumerated, code 1241                           */
	{
		struct dict_avp_data data = {
			1241,	/* Code */
			10415,	/* Vendor */
			"LCS-Client-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/LCS-Client-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* LCS-Data-Coding-Scheme, UTF8String, code 1236                    */
	{
		struct dict_avp_data data = {
			1236,	/* Code */
			10415,	/* Vendor */
			"LCS-Data-Coding-Scheme",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* LCS-Format-Indicator, Enumerated, code 1237                      */
	{
		struct dict_avp_data data = {
			1237,	/* Code */
			10415,	/* Vendor */
			"LCS-Format-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/LCS-Format-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* LCS-Information, Grouped, code 878                               */
	{
		struct dict_avp_data data = {
			878,	/* Code */
			10415,	/* Vendor */
			"LCS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* LCS-Name-String, UTF8String, code 1238                           */
	{
		struct dict_avp_data data = {
			1238,	/* Code */
			10415,	/* Vendor */
			"LCS-Name-String",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V11.8.0 (2013-07) renamed                   */
	/* LCS-Requestor-Id (1239) to LCS-Requestor-ID (1239).              */
	/* LCS-Requestor-ID, Grouped, code 1239                             */
	{
		struct dict_avp_data data = {
			1239,	/* Code */
			10415,	/* Vendor */
			"LCS-Requestor-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 32.299 V11.8.0 (2013-07) renamed                   */
	/* LCS-Requestor-Id-String (1240) to LCS-Requestor-ID-String (1240). */
	/* LCS-Requestor-ID-String, UTF8String, code 1240                   */
	{
		struct dict_avp_data data = {
			1240,	/* Code */
			10415,	/* Vendor */
			"LCS-Requestor-ID-String",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Local-GW-Inserted-Indication, Enumerated, code 2604              */
	{
		struct dict_avp_data data = {
			2604,	/* Code */
			10415,	/* Vendor */
			"Local-GW-Inserted-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Local-GW-Inserted-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Local-Sequence-Number, Unsigned32, code 2063                     */
	{
		struct dict_avp_data data = {
			2063,	/* Code */
			10415,	/* Vendor */
			"Local-Sequence-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Location-Estimate, OctetString, code 1242                        */
	{
		struct dict_avp_data data = {
			1242,	/* Code */
			10415,	/* Vendor */
			"Location-Estimate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Location-Estimate-Type, Enumerated, code 1243                    */
	{
		struct dict_avp_data data = {
			1243,	/* Code */
			10415,	/* Vendor */
			"Location-Estimate-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Location-Estimate-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Location-Info, Grouped, code 3460                                */
	{
		struct dict_avp_data data = {
			3460,	/* Code */
			10415,	/* Vendor */
			"Location-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Location-Type, Grouped, code 1244                                */
	{
		struct dict_avp_data data = {
			1244,	/* Code */
			10415,	/* Vendor */
			"Location-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Low-Balance-Indication, Enumerated, code 2020                    */
	{
		struct dict_avp_data data = {
			2020,	/* Code */
			10415,	/* Vendor */
			"Low-Balance-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Low-Balance-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Low-Priority-Indicator, Enumerated, code 2602                    */
	{
		struct dict_avp_data data = {
			2602,	/* Code */
			10415,	/* Vendor */
			"Low-Priority-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Low-Priority-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MBMS-Charged-Party, Enumerated, code 2323                        */
	{
		struct dict_avp_data data = {
			2323,	/* Code */
			10415,	/* Vendor */
			"MBMS-Charged-Party",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-Charged-Party)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: 3GPP TS 32.299 V11.8.0 (2013-07) table 7.2 changed         */
	/* row MBMS-GW-Address (2307) to have a hyphen instead of space.    */
	/* Generated name renamed from MBMSGW-Address (2307).               */
	/* MBMS-GW-Address, Address, code 2307                              */
	{
		struct dict_avp_data data = {
			2307,	/* Code */
			10415,	/* Vendor */
			"MBMS-GW-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* MBMS-Information, Grouped, code 880                              */
	{
		struct dict_avp_data data = {
			880,	/* Code */
			10415,	/* Vendor */
			"MBMS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MBMS-User-Service-Type, Enumerated, code 1225                    */
	{
		struct dict_avp_data data = {
			1225,	/* Code */
			10415,	/* Vendor */
			"MBMS-User-Service-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MBMS-User-Service-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Media-Initiator-Flag, Enumerated, code 882                       */
	{
		struct dict_avp_data data = {
			882,	/* Code */
			10415,	/* Vendor */
			"Media-Initiator-Flag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Media-Initiator-Flag)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Media-Initiator-Party, UTF8String, code 1288                     */
	{
		struct dict_avp_data data = {
			1288,	/* Code */
			10415,	/* Vendor */
			"Media-Initiator-Party",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Message-Body, Grouped, code 889                                  */
	{
		struct dict_avp_data data = {
			889,	/* Code */
			10415,	/* Vendor */
			"Message-Body",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Message-Class, Grouped, code 1213                                */
	{
		struct dict_avp_data data = {
			1213,	/* Code */
			10415,	/* Vendor */
			"Message-Class",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Message-ID, UTF8String, code 1210                                */
	{
		struct dict_avp_data data = {
			1210,	/* Code */
			10415,	/* Vendor */
			"Message-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Message-Size, Unsigned32, code 1212                              */
	{
		struct dict_avp_data data = {
			1212,	/* Code */
			10415,	/* Vendor */
			"Message-Size",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Message-Type, Enumerated, code 1211                              */
	{
		struct dict_avp_data data = {
			1211,	/* Code */
			10415,	/* Vendor */
			"Message-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Message-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MMBox-Storage-Requested, Enumerated, code 1248                   */
	{
		struct dict_avp_data data = {
			1248,	/* Code */
			10415,	/* Vendor */
			"MMBox-Storage-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/MMBox-Storage-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* MM-Content-Type, Grouped, code 1203                              */
	{
		struct dict_avp_data data = {
			1203,	/* Code */
			10415,	/* Vendor */
			"MM-Content-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MMS-Information, Grouped, code 877                               */
	{
		struct dict_avp_data data = {
			877,	/* Code */
			10415,	/* Vendor */
			"MMS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MMTel-Information, Grouped, code 2030                            */
	{
		struct dict_avp_data data = {
			2030,	/* Code */
			10415,	/* Vendor */
			"MMTel-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MMTel-SService-Type, Unsigned32, code 2031                       */
	{
		struct dict_avp_data data = {
			2031,	/* Code */
			10415,	/* Vendor */
			"MMTel-SService-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitored-PLMN-Identifier, UTF8String, code 3430                 */
	{
		struct dict_avp_data data = {
			3430,	/* Code */
			10415,	/* Vendor */
			"Monitored-PLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Monitoring-Event-Configuration-Activity, Integer32, code 3919    */
	{
		struct dict_avp_data data = {
			3919,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Configuration-Activity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Functionality, Integer32, code 3922             */
	{
		struct dict_avp_data data = {
			3922,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Functionality",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Information, Grouped, code 3921                 */
	{
		struct dict_avp_data data = {
			3921,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Report-Data, Grouped, code 3920                 */
	{
		struct dict_avp_data data = {
			3920,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Report-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-Event-Report-Number, Unsigned32, code 3923            */
	{
		struct dict_avp_data data = {
			3923,	/* Code */
			10415,	/* Vendor */
			"Monitoring-Event-Report-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Monitoring-UE-HPLMN-Identifier, UTF8String, code 3431            */
	{
		struct dict_avp_data data = {
			3431,	/* Code */
			10415,	/* Vendor */
			"Monitoring-UE-HPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Monitoring-UE-Identifier, UTF8String, code 3432                  */
	{
		struct dict_avp_data data = {
			3432,	/* Code */
			10415,	/* Vendor */
			"Monitoring-UE-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Monitoring-UE-VPLMN-Identifier, UTF8String, code 3433            */
	{
		struct dict_avp_data data = {
			3433,	/* Code */
			10415,	/* Vendor */
			"Monitoring-UE-VPLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* MSC-Address, OctetString, code 3417                              */
	{
		struct dict_avp_data data = {
			3417,	/* Code */
			10415,	/* Vendor */
			"MSC-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* MTC-IWF-Address, Address, code 3406                              */
	{
		struct dict_avp_data data = {
			3406,	/* Code */
			10415,	/* Vendor */
			"MTC-IWF-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Neighbour-Node-Address, Address, code 2705                       */
	{
		struct dict_avp_data data = {
			2705,	/* Code */
			10415,	/* Vendor */
			"Neighbour-Node-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Network-Call-Reference-Number, OctetString, code 3418            */
	{
		struct dict_avp_data data = {
			3418,	/* Code */
			10415,	/* Vendor */
			"Network-Call-Reference-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Next-Tariff, Grouped, code 2057                                  */
	{
		struct dict_avp_data data = {
			2057,	/* Code */
			10415,	/* Vendor */
			"Next-Tariff",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NIDD-Submission, Grouped, code 3928                              */
	{
		struct dict_avp_data data = {
			3928,	/* Code */
			10415,	/* Vendor */
			"NIDD-Submission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NNI-Information, Grouped, code 2703                              */
	{
		struct dict_avp_data data = {
			2703,	/* Code */
			10415,	/* Vendor */
			"NNI-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* NNI-Type, Enumerated, code 2704                                  */
	{
		struct dict_avp_data data = {
			2704,	/* Code */
			10415,	/* Vendor */
			"NNI-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/NNI-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Node-Functionality, Enumerated, code 862                         */
	{
		struct dict_avp_data data = {
			862,	/* Code */
			10415,	/* Vendor */
			"Node-Functionality",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Node-Functionality)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Node-Id, UTF8String, code 2064                                   */
	{
		struct dict_avp_data data = {
			2064,	/* Code */
			10415,	/* Vendor */
			"Node-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Number-Of-Diversions, Unsigned32, code 2034                      */
	{
		struct dict_avp_data data = {
			2034,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Diversions",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Messages-Sent, Unsigned32, code 2019                   */
	{
		struct dict_avp_data data = {
			2019,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Messages-Sent",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Participants, Unsigned32, code 885                     */
	{
		struct dict_avp_data data = {
			885,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Participants",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Received-Talk-Bursts, Unsigned32, code 1282            */
	{
		struct dict_avp_data data = {
			1282,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Received-Talk-Bursts",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Talk-Bursts, Unsigned32, code 1283                     */
	{
		struct dict_avp_data data = {
			1283,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Talk-Bursts",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Portability-Routing-Information, UTF8String, code 2024    */
	{
		struct dict_avp_data data = {
			2024,	/* Code */
			10415,	/* Vendor */
			"Number-Portability-Routing-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Offline-Charging, Grouped, code 1278                             */
	{
		struct dict_avp_data data = {
			1278,	/* Code */
			10415,	/* Vendor */
			"Offline-Charging",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Online-Charging-Flag, Enumerated, code 2303                      */
	{
		struct dict_avp_data data = {
			2303,	/* Code */
			10415,	/* Vendor */
			"Online-Charging-Flag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Online-Charging-Flag)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Originating-IOI, UTF8String, code 839                            */
	{
		struct dict_avp_data data = {
			839,	/* Code */
			10415,	/* Vendor */
			"Originating-IOI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Originator, Enumerated, code 864                                 */
	{
		struct dict_avp_data data = {
			864,	/* Code */
			10415,	/* Vendor */
			"Originator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Originator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Originator-Address, Grouped, code 886                            */
	{
		struct dict_avp_data data = {
			886,	/* Code */
			10415,	/* Vendor */
			"Originator-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Originator-Interface, Grouped, code 2009                         */
	{
		struct dict_avp_data data = {
			2009,	/* Code */
			10415,	/* Vendor */
			"Originator-Interface",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Originator-Received-Address, Grouped, code 2027                  */
	{
		struct dict_avp_data data = {
			2027,	/* Code */
			10415,	/* Vendor */
			"Originator-Received-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Originator-SCCP-Address, Address, code 2008                      */
	{
		struct dict_avp_data data = {
			2008,	/* Code */
			10415,	/* Vendor */
			"Originator-SCCP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Outgoing-Session-Id, UTF8String, code 2320                       */
	{
		struct dict_avp_data data = {
			2320,	/* Code */
			10415,	/* Vendor */
			"Outgoing-Session-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Outgoing-Trunk-Group-Id, UTF8String, code 853                    */
	{
		struct dict_avp_data data = {
			853,	/* Code */
			10415,	/* Vendor */
			"Outgoing-Trunk-Group-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Participant-Access-Priority, Enumerated, code 1259               */
	{
		struct dict_avp_data data = {
			1259,	/* Code */
			10415,	/* Vendor */
			"Participant-Access-Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Participant-Access-Priority)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Participant-Action-Type, Enumerated, code 2049                   */
	{
		struct dict_avp_data data = {
			2049,	/* Code */
			10415,	/* Vendor */
			"Participant-Action-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Participant-Action-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Participant-Group, Grouped, code 1260                            */
	{
		struct dict_avp_data data = {
			1260,	/* Code */
			10415,	/* Vendor */
			"Participant-Group",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Participants-Involved, UTF8String, code 887                      */
	{
		struct dict_avp_data data = {
			887,	/* Code */
			10415,	/* Vendor */
			"Participants-Involved",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* PC3-Control-Protocol-Cause, Integer32, code 3434                 */
	{
		struct dict_avp_data data = {
			3434,	/* Code */
			10415,	/* Vendor */
			"PC3-Control-Protocol-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PC3-EPC-Control-Protocol-Cause, Integer32, code 3435             */
	{
		struct dict_avp_data data = {
			3435,	/* Code */
			10415,	/* Vendor */
			"PC3-EPC-Control-Protocol-Cause",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PC5-Radio-Technology, Enumerated, code 1300                      */
	{
		struct dict_avp_data data = {
			1300,	/* Code */
			10415,	/* Vendor */
			"PC5-Radio-Technology",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PC5-Radio-Technology)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PDN-Connection-Charging-ID, Unsigned32, code 2050                */
	{
		struct dict_avp_data data = {
			2050,	/* Code */
			10415,	/* Vendor */
			"PDN-Connection-Charging-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PDP-Address, Address, code 1227                                  */
	{
		struct dict_avp_data data = {
			1227,	/* Code */
			10415,	/* Vendor */
			"PDP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* PDP-Address-Prefix-Length, Unsigned32, code 2606                 */
	{
		struct dict_avp_data data = {
			2606,	/* Code */
			10415,	/* Vendor */
			"PDP-Address-Prefix-Length",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PDP-Context-Type, Enumerated, code 1247                          */
	{
		struct dict_avp_data data = {
			1247,	/* Code */
			10415,	/* Vendor */
			"PDP-Context-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PDP-Context-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed PDG-Address (895). */
	/* PDG-Address, Address, code 895                                   */
	{
		struct dict_avp_data data = {
			895,	/* Code */
			10415,	/* Vendor */
			"PDG-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed PDG-Charging-Id (896). */
	/* PDG-Charging-Id, Unsigned32, code 896                            */
	{
		struct dict_avp_data data = {
			896,	/* Code */
			10415,	/* Vendor */
			"PDG-Charging-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Play-Alternative, Enumerated, code 3913                          */
	{
		struct dict_avp_data data = {
			3913,	/* Code */
			10415,	/* Vendor */
			"Play-Alternative",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Play-Alternative)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PoC-Change-Condition, Enumerated, code 1261                      */
	{
		struct dict_avp_data data = {
			1261,	/* Code */
			10415,	/* Vendor */
			"PoC-Change-Condition",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PoC-Change-Condition)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PoC-Change-Time, Time, code 1262                                 */
	{
		struct dict_avp_data data = {
			1262,	/* Code */
			10415,	/* Vendor */
			"PoC-Change-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* PoC-Controlling-Address, UTF8String, code 858                    */
	{
		struct dict_avp_data data = {
			858,	/* Code */
			10415,	/* Vendor */
			"PoC-Controlling-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* PoC-Event-Type, Enumerated, code 2025                            */
	{
		struct dict_avp_data data = {
			2025,	/* Code */
			10415,	/* Vendor */
			"PoC-Event-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PoC-Event-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PoC-Group-Name, UTF8String, code 859                             */
	{
		struct dict_avp_data data = {
			859,	/* Code */
			10415,	/* Vendor */
			"PoC-Group-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* PoC-Information, Grouped, code 879                               */
	{
		struct dict_avp_data data = {
			879,	/* Code */
			10415,	/* Vendor */
			"PoC-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PoC-Server-Role, Enumerated, code 883                            */
	{
		struct dict_avp_data data = {
			883,	/* Code */
			10415,	/* Vendor */
			"PoC-Server-Role",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PoC-Server-Role)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PoC-Session-Id, UTF8String, code 1229                            */
	{
		struct dict_avp_data data = {
			1229,	/* Code */
			10415,	/* Vendor */
			"PoC-Session-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* PoC-Session-Initiation-type, Enumerated, code 1277               */
	{
		struct dict_avp_data data = {
			1277,	/* Code */
			10415,	/* Vendor */
			"PoC-Session-Initiation-type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PoC-Session-Initiation-type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PoC-Session-Type, Enumerated, code 884                           */
	{
		struct dict_avp_data data = {
			884,	/* Code */
			10415,	/* Vendor */
			"PoC-Session-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PoC-Session-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PoC-User-Role, Grouped, code 1252                                */
	{
		struct dict_avp_data data = {
			1252,	/* Code */
			10415,	/* Vendor */
			"PoC-User-Role",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PoC-User-Role-IDs, UTF8String, code 1253                         */
	{
		struct dict_avp_data data = {
			1253,	/* Code */
			10415,	/* Vendor */
			"PoC-User-Role-IDs",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V13.1.0 (2015-06) renamed                   */
	/* PoC-User-Role-info-Units (1254) to PoC-User-Role-Info-Units (1254). */
	/* PoC-User-Role-Info-Units, Enumerated, code 1254                  */
	{
		struct dict_avp_data data = {
			1254,	/* Code */
			10415,	/* Vendor */
			"PoC-User-Role-Info-Units",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PoC-User-Role-Info-Units)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Positioning-Data, UTF8String, code 1245                          */
	{
		struct dict_avp_data data = {
			1245,	/* Code */
			10415,	/* Vendor */
			"Positioning-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Preferred-AoC-Currency, Unsigned32, code 2315                    */
	{
		struct dict_avp_data data = {
			2315,	/* Code */
			10415,	/* Vendor */
			"Preferred-AoC-Currency",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Priority, Enumerated, code 1209                                  */
	{
		struct dict_avp_data data = {
			1209,	/* Code */
			10415,	/* Vendor */
			"Priority",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Priority)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Privacy-Indicator, Enumerated, code 3915                         */
	{
		struct dict_avp_data data = {
			3915,	/* Code */
			10415,	/* Vendor */
			"Privacy-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Privacy-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-3rd-Party-Application-ID, UTF8String, code 3440            */
	{
		struct dict_avp_data data = {
			3440,	/* Code */
			10415,	/* Vendor */
			"ProSe-3rd-Party-Application-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* ProSe-Direct-Communication-Reception-Data-Container, Grouped, code 3461 */
	{
		struct dict_avp_data data = {
			3461,	/* Code */
			10415,	/* Vendor */
			"ProSe-Direct-Communication-Reception-Data-Container",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Direct-Communication-Transmission-Data-Container, Grouped, code 3441 */
	{
		struct dict_avp_data data = {
			3441,	/* Code */
			10415,	/* Vendor */
			"ProSe-Direct-Communication-Transmission-Data-Container",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Direct-Discovery-Model, Enumerated, code 3442              */
	{
		struct dict_avp_data data = {
			3442,	/* Code */
			10415,	/* Vendor */
			"ProSe-Direct-Discovery-Model",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ProSe-Direct-Discovery-Model)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-Event-Type, Enumerated, code 3443                          */
	{
		struct dict_avp_data data = {
			3443,	/* Code */
			10415,	/* Vendor */
			"ProSe-Event-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ProSe-Event-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-Function-IP-Address, Address, code 3444                    */
	{
		struct dict_avp_data data = {
			3444,	/* Code */
			10415,	/* Vendor */
			"ProSe-Function-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* ProSe-Function-PLMN-Identifier, UTF8String, code 3457            */
	{
		struct dict_avp_data data = {
			3457,	/* Code */
			10415,	/* Vendor */
			"ProSe-Function-PLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* ProSe-Functionality, Enumerated, code 3445                       */
	{
		struct dict_avp_data data = {
			3445,	/* Code */
			10415,	/* Vendor */
			"ProSe-Functionality",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ProSe-Functionality)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-Group-IP-Multicast-Address, Address, code 3446             */
	{
		struct dict_avp_data data = {
			3446,	/* Code */
			10415,	/* Vendor */
			"ProSe-Group-IP-Multicast-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* ProSe-Information, Grouped, code 3447                            */
	{
		struct dict_avp_data data = {
			3447,	/* Code */
			10415,	/* Vendor */
			"ProSe-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-Range-Class, Enumerated, code 3448                         */
	{
		struct dict_avp_data data = {
			3448,	/* Code */
			10415,	/* Vendor */
			"ProSe-Range-Class",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ProSe-Range-Class)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-Reason-For-Cancellation, Enumerated, code 3449             */
	{
		struct dict_avp_data data = {
			3449,	/* Code */
			10415,	/* Vendor */
			"ProSe-Reason-For-Cancellation",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ProSe-Reason-For-Cancellation)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-Request-Timestamp, Time, code 3450                         */
	{
		struct dict_avp_data data = {
			3450,	/* Code */
			10415,	/* Vendor */
			"ProSe-Request-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* ProSe-Role-Of-UE, Enumerated, code 3451                          */
	{
		struct dict_avp_data data = {
			3451,	/* Code */
			10415,	/* Vendor */
			"ProSe-Role-Of-UE",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/ProSe-Role-Of-UE)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* ProSe-Source-IP-Address, Address, code 3452                      */
	{
		struct dict_avp_data data = {
			3452,	/* Code */
			10415,	/* Vendor */
			"ProSe-Source-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* ProSe-Target-Layer-2-ID, OctetString, code 4410                  */
	{
		struct dict_avp_data data = {
			4410,	/* Code */
			10415,	/* Vendor */
			"ProSe-Target-Layer-2-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-UE-ID, OctetString, code 3453                              */
	{
		struct dict_avp_data data = {
			3453,	/* Code */
			10415,	/* Vendor */
			"ProSe-UE-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* ProSe-UE-to-Network-Relay-UE-ID, OctetString, code 4409          */
	{
		struct dict_avp_data data = {
			4409,	/* Code */
			10415,	/* Vendor */
			"ProSe-UE-to-Network-Relay-UE-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Proximity-Alert-Indication, Enumerated, code 3454                */
	{
		struct dict_avp_data data = {
			3454,	/* Code */
			10415,	/* Vendor */
			"Proximity-Alert-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Proximity-Alert-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Proximity-Alert-Timestamp, Time, code 3455                       */
	{
		struct dict_avp_data data = {
			3455,	/* Code */
			10415,	/* Vendor */
			"Proximity-Alert-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Proximity-Cancellation-Timestamp, Time, code 3456                */
	{
		struct dict_avp_data data = {
			3456,	/* Code */
			10415,	/* Vendor */
			"Proximity-Cancellation-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* PS-Append-Free-Format-Data, Enumerated, code 867                 */
	{
		struct dict_avp_data data = {
			867,	/* Code */
			10415,	/* Vendor */
			"PS-Append-Free-Format-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/PS-Append-Free-Format-Data)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* PS-Free-Format-Data, OctetString, code 866                       */
	{
		struct dict_avp_data data = {
			866,	/* Code */
			10415,	/* Vendor */
			"PS-Free-Format-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PS-Furnish-Charging-Information, Grouped, code 865               */
	{
		struct dict_avp_data data = {
			865,	/* Code */
			10415,	/* Vendor */
			"PS-Furnish-Charging-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* PS-Information, Grouped, code 874                                */
	{
		struct dict_avp_data data = {
			874,	/* Code */
			10415,	/* Vendor */
			"PS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Quota-Consumption-Time, Unsigned32, code 881                     */
	{
		struct dict_avp_data data = {
			881,	/* Code */
			10415,	/* Vendor */
			"Quota-Consumption-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Quota-Holding-Time, Unsigned32, code 871                         */
	{
		struct dict_avp_data data = {
			871,	/* Code */
			10415,	/* Vendor */
			"Quota-Holding-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Quota-Indicator, Enumerated, code 3912                           */
	{
		struct dict_avp_data data = {
			3912,	/* Code */
			10415,	/* Vendor */
			"Quota-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Quota-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Radio-Frequency, OctetString, code 3462                          */
	{
		struct dict_avp_data data = {
			3462,	/* Code */
			10415,	/* Vendor */
			"Radio-Frequency",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Radio-Parameter-Set-Info, Grouped, code 3463                     */
	{
		struct dict_avp_data data = {
			3463,	/* Code */
			10415,	/* Vendor */
			"Radio-Parameter-Set-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Radio-Parameter-Set-Values, OctetString, code 3464               */
	{
		struct dict_avp_data data = {
			3464,	/* Code */
			10415,	/* Vendor */
			"Radio-Parameter-Set-Values",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Radio-Resources-Indicator, Integer32, code 3465                  */
	{
		struct dict_avp_data data = {
			3465,	/* Code */
			10415,	/* Vendor */
			"Radio-Resources-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RAN-End-Time, Time, code 1301                                    */
	{
		struct dict_avp_data data = {
			1301,	/* Code */
			10415,	/* Vendor */
			"RAN-End-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* RAN-Secondary-RAT-Usage-Report, Grouped, code 1302               */
	{
		struct dict_avp_data data = {
			1302,	/* Code */
			10415,	/* Vendor */
			"RAN-Secondary-RAT-Usage-Report",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* RAN-Start-Time, Time, code 1303                                  */
	{
		struct dict_avp_data data = {
			1303,	/* Code */
			10415,	/* Vendor */
			"RAN-Start-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Rate-Control-Max-Message-Size, Unsigned32, code 3937             */
	{
		struct dict_avp_data data = {
			3937,	/* Code */
			10415,	/* Vendor */
			"Rate-Control-Max-Message-Size",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Rate-Control-Max-Rate, Unsigned32, code 3938                     */
	{
		struct dict_avp_data data = {
			3938,	/* Code */
			10415,	/* Vendor */
			"Rate-Control-Max-Rate",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Rate-Control-Time-Unit, Unsigned32, code 3939                    */
	{
		struct dict_avp_data data = {
			3939,	/* Code */
			10415,	/* Vendor */
			"Rate-Control-Time-Unit",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Rate-Element, Grouped, code 2058                                 */
	{
		struct dict_avp_data data = {
			2058,	/* Code */
			10415,	/* Vendor */
			"Rate-Element",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Read-Reply-Report-Requested, Enumerated, code 1222               */
	{
		struct dict_avp_data data = {
			1222,	/* Code */
			10415,	/* Vendor */
			"Read-Reply-Report-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Read-Reply-Report-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Real-Time-Tariff-Information, Grouped, code 2305                 */
	{
		struct dict_avp_data data = {
			2305,	/* Code */
			10415,	/* Vendor */
			"Real-Time-Tariff-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reason-Header, UTF8String, code 3401                             */
	{
		struct dict_avp_data data = {
			3401,	/* Code */
			10415,	/* Vendor */
			"Reason-Header",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Received-Talk-Burst-Time, Unsigned32, code 1284                  */
	{
		struct dict_avp_data data = {
			1284,	/* Code */
			10415,	/* Vendor */
			"Received-Talk-Burst-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Received-Talk-Burst-Volume, Unsigned32, code 1285                */
	{
		struct dict_avp_data data = {
			1285,	/* Code */
			10415,	/* Vendor */
			"Received-Talk-Burst-Volume",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Recipient-Address, Grouped, code 1201                            */
	{
		struct dict_avp_data data = {
			1201,	/* Code */
			10415,	/* Vendor */
			"Recipient-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Recipient-Info, Grouped, code 2026                               */
	{
		struct dict_avp_data data = {
			2026,	/* Code */
			10415,	/* Vendor */
			"Recipient-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Recipient-Received-Address, Grouped, code 2028                   */
	{
		struct dict_avp_data data = {
			2028,	/* Code */
			10415,	/* Vendor */
			"Recipient-Received-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Recipient-SCCP-Address, Address, code 2010                       */
	{
		struct dict_avp_data data = {
			2010,	/* Code */
			10415,	/* Vendor */
			"Recipient-SCCP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Refund-Information, OctetString, code 2022                       */
	{
		struct dict_avp_data data = {
			2022,	/* Code */
			10415,	/* Vendor */
			"Refund-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Related-Change-Condition-Information, Grouped, code 3925         */
	{
		struct dict_avp_data data = {
			3925,	/* Code */
			10415,	/* Vendor */
			"Related-Change-Condition-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Related-Trigger, Grouped, code 3926                              */
	{
		struct dict_avp_data data = {
			3926,	/* Code */
			10415,	/* Vendor */
			"Related-Trigger",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Related-IMS-Charging-Identifier, UTF8String, code 2711           */
	{
		struct dict_avp_data data = {
			2711,	/* Code */
			10415,	/* Vendor */
			"Related-IMS-Charging-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Related-IMS-Charging-Identifier-Node, Address, code 2712         */
	{
		struct dict_avp_data data = {
			2712,	/* Code */
			10415,	/* Vendor */
			"Related-IMS-Charging-Identifier-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Relationship-Mode, Enumerated, code 2706                         */
	{
		struct dict_avp_data data = {
			2706,	/* Code */
			10415,	/* Vendor */
			"Relationship-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Relationship-Mode)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Relay-IP-address, Address, code 4411                             */
	{
		struct dict_avp_data data = {
			4411,	/* Code */
			10415,	/* Vendor */
			"Relay-IP-address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Remaining-Balance, Grouped, code 2021                            */
	{
		struct dict_avp_data data = {
			2021,	/* Code */
			10415,	/* Vendor */
			"Remaining-Balance",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Reply-Applic-ID, UTF8String, code 1223                           */
	{
		struct dict_avp_data data = {
			1223,	/* Code */
			10415,	/* Vendor */
			"Reply-Applic-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Reply-Path-Requested, Enumerated, code 2011                      */
	{
		struct dict_avp_data data = {
			2011,	/* Code */
			10415,	/* Vendor */
			"Reply-Path-Requested",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Reply-Path-Requested)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Reporting-Reason, Enumerated, code 872                           */
	{
		struct dict_avp_data data = {
			872,	/* Code */
			10415,	/* Vendor */
			"Reporting-Reason",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Reporting-Reason)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Requested-Party-Address, UTF8String, code 1251                   */
	{
		struct dict_avp_data data = {
			1251,	/* Code */
			10415,	/* Vendor */
			"Requested-Party-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Requested-PLMN-Identifier, UTF8String, code 3436                 */
	{
		struct dict_avp_data data = {
			3436,	/* Code */
			10415,	/* Vendor */
			"Requested-PLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Requestor-PLMN-Identifier, UTF8String, code 3437                 */
	{
		struct dict_avp_data data = {
			3437,	/* Code */
			10415,	/* Vendor */
			"Requestor-PLMN-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Role-Of-Node, Enumerated, code 829                               */
	{
		struct dict_avp_data data = {
			829,	/* Code */
			10415,	/* Vendor */
			"Role-Of-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Role-Of-Node)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Role-Of-ProSe-Function, Enumerated, code 3438                    */
	{
		struct dict_avp_data data = {
			3438,	/* Code */
			10415,	/* Vendor */
			"Role-Of-ProSe-Function",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Role-Of-ProSe-Function)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Route-Header-Received, UTF8String, code 3403                     */
	{
		struct dict_avp_data data = {
			3403,	/* Code */
			10415,	/* Vendor */
			"Route-Header-Received",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Route-Header-Transmitted, UTF8String, code 3404                  */
	{
		struct dict_avp_data data = {
			3404,	/* Code */
			10415,	/* Vendor */
			"Route-Header-Transmitted",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Scale-Factor, Grouped, code 2059                                 */
	{
		struct dict_avp_data data = {
			2059,	/* Code */
			10415,	/* Vendor */
			"Scale-Factor",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SCEF-Address, Address, code 1317                                 */
	{
		struct dict_avp_data data = {
			1317,	/* Code */
			10415,	/* Vendor */
			"SCEF-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* SCS-Address, Address, code 3941                                  */
	{
		struct dict_avp_data data = {
			3941,	/* Code */
			10415,	/* Vendor */
			"SCS-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* SCS-AS-Address, Grouped, code 3940                               */
	{
		struct dict_avp_data data = {
			3940,	/* Code */
			10415,	/* Vendor */
			"SCS-AS-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SCS-Realm, DiameterIdentity, code 3942                           */
	{
		struct dict_avp_data data = {
			3942,	/* Code */
			10415,	/* Vendor */
			"SCS-Realm",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* SDP-Answer-Timestamp, Time, code 1275                            */
	{
		struct dict_avp_data data = {
			1275,	/* Code */
			10415,	/* Vendor */
			"SDP-Answer-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SDP-Media-Component, Grouped, code 843                           */
	{
		struct dict_avp_data data = {
			843,	/* Code */
			10415,	/* Vendor */
			"SDP-Media-Component",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SDP-Media-Description, UTF8String, code 845                      */
	{
		struct dict_avp_data data = {
			845,	/* Code */
			10415,	/* Vendor */
			"SDP-Media-Description",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SDP-Media-Name, UTF8String, code 844                             */
	{
		struct dict_avp_data data = {
			844,	/* Code */
			10415,	/* Vendor */
			"SDP-Media-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SDP-Offer-Timestamp, Time, code 1274                             */
	{
		struct dict_avp_data data = {
			1274,	/* Code */
			10415,	/* Vendor */
			"SDP-Offer-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SDP-Session-Description, UTF8String, code 842                    */
	{
		struct dict_avp_data data = {
			842,	/* Code */
			10415,	/* Vendor */
			"SDP-Session-Description",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SDP-TimeStamps, Grouped, code 1273                               */
	{
		struct dict_avp_data data = {
			1273,	/* Code */
			10415,	/* Vendor */
			"SDP-TimeStamps",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SDP-Type, Enumerated, code 2036                                  */
	{
		struct dict_avp_data data = {
			2036,	/* Code */
			10415,	/* Vendor */
			"SDP-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SDP-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Secondary-RAT-Type, OctetString, code 1304                       */
	{
		struct dict_avp_data data = {
			1304,	/* Code */
			10415,	/* Vendor */
			"Secondary-RAT-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Serving-Node-Identity, DiameterIdentity, code 3929               */
	{
		struct dict_avp_data data = {
			3929,	/* Code */
			10415,	/* Vendor */
			"Serving-Node-Identity",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, DiameterIdentity_type, NULL);
	};

	/* Served-Party-IP-Address, Address, code 848                       */
	{
		struct dict_avp_data data = {
			848,	/* Code */
			10415,	/* Vendor */
			"Served-Party-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Service-Data-Container, Grouped, code 2040                       */
	{
		struct dict_avp_data data = {
			2040,	/* Code */
			10415,	/* Vendor */
			"Service-Data-Container",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Id, UTF8String, code 855                                 */
	{
		struct dict_avp_data data = {
			855,	/* Code */
			10415,	/* Vendor */
			"Service-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Service-Information, Grouped, code 873                           */
	{
		struct dict_avp_data data = {
			873,	/* Code */
			10415,	/* Vendor */
			"Service-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Mode, Unsigned32, code 2032                              */
	{
		struct dict_avp_data data = {
			2032,	/* Code */
			10415,	/* Vendor */
			"Service-Mode",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Specific-Data, UTF8String, code 863                      */
	{
		struct dict_avp_data data = {
			863,	/* Code */
			10415,	/* Vendor */
			"Service-Specific-Data",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Service-Specific-Info, Grouped, code 1249                        */
	{
		struct dict_avp_data data = {
			1249,	/* Code */
			10415,	/* Vendor */
			"Service-Specific-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Specific-Type, Unsigned32, code 1257                     */
	{
		struct dict_avp_data data = {
			1257,	/* Code */
			10415,	/* Vendor */
			"Service-Specific-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Serving-Node-Type, Enumerated, code 2047                         */
	{
		struct dict_avp_data data = {
			2047,	/* Code */
			10415,	/* Vendor */
			"Serving-Node-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Serving-Node-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Session-Direction, Enumerated, code 2707                         */
	{
		struct dict_avp_data data = {
			2707,	/* Code */
			10415,	/* Vendor */
			"Session-Direction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Session-Direction)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SGi-PtP-Tunnelling-Method, Enumerated, code 3931                 */
	{
		struct dict_avp_data data = {
			3931,	/* Code */
			10415,	/* Vendor */
			"SGi-PtP-Tunnelling-Method",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SGi-PtP-Tunnelling-Method)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SGSN-Address, Address, code 1228                                 */
	{
		struct dict_avp_data data = {
			1228,	/* Code */
			10415,	/* Vendor */
			"SGSN-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* SGW-Address, Address, code 2067                                  */
	{
		struct dict_avp_data data = {
			2067,	/* Code */
			10415,	/* Vendor */
			"SGW-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* SGW-Change, Enumerated, code 2065                                */
	{
		struct dict_avp_data data = {
			2065,	/* Code */
			10415,	/* Vendor */
			"SGW-Change",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SGW-Change)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SIP-Method, UTF8String, code 824                                 */
	{
		struct dict_avp_data data = {
			824,	/* Code */
			10415,	/* Vendor */
			"SIP-Method",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SIP-Request-Timestamp, Time, code 834                            */
	{
		struct dict_avp_data data = {
			834,	/* Code */
			10415,	/* Vendor */
			"SIP-Request-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SIP-Request-Timestamp-Fraction, Unsigned32, code 2301            */
	{
		struct dict_avp_data data = {
			2301,	/* Code */
			10415,	/* Vendor */
			"SIP-Request-Timestamp-Fraction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SIP-Response-Timestamp, Time, code 835                           */
	{
		struct dict_avp_data data = {
			835,	/* Code */
			10415,	/* Vendor */
			"SIP-Response-Timestamp",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SIP-Response-Timestamp-Fraction, Unsigned32, code 2302           */
	{
		struct dict_avp_data data = {
			2302,	/* Code */
			10415,	/* Vendor */
			"SIP-Response-Timestamp-Fraction",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Device-Trigger-Indicator, Enumerated, code 3407               */
	{
		struct dict_avp_data data = {
			3407,	/* Code */
			10415,	/* Vendor */
			"SM-Device-Trigger-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SM-Device-Trigger-Indicator)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SM-Device-Trigger-Information, Grouped, code 3405                */
	{
		struct dict_avp_data data = {
			3405,	/* Code */
			10415,	/* Vendor */
			"SM-Device-Trigger-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Discharge-Time, Time, code 2012                               */
	{
		struct dict_avp_data data = {
			2012,	/* Code */
			10415,	/* Vendor */
			"SM-Discharge-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SM-Message-Type, Enumerated, code 2007                           */
	{
		struct dict_avp_data data = {
			2007,	/* Code */
			10415,	/* Vendor */
			"SM-Message-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SM-Message-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SM-Protocol-ID, OctetString, code 2013                           */
	{
		struct dict_avp_data data = {
			2013,	/* Code */
			10415,	/* Vendor */
			"SM-Protocol-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Sequence-Number, Unsigned32, code 3408                        */
	{
		struct dict_avp_data data = {
			3408,	/* Code */
			10415,	/* Vendor */
			"SM-Sequence-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-Service-Type, Enumerated, code 2029                           */
	{
		struct dict_avp_data data = {
			2029,	/* Code */
			10415,	/* Vendor */
			"SM-Service-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SM-Service-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SM-Status, OctetString, code 2014                                */
	{
		struct dict_avp_data data = {
			2014,	/* Code */
			10415,	/* Vendor */
			"SM-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SM-User-Data-Header, OctetString, code 2015                      */
	{
		struct dict_avp_data data = {
			2015,	/* Code */
			10415,	/* Vendor */
			"SM-User-Data-Header",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SMS-Information, Grouped, code 2000                              */
	{
		struct dict_avp_data data = {
			2000,	/* Code */
			10415,	/* Vendor */
			"SMS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SMS-Node, Enumerated, code 2016                                  */
	{
		struct dict_avp_data data = {
			2016,	/* Code */
			10415,	/* Vendor */
			"SMS-Node",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/SMS-Node)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SMS-Result, Unsigned32, code 3409                                */
	{
		struct dict_avp_data data = {
			3409,	/* Code */
			10415,	/* Vendor */
			"SMS-Result",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SMSC-Address, Address, code 2017                                 */
	{
		struct dict_avp_data data = {
			2017,	/* Code */
			10415,	/* Vendor */
			"SMSC-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Start-of-Charging, Time, code 3419                               */
	{
		struct dict_avp_data data = {
			3419,	/* Code */
			10415,	/* Vendor */
			"Start-of-Charging",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Start-Time, Time, code 2041                                      */
	{
		struct dict_avp_data data = {
			2041,	/* Code */
			10415,	/* Vendor */
			"Start-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V11.10.0 (2013-12) renamed                  */
	/* Status (2702) to Status-Code (2702), and then                    */
	/* 3GPP TS 32.299 V11.14.0 (2014-12) renamed                        */
	/* Status-Code (2702) to Status-AS-Code (2702).                     */
	/* Status-AS-Code, Enumerated, code 2702                            */
	{
		struct dict_avp_data data = {
			2702,	/* Code */
			10415,	/* Vendor */
			"Status-AS-Code",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Status-AS-Code)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Stop-Time, Time, code 2042                                       */
	{
		struct dict_avp_data data = {
			2042,	/* Code */
			10415,	/* Vendor */
			"Stop-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Submission-Time, Time, code 1202                                 */
	{
		struct dict_avp_data data = {
			1202,	/* Code */
			10415,	/* Vendor */
			"Submission-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Subscriber-Role, Enumerated, code 2033                           */
	{
		struct dict_avp_data data = {
			2033,	/* Code */
			10415,	/* Vendor */
			"Subscriber-Role",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Subscriber-Role)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Supplementary-Service, Grouped, code 2048                        */
	{
		struct dict_avp_data data = {
			2048,	/* Code */
			10415,	/* Vendor */
			"Supplementary-Service",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TAD-Identifier, Enumerated, code 2717                            */
	{
		struct dict_avp_data data = {
			2717,	/* Code */
			10415,	/* Vendor */
			"TAD-Identifier",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/TAD-Identifier)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Talk-Burst-Exchange, Grouped, code 1255                          */
	{
		struct dict_avp_data data = {
			1255,	/* Code */
			10415,	/* Vendor */
			"Talk-Burst-Exchange",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Talk-Burst-Time, Unsigned32, code 1286                           */
	{
		struct dict_avp_data data = {
			1286,	/* Code */
			10415,	/* Vendor */
			"Talk-Burst-Time",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Talk-Burst-Volume, Unsigned32, code 1287                         */
	{
		struct dict_avp_data data = {
			1287,	/* Code */
			10415,	/* Vendor */
			"Talk-Burst-Volume",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Target-IP-Address, Address, code 4412                            */
	{
		struct dict_avp_data data = {
			4412,	/* Code */
			10415,	/* Vendor */
			"Target-IP-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Tariff-Information, Grouped, code 2060                           */
	{
		struct dict_avp_data data = {
			2060,	/* Code */
			10415,	/* Vendor */
			"Tariff-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Tariff-XML, UTF8String, code 2306                                */
	{
		struct dict_avp_data data = {
			2306,	/* Code */
			10415,	/* Vendor */
			"Tariff-XML",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Teleservice, OctetString, code 3413                              */
	{
		struct dict_avp_data data = {
			3413,	/* Code */
			10415,	/* Vendor */
			"Teleservice",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Terminating-IOI, UTF8String, code 840                            */
	{
		struct dict_avp_data data = {
			840,	/* Code */
			10415,	/* Vendor */
			"Terminating-IOI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Time-First-Reception, Time, code 3466                            */
	{
		struct dict_avp_data data = {
			3466,	/* Code */
			10415,	/* Vendor */
			"Time-First-Reception",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Time-First-Transmission, Time, code 3467                         */
	{
		struct dict_avp_data data = {
			3467,	/* Code */
			10415,	/* Vendor */
			"Time-First-Transmission",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Time-First-Usage, Time, code 2043                                */
	{
		struct dict_avp_data data = {
			2043,	/* Code */
			10415,	/* Vendor */
			"Time-First-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Time-Indicator, Unsigned32, code 3911                            */
	{
		struct dict_avp_data data = {
			3911,	/* Code */
			10415,	/* Vendor */
			"Time-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Time-Last-Usage, Time, code 2044                                 */
	{
		struct dict_avp_data data = {
			2044,	/* Code */
			10415,	/* Vendor */
			"Time-Last-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* Time-Quota-Mechanism, Grouped, code 1270                         */
	{
		struct dict_avp_data data = {
			1270,	/* Code */
			10415,	/* Vendor */
			"Time-Quota-Mechanism",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Time-Quota-Threshold, Unsigned32, code 868                       */
	{
		struct dict_avp_data data = {
			868,	/* Code */
			10415,	/* Vendor */
			"Time-Quota-Threshold",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Time-Quota-Type, Enumerated, code 1271                           */
	{
		struct dict_avp_data data = {
			1271,	/* Code */
			10415,	/* Vendor */
			"Time-Quota-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Time-Quota-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Time-Stamps, Grouped, code 833                                   */
	{
		struct dict_avp_data data = {
			833,	/* Code */
			10415,	/* Vendor */
			"Time-Stamps",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Time-Usage, Unsigned32, code 2045                                */
	{
		struct dict_avp_data data = {
			2045,	/* Code */
			10415,	/* Vendor */
			"Time-Usage",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TLTRI, Unsigned32, code 1318                                     */
	{
		struct dict_avp_data data = {
			1318,	/* Code */
			10415,	/* Vendor */
			"TLTRI",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Token-Text, UTF8String, code 1215                                */
	{
		struct dict_avp_data data = {
			1215,	/* Code */
			10415,	/* Vendor */
			"Token-Text",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Traffic-Data-Volumes, Grouped, code 2046                         */
	{
		struct dict_avp_data data = {
			2046,	/* Code */
			10415,	/* Vendor */
			"Traffic-Data-Volumes",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Transcoder-Inserted-Indication, Enumerated, code 2605            */
	{
		struct dict_avp_data data = {
			2605,	/* Code */
			10415,	/* Vendor */
			"Transcoder-Inserted-Indication",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Transcoder-Inserted-Indication)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Transit-IOI-List, UTF8String, code 2701                          */
	{
		struct dict_avp_data data = {
			2701,	/* Code */
			10415,	/* Vendor */
			"Transit-IOI-List",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Transmitter-Info, Grouped, code 3468                             */
	{
		struct dict_avp_data data = {
			3468,	/* Code */
			10415,	/* Vendor */
			"Transmitter-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trigger, Grouped, code 1264                                      */
	{
		struct dict_avp_data data = {
			1264,	/* Code */
			10415,	/* Vendor */
			"Trigger",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Trigger-Type, Enumerated, code 870                               */
	{
		struct dict_avp_data data = {
			870,	/* Code */
			10415,	/* Vendor */
			"Trigger-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Trigger-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Trunk-Group-Id, Grouped, code 851                                */
	{
		struct dict_avp_data data = {
			851,	/* Code */
			10415,	/* Vendor */
			"Trunk-Group-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* TWAG-Address, Address, code 3903                                 */
	{
		struct dict_avp_data data = {
			3903,	/* Code */
			10415,	/* Vendor */
			"TWAG-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* TWAN-User-Location-Info, Grouped, code 2714                      */
	{
		struct dict_avp_data data = {
			2714,	/* Code */
			10415,	/* Vendor */
			"TWAN-User-Location-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Type-Number, Enumerated, code 1204                               */
	{
		struct dict_avp_data data = {
			1204,	/* Code */
			10415,	/* Vendor */
			"Type-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Type-Number)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* UNI-PDU-CP-Only-Flag, Enumerated, code 3932                      */
	{
		struct dict_avp_data data = {
			3932,	/* Code */
			10415,	/* Vendor */
			"UNI-PDU-CP-Only-Flag",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/UNI-PDU-CP-Only-Flag)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Unit-Cost, Grouped, code 2061                                    */
	{
		struct dict_avp_data data = {
			2061,	/* Code */
			10415,	/* Vendor */
			"Unit-Cost",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Unit-Quota-Threshold, Unsigned32, code 1226                      */
	{
		struct dict_avp_data data = {
			1226,	/* Code */
			10415,	/* Vendor */
			"Unit-Quota-Threshold",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Unused-Quota-Timer, Unsigned32, code 4407                        */
	{
		struct dict_avp_data data = {
			4407,	/* Code */
			10415,	/* Vendor */
			"Unused-Quota-Timer",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Usage-Information-Report-Sequence-Number, Integer32, code 3439   */
	{
		struct dict_avp_data data = {
			3439,	/* Code */
			10415,	/* Vendor */
			"Usage-Information-Report-Sequence-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* User-CSG-Information, Grouped, code 2319                         */
	{
		struct dict_avp_data data = {
			2319,	/* Code */
			10415,	/* Vendor */
			"User-CSG-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* User-Participating-Type, Enumerated, code 1279                   */
	{
		struct dict_avp_data data = {
			1279,	/* Code */
			10415,	/* Vendor */
			"User-Participating-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/User-Participating-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* User-Session-Id, UTF8String, code 830                            */
	{
		struct dict_avp_data data = {
			830,	/* Code */
			10415,	/* Vendor */
			"User-Session-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* UWAN-User-Location-Info, Grouped, code 3918                      */
	{
		struct dict_avp_data data = {
			3918,	/* Code */
			10415,	/* Vendor */
			"UWAN-User-Location-Info",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Variable-Part, Grouped, code 3907                                */
	{
		struct dict_avp_data data = {
			3907,	/* Code */
			10415,	/* Vendor */
			"Variable-Part",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Variable-Part-Order, Unsigned32, code 3908                       */
	{
		struct dict_avp_data data = {
			3908,	/* Code */
			10415,	/* Vendor */
			"Variable-Part-Order",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Variable-Part-Type, Unsigned32, code 3909                        */
	{
		struct dict_avp_data data = {
			3909,	/* Code */
			10415,	/* Vendor */
			"Variable-Part-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Variable-Part-Value, UTF8String, code 3910                       */
	{
		struct dict_avp_data data = {
			3910,	/* Code */
			10415,	/* Vendor */
			"Variable-Part-Value",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* VCS-Information, Grouped, code 3410                              */
	{
		struct dict_avp_data data = {
			3410,	/* Code */
			10415,	/* Vendor */
			"VCS-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* VLR-Number, OctetString, code 3420                               */
	{
		struct dict_avp_data data = {
			3420,	/* Code */
			10415,	/* Vendor */
			"VLR-Number",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Volume-Quota-Threshold, Unsigned32, code 869                     */
	{
		struct dict_avp_data data = {
			869,	/* Code */
			10415,	/* Vendor */
			"Volume-Quota-Threshold",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WAG-Address (890). */
	/* WAG-Address, Address, code 890                                   */
	{
		struct dict_avp_data data = {
			890,	/* Code */
			10415,	/* Vendor */
			"WAG-Address",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WAG-PLMN-Id (891). */
	/* WAG-PLMN-Id, OctetString, code 891                               */
	{
		struct dict_avp_data data = {
			891,	/* Code */
			10415,	/* Vendor */
			"WAG-PLMN-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WLAN-Information (875). */
	/* WLAN-Information, Grouped, code 875                              */
	{
		struct dict_avp_data data = {
			875,	/* Code */
			10415,	/* Vendor */
			"WLAN-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WLAN-Operator-Id, Grouped, code 1306                             */
	{
		struct dict_avp_data data = {
			1306,	/* Code */
			10415,	/* Vendor */
			"WLAN-Operator-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* WLAN-Operator-Name, UTF8String, code 1307                        */
	{
		struct dict_avp_data data = {
			1307,	/* Code */
			10415,	/* Vendor */
			"WLAN-Operator-Name",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* WLAN-PLMN-Id, UTF8String, code 1308                              */
	{
		struct dict_avp_data data = {
			1308,	/* Code */
			10415,	/* Vendor */
			"WLAN-PLMN-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WLAN-Radio-Container (892). */
	/* WLAN-Radio-Container, Grouped, code 892                          */
	{
		struct dict_avp_data data = {
			892,	/* Code */
			10415,	/* Vendor */
			"WLAN-Radio-Container",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WLAN-Session-Id (1246). */
	/* WLAN-Session-Id, UTF8String, code 1246                           */
	{
		struct dict_avp_data data = {
			1246,	/* Code */
			10415,	/* Vendor */
			"WLAN-Session-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WLAN-Technology (893). */
	/* WLAN-Technology, Unsigned32, code 893                            */
	{
		struct dict_avp_data data = {
			893,	/* Code */
			10415,	/* Vendor */
			"WLAN-Technology",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Note: 3GPP TS 32.299 V12.6.0 (2014-09) removed WLAN-UE-Local-IPAddress (894). */
	/* WLAN-UE-Local-IPAddress, Address, code 894                       */
	{
		struct dict_avp_data data = {
			894,	/* Code */
			10415,	/* Vendor */
			"WLAN-UE-Local-IPAddress",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Address_type, NULL);
	};

	/*==================================================================*/
	/* OMA DDS Charging_Data V1.0 20110201-A                            */
	/* From OMA OMA-DDS-Charging_Data-V1_0-20110201-A.pdf               */
	/*==================================================================*/

	/* Application-Server-Id, UTF8String, code 2101, section 8.4        */
	{
		struct dict_avp_data data = {
			2101,	/* Code */
			10415,	/* Vendor */
			"Application-Server-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Note: Application-Service-Type (2102) corrected from UTF8String to Enumerated. */
	/* Application-Service-Type, Enumerated, code 2102, section 8.4     */
	{
		struct dict_avp_data data = {
			2102,	/* Code */
			10415,	/* Vendor */
			"Application-Service-Type",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(3GPP/Application-Service-Type)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* Note: Application–Session–Id (2103) corrected from UTF8String to Unsigned32. */
	/* Application-Session-Id, Unsigned32, code 2103, section 8.4       */
	{
		struct dict_avp_data data = {
			2103,	/* Code */
			10415,	/* Vendor */
			"Application-Session-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Content-ID, UTF8String, code 2116, section 8.4                   */
	{
		struct dict_avp_data data = {
			2116,	/* Code */
			10415,	/* Vendor */
			"Content-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* Content-provider-ID, UTF8String, code 2117, section 8.4          */
	{
		struct dict_avp_data data = {
			2117,	/* Code */
			10415,	/* Vendor */
			"Content-provider-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* DCD-Information, Grouped, code 2115, section 8.5.5               */
	{
		struct dict_avp_data data = {
			2115,	/* Code */
			10415,	/* Vendor */
			"DCD-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Delivery-Status, UTF8String, code 2104, section 8.4              */
	{
		struct dict_avp_data data = {
			2104,	/* Code */
			10415,	/* Vendor */
			"Delivery-Status",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* IM-Information, Grouped, code 2110, section 8.5.6                */
	{
		struct dict_avp_data data = {
			2110,	/* Code */
			10415,	/* Vendor */
			"IM-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Messages-Successfully-Exploded, Unsigned32, code 2111, section 8.4 */
	{
		struct dict_avp_data data = {
			2111,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Messages-Successfully-Exploded",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Number-Of-Messages-Successfully-Sent, Unsigned32, code 2112, section 8.4 */
	{
		struct dict_avp_data data = {
			2112,	/* Code */
			10415,	/* Vendor */
			"Number-Of-Messages-Successfully-Sent",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Service-Generic-Information, Grouped, code 1256, section 8.5.10  */
	{
		struct dict_avp_data data = {
			1256,	/* Code */
			10415,	/* Vendor */
			"Service-Generic-Information",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Total-Number-Of-Messages-Exploded, Unsigned32, code 2113, section 8.4 */
	{
		struct dict_avp_data data = {
			2113,	/* Code */
			10415,	/* Vendor */
			"Total-Number-Of-Messages-Exploded",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* Total-Number-Of-Messages-Sent, Unsigned32, code 2114, section 8.4 */
	{
		struct dict_avp_data data = {
			2114,	/* Code */
			10415,	/* Vendor */
			"Total-Number-Of-Messages-Sent",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* 3GPP2 X.S0057-0 v2.0                                             */
	/* From 3GPP2 X.S0057-0_v2.0_091215.pdf                             */
	/*==================================================================*/

	/* Note: 3GPP2-BSID (9010) corrected from OctetString to UTF8String. */
	/* 3GPP2-BSID, UTF8String, code 9010, section 5.6.2.1.1             */
	{
		struct dict_avp_data data = {
			9010,	/* Code */
			5535,	/* Vendor */
			"3GPP2-BSID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/*==================================================================*/
	/* End of generated data.                                           */
	/*==================================================================*/

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
