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

/* This file contains code to handle Disconnect Peer messages (DPR and DPA) */

/* Handle a received message */
int fd_p_dp_handle(struct msg ** msg, int req, struct fd_peer * peer)
{
	TRACE_ENTRY("%p %d %p", msg, req, peer);
	
	if (req) {
		/* We received a DPR, save the Disconnect-Cause and terminate the connection */
		struct avp * dc;
		int delay = peer->p_hdr.info.config.pic_tctimer ?: fd_g_config->cnf_timer_tc;
		
		CHECK_FCT_DO( fd_msg_search_avp ( *msg, fd_dict_avp_DC, &dc ), return );
		if (dc) {
			/* Check the value is consistent with the saved one */
			struct avp_hdr * hdr;
			CHECK_FCT_DO(  fd_msg_avp_hdr( dc, &hdr ), return  );
			if (hdr->avp_value == NULL) {
				/* This is a sanity check */
				TRACE_DEBUG(NONE, "BUG: Unset value in Disconnect-Cause in DPR");
				fd_msg_dump_one(NONE, dc);
				ASSERT(0); /* To check if this really happens, and understand why... */
			}

			peer->p_hdr.info.runtime.pir_lastDC = hdr->avp_value->u32;
			
			switch (hdr->avp_value->u32) {
				case ACV_DC_REBOOTING:
				default:
					/* We use TcTimer to attempt reconnection */
					break;
				case ACV_DC_BUSY:
					/* No need to hammer the overloaded peer */
					delay *= 10;
					break;
				case ACV_DC_NOT_FRIEND:
					/* He does not want to speak to us... let's retry a lot later maybe */
					delay *= 200;
					break;
			}
		}
		if (TRACE_BOOL(INFO)) {
			if (dc) {
				struct dict_object * dictobj = NULL;
				struct dict_enumval_request er;
				memset(&er, 0, sizeof(er));
				CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_TYPE, TYPE_OF_AVP, fd_dict_avp_DC, &er.type_obj, ENOENT )  );
				er.search.enum_value.u32 = peer->p_hdr.info.runtime.pir_lastDC;
				CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_ENUMVAL, ENUMVAL_BY_STRUCT, &er, &dictobj, 0 )  );
				if (dictobj) {
					CHECK_FCT( fd_dict_getval( dictobj, &er.search ) );
					fd_log_debug("Peer '%s' sent a DPR with cause: %s\n", peer->p_hdr.info.pi_diamid, er.search.enum_name);
				} else {
					fd_log_debug("Peer '%s' sent a DPR with unknown cause: %u\n", peer->p_hdr.info.pi_diamid, peer->p_hdr.info.runtime.pir_lastDC);
				}
			} else {
				fd_log_debug("Peer '%s' sent a DPR without Disconnect-Cause AVP\n", peer->p_hdr.info.pi_diamid);
			}
		}
		
		/* Now reply with a DPA */
		CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, msg, 0 ) );
		CHECK_FCT( fd_msg_rescode_set( *msg, "DIAMETER_SUCCESS", NULL, NULL, 1 ) );
		
		/* Move to CLOSING state to failover outgoing messages (and avoid failing the DPA...) */
		CHECK_FCT( fd_psm_change_state(peer, STATE_CLOSING) );
		
		/* Now send the DPA */
		CHECK_FCT( fd_out_send( msg, NULL, peer) );
		
		/* Move to CLOSED state */
		fd_psm_cleanup(peer, 0);
		
		/* Reset the timer for next connection attempt -- we'll retry sooner or later depending on the disconnection cause */
		fd_psm_next_timeout(peer, 1, delay);
		
	} else {
		/* We received a DPA */
		if (peer->p_hdr.info.runtime.pir_state != STATE_CLOSING) {
			TRACE_DEBUG(INFO, "Ignore DPA received in state %s", STATE_STR(peer->p_hdr.info.runtime.pir_state));
		}
			
		/* In theory, we should control the Result-Code AVP. But since we will not go back to OPEN state here anyway, let's skip it */
		CHECK_FCT_DO( fd_msg_free( *msg ), /* continue */ );
		*msg = NULL;
		
		/* The calling function handles cleaning the PSM and terminating the peer since we return in CLOSING state */
	}
	
	return 0;
}

/* Start disconnection of a peer: send DPR */
int fd_p_dp_initiate(struct fd_peer * peer, char * reason)
{
	struct msg * msg = NULL;
	struct dict_object * dictobj = NULL;
	struct avp * avp = NULL;
	struct dict_enumval_request er;
	union avp_value val;
	
	TRACE_ENTRY("%p %p", peer, reason);
	
	/* Create a new DWR instance */
	CHECK_FCT( fd_msg_new ( fd_dict_cmd_DPR, MSGFL_ALLOC_ETEID, &msg ) );
	
	/* Add the Origin information */
	CHECK_FCT( fd_msg_add_origin ( msg, 0 ) );
	
	/* Add the Disconnect-Cause */
	CHECK_FCT( fd_msg_avp_new ( fd_dict_avp_DC, 0, &avp ) );
	
	/* Search the value in the dictionary */
	memset(&er, 0, sizeof(er));
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_TYPE, TYPE_OF_AVP, fd_dict_avp_DC, &er.type_obj, ENOENT )  );
	er.search.enum_name = reason ?: "REBOOTING";
	CHECK_FCT( fd_dict_search( fd_g_config->cnf_dict, DICT_ENUMVAL, ENUMVAL_BY_STRUCT, &er, &dictobj, ENOENT )  );
	CHECK_FCT( fd_dict_getval( dictobj, &er.search ) );
	
	/* Set the value in the AVP */
	val.u32 = er.search.enum_value.u32;
	CHECK_FCT( fd_msg_avp_setvalue( avp, &val ) );
	CHECK_FCT( fd_msg_avp_add( msg, MSG_BRW_LAST_CHILD, avp ) );
	
	/* Save the value also in the peer */
	peer->p_hdr.info.runtime.pir_lastDC = val.u32;
	
	/* Update the peer state and timer */
	CHECK_FCT( fd_psm_change_state(peer, STATE_CLOSING) );
	fd_psm_next_timeout(peer, 0, DPR_TIMEOUT);
	
	/* Now send the DPR message */
	CHECK_FCT( fd_out_send(&msg, NULL, peer) );
	
	return 0;
}
