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

/* This extension simply receives ASR and sends ASA after displaying the content, but does not store any data */

#include <freeDiameter/extension.h>

struct disp_hdl *asr_handler_hdl;

static int asr_handler(struct msg ** msg, struct avp * avp, struct session * sess, void * data, enum disp_action * act)
{
	struct msg_hdr *hdr = NULL;

	TRACE_ENTRY("%p %p %p %p", msg, avp, sess, act);

	if(msg == NULL)
		return EINVAL;

	CHECK_FCT(fd_msg_hdr(*msg, &hdr));
	if(hdr->msg_flags & CMD_FLAG_REQUEST) {
		/* Request received, answer it */
		struct msg *answer;
		os0_t s;
		size_t sl;

		/* Create the answer message */
		CHECK_FCT(fd_msg_new_answer_from_req(fd_g_config->cnf_dict, msg, 0));
		answer = *msg;

		/* TODO copy/fill in AVPs into the answer */
		/* TODO make result code configurable, depending on an AVP value? */
		CHECK_FCT(fd_msg_rescode_set(answer, "DIAMETER_SUCCESS", NULL, NULL, 1));

		fd_log_notice("--------------Received the following Abort Session Request:--------------");

		CHECK_FCT(fd_sess_getsid(sess, &s, &sl));
		fd_log_notice("Session: %.*s",(int)sl, s);

		fd_log_notice("----------------------------------------------------------------------");

		/* Send the answer */
		CHECK_FCT(fd_msg_send(msg, NULL, NULL));

	} else {
		/* We received an answer message, just discard it */
		CHECK_FCT(fd_msg_free(*msg));
		*msg = NULL;
	}

	return 0;
}

/* entry hook: register callback */
static int as_entry(char * conffile)
{
	struct disp_when data;

	TRACE_ENTRY("%p", conffile);

	memset(&data, 0, sizeof(data));

        /* Advertise the support for the Diameter Abort Session application in the peer */
        CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Credit Control Application", &data.app, ENOENT) );
        CHECK_FCT( fd_disp_app_support ( data.app, NULL, 1, 0 ) );

        /* register handler for ASR */
        CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Abort-Session-Request", &data.command, ENOENT) );
        CHECK_FCT( fd_disp_register( asr_handler, DISP_HOW_CC, &data, NULL, &asr_handler_hdl ) );

	return 0;
}

EXTENSION_ENTRY("test_as", as_entry);
