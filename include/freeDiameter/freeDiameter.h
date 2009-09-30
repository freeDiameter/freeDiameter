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

#ifndef _FREEDIAMETER_H
#define _FREEDIAMETER_H


#include <freeDiameter/libfreeDiameter.h>


/* Structure to hold the configuration of the freeDiameter daemon */
struct fd_config {
	int		 cnf_eyec;	/* Eye catcher: EYEC_CONFIG */
			#define	EYEC_CONFIG	0xC011F16
	
	char		*cnf_file;	/* Configuration file to parse, default is DEFAULT_CONF_FILE */
	
	char   		*cnf_diamid;	/* Diameter Identity of the local peer (FQDN -- UTF-8) */
	size_t		 cnf_diamid_len;	/* length of the previous string */
	char		*cnf_diamrlm;	/* Diameter realm of the local peer, default to realm part of diam_id */
	size_t		 cnf_diamrlm_len;/* length of the previous string */
	
	unsigned int	 cnf_timer_tc;	/* The value in seconds of the default Tc timer */
	unsigned int 	 cnf_timer_tw;	/* The value in seconds of the default Tw timer */
	
	uint16_t	 cnf_port;	/* the local port for legacy Diameter (default: 3868) in host byte order */
	uint16_t	 cnf_port_tls;	/* the local port for Diameter/TLS (default: 3869) in host byte order */
	uint16_t	 cnf_sctp_str;	/* default max number of streams for SCTP associations (def: 30) */
	struct fd_list	 cnf_endpoints;	/* the local endpoints to bind the server to. list of struct fd_endpoint. default is empty (bind all) */
	struct fd_list	 cnf_apps;	/* Applications locally supported (except relay, see flags). Use fd_disp_app_support to add one. list of struct fd_app. */
	struct {
		unsigned no_fwd : 1;	/* the peer does not relay messages (0xffffff app id) */
		unsigned no_ip4 : 1;	/* disable IP */
		unsigned no_ip6 : 1;	/* disable IPv6 */
		unsigned no_tcp : 1;	/* disable use of TCP */
		unsigned no_sctp: 1;	/* disable the use of SCTP */
		unsigned pr_tcp	: 1;	/* prefer TCP over SCTP */
		unsigned tls_alg: 1;	/* TLS algorithm for initiated cnx. 0: separate port. 1: inband-security (old) */
	} 		 cnf_flags;
	
	uint32_t	 cnf_orstateid;	/* The value to use in Origin-State-Id, default to random value */
	struct dictionary *cnf_dict;	/* pointer to the global dictionary */
	struct fifo	  *cnf_main_ev;	/* events for the daemon's main (struct fd_event items) */
};
extern struct fd_config *fd_g_config; /* The pointer to access the global configuration, initalized in main */

/* Endpoints */
struct fd_endpoint {
	struct fd_list  chain;	/* link in cnf_endpoints list */
	sSS		ss;	/* the socket information. */
	struct {
		unsigned conf : 1; /* This endpoint is statically configured in a configuration file */
		unsigned disc : 1; /* This endpoint was resolved from the Diameter Identity or other DNS query */
		unsigned adv  : 1; /* This endpoint was advertized in Diameter CER/CEA exchange */
		unsigned ll   : 1; /* Lower layer mechanism provided this endpoint */
		
		/* To add: a validity timestamp for DNS records ? How do we retrieve this lifetime from DNS ? */

	}		meta;	/* Additional information about the endpoint */
};

/* Applications */
struct fd_app {
	struct fd_list	 chain;	/* link in cnf_apps list. List ordered by appid. */
	struct {
		unsigned auth   : 1;
		unsigned acct   : 1;
		unsigned common : 1;
	}		 flags;
	vendor_id_t	 vndid; /* if not 0, Vendor-Specific-App-Id AVP will be used */
	application_id_t appid;	/* The identifier of the application */
};
	

/* Events */
struct fd_event {
	int	 code; /* codespace depends on the queue */
	void    *data;
};

static __inline__ int fd_event_send(struct fifo *queue, int code, void * data)
{
	struct fd_event * ev;
	CHECK_MALLOC( ev = malloc(sizeof(struct fd_event)) );
	ev->code = code;
	ev->data = data;
	CHECK_FCT( fd_fifo_post(queue, &ev) );
	return 0;
}
static __inline__ int fd_event_get(struct fifo *queue, int *code, void ** data)
{
	struct fd_event * ev;
	CHECK_FCT( fd_fifo_get(queue, &ev) );
	if (code)
		*code = ev->code;
	if (data)
		*data = ev->data;
	free(ev);
	return 0;
}

/* Events codespace for fd_g_config->cnf_main_ev */
enum {
	 FDEV_TERMINATE = 1000	/* request to terminate */
	,FDEV_DUMP_DICT		/* Dump the content of the dictionary */
	,FDEV_DUMP_EXT		/* Dump state of extensions */
	,FDEV_DUMP_QUEUES	/* Dump the message queues */
	,FDEV_DUMP_CONFIG	/* Dump the configuration */
	,FDEV_DUMP_PEERS	/* Dump the list of peers */
};



/***************************************/
/*   Peers information                 */
/***************************************/

/* States of a peer */
enum peer_state {
	/* Stable states */
	STATE_DISABLED = 1,	/* No connexion must be attempted / only this state means that the peer PSM thread is not running */
	STATE_OPEN,		/* Connexion established */
	
	/* Peer state machine */
	STATE_CLOSED,		/* No connection established, will re-attempt after TcTimer. */
	STATE_CLOSING,		/* the connection is being shutdown (DPR/DPA in progress) */
	STATE_WAITCNXACK,	/* Attempting to establish transport-level connection */
	STATE_WAITCNXACK_ELEC,	/* Received a CER from this same peer on an incoming connection (other peer object), while we were waiting for cnx ack */
	STATE_WAITCEA,		/* Connection established, CER sent, waiting for CEA */
	/* STATE_WAITRETURNS_ELEC, */	/* This state is not stable and therefore deprecated:
				   We have sent a CER on our initiated connection, and received a CER from the remote peer on another connection. Election.
				   If we win the election, we must disconnect the initiated connection and send a CEA on the other => we go to OPEN state.
				   If we lose, we disconnect the other connection (receiver) and fallback to WAITCEA state. */
	
	/* Failover state machine */
	STATE_SUSPECT,		/* A DWR was sent and not answered within TwTime. Failover in progress. */
	STATE_REOPEN		/* Connection has been re-established, waiting for 3 DWR/DWA exchanges before putting back to service */
};
extern const char *peer_state_str[];
#define STATE_STR(state) \
	peer_state_str[ ((unsigned)(state)) <= STATE_REOPEN ? ((unsigned)(state)) : 0 ]

/* Information about a remote peer. Same structure is used for creating a new entry, but not all fields are meaningful in that case */
struct peer_info {
	
	char * 		pi_diamid;	/* UTF-8, \0 terminated. The Diameter Identity of the remote peer */
	char * 		pi_realm;	/* Its realm, as received in CER/CEA exchange. */
	
	struct {
		#define PI_P3_DEFAULT	0	/* Use the default L3 protocol configured for the host */
		#define PI_P3_IP	1	/* Use only IP to connect to this peer */
		#define PI_P3_IPv6	2	/* resp, IPv6 */
		unsigned	pro3 :2;
		
		#define PI_P4_DEFAULT	0	/* Use the default L4 proto configured for the host */
		#define PI_P4_TCP	1	/* Only use TCP */
		#define PI_P4_SCTP	2	/* Only use SCTP */
		unsigned	pro4 :2;
		
		#define PI_ALGPREF_SCTP	0	/* SCTP is initially attempted */
		#define PI_ALGPREF_TCP	1	/* TCP is initially attempted */
		unsigned	alg :1;
		
		#define PI_SEC_DEFAULT	0	/* New TLS security (dedicated port protecting also CER/CEA) */
		#define PI_SEC_NONE	1	/* Transparent security with this peer (IPsec) */
		#define PI_SEC_TLS_OLD	2	/* Old TLS security (inband on default port) */
		unsigned	sec :2;
		
		#define PI_EXP_NONE	0	/* the peer entry does not expire */
		#define PI_EXP_INACTIVE	1	/* the peer entry expires (i.e. is deleted) after pi_lft seconds without activity */
		unsigned	exp :1;
		
		/* Following flags are read-only and received from remote peer */
		#define PI_INB_NONE	1	/* Remote peer advertised inband-sec-id 0 (None) */
		#define PI_INB_TLS	2	/* Remote peer advertised inband-sec-id 1 (TLS) */
		unsigned	inband :2;	/* This is only meaningful with pi_flags.sec == 3 */
		
		unsigned	relay :1;	/* The remote peer advertized the relay application */

	} 		pi_flags;	/* Some flags */
	
	/* Additional parameters */
	uint32_t 	pi_lft;		/* lifetime of this peer when inactive (see pi_flags.exp definition) */
	uint16_t	pi_streams; 	/* number of streams for SCTP. 0 = default */
	uint16_t	pi_port; 	/* port to connect to. 0: default. */
	int		pi_tctimer; 	/* use this value for TcTimer instead of global, if != 0 */
	int		pi_twtimer; 	/* use this value for TwTimer instead of global, if != 0 */
	
	struct fd_list	pi_endpoints;	/* Endpoint(s) of the remote peer (configured, discovered, or advertized). list of struct fd_endpoint. DNS resolved if empty. */
	
	/* TLS specific data -- the exact data pointed here depends on the security module in use (ex: gnutls, ...) */
	enum {
		PI_SEC_GNUTLS = 0,	/* The security module is GNUTLS, this is the default */
		PI_SEC_OTHER		/* Another security module (TBD) */
	}		pi_sec_module;
	union {
		/* Security data when pi_sec_module == PI_SEC_GNUTLS */
		struct {
			void *	CA;	/* Authority to use to validate this peer credentials (a CA or root certificate) -- use default if NULL */
			void *	cred;	/* The (valid) credentials that the peer has presented */
		} 	gnutls;
		/* Security data when pi_sec_module == PI_SEC_OTHER */
		struct {
			void * dummy;	/* Something meaningful for the other security module */
		} 	other;
	} 		pi_sec_data;
	
	/* The remaining information is read-only, not used for peer creation */
	enum peer_state	pi_state;
	uint32_t	pi_vendorid;	/* Content of the Vendor-Id AVP, or 0 by default */
	uint32_t	pi_orstate;	/* Origin-State-Id value */
	char *		pi_prodname;	/* copy of UTF-8 Product-Name AVP (\0 terminated) */
	uint32_t	pi_firmrev;	/* Content of the Firmware-Revision AVP */
	struct fd_list	pi_apps;	/* applications advertised by the remote peer, except relay (pi_flags.relay) */
};

struct peer_hdr {
	struct fd_list	 chain;	/* List of all the peers, ordered by their Diameter Id */
	struct peer_info info;	/* The public data */
	
	/* This header is followed by more data in the private peer structure definition */
};

/* the global list of peers. 
  Since we are not expecting so many connections, we don't use a hash, but it might be changed.
  The list items are peer_hdr structures (actually, fd_peer, but the cast is OK) */
extern struct fd_list fd_g_peers;
extern pthread_rwlock_t fd_g_peers_rw; /* protect the list */

/*
 * FUNCTION:	fd_peer_add
 *
 * PARAMETERS:
 *  info 	: Information to create the peer.
 *  orig_dbg	: A string indicating the origin of the peer information, for debug (ex: conf, redirect, ...)
 *  cb		: optional, a callback to call (once) when the peer connection is established or failed
 *  cb_data	: opaque data to pass to the callback.
 *
 * DESCRIPTION: 
 *  Add a peer to the list of peers to which the daemon must maintain a connexion.
 *
 *  The content of info parameter is copied, except for the list of endpoints if 
 * not empty, which is simply moved into the created object. It means that the list
 * items must have been malloc'd, so that they can be freed.
 *
 *  If cb is not null, the callback is called when the connection is in OPEN state or
 * when an error has occurred. The callback should use the pi_state information to 
 * determine which one it is. If the first parameter of the called callback is NULL, it 
 * means that the peer is being destroyed before attempt success / failure. 
 * cb is called to allow freeing cb_data in  * this case.
 *
 *  The orig_dbg string is only useful for easing debug, and can be left to NULL.
 *
 * RETURN VALUE:
 *  0      	: The peer is added.
 *  EINVAL 	: A parameter is invalid.
 *  EEXIST 	: A peer with the same Diameter-Id is already in the list.
 *  (other standard errors may be returned, too, with their standard meaning. Example:
 *    ENOMEM 	: Memory allocation for the new object element failed.)
 */
int fd_peer_add ( struct peer_info * info, char * orig_dbg, void (*cb)(struct peer_info *, void *), void * cb_data );

/*
 * FUNCTION:	peer_validate_register
 *
 * PARAMETERS:
 *  peer_validate 	: Callback as defined bellow.
 *
 * DESCRIPTION: 
 *  Add a callback to authorize / reject incoming peer connections.
 * All registered callbacks are called until a callback sets auth = -1 or auth = 1.
 * If no callback returns a clear decision, the default behavior is applied (reject unknown connections)
 *
 * RETURN VALUE:
 *  0   : The callback is added.
 * !0	: An error occurred.
 */
int fd_peer_validate_register ( int (*peer_validate)(struct peer_info * /* info */, int * /* auth */) );
/*
 * CALLBACK:	peer_validate
 *
 * PARAMETERS:
 *   info     : Structure containing information about the peer attempting the connection.
 *   auth     : Store there the result if the peer is accepted (1), rejected (-1), or unknown (0).
 *
 * DESCRIPTION: 
 *   This callback is called when a new connection is being established from an unknown peer,
 *  after the CER is received. An extension must register such callback with peer_validate_register.
 *
 * RETURN VALUE:
 *  0      	: The authorization decision has been written in the location pointed by auth.
 *  !0 		: An error occurred.
 */

/***************************************/
/*   Sending a message on the network  */
/***************************************/

/*
 * FUNCTION:	fd_msg_send
 *
 * PARAMETERS:
 *  pmsg 	: Location of the message to be sent on the network (set to NULL on function return to avoid double deletion).
 *  anscb	: A callback to be called when answer is received, if msg is a request (optional)
 *  anscb_data	: opaque data to be passed back to the anscb when it is called.
 *
 * DESCRIPTION: 
 *   Sends a message on the network. (actually simply queues it in a global queue, to be picked by a daemon's thread)
 * For requests, the end-to-end id must be set (see fd_msg_get_eteid / MSGFL_ALLOC_ETEID).
 * For answers, the message must be created with function fd_msg_new_answ.
 *
 * The routing module will handle sending to the correct peer, usually based on the Destination-Realm / Destination-Host AVP.
 *
 * If the msg is a request, there are two ways of receiving the answer:
 *  - either having registered a callback in the dispatch module (see disp_register)
 *  - or provide a callback as parameter here. If such callback is provided, it is called before the dispatch callbacks.
 *    The prototype for this callback function is:
 *     void anscb(void * data, struct msg ** answer)
 *	where:
 *		data   : opaque data that was registered along with the callback.
 *		answer : location of the pointer to the answer.
 *      note1: on function return, if *answer is not NULL, the message is passed to the dispatch module for regular callbacks.
 *	       otherwise, the callback must take care of freeing the message (msg_free).
 *	note2: the opaque data is not freed by the daemon in any case, extensions should ensure clean handling in waaad_ext_fini.
 * 
 * If no callback is registered to handle an answer, the message is discarded and an error is logged.
 *
 * RETURN VALUE:
 *  0      	: The message has been queued for sending (sending may fail asynchronously).
 *  EINVAL 	: A parameter is invalid (ex: anscb provided but message is not a request).
 *  ...
 */
int fd_msg_send ( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data );

/*
 * FUNCTION:	fd_msg_rescode_set
 *
 * PARAMETERS:
 *  msg		: A msg object -- it must be an answer.
 *  rescode	: The name of the returned error code (ex: "DIAMETER_INVALID_AVP")
 *  errormsg    : (optional) human-readable error message to put in Error-Message AVP
 *  optavp	: (optional) If provided, the content will be put inside a Failed-AVP
 *  type_id	: 0 => nothing; 1 => adds Origin-Host and Origin-Realm with local info. 2=> adds Error-Reporting-Host.
 *
 * DESCRIPTION: 
 *   This function adds a Result-Code AVP to a message, and optionally
 *  - sets the 'E' error flag in the header,
 *  - adds Error-Message, Error-Reporting-Host and Failed-AVP AVPs.
 *
 * RETURN VALUE:
 *  0      	: Operation complete.
 *  !0      	: an error occurred.
 */
int fd_msg_rescode_set( struct msg * msg, char * rescode, char * errormsg, struct avp * optavp, int type_id );

/* Add Origin-Host, Origin-Realm, (if osi) Origin-State-Id AVPS at the end of the message */
int fd_msg_add_origin ( struct msg * msg, int osi ); 



/***************************************/
/*   Dispatch module, daemon's part    */
/***************************************/

/*
 * FUNCTION:	fd_disp_app_support
 *
 * PARAMETERS:
 *  app		: The dictionary object corresponding to the Application.
 *  vendor	: (Optional) the dictionary object of a Vendor to claim support in Vendor-Specific-Application-Id
 *  auth	: Support auth app part.
 *  acct	: Support acct app part.
 *
 * DESCRIPTION: 
 *   Registers an application to be advertized in CER/CEA exchanges.
 *  Messages with an application-id matching a registered value are passed to the dispatch module,
 * while other messages are simply relayed or an error is returned (if local node does not relay)
 *
 * RETURN VALUE:
 *  0      	: The application support is registered.
 *  EINVAL 	: A parameter is invalid.
 */
int fd_disp_app_support ( struct dict_object * app, struct dict_object * vendor, int auth, int acct );

/* Note: if we want to support capabilities updates, we'll have to add possibility to remove an app as well... */


#endif /* _FREEDIAMETER_H */
