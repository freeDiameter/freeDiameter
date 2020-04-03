/* 
 * Dictionary definitions of objects specified in DCCA by Starent.
 */
#include <freeDiameter/extension.h>


/* The content of this file follows the same structure as dict_base_proto.c */

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
			LOG_E( "AVP Not found: '%s'", (_rulearray)[__ar].avp_name );			\
			return ENOENT;									\
		}											\
		CHECK_FCT_DO( fd_dict_new( fd_g_config->cnf_dict, DICT_RULE, &__data, _parent, NULL),	\
			{							        		\
				LOG_E( "Error on rule with AVP '%s'",      				\
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
	
	extern int add_avps();
	CHECK_FCT( add_avps() );

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
