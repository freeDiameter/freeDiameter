/**********************************************************************************************************
 * Software License Agreement(BSD License)                                                                *
 * Author: Thomas Klausner <tk@giga.or.at>                                                                *
 *                                                                                                        *
 * Copyright(c) 2019, Thomas Klausner                                                                     *
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
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT      *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS    *
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR *
 * TORT(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF    *
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                                             *
 **********************************************************************************************************/

/* This extension waits for a signal (SIGUSR2). When it gets it, it
 * generates messages as quickly as possible to the configured target
 * host; a second SIGUSR2 signal will stop this.
 * If the target host starts with "REALM:", send to that Destination-Realm instead
 * of a particular host. */

#include <freeDiameter/extension.h>

#include <pthread.h>
#include <signal.h>

#define MODULE_NAME "test_ccload"

static pthread_t gen_thr  = (pthread_t)NULL;
struct disp_hdl *ccr_local_hdl;
struct fd_rt_fwd_hdl *ccr_fwd_hdl;
volatile int do_generate = 0;

const char *target = NULL;

struct dict_object * aai_avp_do; /* cache the Auth-Application-Id dictionary object */
struct dict_object * crn_avp_do; /* cache the CC-Request-Number dictionary object */
struct dict_object * crt_avp_do; /* cache the CC-Request-Type dictionary object */
struct dict_object * dh_avp_do; /* cache the Destination-Host dictionary object */
struct dict_object * dr_avp_do; /* cache the Destination-Realm dictionary object */
struct dict_object * rc_avp_do; /* cache the Result-Code dictionary object */
struct dict_object * sci_avp_do; /* cache the Service-Context-Id dictionary object */
struct dict_object * si_avp_do; /* cache the Session-Id dictionary object */
struct dict_object * pi_avp_do; /* cache the Proxy-Info dictionary object */
struct dict_object * ph_avp_do; /* cache the Proxy-Host dictionary object */
struct dict_object * ps_avp_do; /* cache the Proxy-State dictionary object */

struct dict_object * ccr_do; /* cache the Credit-Control-Request command dictionary object */

struct statistics {
	uint64_t sent;
	uint64_t success;
	uint64_t error;
	time_t first;
	time_t first_reply;
	time_t last;
	time_t last_reply;
} statistics;

void print_statistics(void) {
	uint64_t missing, received;

	if (statistics.first == 0 || statistics.last == 0 || statistics.last == statistics.first) {
		return;
	}

	received = statistics.error + statistics.success;
	missing = statistics.sent - received;

	fd_log_error("%s: %lld CCR messages sent in %llds (%.2f messages/second), %lld CCA messages received in %llds (%.2f messages/second), %lld success (%.2f%%), %lld errors (%.2f%%), %lld missing (%.2f%%)",
		     fd_g_config->cnf_diamid,
		     (long long)statistics.sent, (long long)(statistics.last-statistics.first), (float)statistics.sent / (statistics.last-statistics.first),
		     (long long)received, (long long)(statistics.last_reply-statistics.first_reply), (float)received / (statistics.last_reply-statistics.first_reply),
		     (long long)statistics.success,
		     100*(float)statistics.success/statistics.sent, (long long)statistics.error, 100*(float)statistics.error/statistics.sent,
		     missing, 100*(float)missing/statistics.sent);
}

static int handle_message(struct msg **msg) {
	struct msg_hdr *hdr = NULL;
	struct avp_hdr *ahdr = NULL;
	struct avp *rc;

	if (msg == NULL) {
		fd_log_error("[%s] NULL CCA message", MODULE_NAME);
		return -1;
	}

	CHECK_FCT(fd_msg_hdr(*msg, &hdr));
	/* handle CCAs only */
	if ((hdr->msg_code != 272) || (hdr->msg_flags & CMD_FLAG_REQUEST)) {
		fd_log_error("[%s] invalid message type (type %d, flags 0x%x)", MODULE_NAME, hdr->msg_code, hdr->msg_flags);
		return -1;
	}

	if (statistics.first_reply == 0) {
		statistics.first_reply = time(NULL);
	}
	/* Answer received, check it */
	if (fd_msg_search_avp(*msg, rc_avp_do, &rc) < 0 || rc == NULL) {
		fd_log_error("[%s] Result-Code not found in CCA", MODULE_NAME);
		return -1;
	}

	if (fd_msg_avp_hdr(rc, &ahdr) < 0) {
		fd_log_error("[%s] error parsing Result-Code in CCA", MODULE_NAME);
		return -1;
	}
	statistics.last_reply = time(NULL);
	fd_log_debug("Credit-Control-Answer with Result-Code %d received", ahdr->avp_value->i32);
	switch (ahdr->avp_value->i32/1000) {
	case 2:
		statistics.success++;
		break;
	default:
		statistics.error++;
		break;
	}
	if (statistics.sent - statistics.error - statistics.success == 0) {
		print_statistics();
	}

	return 0;
}

static int ccr_local_handler(struct msg ** msg, struct avp * avp, struct session * sess, void * data, enum disp_action * act)
{
	fd_log_debug("[%s] CCR local handler called", MODULE_NAME);

	if (handle_message(msg) < 0) {
		fd_log_error("dropping message");
		CHECK_FCT(fd_msg_free(*msg));
		*msg = NULL;
		return 0;
	}

	CHECK_FCT(fd_msg_free(*msg));
	*msg = NULL;
	return 0;
}

static int ccr_fwd_handler(void *cb_data, struct msg **msg)
{
	fd_log_debug("[%s] CCR FWD handler called", MODULE_NAME);

	if (handle_message(msg) < 0) {
		return 0;
	}

	return 0;
}

/* create message to send */
struct msg *create_message(const char *destination)
{
	struct msg *msg;
	struct avp *avp, *avp1;
	union avp_value val;
	struct msg_hdr *msg_hdr;
	const char *realm;
	char session_id[800];
	const char *service_context_id = "version2.clci.ipc@vodafone.com";
	const char *proxy_host = "Dummy-Proxy-Host-to-Increase-Package-Size";
	const char *proxy_state = "This is just data to increase the package size\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";

	if (fd_msg_new(ccr_do, MSGFL_ALLOC_ETEID, &msg) != 0) {
		fd_log_error("can't create new 'Credit-Control-Request' message");
		return NULL;
	}

	/* Application Id in header needs to be set to for since this Credit-Control-Request is for Diameter Credit Control */
	if (fd_msg_hdr(msg, &msg_hdr) != 0) {
		fd_log_error("can't get message header for 'Credit-Control-Request' message");
		fd_msg_free(msg);
		return NULL;
	}
	msg_hdr->msg_appl = 4;

	if (fd_msg_add_origin(msg, 0) != 0) {
		fd_log_error("can't set Origin for 'Credit-Control-Request' message");
		fd_msg_free(msg);
		return NULL;
	}

	if (strncmp("REALM:", target, 6) != 0) {
		/* Destination-Host */
		fd_msg_avp_new(dh_avp_do, 0, &avp);
		memset(&val, 0, sizeof(val));
		val.os.data = (uint8_t *)target;
		val.os.len = strlen(target);
		if (fd_msg_avp_setvalue(avp, &val) != 0) {
			fd_msg_free(msg);
			fd_log_error("can't set value for 'Destination-Host' for 'Credit-Control-Request' message");
			return NULL;
		}
		fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

		if ((realm = strchr(target, '.')) == NULL) {
			fd_msg_free(msg);
			fd_log_error("can't extract realm from host '%s'", target);
			return NULL;
		}
		/* skip dot */
		realm++;
	} else {
		realm = target + 6; /* skip "REALM:" */
	}
	/* Destination-Realm */
	fd_msg_avp_new(dr_avp_do, 0, &avp);
	memset(&val, 0, sizeof(val));
	val.os.data = (uint8_t *)realm;
	val.os.len = strlen(realm);
	if (fd_msg_avp_setvalue(avp, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'Destination-Realm' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

	/* Session-Id */
	snprintf(session_id, sizeof(session_id), "session %ld", random());
	fd_msg_avp_new(si_avp_do, 0, &avp);
	memset(&val, 0, sizeof(val));
	val.os.data = (uint8_t *)session_id;
	val.os.len = strlen(session_id);
	if (fd_msg_avp_setvalue(avp, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'Session-Id' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(msg, MSG_BRW_FIRST_CHILD, avp);

	/* Auth-Application-Id */
	fd_msg_avp_new(aai_avp_do, 0, &avp);
	memset(&val, 0, sizeof(val));
	val.i32 = 4;
	if (fd_msg_avp_setvalue(avp, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'Auth-Application-Id' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

	/* Service-Context-Id */
	fd_msg_avp_new(sci_avp_do, 0, &avp);
	memset(&val, 0, sizeof(val));
	val.os.data = (uint8_t *)service_context_id;
	val.os.len = strlen(service_context_id);
	if (fd_msg_avp_setvalue(avp, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'Service-Context-Id' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

	/* CC-Request-Type */
	fd_msg_avp_new(crt_avp_do, 0, &avp);
	memset(&val, 0, sizeof(val));
	val.i32 = 1; /* Initial */
	if (fd_msg_avp_setvalue(avp, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'CC-Request-Type' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

	/* CC-Request-Number */
	fd_msg_avp_new(crn_avp_do, 0, &avp);
	memset(&val, 0, sizeof(val));
	val.i32 = 1;
	if (fd_msg_avp_setvalue(avp, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'CC-Request-Number' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

	/* Proxy-Info */
	fd_msg_avp_new(pi_avp_do, 0, &avp);
	/* Proxy-Host */
	fd_msg_avp_new(ph_avp_do, 0, &avp1);
	memset(&val, 0, sizeof(val));
	val.os.data = (uint8_t *)proxy_host;
	val.os.len = strlen(proxy_host);
	if (fd_msg_avp_setvalue(avp1, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'Proxy-Host' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(avp, MSG_BRW_LAST_CHILD, avp1);
	/* Proxy-State */
	fd_msg_avp_new(ps_avp_do, 0, &avp1);
	memset(&val, 0, sizeof(val));
	val.os.data = (uint8_t *)proxy_state;
	val.os.len = strlen(proxy_state);
	if (fd_msg_avp_setvalue(avp1, &val) != 0) {
		fd_msg_free(msg);
		fd_log_error("can't set value for 'Proxy-State' for 'Credit-Control-Request' message");
		return NULL;
	}
	fd_msg_avp_add(avp, MSG_BRW_LAST_CHILD, avp1);
	fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp);

	return msg;
}

/* The thread that handles expired entries cleanup. */
void * gen_thr_fct(void * arg)
{
	struct msg *msg;
	fd_log_threadname ( "Loadtest/Generator" );

	do {
		if (do_generate) {
			time_t now;
			if (statistics.first == 0) {
				statistics.first = time(NULL);
			}
			msg = create_message(target);
			fd_msg_send(&msg, NULL, NULL);
			fd_log_debug("[%s] sent message", MODULE_NAME);
			now = time(NULL);
			if (statistics.last != now) {
				print_statistics();
			}
			statistics.last = time(NULL);
			statistics.sent++;
		} else {
			sleep(1);
		}
	} while (1);
}

/* signal handler */
static void sig_hdlr(void)
{
	if (do_generate) {
		do_generate = 0;
	} else {
		do_generate = 1;
	}
	fd_log_notice("%s: switched generation of CCRs %s", MODULE_NAME, do_generate ? "on" : "off");
}

/* entry hook: register callback */
static int cc_entry(char * conffile)
{
	struct disp_when data;

	/* initialize random number generator for session IDs */
	srandom(time(NULL) | (unsigned int)getpid());
	if ((target = conffile) == NULL) {
		fd_log_error("invalid conffile");
		return EINVAL;
	}

	memset(&data, 0, sizeof(data));

        /* Advertise the support for the Diameter Credit Control application in the peer */
        CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_APPLICATION, APPLICATION_BY_NAME, "Diameter Credit Control Application", &data.app, ENOENT) );
        CHECK_FCT( fd_disp_app_support ( data.app, NULL, 1, 0 ) );

	/* the handling of requests is different if it might be locally handled or not */
	/* for possibly locally handled requests, fd_rt_fwd_register is not called, so we need to add handlers for both cases */
	/* register forward handler for CCR -- needs to look at all requests */
	CHECK_FCT(fd_rt_fwd_register(ccr_fwd_handler, NULL, RT_FWD_REQ, &ccr_fwd_hdl));
	/* register local handler for CCR - if not Destination-Host is set, or the local host is used */
	memset(&data, 0, sizeof(data));
	CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Credit-Control-Answer", &data.command, ENOENT));
	CHECK_FCT(fd_disp_register(ccr_local_handler, DISP_HOW_CC, &data, NULL, &ccr_local_hdl));

	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Auth-Application-Id", &aai_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Auth-Application-Id' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "CC-Request-Number", &crn_avp_do, ENOENT),
		     { LOG_E("Unable to find 'CC-Request-Number' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "CC-Request-Type", &crt_avp_do, ENOENT),

		     { LOG_E("Unable to find 'CC-Request-Type' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Host", &dh_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Destination-Host' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Destination-Realm", &dr_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Destination-Realm' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Result-Code", &rc_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Result-Code' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Session-Id", &si_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Session-Id' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Service-Context-Id", &sci_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Service-Context-Id' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Proxy-Info", &pi_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Proxy-Info' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Proxy-Host", &ph_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Proxy-Host' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Proxy-State", &ps_avp_do, ENOENT),
		     { LOG_E("Unable to find 'Proxy-State' AVP in the loaded dictionaries."); });
	CHECK_FCT_DO(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME, "Credit-Control-Request", &ccr_do, ENOENT),
		     { LOG_E("Unable to find 'Credit-Control-Request' command in the loaded dictionaries."); });

	/* Start the generator thread */
	CHECK_POSIX( pthread_create( &gen_thr, NULL, gen_thr_fct, NULL ) );

	/* Register generator callback */
	CHECK_FCT(fd_event_trig_regcb(SIGUSR2, MODULE_NAME, sig_hdlr));

	return 0;
}

/* And terminate it */
void fd_ext_fini(void)
{
	/* stop sending */
	do_generate = 0;

	/* Stop the expiry thread */
	CHECK_FCT_DO( fd_thr_term(&gen_thr), );

	/* Unregister the callbacks */
	if (ccr_local_hdl) {
		CHECK_FCT_DO( fd_disp_unregister(&ccr_local_hdl, NULL), );
		ccr_local_hdl = NULL;
	}

	print_statistics();

	return;
}


EXTENSION_ENTRY(MODULE_NAME, cc_entry);
