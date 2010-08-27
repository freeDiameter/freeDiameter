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

/* This file contains all support functions to parse, create, and manipulate RADIUS messages. Other 
modules do not need to "know" the actual representation of RADIUS messages on the network. They only 
receive the logical view as exposed in the rgw.h file. This file extends the content of the radius.c 
file functions (from hostap project).*/

#include "rgw.h"

/* Destroy a message */
void rgw_msg_free(struct rgw_radius_msg_meta ** msg)
{
	if (!msg || !*msg)
		return;
	
	radius_msg_free(&(*msg)->radius);
	free(*msg);
	*msg = NULL;
}

/* This function creates a rgw_radius_msg_meta structure after parsing a RADIUS buffer */
int rgw_msg_parse(unsigned char * buf, size_t len, struct rgw_radius_msg_meta ** msg)
{
	struct radius_msg * temp_msg = NULL;
	
	TRACE_ENTRY("%p %g %p", buf, len, msg);
	
	CHECK_PARAMS( buf && len && msg );
	
	*msg = NULL;
	
	/* Parse the RADIUS message */
	temp_msg = radius_msg_parse(buf, len);
	if (temp_msg == NULL) {
		TRACE_DEBUG(INFO, "Error parsing the RADIUS message, discarding");
		return EINVAL;
	}
	
	/* Now alloc space for the meta-data */
	CHECK_MALLOC( *msg = realloc(temp_msg, sizeof(struct rgw_radius_msg_meta)) );
	
	/* Clear memory after the parsed data */
	memset( &(*msg)->radius + 1, 0, sizeof(struct rgw_radius_msg_meta) - sizeof(struct radius_msg) );
	
	return 0;
}

/* Dump a message (inspired from radius_msg_dump) -- can be used safely with a struct radius_msg as parameter (we don't dump the metadata) */
void rgw_msg_dump(struct rgw_radius_msg_meta * msg)
{
	unsigned char *auth;
	size_t i;
	if (! TRACE_BOOL(FULL) )
		return;
	
	auth =  &(msg->radius.hdr->authenticator[0]);
	
	fd_log_debug("------ RADIUS msg dump -------\n");
	fd_log_debug(" id  : 0x%02hhx, code : %hhd (%s)\n", msg->radius.hdr->identifier, msg->radius.hdr->code, rgw_msg_code_str(msg->radius.hdr->code));
	fd_log_debug(" auth: %02hhx %02hhx %02hhx %02hhx  %02hhx %02hhx %02hhx %02hhx\n",
			auth[0], auth[1], auth[2], auth[3], 
			auth[4], auth[5], auth[6], auth[7]);
	fd_log_debug("       %02hhx %02hhx %02hhx %02hhx  %02hhx %02hhx %02hhx %02hhx\n",
			auth[8],  auth[9],  auth[10], auth[11], 
			auth[12], auth[13], auth[14], auth[15]);
	for (i = 0; i < msg->radius.attr_used; i++) {
		struct radius_attr_hdr *attr = (struct radius_attr_hdr *)(msg->radius.buf + msg->radius.attr_pos[i]);
		fd_log_debug("    - Type: 0x%02hhx (%s)\n       Len: %-3hhu", attr->type, rgw_msg_attrtype_str(attr->type), attr->length);
		radius_msg_dump_attr_val(attr);
	}
	fd_log_debug("-----------------------------\n");
}

