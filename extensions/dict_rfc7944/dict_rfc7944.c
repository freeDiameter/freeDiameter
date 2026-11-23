/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Mahdi Hamidpour <m.coder@live.com>								 *
*													 *
* Copyright (c) 2026, Mahdi Hamidpour									 *
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
* * Neither the name of the copyright holder nor the  						 *
*   names of its contributors may be used to endorse or 						 *
*   promote products derived from this software without 						 *
*   specific prior written permission.									 *
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

/*
 * Dictionary definitions for the Diameter Routing Message Priority (DRMP) AVP,
 * as defined in RFC 7944.
 *
 *	NAME	AVP	RFC		TYPE
 *	DRMP	301	7944 9.1	Enumerated
 *
 * Flag rules (RFC 7944 section 9.2): the V bit MUST NOT be set. The RFC sets no
 * requirement on the M bit, so it is left free.
 *
 * Values: PRIORITY_0 (0, highest priority) .. PRIORITY_15 (15, lowest priority).
 *
 * Most Diameter commands accept additional optional AVPs (*[ AVP ]), so this
 * extension does not modify the rules of any existing command.
 */

#include <freeDiameter/extension.h>

#define CHECK_dict_new( _type, _data, _parent, _ref )	\
	CHECK_FCT(  fd_dict_new( fd_g_config->cnf_dict, (_type), (_data), (_parent), (_ref))  );

#define CHECK_dict_search( _type, _criteria, _what, _result )	\
	CHECK_FCT(  fd_dict_search( fd_g_config->cnf_dict, (_type), (_criteria), (_what), (_result), ENOENT) );

static int dict_rfc7944_init(char * conffile)
{
	struct dict_object * avp = NULL;

	TRACE_ENTRY("%p", conffile);

	/* Do not fail if another component (e.g. an application) already defined DRMP */
	{
		struct dict_avp_request req = { 0, 301, NULL };
		if (fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_CODE_AND_VENDOR, &req, &avp, ENOENT) == 0) {
			LOG_N( "Dictionary Extension 'Diameter Routing Message Priority (RFC 7944)': DRMP (AVP code 301) is already defined, skipping");
			return 0;
		}
	}

	/* DRMP - RFC 7944 */
	{
		/*
			The DRMP (AVP code 301) is of type Enumerated. The value of the
			AVP indicates the routing message priority for the message.
			PRIORITY_0 is the highest priority and PRIORITY_15 the lowest.
		*/
		struct dict_object	*	type;
		struct dict_type_data		tdata = { AVP_TYPE_INTEGER32, "Enumerated(DRMP)", NULL, NULL, NULL };
		struct dict_enumval_data	t[] = {
			{ "PRIORITY_0",		{ .i32 =  0 }},
			{ "PRIORITY_1",		{ .i32 =  1 }},
			{ "PRIORITY_2",		{ .i32 =  2 }},
			{ "PRIORITY_3",		{ .i32 =  3 }},
			{ "PRIORITY_4",		{ .i32 =  4 }},
			{ "PRIORITY_5",		{ .i32 =  5 }},
			{ "PRIORITY_6",		{ .i32 =  6 }},
			{ "PRIORITY_7",		{ .i32 =  7 }},
			{ "PRIORITY_8",		{ .i32 =  8 }},
			{ "PRIORITY_9",		{ .i32 =  9 }},
			{ "PRIORITY_10",	{ .i32 = 10 }},
			{ "PRIORITY_11",	{ .i32 = 11 }},
			{ "PRIORITY_12",	{ .i32 = 12 }},
			{ "PRIORITY_13",	{ .i32 = 13 }},
			{ "PRIORITY_14",	{ .i32 = 14 }},
			{ "PRIORITY_15",	{ .i32 = 15 }}
		};
		struct dict_avp_data	data = {
				301,				/* Code */
				0,				/* Vendor */
				"DRMP",				/* Name */
				AVP_FLAG_VENDOR,		/* Fixed flags */
				0,				/* Fixed flag values */
				AVP_TYPE_INTEGER32		/* base type of data */
				};
		size_t i;

		/* Create the Enumerated type, its values, and then the AVP */
		CHECK_dict_new( DICT_TYPE, &tdata, NULL, &type);
		for (i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
			CHECK_dict_new( DICT_ENUMVAL, &t[i], type, NULL);
		}
		CHECK_dict_new( DICT_AVP, &data, type, NULL);
	}

	LOG_D( "Dictionary Extension 'Diameter Routing Message Priority (RFC 7944)' initialized");
	return 0;
}

EXTENSION_ENTRY("dict_rfc7944", dict_rfc7944_init);
