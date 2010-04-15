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

/* Check if the message has a valid authenticator, and update the meta-data accordingly */
int rgw_msg_auth_check(struct rgw_radius_msg_meta * msg, struct rgw_client * cli, uint8_t * req_auth)
{
	unsigned char * key;
	size_t keylen;
	int count;
	
	TRACE_ENTRY("%p %p %p", msg, cli, req_auth);
	
	CHECK_PARAMS(msg && cli);
	
	CHECK_FCT(rgw_clients_getkey(cli, &key, &keylen));
	
	count = radius_msg_count_attr(&msg->radius, RADIUS_ATTR_MESSAGE_AUTHENTICATOR, 0);
	if (count > 1) {
		TRACE_DEBUG(INFO, "Too many Message-Authenticator attributes (%d), discarding message.", count);
		return EINVAL;
	}
	if (count == 0) {
		TRACE_DEBUG(FULL, "Message does not contain a Message-Authenticator attributes.");
		msg->valid_mac = 0;
	} else {
		if (radius_msg_verify_msg_auth( &msg->radius, key, keylen, req_auth )) {
			TRACE_DEBUG(INFO, "Invalid Message-Authenticator received, discarding message.");
			return EINVAL;
		}
		msg->valid_mac = 1;
	}
	
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
		fd_log_debug("    - len:%3hhu, type:0x%02hhx (%s)\n", attr->length, attr->type, rgw_msg_attrtype_str(attr->type));
		/* If we need to dump the value, it's better to call directly radius_msg_dump instead... */
	}
	fd_log_debug("-----------------------------\n");
}

static struct dict_object * cache_sess_id = NULL;
static struct dict_object * cache_dest_host = NULL;
static struct dict_object * cache_dest_realm = NULL;
static struct dict_object * cache_orig_host = NULL;
static struct dict_object * cache_orig_realm = NULL;

int rgw_msg_init(void)
{
	TRACE_ENTRY();
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Session-Id", &cache_sess_id, ENOENT) );
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Host", &cache_dest_host, ENOENT) );
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Realm", &cache_dest_realm, ENOENT) );
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &cache_orig_host, ENOENT) );
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Realm", &cache_orig_realm, ENOENT) );
	return 0;
}

/* Create a msg with origin-host & realm, and session-id, and a session object from a RADIUS request message */
int rgw_msg_create_base(struct rgw_radius_msg_meta * msg, struct rgw_client * cli, struct session ** session, struct msg ** diam)
{
	int idx, i;
	const char * prefix = "Diameter/";
	size_t pref_len;
	char * dh = NULL;
	size_t dh_len = 0;
	char * dr = NULL;
	size_t dr_len = 0;
	char * si = NULL;
	size_t si_len = 0;
	char * un = NULL;
	size_t un_len = 0;
	
	char * fqdn;
	char * realm;
	char * sess_str = NULL;
	
	struct avp *avp = NULL;
	union avp_value avp_val;
	
	TRACE_ENTRY("%p %p %p %p", msg, cli, session, diam);
	CHECK_PARAMS( msg && cli && session && (*session == NULL) && diam && (*diam == NULL) );
	
	pref_len = strlen(prefix);
	
	/* Is there a State attribute with prefix "Diameter/" in the message? (in that case: Diameter/Destination-Host/Destination-Realm/Session-Id) */
	/* NOTE: RFC4005 says "Origin-Host" here, but it's not coherent with the rules for answers. Destination-Host makes more sense */
	/* Is there a Class attribute with prefix "Diameter/" in the message? (in that case: Diameter/Session-Id) */
	for (idx = 0; idx < msg->radius.attr_used; idx++) {
		struct radius_attr_hdr * attr = (struct radius_attr_hdr *)(msg->radius.buf + msg->radius.attr_pos[idx]);
		char * attr_val = (char *)(attr + 1);
		size_t attr_len = attr->length - sizeof(struct radius_attr_hdr);
		
		if ((attr->type == RADIUS_ATTR_USER_NAME) 
				&& attr_len) {
			TRACE_DEBUG(ANNOYING, "Found a User-Name attribute: '%.*s'", attr_len, attr_val);
			un = attr_val;
			un_len = attr_len;
			continue;
		}
		
		if ((attr->type == RADIUS_ATTR_STATE) 
				&& (attr_len > pref_len + 5 /* for the '/'s and non empty strings */ ) 
				&& ! strncmp(attr_val, prefix, pref_len)) { /* should we make it strncasecmp? */
			int i, start;
		
			TRACE_DEBUG(ANNOYING, "Found a State attribute with '%s' prefix (attr #%d).", prefix, idx);

			/* Now parse the value and check its content is valid. Unfortunately we cannot use strchr here since strings are not \0-terminated */

			i = start = pref_len;
			dh = attr_val + i;
			for (; (i < attr_len - 2) && (attr_val[i] != '/'); i++) /* loop */;
			if ( i >= attr_len - 2 ) continue; /* the attribute format is not good */
			dh_len = i - start;

			start = ++i;
			dr = attr_val + i;
			for (; (i < attr_len - 1) && (attr_val[i] != '/'); i++) /* loop */;
			if ( i >= attr_len - 1 ) continue; /* the attribute format is not good */
			dr_len = i - start;

			i++;
			si = attr_val + i;
			si_len = attr_len - i;

			TRACE_DEBUG(ANNOYING, "Attribute parsed successfully: DH:'%.*s' DR:'%.*s' SI:'%.*s'.", dh_len, dh, dr_len, dr, si_len, si);
			/* Remove from the message */
			for (i = idx + 1; i < msg->radius.attr_used; i++)
				msg->radius.attr_pos[i - 1] = msg->radius.attr_pos[i];
			msg->radius.attr_used -= 1;
			break;
		}
		
		if ((attr->type == RADIUS_ATTR_CLASS) 
				&& (attr_len > pref_len ) 
				&& ! strncmp(attr_val, prefix, pref_len)) {
			si = attr_val + pref_len;
			si_len = attr_len - pref_len;
			TRACE_DEBUG(ANNOYING, "Found Class attribute with '%s' prefix (attr #%d), SI:'%.*s'.", prefix, idx, si_len, si);
			/* Remove from the message */
			for (i = idx + 1; i < msg->radius.attr_used; i++)
				msg->radius.attr_pos[i - 1] = msg->radius.attr_pos[i];
			msg->radius.attr_used -= 1;
			break;
		}
		
	}
	
	/* Get information on this peer */
	CHECK_FCT( rgw_clients_get_origin(cli, &fqdn, &realm) );
	
	/* Create the session object */
	if (si_len) {
		CHECK_FCT( fd_sess_fromsid ( si, si_len, session, &idx) );
	} else {
		if (un) {
			int len;
			/* If not found, create a new Session-Id. The format is: {fqdn;hi32;lo32;username;diamid} */
			CHECK_MALLOC( sess_str = malloc(un_len + 1 /* ';' */ + fd_g_config->cnf_diamid_len + 1 /* '\0' */) );
			len = sprintf(sess_str, "%.*s;%s", un_len, un, fd_g_config->cnf_diamid);
			CHECK_FCT( fd_sess_new(session, fqdn, sess_str, len) );
			free(sess_str);
			idx = 1;
		}
	}
	
	/* Create an empty Diameter message so that extensions can store their AVPs */
	CHECK_FCT(  fd_msg_new ( NULL, MSGFL_ALLOC_ETEID, diam )  );
	
	if (*session) {
		CHECK_FCT( fd_sess_getsid(*session, &sess_str) );
		TRACE_DEBUG(FULL, "Session '%s' has been successfully %s.", sess_str, idx ? "created" : "retrieved");
		
		/* Add the Session-Id AVP as first AVP */
		CHECK_FCT( fd_msg_avp_new ( cache_sess_id, 0, &avp ) );
		memset(&avp_val, 0, sizeof(avp_val));
		avp_val.os.data = (unsigned char *)sess_str;
		avp_val.os.len = strlen(sess_str);
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
		CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_FIRST_CHILD, avp) );

	} else {
		TRACE_DEBUG(FULL, "No session has been created for this message");
	}
	
	/* Add the Destination-Realm as next AVP */
	CHECK_FCT( fd_msg_avp_new ( cache_dest_realm, 0, &avp ) );
	memset(&avp_val, 0, sizeof(avp_val));
	if (dr) {
		avp_val.os.data = (unsigned char *)dr;
		avp_val.os.len = dr_len;
	} else {
		int i = 0;
		if (un) {
			/* Is there an '@' in the user name? We don't care for decorated NAI here */
			for (i = un_len - 2; i > 0; i--) {
				if (un[i] == '@') {
					i++;
					break;
				}
			}
		}
		if (i == 0) {
			/* Not found in the User-Name => we use the local domain of this gateway */
			avp_val.os.data = fd_g_config->cnf_diamrlm;
			avp_val.os.len  = fd_g_config->cnf_diamrlm_len;
		} else {
			avp_val.os.data = un + i;
			avp_val.os.len  = un_len - i;
		}
	}
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
	CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	
	/* Add the Destination-Host as next AVP */
	if (dh) {
		CHECK_FCT( fd_msg_avp_new ( cache_dest_host, 0, &avp ) );
		memset(&avp_val, 0, sizeof(avp_val));
		avp_val.os.data = (unsigned char *)dh;
		avp_val.os.len = dh_len;
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
		CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	}
	
	/* Add the Origin-Host as next AVP */
	CHECK_FCT( fd_msg_avp_new ( cache_orig_host, 0, &avp ) );
	memset(&avp_val, 0, sizeof(avp_val));
	avp_val.os.data = (unsigned char *)fqdn;
	avp_val.os.len = strlen(fqdn);
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
	CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	
	/* Add the Origin-Realm as next AVP */
	CHECK_FCT( fd_msg_avp_new ( cache_orig_realm, 0, &avp ) );
	memset(&avp_val, 0, sizeof(avp_val));
	avp_val.os.data = (unsigned char *)realm;
	avp_val.os.len = strlen(realm);
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
	CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	
	/* Done! */
	return 0;
}
