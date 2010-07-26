/*****************************************************************************************************
 * Software License Agreement (BSD License)
 * Author : Souheil Ben Ayed <souheil@tera.ics.keio.ac.jp>
 *
 * Copyright (c) 2009-2010, Souheil Ben Ayed, Teraoka Laboratory of Keio University, and the WIDE Project
 * All rights reserved.
 *
 * Redistribution and use of this software in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Souheil Ben Ayed <souheil@tera.ics.keio.ac.jp>.
 *
 * 4. Neither the name of Souheil Ben Ayed, Teraoka Laboratory of Keio University or the WIDE Project nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ''AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************************************/


#ifndef DIAMEAP_SERVER_H_
#define DIAMEAP_SERVER_H_

/* handler for DiamEAP server callback */
static struct disp_hdl * handle;

/* session handler for DiamEAP sessions state machine */
static struct session_handler * diameap_server_reg = NULL;

/* session data structure to store */
struct diameap_sess_data_sm
{
	int invalid_eappackets; /* Number of invalid EAP Packet received*/

	eap_type currentMethod;
	u32 currentVendor;
	int currentId;
	int lastId;
	void * methodData;

	u8 NAKproposedMethods[251];

	eap_method_state methodState;

	struct eap_user user;
};

typedef enum
{
	AUTHENTICATE_ONLY = 1, AUTHORIZE_ONLY = 2, AUTHORIZE_AUTHENTICATE = 3
} auth_request;

struct diameap_state_machine
{
	int invalid_eappackets; /* Number of invalid EAP Packet received*/
	struct avp * lastReqEAPavp; //last EAP-Payload AVP

	int result_code; /*Error number for Result_code*/
	struct fd_list attributes; //database attributes
	struct fd_list req_attributes; //attributes from DER
	struct fd_list ans_attributes; //attributes to be set for DEA
	struct avp * failedavp; /* The Failed-AVP AVP. should be update whenever a Failed AVP is encountered during authentication. */
	struct eap_state_machine eap_sm; /* EAP State Machine */
	auth_request auth_request_val; /*the Request Type of Auth-Request-Type AVP*/
	boolean verify_authorization; /* Set to TRUE at the authorization state. Parameter used to indicate that authorization is performed.*/
	boolean authSuccess; // Set to TRUE if client authenticated and authorized
	boolean authFailure; //set to TRUE if client is not authenticated
	boolean authorized; //set to TRUE if client is authorized
	enum
	{
		DIAMEAP_DISABLED,
		DIAMEAP_INITIALIZE,
		DIAMEAP_RECEIVED,
		DIAMEAP_IDLE,
		DIAMEAP_AUTHENTICATION_VERIFY,
		DIAMEAP_SEND_ERROR_MSG,
		DIAMEAP_SELECT_DECISION,
		DIAMEAP_DIAMETER_EAP_ANSWER,
		DIAMEAP_END,
		DIAMEAP_AUTHORIZATION_VERIFY,
		DIAMEAP_SEND_REQUEST,
		DIAMEAP_SEND_SUCCESS,
		DIAMEAP_SEND_FAILURE

	} state; // state of DiamEAP

	boolean privateUser;//TD
};

struct avp_max_occurences
{
	char * avp_attribute;
	int max; //-1 means no limits
};



/* start server */
int diameap_start_server(void);

/* stop server*/
int diameap_stop_server(void);

/* Initialize DiamEAP state machine variables */
static int diameap_initialize_diameap_sm(
		struct diameap_state_machine * diameap_sm,
		struct diameap_sess_data_sm * diameap_sess_data);

/* Initialize interface between the diameap and the eap states machines */
static int diameap_initialize_diameap_eap_interface(
		struct diameap_eap_interface * eap_i);

/* Parse received message */
static int diameap_parse_avps(struct diameap_state_machine * diameap_sm,
		struct msg * req, struct diameap_eap_interface * eap_i);

/* Add an avp to Failed_AVP AVP for answer message */
static int diameap_failed_avp(struct diameap_state_machine * diameap_sm,
		struct avp * invalidavp);

/* Parse EAP Response */
static int diameap_parse_eap_resp(struct eap_state_machine * eap_sm,
		struct eap_packet eappacket);

/* */
static int diameap_eappacket_new(struct eap_packet * eapPacket,
		struct avp_hdr * avpdata);

/* */
static int diameap_sess_data_new(
		struct diameap_sess_data_sm *diameap_sess_data,
		struct diameap_state_machine *diameap_sm);

/* */
static int diameap_unlink_attributes_lists(
		struct diameap_state_machine * diameap_sm);

/**/
static int diameap_answer_avp_attributes(
		struct diameap_state_machine * diameap_sm);

/**/
static int diameap_answer_authorization_attributes(
		struct diameap_state_machine * diameap_sm);

static void free_attrib(struct auth_attribute * auth_attrib);
static void free_avp_attrib(struct avp_attribute * avp_attrib);
static void free_ans_attrib(struct avp_attribute * ans_attrib);

/* */
static int diameap_get_avp_attribute(struct fd_list * avp_attributes,
		char * attribute, struct avp_attribute ** avp_attrib, int unlink,
		int *ret);

/* */
static int diameap_get_auth_attribute(struct fd_list * auth_attributes,
		char * attribute, struct auth_attribute ** auth_attrib, int unlink,
		int *ret);

/**/
static int diameap_get_ans_attribute(struct fd_list * ans_attributes,
		char * attribute, struct avp_attribute ** ans_attrib, int unlink,
		int *ret);

/* */
static int diameap_policy_decision(struct diameap_state_machine * diameap_sm,
		struct diameap_eap_interface eap_i);

/* */
static int diameap_add_avps(struct diameap_state_machine * diameap_sm,
		struct msg * ans, struct msg * req);

/* */
static int diameap_add_user_sessions_avps(
		struct diameap_state_machine * diameap_sm, struct msg * ans);

/* */
static int diameap_add_result_code(struct diameap_state_machine * diameap_sm,
		struct msg * ans, struct session * sess);

/* */
static int diameap_add_eap_payload(struct diameap_state_machine * diameap_sm,
		struct msg * ans, struct diameap_eap_interface eap_i);

/* */
static int diameap_add_authorization_avps(struct diameap_state_machine * diameap_sm,
		struct msg * ans);

/* */
static int diameap_send(struct msg ** rmsg);

/* */
static int diameap_add_eap_success_avps(
		struct diameap_state_machine * diameap_sm, struct msg * ans,
		struct diameap_eap_interface eap_i);

/* */
void diameap_cli_sess_cleanup(void * arg, char * sid);

/* */
static void diameap_free(struct diameap_state_machine * diameap_sm);

/* */
static void diameap_sess_data_free(
		struct diameap_sess_data_sm * diameap_sess_data);

/* */
static int diameap_add_accounting_eap_auth_method(
		struct diameap_state_machine * diameap_sm, struct msg * ans);

/* */
static int diameap_add_eap_reissued_payload(struct msg * ans,struct msg * req);
#endif /* DIAMEAP_SERVER_H_ */
