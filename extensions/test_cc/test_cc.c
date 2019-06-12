/**********************************************************************************************************
 * Software License Agreement(BSD License)                                                                *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright(c) 2019, Thomas Klausner                                                                     *
 * All rights reserved.                                                                                   *
 *                                                                                                        *
 * Written under contract by Effortel Technologies SA, http://effortel.com/                               *
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
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT      *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
 * TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF    *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
 **********************************************************************************************************/

/* This extension simply receives CCR and sends CCA after displaying the content, but does not store any data */

#include <freeDiameter/extension.h>

struct disp_hdl *ccr_handler_hdl;

struct dict_object * aai_avp_do; /* cache the Auth-Application-Id dictionary object */
struct dict_object * crn_avp_do; /* cache the CC-Request-Number dictionary object */
struct dict_object * crt_avp_do; /* cache the CC-Request-Type dictionary object */

#define MODULE_NAME "test_cc"

struct statistics {
        uint64_t sent;
        time_t first;
        time_t last;
} statistics;

void print_statistics(void) {
        if (statistics.first == 0 || statistics.last == 0 || statistics.last == statistics.first) {
                return;
        }

        fd_log_error("%s: %lld CCA messages sent in %llds (%.2f messages/second)", fd_g_config->cnf_diamid,
                     (long long)statistics.sent, (long long)(statistics.last-statistics.first), (float)statistics.sent / (statistics.last-statistics.first));
}

static int ccr_handler(struct msg ** msg, struct avp * avp, struct session * sess, void * data, enum disp_action * act)
{
	struct msg_hdr *hdr = NULL;
	time_t now;

	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);

	if(msg == NULL)
		return EINVAL;

	CHECK_FCT(fd_msg_hdr(*msg, &hdr));
	if(hdr->msg_flags & CMD_FLAG_REQUEST) {
		/* Request received, answer it */
		struct msg *answer;
		os0_t s;
		size_t sl;
		struct avp *avp;
		union avp_value val;
		struct avp *avp_data;
		struct avp_hdr *ahdr;
		uint32_t crt, crn;

		/* get some necessary information from request */
		if (fd_msg_search_avp(*msg, crt_avp_do, &avp_data) < 0 || avp_data == NULL) {
			fd_log_error("[%s] CC-Request-Type not found in CCR", MODULE_NAME);
			return 0;
		}
		if (fd_msg_avp_hdr(avp_data, &ahdr) < 0) {
			fd_log_error("[%s] error parsing CC-Request-Type in CCR", MODULE_NAME);
			return 0;
		}
		crt = ahdr->avp_value->i32;
		if (fd_msg_search_avp(*msg, crn_avp_do, &avp_data) < 0 || avp_data == NULL) {
			fd_log_error("[%s] CC-Request-Number not found in CCR", MODULE_NAME);
			return 0;
		}
		if (fd_msg_avp_hdr(avp_data, &ahdr) < 0) {
			fd_log_error("[%s] error parsing CC-Request-Type in CCR", MODULE_NAME);
			return 0;
		}
		crn = ahdr->avp_value->i32;

		/* Create the answer message */
		CHECK_FCT(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0));
		answer = *msg;

		/* TODO copy/fill in more AVPs in the answer */

		/* Auth-Application-Id */
		fd_msg_avp_new(aai_avp_do, 0, &avp);
		memset(&val, 0, sizeof(val));
		val.i32 = 4;
		if (fd_msg_avp_setvalue(avp, &val) != 0) {
			fd_msg_free(answer);
			fd_log_error("can't set value for 'Auth-Application-Id' for 'Credit-Control-Request' message");
			return 0;
		}
		fd_msg_avp_add(answer, MSG_BRW_LAST_CHILD, avp);

		/* CC-Request-Type */
		fd_msg_avp_new(crt_avp_do, 0, &avp);
		memset(&val, 0, sizeof(val));
		val.i32 = crt;
		if (fd_msg_avp_setvalue(avp, &val) != 0) {
			fd_msg_free(answer);
			fd_log_error("can't set value for 'CC-Request-Type' for 'Credit-Control-Request' message");
			return 0;
		}
		fd_msg_avp_add(answer, MSG_BRW_LAST_CHILD, avp);

		/* CC-Request-Number */
		fd_msg_avp_new(crn_avp_do, 0, &avp);
		memset(&val, 0, sizeof(val));
		val.i32 = crn;
		if (fd_msg_avp_setvalue(avp, &val) != 0) {
			fd_msg_free(answer);
			fd_log_error("can't set value for 'CC-Request-Number' for 'Credit-Control-Request' message");
			return 0;
		}
		fd_msg_avp_add(answer, MSG_BRW_LAST_CHILD, avp);

		/* TODO make result configurable (depending on an AVP?) */
		CHECK_FCT(fd_msg_rescode_set(answer, "DIAMETER_SUCCESS", NULL, NULL, 1));

		fd_log_debug("--------------Received the following Credit Control Request:--------------");

		CHECK_FCT(fd_sess_getsid(sess, &s, &sl));
		fd_log_debug("Session: %.*s",(int)sl, s);

		fd_log_debug("----------------------------------------------------------------------");

		/* Send the answer */
		CHECK_FCT(fd_msg_send(msg, NULL, NULL));
		now = time(NULL);
		if (!statistics.first) {
			statistics.first = now;
		}
		if (statistics.last != now) {
			print_statistics();
		}
		statistics.last = now;
		statistics.sent++;
		fd_log_debug("reply sent");
	} else {
		/* We received an answer message, just discard it */
		CHECK_FCT(fd_msg_free(*msg));
		*msg = NULL;
	}

	return 0;
}

/* entry hook: register callback */
static int cc_entry(char * conffile)
{
	struct disp_when data;

	TRACE_ENTRY("%p", conffile);

	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Auth-Application-Id", &aai_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Auth-Application-Id' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "CC-Request-Number", &crn_avp_do, ENOENT),
		     { LOG_E("Unable to find 'CC-Request-Number' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "CC-Request-Type", &crt_avp_do, ENOENT),
		     { LOG_E("Unable to find 'CC-Request-Type' AVP in the loaded dictionaries."); });

	memset(&data, 0, sizeof(data));

        /* Advertise the support for the Diameter Credit Control application in the peer */
        CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Credit Control Application", &data.app, ENOENT) );
        CHECK_FCT( fd_disp_app_support ( data.app, NULL, 1, 0 ) );

        /* register handler for CCR */
        CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Credit-Control-Request", &data.command, ENOENT) );
        CHECK_FCT( fd_disp_register( ccr_handler, DISP_HOW_CC, &data, NULL, &ccr_handler_hdl ) );

	return 0;
}

/* And terminate it */
void fd_ext_fini(void)
{
        /* Unregister the callbacks */
        if (ccr_handler_hdl) {
                CHECK_FCT_DO( fd_disp_unregister(&ccr_handler_hdl, NULL), );
                ccr_handler_hdl = NULL;
        }

	print_statistics();

        return;
}


EXTENSION_ENTRY(MODULE_NAME, cc_entry);
