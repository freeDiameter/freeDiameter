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

/* This file contains code to handle Capabilities Exchange messages (CER and CEA) and election process */

/* Save a connection as peer's principal */
static int set_peer_cnx(struct fd_peer * peer, struct cnxctx **cnx)
{
	TODO("Save *cnx into peer->p_cnxctx");
	TODO("Set fifo of *cnx to peer->p_events");
	TODO("If connection is already TLS, read the credentials");
	TODO("Read the remote endpoints");
	
	return ENOTSUP;
}

static int process_valid_CEA(struct fd_peer * peer, struct msg ** cea)
{
	/* Save info from the CEA into the peer */
	
	/* Handshake if needed */
	
	/* Save credentials if needed */
	
	TODO("...");
	return ENOTSUP;
	
}

/* We have received a Capabilities Exchange message on the peer connection */
int fd_p_ce_msgrcv(struct msg ** msg, int req, struct fd_peer * peer)
{
	TRACE_ENTRY("%p %p", msg, peer);
	CHECK_PARAMS( msg && *msg && CHECK_PEER(peer) );
	
	/* The only valid situation where we are called is in WAITCEA and we receive a CEA */
	
	/* Note : to implement Capabilities Update, we would need to change here */
	
	/* If it is a CER, just reply an error */
	if (req) {
		/* Create the error message */
		CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, msg, MSGFL_ANSW_ERROR ) );
		
		/* Set the error code */
		CHECK_FCT( fd_msg_rescode_set(*msg, "DIAMETER_COMMAND_UNSUPPORTED", "No CER in current state", NULL, 1 ) );

		/* msg now contains an answer message to send back */
		CHECK_FCT_DO( fd_out_send(msg, peer->p_cnxctx, peer), /* In case of error the message has already been dumped */ );

	}
	
	/* If the state is not WAITCEA, just discard the message */
	if ((req) || (peer->p_hdr.info.runtime.pir_state != STATE_WAITCEA)) {
		if (*msg) {
			fd_log_debug("Received CER/CEA message while in state '%s', discarded.\n", STATE_STR(peer->p_hdr.info.runtime.pir_state));
			fd_msg_dump_walk(NONE, *msg);
			CHECK_FCT_DO( fd_msg_free(*msg), /* continue */);
			*msg = NULL;
		}
		
		return 0;
	}
	
	/* Ok, now we can accept the CEA */
	TODO("process_valid_CEA");
	
	return ENOTSUP;
}

/* We have received a CER on a new connection for this peer */
int fd_p_ce_handle_newCER(struct msg ** msg, struct fd_peer * peer, struct cnxctx ** cnx, int valid)
{
	switch (peer->p_hdr.info.runtime.pir_state) {
		case STATE_CLOSED:
			TODO("Handle the CER, validate the peer if needed (and set expiry), set the alt_fifo in the connection, reply a CEA, eventually handshake (OPEN_HANDSHAKE), move to OPEN or REOPEN state");
			/* In case of error : DIAMETER_UNKNOWN_PEER */
			break;

		case STATE_WAITCNXACK:
			TODO("Save the parameters in the peer, move to STATE_WAITCNXACK_ELEC");
			break;
			
		case STATE_WAITCEA:
			TODO("Election");
			break;

		default:
			TODO("Reply with error CEA");
			TODO("Close the connection");
			/* reject_incoming_connection */

	}
				
	
	return ENOTSUP;
}

static int do_election(struct fd_peer * peer)
{
	TODO("Compare diameter Ids");
	
	/* ELECTION_LOST error code ?*/
	
	TODO("If we lost the election, we close the received connection and go to WAITCEA");
	TODO("If we won the election, we close initiator cnx, then call fd_p_ce_winelection");
	
	
}

/* We have established a new connection to the remote peer, send CER and eventually do election */
int fd_p_ce_handle_newcnx(struct fd_peer * peer, struct cnxctx * initiator)
{
	/* if not election */
	TODO("Set the connection as peer's (setaltfifo)");
	TODO("Send CER");
	TODO("Move to WAITCEA");
	TODO("Change timer to CEA_TIMEOUT");
	
	/* if election */				
	TODO("Send CER on cnx");
	TODO("Do the election");
	
	return ENOTSUP;
}

/* Handle the receiver side after winning an election (or timeout on initiator side) */
int fd_p_ce_winelection(struct fd_peer * peer)
{
	TODO("Move the receiver side connection to peer principal, set the altfifo");
	TODO("Then handle the received CER");
}
