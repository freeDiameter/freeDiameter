/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2013, WIDE Project and NICT								 *
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

#include "tests.h"
#include "../libfdcore/discovery_config.h"
#include <freeDiameter/discovery.h>

const char * ids[] = { "b11", "b14", "b1", "b4" };
#define DomainName "localdomain"

static int parse_peer_configuration(const char *text)
{
	extern void fddrestart(FILE *input);
	FILE *input;
	int ret;

	input = tmpfile();
	CHECK(1, input != NULL);
	CHECK(1, fputs(text, input) >= 0);
	CHECK(0, fflush(input));
	CHECK(0, fseek(input, 0, SEEK_SET));
	fddrestart(input);
	ret = fddparse(fd_g_config);
	CHECK(0, fclose(input));
	return ret;
}

static void test_sctp_security_configuration(void)
{
	struct peer_hdr *peer;
	CHECK(0, parse_peer_configuration("ConnectPeer = \"legacy.localdomain\" { SctpSec3436; Port = 15658; };\n"
#ifdef ENABLE_DTLS_SCTP
		"ConnectPeer = \"dtls.localdomain\" { No_TCP; SctpSecDTLS; };\n"
#endif
		));

	CHECK(0, fd_peer_getbyid("legacy.localdomain", strlen("legacy.localdomain"), 0, &peer));
	CHECK(PI_SCTPSEC_3436, peer->info.config.pic_flags.sctpsec);
	CHECK(15658, peer->info.config.pic_port);
#ifdef ENABLE_DTLS_SCTP
	CHECK(0, fd_peer_getbyid("dtls.localdomain", strlen("dtls.localdomain"), 0, &peer));
	CHECK(PI_SCTPSEC_DTLS, peer->info.config.pic_flags.sctpsec);
#endif
}

static void test_discovery_configuration(void)
{
	static const char *invalid[] = {
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; };",
		"DiscoverPeer = \"example.org\" { AuthorizedPeer = \"peer.example.org\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4294967296\"; AuthorizedPeer = \"peer.example.org\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"x\"; AuthorizedPeer = \"peer.example.org\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; ApplicationId = \"5\"; AuthorizedPeer = \"peer.example.org\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; AuthorizedPeer = \"PEER.EXAMPLE.ORG\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; };"
		"DiscoverPeer = \"EXAMPLE.ORG\" { ApplicationId = \"4\"; AuthorizedPeer = \"other.example.org\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; No_IP; No_IPv6; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; No_TCP; No_SCTP; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; TLS_old_method; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; Port = 3868; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\"; Realm = \"bad realm\"; };",
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4\"; AuthorizedPeer = \"peer.example.org\";"
	};
	size_t i;
	CHECK(0, parse_peer_configuration(
		"DiscoverPeer = \"example.org\" { ApplicationId = \"4294967295\"; "
		"AuthorizedPeer = \"peer.example.org\"; AuthorizedPeer = \"other.example.org\"; "
		"No_IPv6; No_SCTP; Prefer_TCP; TcTimer = 60; TwTimer = 45; "
		"TLS_Prio = \"NORMAL\"; Realm = \"origin.example.org\"; };"));
	CHECK(0, fd_disc_conf_fini());
	CHECK(0, parse_peer_configuration(
		"DiscoverPeer = \"example.org\" { ApplicationId = \"0\"; "
		"AuthorizedPeer = \"peer.example.org\"; };"));
	CHECK(0, fd_disc_conf_fini());
#if defined(ENABLE_DTLS_SCTP) && !defined(DISABLE_SCTP)
	CHECK(0, parse_peer_configuration(
		"DiscoverPeer = \"example.org\" { ApplicationId = \"0\"; "
		"AuthorizedPeer = \"peer.example.org\"; No_TCP; };"));
	CHECK(0, fd_disc_conf_fini());
#endif
	for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		CHECK(1, parse_peer_configuration(invalid[i]) != 0);
		CHECK(0, fd_disc_conf_fini());
	}
}

static void test_sctp_security_rejections(void)
{
	static const char *cases[] = {
#ifndef ENABLE_DTLS_SCTP
		"ConnectPeer = \"bad.localdomain\" { SctpSecDTLS; };",
#endif
		"ConnectPeer = \"bad.localdomain\" { Realm = \"example.org\"; TLS_Prio = \"NORMAL\"; ConnectTo = \"127.0.0.1\"; SctpSecDTLS; SctpSec3436; };",
		"ConnectPeer = \"bad.localdomain\" { SctpSec3436; SctpSecDTLS; };",
		"ConnectPeer = \"bad.localdomain\" { Realm = \"example.org\"; TLS_Prio = \"NORMAL\"; ConnectTo = \"127.0.0.1\"; SctpSecDTLS; No_TLS; };",
		"ConnectPeer = \"bad.localdomain\" { No_TLS; SctpSecDTLS; };",
		"ConnectPeer = \"bad.localdomain\" { SctpSecDTLS; TLS_old_method; };",
		"ConnectPeer = \"bad.localdomain\" { TLS_old_method; SctpSecDTLS; };",
		"TLS_old_method; ConnectPeer = \"bad.localdomain\" { SctpSecDTLS; };",
		"ConnectPeer = \"global-after.localdomain\" { SctpSecDTLS; }; TLS_old_method;"
	};
	size_t i;
	int saved_tls_alg = fd_g_config->cnf_flags.tls_alg;

	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		fd_g_config->cnf_flags.tls_alg = 0;
		CHECK(1, parse_peer_configuration(cases[i]) != 0);
	}
	fd_g_config->cnf_flags.tls_alg = 0;
	CHECK(0, parse_peer_configuration(
		"ConnectPeer = \"default.localdomain\" { };"
		"ConnectPeer = \"global-legacy.localdomain\" { SctpSec3436; };"
		"TLS_old_method;"));
	fd_g_config->cnf_flags.tls_alg = saved_tls_alg;
}

static void test_discovery_peer(void)
{
	struct fd_disc_dns_candidate candidate;
	struct fd_discovery_peer discovery;
	struct peer_hdr *peer, *duplicate;
	int disabled = fd_g_config->cnf_sec_data.tls_disabled;
	memset(&candidate, 0, sizeof(candidate));
	memset(&discovery, 0, sizeof(discovery));
	candidate.protocol = FD_DISC_DNS_TLS_TCP;
	candidate.port = 15658;
	candidate.ttl = 60;
	candidate.address_count = 1;
	candidate.addresses[0].length = 4;
	candidate.addresses[0].bytes[0] = 127;
	candidate.addresses[0].bytes[3] = 1;
	strcpy(candidate.target, "dns-target.localdomain");
	discovery.identity = "authorized.localdomain";
	discovery.realm = "localdomain";
	discovery.candidate = &candidate;
	discovery.options.ip_family = FD_DISCOVERY_IPV4;
	discovery.options.tc_timer = 60;
	discovery.options.tw_timer = 45;
	discovery.options.tls_priority = "NORMAL";
	discovery.options.peer_realm = "origin.localdomain";
	discovery.token = 1;
	CHECK(0, clock_gettime(CLOCK_REALTIME, &discovery.expires));
	CHECK(ETIMEDOUT, fd_peer_add_discovered(&discovery, NULL, NULL));
	discovery.expires.tv_sec += 60;
	candidate.protocol = FD_DISC_DNS_TCP;
	CHECK(ENOTSUP, fd_peer_add_discovered(&discovery, NULL, NULL));
	candidate.protocol = FD_DISC_DNS_TLS_TCP;
	fd_g_config->cnf_sec_data.tls_disabled = 1;
	CHECK(ENOTSUP, fd_peer_add_discovered(&discovery, NULL, NULL));
	fd_g_config->cnf_sec_data.tls_disabled = 0;
	CHECK(0, fd_peer_add_discovered(&discovery, NULL, NULL));
	CHECK(0, fd_peer_getbyid((DiamId_t)discovery.identity, strlen(discovery.identity), 0, &peer));
	CHECK(1, peer != NULL);
	CHECK(0, strcmp(discovery.identity, peer->info.pi_diamid));
	CHECK(15658, peer->info.config.pic_port);
	CHECK(PI_P3_IP, peer->info.config.pic_flags.pro3);
	CHECK(60, peer->info.config.pic_tctimer);
	CHECK(45, peer->info.config.pic_twtimer);
	CHECK(0, strcmp("NORMAL", peer->info.config.pic_priority));
	CHECK(0, strcmp("origin.localdomain", peer->info.config.pic_realm));
	CHECK(0, strcmp("localdomain", ((struct fd_peer *)peer)->p_discovery_realm));
	CHECK(0, FD_IS_LIST_EMPTY(&peer->info.pi_endpoints));
	CHECK(0, fd_peer_getbyid(candidate.target, strlen(candidate.target), 0, &duplicate));
	CHECK(1, duplicate == NULL);
	discovery.identity = "legacy.localdomain";
	CHECK(EEXIST, fd_peer_add_discovered(&discovery, NULL, NULL));
	CHECK(0, fd_peer_getbyid((DiamId_t)discovery.identity, strlen(discovery.identity), 0, &duplicate));
	CHECK(1, ((struct fd_peer *)duplicate)->p_discovery == NULL);
	CHECK(ENOENT, fd_peer_discovery_cancel(discovery.identity, discovery.token));
	discovery.identity = "authorized.localdomain";
	CHECK(ENOENT, fd_peer_discovery_cancel(discovery.identity, discovery.token + 1));
	CHECK(0, fd_peer_discovery_cancel(discovery.identity, discovery.token));
	CHECK(0, fd_peer_getbyid((DiamId_t)discovery.identity, strlen(discovery.identity), 0, &duplicate));
	CHECK(1, duplicate == NULL);
	CHECK(ETIMEDOUT, fd_psm_change_state((struct fd_peer *)peer, STATE_OPEN));
	discovery.token = 3;
	CHECK(0, fd_peer_add_discovered(&discovery, NULL, NULL));
	CHECK(0, fd_peer_discovery_cancel(discovery.identity, discovery.token));
	fd_g_config->cnf_sec_data.tls_disabled = disabled;
}

static pthread_mutex_t discovery_cb_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t discovery_cb_cond = PTHREAD_COND_INITIALIZER;
static int discovery_cb_count;
static int discovery_cb_success;

static void discovery_callback(int status, void *unused)
{
	pthread_mutex_lock(&discovery_cb_lock);
	discovery_cb_count++;
	discovery_cb_success = status == 0;
	pthread_cond_signal(&discovery_cb_cond);
	pthread_mutex_unlock(&discovery_cb_lock);
}

static void test_discovery_deadline(void)
{
	struct fd_disc_dns_candidate candidate;
	struct fd_discovery_peer discovery;
	struct peer_hdr *peer;
	struct timespec wait;
	int disabled = fd_g_config->cnf_sec_data.tls_disabled;
	memset(&candidate, 0, sizeof(candidate));
	memset(&discovery, 0, sizeof(discovery));
	candidate.protocol = FD_DISC_DNS_TLS_TCP;
	candidate.port = 15658;
	candidate.ttl = 60;
	candidate.address_count = 1;
	candidate.addresses[0].length = 4;
	candidate.addresses[0].bytes[0] = 127;
	candidate.addresses[0].bytes[3] = 1;
	strcpy(candidate.target, "dns-target.localdomain");
	discovery.identity = "deadline.localdomain";
	discovery.realm = "localdomain";
	discovery.candidate = &candidate;
	discovery.token = 2;
	CHECK(0, clock_gettime(CLOCK_REALTIME, &discovery.expires));
	discovery.expires.tv_sec++;
	fd_g_config->cnf_sec_data.tls_disabled = 0;
	CHECK(0, fd_peer_add_discovered(&discovery, discovery_callback, NULL));
	CHECK(0, fd_peer_getbyid((DiamId_t)discovery.identity, strlen(discovery.identity), 0, &peer));
	CHECK(0, fd_peer_discovery_is_open(discovery.identity, discovery.token));
	/* DNS validity expires even while the PSM is waiting for core startup. */
	usleep(1100000);
	CHECK(0, fd_psm_start());
	CHECK(0, clock_gettime(CLOCK_REALTIME, &wait));
	wait.tv_sec += 5;
	CHECK(0, pthread_mutex_lock(&discovery_cb_lock));
	while (!discovery_cb_count)
		CHECK(0, pthread_cond_timedwait(&discovery_cb_cond, &discovery_cb_lock, &wait));
	CHECK(1, discovery_cb_count);
	CHECK(0, discovery_cb_success);
	CHECK(0, pthread_mutex_unlock(&discovery_cb_lock));
	CHECK(0, fd_peer_getbyid((DiamId_t)discovery.identity, strlen(discovery.identity), 0, &peer));
	CHECK(1, peer == NULL);
	CHECK(0, fd_peer_discovery_is_open(discovery.identity, discovery.token));
	fd_g_config->cnf_sec_data.tls_disabled = disabled;
}

static void test_discovery_reaping(void)
{
	struct fd_disc_dns_candidate candidate;
	struct fd_discovery_peer discovery;
	struct timespec wait;
	size_t reaped = 0;
	int i, disabled = fd_g_config->cnf_sec_data.tls_disabled;
	memset(&candidate, 0, sizeof(candidate));
	memset(&discovery, 0, sizeof(discovery));
	candidate.protocol = FD_DISC_DNS_TLS_TCP;
	candidate.port = 15658;
	candidate.ttl = 60;
	candidate.address_count = 1;
	candidate.addresses[0].length = 4;
	candidate.addresses[0].bytes[0] = 127;
	candidate.addresses[0].bytes[3] = 1;
	strcpy(candidate.target, "dns-target.localdomain");
	discovery.identity = "reaped.localdomain";
	discovery.realm = "localdomain";
	discovery.candidate = &candidate;
	CHECK(0, clock_gettime(CLOCK_REALTIME, &discovery.expires));
	discovery.expires.tv_sec += 60;
	CHECK(0, pthread_mutex_lock(&discovery_cb_lock));
	discovery_cb_count = 0;
	CHECK(0, pthread_mutex_unlock(&discovery_cb_lock));
	fd_g_config->cnf_sec_data.tls_disabled = 0;
	for (i = 0; i < 20; i++) {
		int ret;
		discovery.token = 100 + i;
		CHECK(0, fd_peer_add_discovered(&discovery, discovery_callback, NULL));
		ret = fd_peer_discovery_cancel(discovery.identity, discovery.token);
		CHECK(1, ret == 0 || ret == ENOENT || ret == EPIPE);
	}
	CHECK(0, clock_gettime(CLOCK_REALTIME, &wait));
	wait.tv_sec += 5;
	CHECK(0, pthread_mutex_lock(&discovery_cb_lock));
	while (discovery_cb_count < 20)
		CHECK(0, pthread_cond_timedwait(&discovery_cb_cond, &discovery_cb_lock, &wait));
	CHECK(20, discovery_cb_count);
	CHECK(0, discovery_cb_success);
	CHECK(0, pthread_mutex_unlock(&discovery_cb_lock));
	/* Callbacks finish before the PSM completion flag; reap only afterward. */
	for (i = 0; i < 100; i++) {
		reaped += fd_peer_discovery_reap();
		usleep(10000);
	}
	CHECK(1, reaped >= 20);
	CHECK(0, fd_peer_discovery_reap());
	fd_g_config->cnf_sec_data.tls_disabled = disabled;
}

struct dpr_observation {
	struct fd_peer *peer;
	int calls, status, state;
};

static void dpr_callback(int status, void *data)
{
	struct dpr_observation *observation = data;
	observation->calls++;
	observation->status = status;
	observation->state = fd_peer_getstate(observation->peer);
}

static void test_discovery_dpr(void)
{
	static const struct { uint32_t cause; int status, authorized; } cases[] = {
		{ ACV_DC_BUSY, FD_DISCOVERY_PEER_BUSY, 1 },
		{ ACV_DC_NOT_FRIEND, FD_DISCOVERY_PEER_NOT_FRIEND, 1 },
		{ ACV_DC_REBOOTING, ECONNRESET, 1 },
		{ ACV_DC_BUSY, ECONNRESET, 0 }
	};
	size_t i;
	CHECK(0, fd_msg_init());
	for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		struct fd_peer *peer = NULL;
		struct msg *msg = NULL;
		struct avp *avp;
		union avp_value value;
		struct dpr_observation observation;
		CHECK(0, fd_peer_alloc(&peer));
		peer->p_hdr.info.pi_diamid = strdup("dpr.localdomain");
		CHECK(1, peer->p_hdr.info.pi_diamid != NULL);
		peer->p_discovery = calloc(1, sizeof(*peer->p_discovery));
		CHECK(1, peer->p_discovery != NULL);
		peer->p_state = STATE_OPEN;
		peer->p_discovery_open_reported = cases[i].authorized;
		memset(&observation, 0, sizeof(observation));
		observation.peer = peer;
		peer->p_discovery_cb = dpr_callback;
		peer->p_discovery_cb_data = &observation;
		CHECK(0, fd_msg_new(fd_dict_cmd_DPR, 0, &msg));
		CHECK(0, fd_msg_avp_new(fd_dict_avp_DC, 0, &avp));
		value.u32 = cases[i].cause;
		CHECK(0, fd_msg_avp_setvalue(avp, &value));
		CHECK(0, fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp));
		/* No transport is installed: the DPA cannot be sent. */
		CHECK(1, fd_p_dp_handle(&msg, 1, peer) != 0);
		if (!cases[i].authorized) {
			CHECK(0, observation.calls);
			fd_peer_discovery_notify(peer, ECONNRESET);
		}
		CHECK(1, observation.calls);
		CHECK(cases[i].status, observation.status);
		if (cases[i].authorized)
			CHECK(STATE_OPEN, observation.state);
		fd_peer_discovery_notify(peer, ECONNRESET);
		CHECK(1, observation.calls);
		if (msg)
			CHECK(0, fd_msg_free(msg));
		CHECK(0, fd_peer_free(&peer));
	}
}

/* Main test routine */
int main(int argc, char *argv[])
{
	/* First, initialize the daemon modules */
	INIT_FD();
	test_discovery_dpr();
	test_discovery_configuration();
	test_sctp_security_configuration();
	test_sctp_security_rejections();
	test_discovery_peer();
	
	/* Create 4 peers with these ids */
	{
		int i;
		struct peer_info inf;
		char locid[255];
		memset(&inf, 0, sizeof(inf));
		inf.pi_diamid = (char *)locid;
		for (i=0; i < sizeof(ids) / sizeof(ids[0]); i++) {
			snprintf(locid, sizeof(locid), "%s." DomainName, ids[i]);
			CHECK( 0, fd_peer_add(&inf, __FILE__, NULL, NULL));
		}
	}
	fd_log_debug("%s", fd_peer_dump_list(FD_DUMP_TEST_PARAMS, 0));
	/* Check we are able to find again any of these */
	{
		int i;
		char locid[255];
		struct peer_hdr *p;
		for (i=0; i < sizeof(ids) / sizeof(ids[0]); i++) {
			snprintf(locid, sizeof(locid), "%s." DomainName, ids[i]);
			CHECK( 0, fd_peer_getbyid((DiamId_t)locid, strlen((char *)locid), 0, &p));
			CHECK( 0, strcmp((char *)locid, p->info.pi_diamid));
			CHECK( 0, fd_peer_getbyid((DiamId_t)locid, strlen((char *)locid), 1, &p));
			CHECK( 0, strcmp((char *)locid, p->info.pi_diamid));
		}
	}

	/* Forced disconnection removes peers from lookup but keeps outstanding references valid. */
	{
		struct peer_hdr *old, *found;
		const char *id = "b11." DomainName;
		CHECK(0, fd_peer_getbyid((DiamId_t)id, strlen(id), 0, &old));
		CHECK(1, old != NULL);
		CHECK(0, fd_connections_shutdown());
		CHECK(0, fd_peer_getbyid((DiamId_t)id, strlen(id), 0, &found));
		CHECK(1, found == NULL);
		CHECK(0, strcmp(id, old->info.pi_diamid));
	}
	test_discovery_deadline();
	test_discovery_reaping();
	
	
	/* That's all for the tests yet */
	PASSTEST();
} 
	
