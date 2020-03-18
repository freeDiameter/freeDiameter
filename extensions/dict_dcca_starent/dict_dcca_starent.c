/* 
 * Dictionary definitions of objects specified in DCCA by Starent.
 */
#include <freeDiameter/extension.h>


/* The content of this file follows the same structure as dict_base_proto.c */

#define CHECK_dict_new( _type, _data, _parent, _ref )	\
	CHECK_FCT(  fd_dict_new( fd_g_config->cnf_dict, (_type), (_data), (_parent), (_ref))  );

#define CHECK_dict_search( _type, _criteria, _what, _result )	\
	CHECK_FCT(  fd_dict_search( fd_g_config->cnf_dict, (_type), (_criteria), (_what), (_result), ENOENT) );

struct local_rules_definition {
	char 			*avp_name;
	enum rule_position	position;
	int 			min;
	int			max;
};

#define RULE_ORDER( _position ) ((((_position) == RULE_FIXED_HEAD) || ((_position) == RULE_FIXED_TAIL)) ? 1 : 0 )

/* Attention! This version of the macro uses AVP_BY_NAME_ALL_VENDORS, in contrast to most other copies! */
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
			AVP_BY_NAME_ALL_VENDORS, 							\
			(_rulearray)[__ar].avp_name, 							\
			&__data.rule_avp, 0 ) );							\
		if ( !__data.rule_avp ) {								\
			TRACE_DEBUG(INFO, "AVP Not found: '%s'", (_rulearray)[__ar].avp_name );		\
			return ENOENT;									\
		}											\
		CHECK_FCT_DO( fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &__data, _parent, NULL),	\
			{							        		\
				TRACE_DEBUG(INFO, "Error on rule with AVP '%s'",      			\
					 (_rulearray)[__ar].avp_name );		      			\
				return EINVAL;					      			\
			} );							      			\
	}									      			\
}

#define enumval_def_u32( _val_, _str_ ) \
		{ _str_, 		{ .u32 = _val_ }}

#define enumval_def_os( _len_, _val_, _str_ ) \
		{ _str_, 		{ .os = { .data = (unsigned char *)_val_, .len = _len_ }}}


static int dict_dcca_starent_entry(char * conffile)
{
	/* Applications section */
	{		
                /* Create the vendors */
                {
                        struct dict_vendor_data vendor_data = { 8164, "Starent" };
                        CHECK_FCT(fd_dict_new(fd_g_config->cnf_dict, DICT_VENDOR, &vendor_data, NULL, NULL));
                }                                
  
	}
	

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
	/* Start of generated data.                                         */
	/*                                                                  */
	/* The following is created automatically with:                     */
	/*     csv_to_fd -p fdc dict_dcca_starent.csv                       */
	/* Changes will be lost during the next update.                     */
	/* Do not modify; modify the source .csv file instead.              */
	/*==================================================================*/

	/*==================================================================*/
	/* Cisco ASR 5000 Series AAA Interface                              */
	/* Administration and Reference                                     */
	/* Release 8.x and 9.0                                              */
	/* Last Updated June 30, 2010                                       */
	/* updated using v15 docs from Jan 2014                             */
	/* www.cisco.com/c/dam/en/us/td/docs/wireless/asr_5000/15-0/15-0-AAA-Reference.pdf */
	/*==================================================================*/

	/* SN-Volume-Quota-Threshold, Unsigned32, code 501                  */
	{
		struct dict_avp_data data = {
			501,	/* Code */
			8164,	/* Vendor */
			"SN-Volume-Quota-Threshold",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Unit-Quota-Threshold, Unsigned32, code 502                    */
	{
		struct dict_avp_data data = {
			502,	/* Code */
			8164,	/* Vendor */
			"SN-Unit-Quota-Threshold",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Time-Quota-Threshold, Unsigned32, code 503                    */
	{
		struct dict_avp_data data = {
			503,	/* Code */
			8164,	/* Vendor */
			"SN-Time-Quota-Threshold",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Total-Used-Service-Unit, Grouped, code 504                    */
	{
		struct dict_avp_data data = {
			504,	/* Code */
			8164,	/* Vendor */
			"SN-Total-Used-Service-Unit",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Absolute-Validity-Time, Time, code 505                        */
	{
		struct dict_avp_data data = {
			505,	/* Code */
			8164,	/* Vendor */
			"SN-Absolute-Validity-Time",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SN-Bandwidth-Control, Enumerated, code 512                       */
	{
		struct dict_avp_data data = {
			512,	/* Code */
			8164,	/* Vendor */
			"SN-Bandwidth-Control",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(Starent/SN-Bandwidth-Control)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SN-Transparent-Data, OctetString, code 513                       */
	{
		struct dict_avp_data data = {
			513,	/* Code */
			8164,	/* Vendor */
			"SN-Transparent-Data",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Traffic-Policy, UTF8String, code 514                          */
	{
		struct dict_avp_data data = {
			514,	/* Code */
			8164,	/* Vendor */
			"SN-Traffic-Policy",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SN-Firewall-Policy, UTF8String, code 515                         */
	{
		struct dict_avp_data data = {
			515,	/* Code */
			8164,	/* Vendor */
			"SN-Firewall-Policy",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SN-Usage-Monitoring-Control, Grouped, code 517                   */
	{
		struct dict_avp_data data = {
			517,	/* Code */
			8164,	/* Vendor */
			"SN-Usage-Monitoring-Control",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Monitoring-Key, Unsigned32, code 518                          */
	{
		struct dict_avp_data data = {
			518,	/* Code */
			8164,	/* Vendor */
			"SN-Monitoring-Key",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Usage-Volume, Unsigned64, code 519                            */
	{
		struct dict_avp_data data = {
			519,	/* Code */
			8164,	/* Vendor */
			"SN-Usage-Volume",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED64	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Service-Flow-Detection, Enumerated, code 520                  */
	{
		struct dict_avp_data data = {
			520,	/* Code */
			8164,	/* Vendor */
			"SN-Service-Flow-Detection",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(Starent/SN-Service-Flow-Detection)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SN-Usage-Monitoring, Enumerated, code 521                        */
	{
		struct dict_avp_data data = {
			521,	/* Code */
			8164,	/* Vendor */
			"SN-Usage-Monitoring",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_INTEGER32	/* base type of data */
		};
		struct dict_object	*type;
		struct dict_type_data	 tdata = { AVP_TYPE_INTEGER32, "Enumerated(Starent/SN-Usage-Monitoring)", NULL, NULL, NULL };
		CHECK_dict_new(DICT_TYPE, &tdata, NULL, &type);
		CHECK_dict_new(DICT_AVP, &data, type, NULL);
	};

	/* SN-Session-Start-Indicator, OctetString, code 522                */
	{
		struct dict_avp_data data = {
			522,	/* Code */
			8164,	/* Vendor */
			"SN-Session-Start-Indicator",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Phase0-PSAPName, UTF8String, code 523                         */
	{
		struct dict_avp_data data = {
			523,	/* Code */
			8164,	/* Vendor */
			"SN-Phase0-PSAPName",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SN-Charging-Id, OctetString, code 525                            */
	{
		struct dict_avp_data data = {
			525,	/* Code */
			8164,	/* Vendor */
			"SN-Charging-Id",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Remaining-Service-Unit, Grouped, code 526                     */
	{
		struct dict_avp_data data = {
			526,	/* Code */
			8164,	/* Vendor */
			"SN-Remaining-Service-Unit",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_GROUPED	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Service-Start-Timestamp, Time, code 527                       */
	{
		struct dict_avp_data data = {
			527,	/* Code */
			8164,	/* Vendor */
			"SN-Service-Start-Timestamp",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, Time_type, NULL);
	};

	/* SN-Rulebase-Id, UTF8String, code 528                             */
	{
		struct dict_avp_data data = {
			528,	/* Code */
			8164,	/* Vendor */
			"SN-Rulebase-Id",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SN-CF-Policy-ID, Unsigned32, code 529                            */
	{
		struct dict_avp_data data = {
			529,	/* Code */
			8164,	/* Vendor */
			"SN-CF-Policy-ID",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_UNSIGNED32	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Charging-Collection-Function-Name, UTF8String, code 530       */
	{
		struct dict_avp_data data = {
			530,	/* Code */
			8164,	/* Vendor */
			"SN-Charging-Collection-Function-Name",	/* Name */
			AVP_FLAG_VENDOR,	/* Fixed flags */
			AVP_FLAG_VENDOR,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, UTF8String_type, NULL);
	};

	/* SN-Fast-Reauth-Username, OctetString, code 11010                 */
	{
		struct dict_avp_data data = {
			11010,	/* Code */
			8164,	/* Vendor */
			"SN-Fast-Reauth-Username",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/* SN-Pseudonym-Username, OctetString, code 11011                   */
	{
		struct dict_avp_data data = {
			11011,	/* Code */
			8164,	/* Vendor */
			"SN-Pseudonym-Username",	/* Name */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flags */
			AVP_FLAG_VENDOR |AVP_FLAG_MANDATORY,	/* Fixed flag values */
			AVP_TYPE_OCTETSTRING	/* base type of data */
		};
		CHECK_dict_new(DICT_AVP, &data, NULL, NULL);
	};

	/*==================================================================*/
	/* End of generated data.                                           */
	/*==================================================================*/

	/* Rules section */

	/* SN-Remaining-Service-Unit */
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 8164;
		vpa.avp_name = "SN-Remaining-Service-Unit";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] = {
			{  "Tariff-Change-Usage",	RULE_OPTIONAL,	-1, 1 },
			{  "CC-Time",	       		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Total-Octets",		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Input-Octets",		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Output-Octets",		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Service-Specific-Units",	RULE_OPTIONAL,	-1, 1 },
			{  "Reporting-Reason",		RULE_OPTIONAL,	-1, 1 }
		};
		PARSE_loc_rules( rules, rule_avp );
	}

	/* SN-Total-Used-Service-Unit */
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 8164;
		vpa.avp_name = "SN-Total-Used-Service-Unit";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] = {
			{  "Tariff-Change-Usage",	RULE_OPTIONAL,	-1, 1 },
			{  "CC-Time",	       		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Total-Octets",		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Input-Octets",		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Output-Octets",		RULE_OPTIONAL,	-1, 1 },
			{  "CC-Service-Specific-Units",	RULE_OPTIONAL,	-1, 1 },
			{  "Reporting-Reason",		RULE_OPTIONAL,	-1, 1 }
		};
		PARSE_loc_rules( rules, rule_avp );
	}

	/* SN-Usage-Monitoring-Control */
	{
		struct dict_object *rule_avp;
		struct dict_avp_request vpa;
		vpa.avp_vendor = 8164;
		vpa.avp_name = "SN-Usage-Monitoring-Control";
		CHECK_dict_search(DICT_AVP, AVP_BY_NAME_AND_VENDOR, &vpa, &rule_avp);
		struct local_rules_definition rules[] = {
			{  "SN-Monitoring-Key",		RULE_OPTIONAL,	-1, 1 },
			{  "SN-Usage-Monitoring",	RULE_OPTIONAL,	-1, 1 },
			{  "SN-Usage-Volume",		RULE_OPTIONAL,	-1, 1 },
		};
		PARSE_loc_rules( rules, rule_avp );
	}

	LOG_D( "Extension 'Dictionary definitions for DCCA Starent' initialized");
	return 0;
}

EXTENSION_ENTRY("dict_dcca_starent", dict_dcca_starent_entry, "dict_dcca_3gpp");
