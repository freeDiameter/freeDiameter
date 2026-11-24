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

/* Main test routine */
int main(int argc, char *argv[])
{
	/* First, initialize the daemon modules */
	INIT_FD();
	test_sctp_security_configuration();
	test_sctp_security_rejections();
	
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
	
	
	/* That's all for the tests yet */
	PASSTEST();
} 
	
