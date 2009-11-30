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

#include "fD.h"

/* Configuration management */

#ifndef GNUTLS_DEFAULT_PRIORITY
# define GNUTLS_DEFAULT_PRIORITY "NORMAL"
#endif /* GNUTLS_DEFAULT_PRIORITY */
#ifndef GNUTLS_DEFAULT_DHBITS
# define GNUTLS_DEFAULT_DHBITS 1024
#endif /* GNUTLS_DEFAULT_DHBITS */

/* Initialize the fd_g_config structure to default values */
int fd_conf_init()
{
	TRACE_ENTRY();
	
	fd_g_config->cnf_eyec = EYEC_CONFIG;
	fd_g_config->cnf_file = DEFAULT_CONF_FILE;
	
	fd_g_config->cnf_timer_tc = 30;
	fd_g_config->cnf_timer_tw = 30;
	
	fd_g_config->cnf_port     = 3868;
	fd_g_config->cnf_port_tls = 3869;
	fd_g_config->cnf_sctp_str = 30;
	fd_list_init(&fd_g_config->cnf_endpoints, NULL);
	fd_list_init(&fd_g_config->cnf_apps, NULL);
	#ifdef DISABLE_SCTP
	fd_g_config->cnf_flags.no_sctp = 1;
	#endif /* DISABLE_SCTP */
	
	fd_g_config->cnf_orstateid = (uint32_t) time(NULL);
	
	CHECK_FCT( fd_dict_init(&fd_g_config->cnf_dict) );
	CHECK_FCT( fd_fifo_new(&fd_g_config->cnf_main_ev) );
	
	/* TLS parameters */
	CHECK_GNUTLS_DO( gnutls_certificate_allocate_credentials (&fd_g_config->cnf_sec_data.credentials), return ENOMEM );
	CHECK_GNUTLS_DO( gnutls_dh_params_init (&fd_g_config->cnf_sec_data.dh_cache), return ENOMEM );

	return 0;
}

void fd_conf_dump()
{
	if (!TRACE_BOOL(INFO))
		return;
	
	fd_log_debug("-- Configuration :\n");
	fd_log_debug("  Debug trace level ...... : %+d\n", fd_g_debug_lvl);
	fd_log_debug("  Configuration file ..... : %s\n", fd_g_config->cnf_file);
	fd_log_debug("  Diameter Identity ...... : %s (l:%Zi)\n", fd_g_config->cnf_diamid, fd_g_config->cnf_diamid_len);
	fd_log_debug("  Diameter Realm ......... : %s (l:%Zi)\n", fd_g_config->cnf_diamrlm, fd_g_config->cnf_diamrlm_len);
	fd_log_debug("  Tc Timer ............... : %u\n", fd_g_config->cnf_timer_tc);
	fd_log_debug("  Tw Timer ............... : %u\n", fd_g_config->cnf_timer_tw);
	fd_log_debug("  Local port ............. : %hu\n", fd_g_config->cnf_port);
	fd_log_debug("  Local secure port ...... : %hu\n", fd_g_config->cnf_port_tls);
	fd_log_debug("  Number of SCTP streams . : %hu\n", fd_g_config->cnf_sctp_str);
	if (FD_IS_LIST_EMPTY(&fd_g_config->cnf_endpoints)) {
		fd_log_debug("  Local endpoints ........ : Default (use all available)\n");
	} else {
		struct fd_list * li = fd_g_config->cnf_endpoints.next;
		fd_log_debug("  Local endpoints ........ : \n");
		fd_ep_dump( 29, &fd_g_config->cnf_endpoints );
	}
	if (FD_IS_LIST_EMPTY(&fd_g_config->cnf_apps)) {
		fd_log_debug("  Local applications ..... : (none)\n");
	} else {
		struct fd_list * li = fd_g_config->cnf_apps.next;
		fd_log_debug("  Local applications ..... : ");
		while (li != &fd_g_config->cnf_apps) {
			struct fd_app * app = (struct fd_app *)li;
			if (li != fd_g_config->cnf_apps.next) fd_log_debug("                             ");
			fd_log_debug("App: %u\t%s%s%s\tVnd: %u\n", 
					app->appid,
					app->flags.auth ? "Au" : "--",
					app->flags.acct ? "Ac" : "--",
					app->flags.common ? "C" : "-",
					app->vndid);
			li = li->next;
		}
	}
	
	fd_log_debug("  Flags : - IP ........... : %s\n", fd_g_config->cnf_flags.no_ip4 ? "DISABLED" : "Enabled");
	fd_log_debug("          - IPv6 ......... : %s\n", fd_g_config->cnf_flags.no_ip6 ? "DISABLED" : "Enabled");
	fd_log_debug("          - Relay app .... : %s\n", fd_g_config->cnf_flags.no_fwd ? "DISABLED" : "Enabled");
	fd_log_debug("          - TCP .......... : %s\n", fd_g_config->cnf_flags.no_tcp ? "DISABLED" : "Enabled");
	#ifdef DISABLE_SCTP
	fd_log_debug("          - SCTP ......... : DISABLED (at compilation)\n");
	#else /* DISABLE_SCTP */
	fd_log_debug("          - SCTP ......... : %s\n", fd_g_config->cnf_flags.no_sctp ? "DISABLED" : "Enabled");
	#endif /* DISABLE_SCTP */
	fd_log_debug("          - Pref. proto .. : %s\n", fd_g_config->cnf_flags.pr_tcp ? "TCP" : "SCTP");
	fd_log_debug("          - TLS method ... : %s\n", fd_g_config->cnf_flags.tls_alg ? "INBAND" : "Separate port");
	
	fd_log_debug("  TLS :   - Certificate .. : %s\n", fd_g_config->cnf_sec_data.cert_file ?: "(NONE)");
	fd_log_debug("          - Private key .. : %s\n", fd_g_config->cnf_sec_data.key_file ?: "(NONE)");
	fd_log_debug("          - CA (trust) ... : %s\n", fd_g_config->cnf_sec_data.ca_file ?: "(none)");
	fd_log_debug("          - CRL .......... : %s\n", fd_g_config->cnf_sec_data.crl_file ?: "(none)");
	fd_log_debug("          - Priority ..... : %s\n", fd_g_config->cnf_sec_data.prio_string ?: "(default: '" GNUTLS_DEFAULT_PRIORITY "')");
	fd_log_debug("          - DH bits ...... : %d\n", fd_g_config->cnf_sec_data.dh_bits ?: GNUTLS_DEFAULT_DHBITS);
	
	fd_log_debug("  Origin-State-Id ........ : %u\n", fd_g_config->cnf_orstateid);
}

/* Parse the configuration file (using the yacc parser) */
int fd_conf_parse()
{
	extern FILE * fddin;
	
	TRACE_DEBUG (FULL, "Parsing configuration file: %s", fd_g_config->cnf_file);
	
	fddin = fopen(fd_g_config->cnf_file, "r");
	if (fddin == NULL) {
		int ret = errno;
		fprintf(stderr, "Unable to open configuration file %s for reading: %s\n", fd_g_config->cnf_file, strerror(ret));
		return ret;
	}
	
	/* call yacc parser */
	CHECK_FCT(  fddparse(fd_g_config)  );
	
	/* close the file */
	fclose(fddin);
	
	/* Check that TLS private key was given */
	if (! fd_g_config->cnf_sec_data.key_file) {
		fprintf(stderr, "Missing private key configuration for TLS. Please provide the TLS_cred configuration directive.\n");
		return EINVAL;
	}
	
	/* Resolve hostname if not provided */
	if (fd_g_config->cnf_diamid == NULL) {
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 1024
#endif /* HOST_NAME_MAX */
		char buf[HOST_NAME_MAX + 1];
		struct addrinfo hints, *info;
		int ret;
		
		/* local host name */
		CHECK_SYS(gethostname(buf, sizeof(buf)));
		
		/* get FQDN */
		memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_CANONNAME;

		ret = getaddrinfo(buf, NULL, &hints, &info);
		if (ret != 0) {
			fprintf(stderr, "Error resolving local FQDN :\n"
					" '%s' : %s\n"
					"Please provide LocalIdentity in configuration file.\n",
					buf, gai_strerror(ret));
			return EINVAL;
		}
		CHECK_MALLOC( fd_g_config->cnf_diamid = strdup(info->ai_canonname) );
		freeaddrinfo(info);
	}
	
	/* cache the length of the diameter id for the session module */
	fd_g_config->cnf_diamid_len = strlen(fd_g_config->cnf_diamid);
	
	/* Handle the realm part */
	if (fd_g_config->cnf_diamrlm == NULL) {
		char * start = NULL;
		
		/* Check the diameter identity is a fqdn */
		start = strchr(fd_g_config->cnf_diamid, '.');
		if ((start == NULL) || (start[1] == '\0')) {
			fprintf(stderr, "Unable to extract realm from the LocalIdentity '%s'.\n"
					"Please fix your LocalIdentity setting or provide LocalRealm.\n",
					fd_g_config->cnf_diamid);
			return EINVAL;
		}		
		
		CHECK_MALLOC( fd_g_config->cnf_diamrlm = strdup( start + 1 )  ); 
	}
	fd_g_config->cnf_diamrlm_len = strlen(fd_g_config->cnf_diamrlm);
	
	/* Validate some flags */
	if (fd_g_config->cnf_flags.no_ip4 && fd_g_config->cnf_flags.no_ip6) {
		fprintf(stderr, "IP and IPv6 cannot be disabled at the same time.\n");
		return EINVAL;
	}
	if (fd_g_config->cnf_flags.no_tcp && fd_g_config->cnf_flags.no_sctp) {
		fprintf(stderr, "TCP and SCTP cannot be disabled at the same time.\n");
		return EINVAL;
	}
	
	/* Validate local endpoints */
	if ((!FD_IS_LIST_EMPTY(&fd_g_config->cnf_endpoints)) && (fd_g_config->cnf_flags.no_ip4 || fd_g_config->cnf_flags.no_ip6)) {
		struct fd_list * li;
		for ( li = fd_g_config->cnf_endpoints.next; li != &fd_g_config->cnf_endpoints; li = li->next) {
			struct fd_endpoint * ep = (struct fd_endpoint *)li;
			if ( (fd_g_config->cnf_flags.no_ip4 && (ep->sa.sa_family == AF_INET))
			   ||(fd_g_config->cnf_flags.no_ip6 && (ep->sa.sa_family == AF_INET6)) ) {
				li = li->prev;
				fd_list_unlink(&ep->chain);
				if (TRACE_BOOL(INFO)) {
					fd_log_debug("Info: Removing local address conflicting with the flags no_IP / no_IP6 : ");
					sSA_DUMP_NODE( &ep->sa, AI_NUMERICHOST );
					fd_log_debug("\n");
				}
				free(ep);
			}
		}
	}
	
	/* Configure TLS default parameters */
	if (! fd_g_config->cnf_sec_data.prio_string) {
		const char * err_pos = NULL;
		CHECK_GNUTLS_DO( gnutls_priority_init( 
					&fd_g_config->cnf_sec_data.prio_cache,
					GNUTLS_DEFAULT_PRIORITY,
					&err_pos),
				 { TRACE_DEBUG(INFO, "Error in priority string at position : %s", err_pos); return EINVAL; } );
	}
	if (! fd_g_config->cnf_sec_data.dh_bits) {
		TRACE_DEBUG(INFO, "Generating Diffie-Hellman parameters of size %d (this takes a few seconds)... ", GNUTLS_DEFAULT_DHBITS);
		CHECK_GNUTLS_DO( gnutls_dh_params_generate2( 
					fd_g_config->cnf_sec_data.dh_cache,
					GNUTLS_DEFAULT_DHBITS),
				 { TRACE_DEBUG(INFO, "Error in DH bits value : %d", GNUTLS_DEFAULT_DHBITS); return EINVAL; } );
	}
	
	return 0;
}
