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

/* 
 * Load-balancing routing extension for numeric-based User-Name values
 *
 *   This extension relies on a User-Name AVP in the format: "<number> text".
 *   It will load-balance the messages between the servers defined in the configuration,
 *  based on the number.
 *  For example, if the configuratino contains 3 servers A, B, C, then:
 * "1 first user" -> A
 * "2 second user" -> B
 * "3 third" -> C
 * "4 fourth" -> A
 * "99 and so on" -> C
 *
 *  The message is sent to the server selected only if it is in OPEN state, of course. 
 * Otherwise, this extension has no effect, and the default routing behavior will be used.
 *
 *  Note that the score added is FD_SCORE_LOAD_BALANCE, which mean any other routing 
 * indication will take precedence (for example Destination-Host AVP).
 */

#include "rtun.h"

/* The configuration structure */
struct rtun_conf rtun_conf;

/* The callback called on new messages */
static int rtun_out(void * cbdata, struct msg * msg, struct fd_list * candidates)
{
	struct avp * avp = NULL;
	
	TRACE_ENTRY("%p %p %p", cbdata, msg, candidates);
	
	CHECK_PARAMS(msg && candidates);
	
	/* Check if it is worth processing the message */
	if (FD_IS_LIST_EMPTY(candidates)) {
		return 0;
	}
	
	/* Now search the user-name AVP */
	CHECK_FCT( fd_msg_search_avp ( msg, rtun_conf.username, &avp ) );
	if (avp != NULL) {
		struct avp_hdr * ahdr = NULL;
		CHECK_FCT( fd_msg_avp_hdr ( avp, &ahdr ) );
		if (ahdr->avp_value != NULL) {
			int conv = 0;
			int idx;
			
			/* We cannot use strtol or sscanf functions without copying the AVP value and \0-terminate it... */
			for (idx = 0; idx < ahdr->avp_value->os.len; idx++) {
				char c = (char) ahdr->avp_value->os.data[idx];
				if (c == ' ')
					continue;
				if ((c >= '0') && (c <= '9')) {
					conv *= 10;
					conv += c - '0';
					continue;
				}
				/* we found a non-numeric character, stop */
				break;
			}
		
			if (conv) {
				/* We succeeded in reading a numerical value */
				struct fd_list * c;
				char *s;
				
				idx = conv % rtun_conf.serv_nb;
				s = rtun_conf.servs[idx];
				
				/* We should send the message to 's', search in the candidates list */
				for (c = candidates->next; c != candidates; c = c->next) {
					struct rtd_candidate * cand = (struct rtd_candidate *)c;
					
					if (strcmp(s, cand->diamid) == 0) {
						cand->score += FD_SCORE_LOAD_BALANCE;
						break;
					}
				}
			}
		}
	}
	
	return 0;
}

/* handler */
static struct fd_rt_out_hdl * rtun_hdl = NULL;

/* entry point */
static int rtun_entry(char * conffile)
{
	TRACE_ENTRY("%p", conffile);
	
	/* Initialize the configuration */
	memset(&rtun_conf, 0, sizeof(rtun_conf));
	
	/* Search the User-Name AVP in the dictionary */
	CHECK_FCT( fd_dict_search ( fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "User-Name", &rtun_conf.username, ENOENT ));
	
	/* Parse the configuration file */
	CHECK_FCT( rtun_conf_handle(conffile) );
	
	/* Check the configuration */
	CHECK_PARAMS_DO( rtun_conf.serv_nb > 1,
		{
			fd_log_debug("[rt_un_number] Invalid configuration: you need at least 2 servers to perform load-balancing.\n");
			return EINVAL;
		} );
	
	/* Register the callback */
	CHECK_FCT( fd_rt_out_register( rtun_out, NULL, 1, &rtun_hdl ) );
	
	/* We're done */
	return 0;
}

/* Unload */
void fd_ext_fini(void)
{
	int i;
	TRACE_ENTRY();
	
	/* Unregister the cb */
	CHECK_FCT_DO( fd_rt_out_unregister ( rtun_hdl, NULL ), /* continue */ );
	
	/* Destroy the data */
	if (rtun_conf.servs) 
		for (i = 0; i < rtun_conf.serv_nb; i++)
			free(rtun_conf.servs[i]);
	free(rtun_conf.servs);
	
	/* Done */
	return ;
}

EXTENSION_ENTRY("rt_un_number", rtun_entry);
