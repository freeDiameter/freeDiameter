/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Alexandre Westfahl <awestfahl@freediameter.net>						 *
*													 *
* Copyright (c) 2010, Alexandre Westfahl, Teraoka Laboratory (Keio University), and the WIDE Project. 	 *		
*													 *
* All rights reserved.											 *
* Based on rgwx_auth plugin (Sebastien Decugis <sdecugis@nict.go.jp>)					 *
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


/* RADIUS Access-Request messages translation plugin */

#include "rgw_common.h"
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 

/* Other constants we use */
#define AI_SIP				6	/* Diameter SIP application */
#define CC_MULTIMEDIA_AUTH_REQUEST	286	/* MAR */
#define CC_MULTIMEDIA_AUTH_ANSWER	286	/* MAA */
#define ACV_ART_AUTHORIZE_AUTHENTICATE	3	/* AUTHORIZE_AUTHENTICATE */
#define ACV_OAP_RADIUS			1	/* RADIUS */
#define ACV_ASS_STATE_MAINTAINED	0	/* STATE_MAINTAINED */
#define ACV_ASS_NO_STATE_MAINTAINED	1	/* NO_STATE_MAINTAINED */
#define ER_DIAMETER_MULTI_ROUND_AUTH	1001
#define ER_DIAMETER_SUCCESS		2001
#define ER_DIAMETER_LIMITED_SUCCESS	2002
#define ER_DIAMETER_SUCCESS_AUTH_SENT_SERVER_NOT_STORED	2008
#define ER_DIAMETER_SUCCESS_SERVER_NAME_NOT_STORED	2006



/* This macro converts a RADIUS attribute to a Diameter AVP of type OctetString */
#define CONV2DIAM_STR( _dictobj_ )	\
	CHECK_PARAMS( attr->length >= 2 );						\
	/* Create the AVP with the specified dictionary model */			\
	CHECK_FCT( fd_msg_avp_new ( cs->dict._dictobj_, 0, &avp ) );			\
	value.os.len = attr->length - 2;						\
	value.os.data = (unsigned char *)(attr + 1);					\
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );				\
	/* Add the AVP in the Diameter message. */					\
	CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );		\

#define CONV2DIAM_STR_AUTH( _dictobj_ )	\
	CHECK_PARAMS( attr->length >= 2 );						\
	/* Create the AVP with the specified dictionary model */			\
	CHECK_FCT( fd_msg_avp_new ( cs->dict._dictobj_, 0, &avp ) );			\
	value.os.len = attr->length - 2;						\
	value.os.data = (unsigned char *)(attr + 1);					\
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );				\
	/* Add the AVP in the Diameter message. */					\
	CHECK_FCT( fd_msg_avp_add ( auth, MSG_BRW_LAST_CHILD, avp) );		\
		
/* Same thing, for scalar AVPs of 32 bits */
#define CONV2DIAM_32B( _dictobj_ )	\
	CHECK_PARAMS( attr->length == 6 );						\
	CHECK_FCT( fd_msg_avp_new ( cs->dict._dictobj_, 0, &avp ) );			\
	{										\
		uint8_t * v = (uint8_t *)(attr + 1);					\
		value.u32  = (v[0] << 24)						\
		           | (v[1] << 16)						\
		           | (v[2] <<  8)						\
		           |  v[3] ;							\
	}										\
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );				\
	CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );
				
				
				
				
				
/* The state we keep for this plugin */
struct rgwp_config {
	struct {
		struct dict_object * Session_Id;		
		struct dict_object * Auth_Application_Id;	
		struct dict_object * Auth_Session_State;	
		struct dict_object * Origin_Host;		
		struct dict_object * Origin_Realm;		
		struct dict_object * Destination_Realm;		
		struct dict_object * SIP_AOR;			
		struct dict_object * SIP_Method;			
		struct dict_object * Destination_Host;		
		struct dict_object * User_Name;			
		struct dict_object * SIP_Server_URI;		
		struct dict_object * SIP_Number_Auth_Items;	
		struct dict_object * SIP_Authorization;	
		struct dict_object * SIP_Authentication_Scheme;	
		struct dict_object * SIP_Authentication_Info;	
		struct dict_object * SIP_Auth_Data_Item;	
		struct dict_object * Proxy_Info;		
		struct dict_object * Route_Record;		
		struct dict_object * Service_Type;		
		struct dict_object * Result_Code;		
		struct dict_object * Digest_URI;		
		struct dict_object * Digest_Nonce;
		struct dict_object * Digest_CNonce;
		struct dict_object * Digest_Nonce_Count;				
		struct dict_object * Digest_Realm;		
		struct dict_object * Digest_Response;
		struct dict_object * Digest_Method;
		struct dict_object * Digest_Response_Auth;		
		struct dict_object * Digest_Username;
		struct dict_object * Digest_Algorithm;	
		struct dict_object * Digest_QOP;

		
		
	} dict; /* cache of the dictionary objects we use */
	struct session_handler * sess_hdl; /* We store RADIUS request authenticator information in the session */
	char * confstr;
	//Global variable which points to chained list of nonce
	struct fd_list listnonce;
	//This will be used to lock access to chained list
	pthread_mutex_t nonce_mutex; 
};

typedef struct noncechain noncechain;
struct noncechain
{
	struct fd_list chain;
    char * sid;
    char * nonce;
    
};






int nonce_add_element(char * nonce, char * sid, struct rgwp_config *state)
{
	noncechain *newelt;
	CHECK_MALLOC(newelt=malloc(sizeof(noncechain)));
	int lenghtsid=strlen(sid);
	
	CHECK_MALLOC(newelt->nonce=malloc(33));
	memcpy(newelt->nonce,nonce,32);
	newelt->nonce[32]='\0';
	CHECK_MALLOC(newelt->sid=malloc(lenghtsid+1));
	strncpy(newelt->sid,sid,lenghtsid);
	newelt->sid[lenghtsid]='\0';
	
	FD_LIST_INITIALIZER(&newelt->chain);
	
	CHECK_POSIX(pthread_mutex_lock(&state->nonce_mutex));
	fd_list_insert_before(&state->listnonce,&newelt->chain);
	CHECK_POSIX(pthread_mutex_unlock(&state->nonce_mutex));
}

void nonce_del_element(char * nonce, struct rgwp_config *state)
{
	if(!FD_IS_LIST_EMPTY(&state->listnonce))
	{
		/*
		noncechain *temp=listnonce, *tempbefore=NULL;
		
		if(listnonce->next==NULL && strcmp(listnonce->nonce,nonce)==0)
		{
			free(listnonce->nonce);
			free(listnonce->sid);
			free(listnonce);
			listnonce=NULL;
			return;
		}
		while(temp->next != NULL)
		{
			if(strcmp(temp->nonce,nonce)==0)
			{
				if(tempbefore==NULL)
				{
					listnonce=temp->next;
					free(temp->nonce);
					free(temp->sid);
					free(temp);
					return;
				}
				tempbefore->next=temp->next;
				free(temp->nonce);
				free(temp->sid);
				free(temp);
				break;
			}
			tempbefore=temp;
			temp = temp->next;
		}*/
	}
	
}
//Retrieve sid from nonce
char * nonce_check_element(char * nonce)
{
	/*
	if(listnonce==NULL)
	{
		//Not found
		return NULL;
	}
	else
	{
		noncechain* temp=listnonce;
		
		if(strcmp(temp->nonce,nonce)==0)
				return temp->sid;
		
		while(temp->next != NULL)
		{
			
			if(strcmp(temp->nonce,nonce)==0)
			{
				TRACE_DEBUG(FULL,"We found the nonce!");
				return temp->sid;
			}
			else
				temp = temp->next;
		}
		
		
	}
	return NULL;
	*/
}

void nonce_deletelistnonce()
{
	/*
	if(listnonce !=NULL)
	{
		while(listnonce->next != NULL)
		{
			noncechain* temp=listnonce->next;
			
			free(listnonce->nonce);
			free(listnonce->sid);
			free(listnonce);
		
			listnonce=temp;
		}
		free(listnonce->nonce);
		free(listnonce->sid);
		free(listnonce);
		listnonce=NULL;
	}
	*/
}

/* Initialize the plugin */
static int sip_conf_parse(char * conffile, struct rgwp_config ** state)
{
	struct rgwp_config * new;
	struct dict_object * app;
	
	
	TRACE_ENTRY("%p %p", conffile, state);
	CHECK_PARAMS( state );
	
	CHECK_MALLOC( new = malloc(sizeof(struct rgwp_config)) );
	memset(new, 0, sizeof(struct rgwp_config));
	
	CHECK_FCT( fd_sess_handler_create( &new->sess_hdl, free ) );
	new->confstr = conffile;
	
	/* Resolve all dictionary objects we use */
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Session-Id", &new->dict.Session_Id, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Auth-Application-Id", &new->dict.Auth_Application_Id, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Auth-Session-State", &new->dict.Auth_Session_State, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &new->dict.Origin_Host, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Realm", &new->dict.Origin_Realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Realm", &new->dict.Destination_Realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-AOR", &new->dict.SIP_AOR, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Method", &new->dict.SIP_Method, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Host", &new->dict.Destination_Host, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "User-Name", &new->dict.User_Name, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Server-URI", &new->dict.SIP_Server_URI, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Number-Auth-Items", &new->dict.SIP_Number_Auth_Items, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authorization", &new->dict.SIP_Authorization, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Auth-Data-Item", &new->dict.SIP_Auth_Data_Item, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authentication-Scheme", &new->dict.SIP_Authentication_Scheme, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "SIP-Authentication-Info", &new->dict.SIP_Authentication_Info, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Proxy-Info", &new->dict.Proxy_Info, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Route-Record", &new->dict.Route_Record, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &new->dict.Result_Code, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-URI", &new->dict.Digest_URI, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Nonce", &new->dict.Digest_Nonce, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Method", &new->dict.Digest_Method, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-CNonce", &new->dict.Digest_CNonce, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Nonce-Count", &new->dict.Digest_Nonce_Count, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Realm", &new->dict.Digest_Realm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Response", &new->dict.Digest_Response, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Response-Auth", &new->dict.Digest_Response_Auth, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Username", &new->dict.Digest_Username, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-Algorithm", &new->dict.Digest_Algorithm, ENOENT) );
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Digest-QoP", &new->dict.Digest_QOP, ENOENT) );


	
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Session Initiation Protocol (SIP) Application", &app, ENOENT) );
	CHECK_FCT( fd_disp_app_support ( app, NULL, 1, 0 ) );
	
	//chained list
	FD_LIST_INITIALIZER(&new->listnonce);
	CHECK_POSIX(pthread_mutex_init(&new->nonce_mutex,NULL));
	
	*state = new;
	return 0;
}

/* deinitialize */
static void sip_conf_free(struct rgwp_config * state)
{
	TRACE_ENTRY("%p", state);
	CHECK_PARAMS_DO( state, return );
	
	CHECK_FCT_DO( fd_sess_handler_destroy( &state->sess_hdl ),  );
	
	nonce_deletelistnonce(&state->listnonce);
	CHECK_POSIX_DO(pthread_mutex_destroy(&state->nonce_mutex), /*continue*/);
	
	free(state);
	return;
}


/* Handle an incoming RADIUS request */
static int sip_rad_req( struct rgwp_config * cs, struct session ** session, struct radius_msg * rad_req, struct radius_msg ** rad_ans, struct msg ** diam_fw, struct rgw_client * cli )
{
	int idx;
	int got_username = 0;
	int got_AOR = 0;
	int got_Dusername = 0;
	int got_Drealm = 0;
	int got_Duri = 0;
	int got_Dmethod = 0;
	int got_Dqop = 0;
	int got_Dnonce_count = 0;
	int got_Dnonce = 0;
	int got_Dcnonce = 0;
	int got_Dresponse = 0;
	int got_Dalgorithm = 0;
	
	uint32_t status_type;
	size_t nattr_used = 0;
	struct avp *auth_data=NULL, *auth=NULL, *avp = NULL;
	union avp_value value;
	
	TRACE_ENTRY("%p %p %p %p %p %p", cs, session, rad_req, rad_ans, diam_fw, cli);
	
	CHECK_PARAMS(rad_req && (rad_req->hdr->code == RADIUS_CODE_ACCESS_REQUEST) && rad_ans && diam_fw && *diam_fw);
	
	//We check that session is not already filled
	if(*session)
	{
		TRACE_DEBUG(INFO,"We are not supposed to receive a session in radSIP plugin.");
		return EINVAL;
	}
	
	/*
	   RFC5090 RADIUS Extension Digest Application
	*/
	
	/* Check basic information is there */
	for (idx = 0; idx < rad_req->attr_used; idx++) {
		struct radius_attr_hdr * attr = (struct radius_attr_hdr *)(rad_req->buf + rad_req->attr_pos[idx]);
		
		
		switch (attr->type) {
			
			case RADIUS_ATTR_USER_NAME:
				got_username = 1;
			break;
			case RADIUS_ATTR_DIGEST_USERNAME:
				got_Dusername = 1;
			break;
			case RADIUS_ATTR_DIGEST_REALM:
				got_Drealm = 1;
			break;
			case RADIUS_ATTR_DIGEST_URI:
				got_Duri = 1;
			break;
			case RADIUS_ATTR_DIGEST_METHOD:
				got_Dmethod = 1;
			break;
			case RADIUS_ATTR_DIGEST_QOP:
				got_Dqop = 1;
			break;
			case RADIUS_ATTR_DIGEST_NONCE_COUNT:
				got_Dnonce_count = 1;
			break;
			case RADIUS_ATTR_DIGEST_NONCE:
				got_Dnonce = 1;
			break;
			case RADIUS_ATTR_DIGEST_CNONCE:
				got_Dcnonce = 1;
			break;
			case RADIUS_ATTR_DIGEST_RESPONSE:
				got_Dresponse = 1;
			break;			
			case RADIUS_ATTR_DIGEST_ALGORITHM:
				got_Dalgorithm = 1;
			break;
			case RADIUS_ATTR_SIP_AOR:
				got_AOR = 1;
			break;
		}
	}
	if(!got_username)
	{
		TRACE_DEBUG(INFO,"No Username in request");
		return 1;
	}
	if(!got_Dnonce)
	{
		/* Add the Session-Id AVP as first AVP */
		CHECK_FCT( fd_msg_avp_new ( cs->dict.Session_Id, 0, &avp ) );
		
		char *sid=NULL;
		fd_sess_getsid (session, &sid );
		memset(&value, 0, sizeof(value));
		value.os.data = (unsigned char *)sid;
		value.os.len = strlen(sid);
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
		CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_FIRST_CHILD, avp) );
	}
	/*
	If the RADIUS Access-Request message does not
	contain any Digest-* attribute, then the RADIUS client does not want
	to apply HTTP Digest authentication, in which case, actions at the
	gateway are outside the scope of this document.
	*/
	
	if(!(got_Dmethod && got_Duri))
	{
		TRACE_DEBUG(INFO,"No Digest attributes in request, we drop it...");
		return 1;
	}

	/* Add the appropriate command code & Auth-Application-Id */
	{
		struct msg_hdr * header = NULL;
		CHECK_FCT( fd_msg_hdr ( *diam_fw, &header ) );
		header->msg_flags = CMD_FLAG_REQUEST | CMD_FLAG_PROXIABLE;
		header->msg_code = CC_MULTIMEDIA_AUTH_REQUEST;
		header->msg_appl = AI_SIP;
	
	
		/* Add the Auth-Application-Id */
		{
			CHECK_FCT( fd_msg_avp_new ( cs->dict.Auth_Application_Id, 0, &avp ) );
			value.i32 = header->msg_appl;
			CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
			CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );
		}
	}
	/*Add Auth_Session_State  AVP */
	{
		CHECK_FCT( fd_msg_avp_new ( cs->dict.Auth_Session_State, 0, &avp ) );
		value.i32 = ACV_ASS_NO_STATE_MAINTAINED;
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
		CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );
	}

	
	/*Add SIP_Number_Auth_Items  AVP */
	{
		CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Number_Auth_Items, 0, &avp ) );
		value.i32 = 1; //We just treat one auth per request in gateway
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
		CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );
	}

	/* Add SIP_Auth_Data_Item AVP */
	{
		CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Auth_Data_Item, 0, &auth_data ) );
	}
	/* Add SIP_Authentication_Scheme AVP */
	{
		CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Authentication_Scheme, 0, &avp ) );
		value.i32=0; //There is only Digest Auth in RFC for now
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
		CHECK_FCT( fd_msg_avp_add ( auth_data, MSG_BRW_LAST_CHILD, avp) );
	
	}

	
	/* Add SIP_Authorization AVP */
	{
		CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Authorization, 0, &auth ) );
		CHECK_FCT( fd_msg_avp_add ( auth_data, MSG_BRW_LAST_CHILD, auth) );
	}
	char * temp=NULL,*sipuri=NULL;

	for (idx = 0; idx < rad_req->attr_used; idx++) 
	{
		struct radius_attr_hdr * attr = (struct radius_attr_hdr *)(rad_req->buf + rad_req->attr_pos[idx]);
		
		switch (attr->type) {

			case RADIUS_ATTR_USER_NAME:
			CONV2DIAM_STR( User_Name );
			
			if(!got_Dusername)
			{
				CONV2DIAM_STR_AUTH(Digest_Username);
				got_Dusername=1;
			}
			
			break;

			case RADIUS_ATTR_DIGEST_URI:
			
			CONV2DIAM_STR_AUTH(Digest_URI);
			
			//All of these attributes are required by Diameter but not defined in RFC5090 so we provide FAKE values (only in first exchange)
			if(!got_AOR)
			{
				CONV2DIAM_STR( SIP_AOR );
				got_AOR=1;
			}
			/*
			We must provide a fake nonce because of RFC4740 problem
			TODO: remove when RFC is updated
			==START of FAKE
			*/
			if(!got_Dresponse)
			{
				CONV2DIAM_STR_AUTH(Digest_Response);
				got_Dresponse=1;
			}
			/*
			==END of FAKE
			*/
			if(!got_Drealm)
			{
				//We extract Realm from Digest_URI
				char *realm=NULL;
			
				CHECK_MALLOC(temp=malloc(attr->length -1));
				strncpy(temp, (char *)(attr + 1), attr->length -2);
				temp[attr->length-2] = '\0';
			
				realm = strtok( (char *)(temp), "@" );
				realm = strtok( NULL, "@" );
				free(temp);
				temp=NULL;
				if(realm!=NULL)
				{
					CHECK_FCT( fd_msg_avp_new ( cs->dict.Digest_Realm, 0, &avp ) );
					value.os.data=(unsigned char *)realm;
					value.os.len=strlen(realm);
					CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
					CHECK_FCT( fd_msg_avp_add ( auth, MSG_BRW_LAST_CHILD, avp) );
					
					//We add SIP-Server-URI AVP because SIP server is registrar (through gateway)
					CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Server_URI, 0, &avp ) );
					value.os.data=(unsigned char *)realm;
					value.os.len=strlen(realm);
					CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
					CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );
					
				}
				else
				{
					TRACE_DEBUG(INFO, "Can't extract domain from URI, droping request...");
					return 1;
				}	
				got_Drealm=1;
			}
			break;

			case RADIUS_ATTR_DIGEST_METHOD:
			CONV2DIAM_STR(SIP_Method);
			CONV2DIAM_STR_AUTH(Digest_Method);
			break;
			case RADIUS_ATTR_DIGEST_REALM:
			CONV2DIAM_STR_AUTH(Digest_Realm);
			
			//We add SIP-Server-URI AVP because SIP server is registrar (through gateway)
			CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Server_URI, 0, &avp ) );
			
			
			CHECK_MALLOC(temp=malloc(attr->length -1));
			strncpy(temp, (char *)(attr + 1), attr->length -2);
			
			
			CHECK_MALLOC(sipuri=malloc(attr->length +3));
			strcpy(sipuri,"sip:");
			strcat(sipuri,(unsigned char *)temp);
			value.os.data=(unsigned char *)sipuri;
			value.os.len=attr->length +2;
			
			free(temp);
			temp=NULL;
			CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
			CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, avp) );
			break;
			
			case RADIUS_ATTR_DIGEST_USERNAME:
			CONV2DIAM_STR_AUTH(Digest_Username);
			break;
				
			case RADIUS_ATTR_DIGEST_QOP:
			CONV2DIAM_STR_AUTH( Digest_QOP );
			break;
			case RADIUS_ATTR_DIGEST_ALGORITHM:		
			CONV2DIAM_STR_AUTH( Digest_Algorithm );
			break;
			case RADIUS_ATTR_DIGEST_CNONCE:
			CONV2DIAM_STR_AUTH( Digest_CNonce );
			break;
			case RADIUS_ATTR_DIGEST_NONCE:
				CONV2DIAM_STR_AUTH( Digest_Nonce );
				
				
				int new=0;
				int sidlen=0;
				struct session * temp;
				char *nonce=malloc(attr->length-1);
				char *sid=malloc(sidlen+1);
				
				strncpy(nonce,(char *)(attr+1), attr->length-2);
				nonce[attr->length-2]='\0';
				
				//**Start mutex
				pthread_mutex_lock(&state->nonce_mutex); 
				sidlen=strlen(nonce_check_element(nonce));
				strcpy(sid,nonce_check_element(nonce));
				sid[sidlen+1]='\0';
				nonce_del_element(nonce);
				free(nonce); //TODO: free nonce inside delete
				pthread_mutex_unlock(&state->nonce_mutex); 
				//**Stop mutex
				
				CHECK_FCT(fd_sess_fromsid ( (char *)sid, (size_t)sidlen, &temp, &new));
				//free(sid);
				
				if(new==0)
				{
					session=temp;
					/* Add the Session-Id AVP as first AVP */
					CHECK_FCT( fd_msg_avp_new ( cs->dict.Session_Id, 0, &avp ) );
					//memset(&value, 0, sizeof(value));
					value.os.data = (unsigned char *)sid;
					value.os.len = sidlen;
					CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
					CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_FIRST_CHILD, avp) );

				}
				else
				{
					TRACE_DEBUG(INFO,"Can't find previously established session, message droped!");
					return 1;
				}
				//free(sid);
				free(nonce);
				//fd_sess_dump(FULL,session);
				
				
		
			break;
			case RADIUS_ATTR_DIGEST_NONCE_COUNT:
			CONV2DIAM_STR_AUTH( Digest_Nonce_Count );
			break;
			case RADIUS_ATTR_DIGEST_RESPONSE:
			CONV2DIAM_STR_AUTH( Digest_Response );
			break;
			case RADIUS_ATTR_SIP_AOR:
			CONV2DIAM_STR( SIP_AOR );
			break;
				
			default:
			if(!got_Dalgorithm)
			{
				//[Note 3] If Digest-Algorithm is missing, 'MD5' is assumed.
										
				CHECK_FCT( fd_msg_avp_new ( cs->dict.Digest_Algorithm, 0, &avp ) );			
				value.os.len = 3;						
				value.os.data = "MD5";					
				CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );					
				CHECK_FCT( fd_msg_avp_add ( auth, MSG_BRW_LAST_CHILD, avp) );	
				got_Dalgorithm=1;	
			}
			
			if(!got_Dnonce)
			{
				//We give a fake nonce because it will be calculated at the server.
				CHECK_FCT( fd_msg_avp_new ( cs->dict.Digest_Nonce, 0, &avp ) );
				value.os.data="nonce";
				value.os.len=5;
				CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
				CHECK_FCT( fd_msg_avp_add ( auth, MSG_BRW_LAST_CHILD, avp) );	
				got_Dnonce=1;
			}
			break;
	
		}
	}

	
	CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, auth_data) );
	
	/* Update the radius message to remove all handled attributes */
	rad_req->attr_used = nattr_used;

	//fd_msg_dump_walk(1,*diam_fw);
	
	/* Store the request identifier in the session (if provided) */
	
	
	if (session) {
		unsigned char * req_sip;
		CHECK_MALLOC(req_sip = malloc(16));
		memcpy(req_sip, &rad_req->hdr->authenticator[0], 16);
		
		CHECK_FCT( fd_sess_state_store( cs->sess_hdl, session, &req_sip ) );
	}
	
	
	return 0;
}

static int sip_diam_ans( struct rgwp_config * cs, struct session * session, struct msg ** diam_ans, struct radius_msg ** rad_fw, struct rgw_client * cli, int * statefull )
{
	
	struct msg_hdr * hdr;
	struct avp *avp, *next, *asid;
	struct avp_hdr *ahdr, *sid, *oh;
	char buf[254]; /* to store some attributes values (with final '\0') */
	int ta_set = 0;
	int no_str = 0; /* indicate if an STR is required for this server */
	uint8_t	tuntag = 0;
	unsigned char * req_sip = NULL;
	int in_success=0;
	
	TRACE_ENTRY("%p %p %p %p %p", cs, session, diam_ans, rad_fw, cli);
	CHECK_PARAMS(cs && session && diam_ans && *diam_ans && rad_fw && *rad_fw);
	
	
	
	
	
	/* MACROS to help in the process: convert AVP data to RADIUS attributes. */
	/* Control large attributes:  _trunc_ = 0 => error; _trunc_ = 1 => truncate; _trunc = 2 => create several attributes */
	#define CONV2RAD_STR( _attr_, _data_, _len_, _trunc_)	{					\
		size_t __l = (size_t)(_len_);								\
		size_t __off = 0;									\
		TRACE_DEBUG(FULL, "Converting AVP to "#_attr_);						\
		if ((_trunc_) == 0) {									\
			CHECK_PARAMS( __l <= 253 );							\
		}											\
		if ((__l > 253) && (_trunc_ == 1)) {							\
			TRACE_DEBUG(INFO, "[authSIP.rgwx] AVP truncated in "#_attr_);			\
			__l = 253;									\
		}											\
		do {											\
			size_t __w = (__l > 253) ? 253 : __l;						\
			CHECK_MALLOC(radius_msg_add_attr(*rad_fw, (_attr_), (_data_) + __off, __w));	\
			__off += __w;									\
			__l   -= __w;									\
		} while (__l);										\
	}

	#define CONV2RAD_32B( _attr_, _data_)	{							\
		uint32_t __v = htonl((uint32_t)(_data_));						\
		TRACE_DEBUG(FULL, "Converting AVP to "#_attr_);						\
		CHECK_MALLOC(radius_msg_add_attr(*rad_fw, (_attr_), (uint8_t *)&__v, sizeof(__v)));	\
	}


	/* Search the different AVPs we handle here */
	CHECK_FCT( fd_msg_search_avp (*diam_ans, cs->dict.Session_Id, &asid) );
	CHECK_FCT( fd_msg_avp_hdr ( asid, &sid ) );
	CHECK_FCT( fd_msg_search_avp (*diam_ans, cs->dict.Origin_Host, &asid) );
	CHECK_FCT( fd_msg_avp_hdr ( asid, &oh ) );

	/* Check the Diameter error code */
	CHECK_FCT( fd_msg_search_avp (*diam_ans, cs->dict.Result_Code, &avp) );
	ASSERT( avp ); /* otherwise the message should have been discarded a lot earlier because of ABNF */
	CHECK_FCT( fd_msg_avp_hdr ( avp, &ahdr ) );
	switch (ahdr->avp_value->u32) {
		case ER_DIAMETER_MULTI_ROUND_AUTH:
		case ER_DIAMETER_SUCCESS_AUTH_SENT_SERVER_NOT_STORED:		
			(*rad_fw)->hdr->code = RADIUS_CODE_ACCESS_CHALLENGE;
			*statefull=1;
			struct timespec nowts;
			CHECK_SYS(clock_gettime(CLOCK_REALTIME, &nowts));
			nowts.tv_sec+=600;
			CHECK_FCT(fd_sess_settimeout(session, &nowts ));
			break;
		case ER_DIAMETER_SUCCESS_SERVER_NAME_NOT_STORED:
		case ER_DIAMETER_SUCCESS:
			(*rad_fw)->hdr->code = RADIUS_CODE_ACCESS_ACCEPT;
			in_success=1;
			break;
		
		default:
			(*rad_fw)->hdr->code = RADIUS_CODE_ACCESS_REJECT;
			fd_log_debug("[authSIP.rgwx] Received Diameter answer with error code '%d' from server '%.*s', session %.*s, translating into Access-Reject\n",
					ahdr->avp_value->u32, 
					oh->avp_value->os.len, oh->avp_value->os.data,
					sid->avp_value->os.len, sid->avp_value->os.data);
			return 0;
	}
	/* Remove this Result-Code avp */
	CHECK_FCT( fd_msg_free( avp ) );
	
	/* Creation of the State or Class attribute with session information */
	CHECK_FCT( fd_msg_search_avp (*diam_ans, cs->dict.Origin_Realm, &avp) );
	CHECK_FCT( fd_msg_avp_hdr ( avp, &ahdr ) );

	
	/* Now, save the session-id and eventually server info in a STATE or CLASS attribute */
	if ((*rad_fw)->hdr->code == RADIUS_CODE_ACCESS_CHALLENGE) {
		if (sizeof(buf) < snprintf(buf, sizeof(buf), "Diameter/%.*s/%.*s/%.*s", 
				oh->avp_value->os.len,  oh->avp_value->os.data,
				ahdr->avp_value->os.len,  ahdr->avp_value->os.data,
				sid->avp_value->os.len, sid->avp_value->os.data)) {
			TRACE_DEBUG(INFO, "Data truncated in State attribute: %s", buf);
		}
		CONV2RAD_STR(RADIUS_ATTR_STATE, buf, strlen(buf), 0);
		
	}

	if ((*rad_fw)->hdr->code == RADIUS_CODE_ACCESS_ACCEPT) {
		/* Add the Session-Id */
		if (sizeof(buf) < snprintf(buf, sizeof(buf), "Diameter/%.*s", 
				sid->avp_value->os.len, sid->avp_value->os.data)) {
			TRACE_DEBUG(INFO, "Data truncated in Class attribute: %s", buf);
		}
		CONV2RAD_STR(RADIUS_ATTR_CLASS, buf, strlen(buf), 0);
	}
	
	/* Unlink the Origin-Realm now; the others are unlinked at the end of this function */
	CHECK_FCT( fd_msg_free( avp ) );
	

	
	/* Now loop in the list of AVPs and convert those that we know how */
	CHECK_FCT( fd_msg_browse(*diam_ans, MSG_BRW_FIRST_CHILD, &next, NULL) );
	
	while (next) {
		int handled = 1;
		avp = next;
		CHECK_FCT( fd_msg_browse(avp, MSG_BRW_WALK, &next, NULL) );
		
		CHECK_FCT( fd_msg_avp_hdr ( avp, &ahdr ) );
		
		if (!(ahdr->avp_flags & AVP_FLAG_VENDOR)) {
			switch (ahdr->avp_code) {
				
				case DIAM_ATTR_AUTH_SESSION_STATE:
					if ((!ta_set) && (ahdr->avp_value->u32 == ACV_ASS_STATE_MAINTAINED)) {
						CONV2RAD_32B( RADIUS_ATTR_TERMINATION_ACTION, RADIUS_TERMINATION_ACTION_RADIUS_REQUEST );
					}
					
					if (ahdr->avp_value->u32 == ACV_ASS_NO_STATE_MAINTAINED) {
						no_str = 1;
					}
					break;
				case DIAM_ATTR_DIGEST_NONCE:
					CONV2RAD_STR(DIAM_ATTR_DIGEST_NONCE, ahdr->avp_value->os.data, ahdr->avp_value->os.len, 0);
					/* Retrieve the request identified which was stored in the session */
					if (session) {
						char *sid=NULL;
						
						fd_sess_getsid (session, &sid );
						
						//***Start mutex
						CHECK_POSIX(pthread_mutex_lock(&state->nonce_mutex)); 
						nonce_add_element(ahdr->avp_value->os.data, sid, state);
						CHECK_POSIX(pthread_mutex_unlock(&state->nonce_mutex)); 
						//***Stop mutex
					}
					break;
				case DIAM_ATTR_DIGEST_REALM:
					CONV2RAD_STR(DIAM_ATTR_DIGEST_REALM, ahdr->avp_value->os.data, ahdr->avp_value->os.len, 1);
					break;
				case DIAM_ATTR_DIGEST_QOP:
					CONV2RAD_STR(DIAM_ATTR_DIGEST_QOP, ahdr->avp_value->os.data, ahdr->avp_value->os.len, 1);
					break;
				case DIAM_ATTR_DIGEST_ALGORITHM:
					CONV2RAD_STR(DIAM_ATTR_DIGEST_ALGORITHM, ahdr->avp_value->os.data, ahdr->avp_value->os.len, 1);
					break;		
				case DIAM_ATTR_DIGEST_RESPONSE_AUTH:
					CONV2RAD_STR(DIAM_ATTR_DIGEST_RESPONSE_AUTH, ahdr->avp_value->os.data, ahdr->avp_value->os.len, 0);
					break;
			}
		} 
		else 
		{
			/* Vendor-specific AVPs */
			switch (ahdr->avp_vendor) {
				
				default: /* unknown vendor */
					handled = 0;
			}
		}
		
		
		if (session) 
		{
			CHECK_FCT( fd_sess_state_retrieve( cs->sess_hdl, session, &req_sip ) );
		}
	}

	req_sip=NULL;
	
	return 0;
}

/* The exported symbol */
struct rgw_api rgwp_descriptor = {
	.rgwp_name       = "sip",
	.rgwp_conf_parse = sip_conf_parse,
	.rgwp_conf_free  = sip_conf_free,
	.rgwp_rad_req    = sip_rad_req,
	.rgwp_diam_ans   = sip_diam_ans
	
};	
/*}
	/* Add FAKE Digest_Realm AVP 
	{
		//We give a fake realm because it will be provided in the second access request.
		CHECK_FCT( fd_msg_avp_new ( cs->dict.Digest_Realm, 0, &avp ) );
		
		u8  *realm="example.com";
		
		value.os.data=(unsigned char *)realm;
		value.os.len=strlen(realm);
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );
		CHECK_FCT( fd_msg_avp_add ( auth, MSG_BRW_LAST_CHILD, avp) );

	}
	else
	{
		TRACE_DEBUG(FULL,"\nAnswer to challenge!\n");
		//We need a client nonce, count nonce, digest realm, username and response to handle authentication
		if (got_Dnonce_count && got_Dcnonce && got_Dresponse && got_Drealm && got_Dusername)
		{
			/* Add SIP_Authorization AVP 
			{
				CHECK_FCT( fd_msg_avp_new ( cs->dict.SIP_Authorization, 0, &auth ) );
				CHECK_FCT( fd_msg_avp_add ( auth_data, MSG_BRW_LAST_CHILD, auth) );
			}
			for (idx = 0; idx < rad_req->attr_used; idx++) 
			{
				struct radius_attr_hdr * attr = (struct radius_attr_hdr *)(rad_req->buf + rad_req->attr_pos[idx]);
				char * temp;
			
				switch (attr->type) {
					
					
					default:
					
					if(!got_Dalgorithm)
					{
						//[Note 3] If Digest-Algorithm is missing, 'MD5' is assumed.
				
						CHECK_PARAMS( attr->length >= 2 );						
						CHECK_FCT( fd_msg_avp_new ( cs->dict.Digest_Algorithm, 0, &avp ) );			
						value.os.len = attr->length - 2;						
						value.os.data = (unsigned char *)(attr + 1);					
						CHECK_FCT( fd_msg_avp_setvalue ( avp, &value ) );					
						CHECK_FCT( fd_msg_avp_add ( auth, MSG_BRW_LAST_CHILD, avp) );		
					}
				
				}
				
			}
			CHECK_FCT( fd_msg_avp_add ( *diam_fw, MSG_BRW_LAST_CHILD, auth_data) );
			
		}
		else
		{
			TRACE_DEBUG(INFO,"Missing Digest attributes in request, we drop it...");
			return 1;
		}
	}*/

