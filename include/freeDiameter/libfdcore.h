/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2012, WIDE Project and NICT								 *
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

#ifndef _LIBFDCORE_H
#define _LIBFDCORE_H


#include <freeDiameter/libfdproto.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

/* GNUTLS version */
#ifndef GNUTLS_VERSION
#define GNUTLS_VERSION LIBGNUTLS_VERSION
#endif /* GNUTLS_VERSION */

/* GNUTLS calls debug level */
#ifndef GNUTLS_DBG_LEVEL
#define GNUTLS_DBG_LEVEL ANNOYING
#endif /* GNUTLS_DBG_LEVEL */


/* Check the return value of a GNUTLS function, log and propagate */
#define CHECK_GNUTLS_DO( __call__, __fallback__ ) {						\
	int __ret__;										\
	TRACE_DEBUG(GNUTLS_DBG_LEVEL, "GNUTLS call: " #__call__ );				\
	__ret__ = (__call__);									\
	if (__ret__ < 0) {									\
		TRACE_DEBUG(INFO, "Error in '" #__call__ "':\t%s", gnutls_strerror(__ret__));	\
		__fallback__;									\
	}											\
}

/* For GNUTLS routines that do not return a value */
#define GNUTLS_TRACE( __call__) {					\
	TRACE_DEBUG(GNUTLS_DBG_LEVEL, "GNUTLS call: " #__call__ );	\
	(__call__);							\
}


/*============================================================*/
/*                      INITIALIZATION                        */
/*============================================================*/


/* Initialize the libfdcore internals. This also initializes libfdproto */
int fd_core_initialize(void);

/* Return a string describing the version of the library */
const char *fd_core_version(void);

/* Parse the freeDiameter.conf configuration file, load the extensions */
int fd_core_parseconf(char * conffile);

/* Start the server & client threads */
int fd_core_start(void);

/* Block until the framework has completed its initialization -- useful for extensions */
int fd_core_waitstartcomplete(void);

/* Initialize shutdown of the framework */
int fd_core_shutdown(void);

/* Wait for the shutdown to be complete -- this should always be called after fd_core_shutdown */
int fd_core_wait_shutdown_complete(void);


/*============================================================*/
/*                          CONFIG                            */
/*============================================================*/

/* Structure to hold the configuration of the freeDiameter daemon */
#define	EYEC_CONFIG	0xC011F16
struct fd_config {
	int		 cnf_eyec;	/* Eye catcher: EYEC_CONFIG */
	
	char		*cnf_file;	/* Configuration file to parse, default is DEFAULT_CONF_FILE */
	
	DiamId_t  	 cnf_diamid;	/* Diameter Identity of the local peer (FQDN -- ASCII) */
	size_t		 cnf_diamid_len;/* cached length of the previous string */
	DiamId_t	 cnf_diamrlm;	/* Diameter realm of the local peer, default to realm part of cnf_diamid */
	size_t		 cnf_diamrlm_len;/* length of the previous string */
	
	unsigned int	 cnf_timer_tc;	/* The value in seconds of the default Tc timer */
	unsigned int 	 cnf_timer_tw;	/* The value in seconds of the default Tw timer */
	
	uint16_t	 cnf_port;	/* the local port for legacy Diameter (default: 3868) in host byte order */
	uint16_t	 cnf_port_tls;	/* the local port for Diameter/TLS (default: 3869) in host byte order */
	uint16_t	 cnf_sctp_str;	/* default max number of streams for SCTP associations (def: 30) */
	struct fd_list	 cnf_endpoints;	/* the local endpoints to bind the server to. list of struct fd_endpoint. default is empty (bind all) */
	struct fd_list	 cnf_apps;	/* Applications locally supported (except relay, see flags). Use fd_disp_app_support to add one. list of struct fd_app. */
	uint16_t	 cnf_dispthr;	/* Number of dispatch threads to create */
	struct {
		unsigned no_fwd : 1;	/* the peer does not relay messages (0xffffff app id) */
		unsigned no_ip4 : 1;	/* disable IP */
		unsigned no_ip6 : 1;	/* disable IPv6 */
		unsigned no_tcp : 1;	/* disable use of TCP */
		unsigned no_sctp: 1;	/* disable the use of SCTP */
		unsigned pr_tcp	: 1;	/* prefer TCP over SCTP */
		unsigned tls_alg: 1;	/* TLS algorithm for initiated cnx. 0: separate port. 1: inband-security (old) */
	} 		 cnf_flags;
	
	struct {
		/* Credentials parameters (backup) */
		char *  			 cert_file;
		char *				 key_file;
		
		char *  			 ca_file;
		int				 ca_file_nr;
		char *  			 crl_file;
		
		char *				 prio_string;
		unsigned int 			 dh_bits;
		char *				 dh_file;
		
		/* GNUTLS parameters */
		gnutls_priority_t 		 prio_cache;
		gnutls_dh_params_t 		 dh_cache;
		
		/* GNUTLS server credential(s) */
		gnutls_certificate_credentials_t credentials; /* contains local cert + trust anchors */
		#ifdef GNUTLS_VERSION_300
		gnutls_x509_trust_list_t         trustlist; /* the logic to check local certificate has changed */
		#endif /* GNUTLS_VERSION_300 */
		
	} 		 cnf_sec_data;
	
	uint32_t	 cnf_orstateid;	/* The value to use in Origin-State-Id, default to random value */
	struct dictionary *cnf_dict;	/* pointer to the global dictionary */
	struct fifo	  *cnf_main_ev;	/* events for the daemon's main (struct fd_event items) */
};
extern struct fd_config *fd_g_config; /* The pointer to access the global configuration, initalized in main */



/*============================================================*/
/*                         PEERS                              */
/*============================================================*/

/* States of a peer */
enum peer_state {
	/* Stable states */
	STATE_NEW = 0,		/* The peer has been just been created, PSM thread not started yet */
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
	STATE_OPEN_HANDSHAKE,	/* TLS Handshake and validation are in progress in open state -- we use it only for debug purpose, it is never displayed */
	
	/* Failover state machine */
	STATE_SUSPECT,		/* A DWR was sent and not answered within TwTime. Failover in progress. */
	STATE_REOPEN,		/* Connection has been re-established, waiting for 3 DWR/DWA exchanges before putting back to service */
	
	/* Ordering issues with multistream & state machine. -- see top of p_psm.c for explanation */
	STATE_OPEN_NEW, 	/* after CEA is sent, until a new message is received. Force ordering in this state */
	STATE_CLOSING_GRACE,	/* after DPA is sent or received, give a short delay for messages in the pipe to be received. */
	
	/* Error state */
	STATE_ZOMBIE		/* The PSM thread is not running anymore; it must be re-started or peer should be deleted. */
#define STATE_MAX STATE_ZOMBIE
};
/* The following macro is called in freeDiameter/p_psm.c */
#define DECLARE_STATE_STR()		\
const char *peer_state_str[] = { 	\
	  "STATE_NEW"			\
	, "STATE_OPEN"			\
	, "STATE_CLOSED"		\
	, "STATE_CLOSING"		\
	, "STATE_WAITCNXACK"		\
	, "STATE_WAITCNXACK_ELEC"	\
	, "STATE_WAITCEA"		\
	, "STATE_OPEN_HANDSHAKE"	\
	, "STATE_SUSPECT"		\
	, "STATE_REOPEN"		\
	, "STATE_OPEN_NEW"		\
	, "STATE_CLOSING_GRACE"		\
	, "STATE_ZOMBIE"		\
	};
extern const char *peer_state_str[];
#define STATE_STR(state) \
	(((unsigned)(state)) <= STATE_MAX ? peer_state_str[((unsigned)(state)) ] : "<Invalid>")

/* Constants for the peer_info structure bellow */
#define PI_P3_DEFAULT	0	/* Use any available protocol */
#define PI_P3_IP	1	/* Use only IP to connect to this peer */
#define PI_P3_IPv6	2	/* resp, IPv6 */

#define PI_P4_DEFAULT	0	/* Attempt any available protocol */
#define PI_P4_TCP	1	/* Only use TCP */
#define PI_P4_SCTP	2	/* Only use SCTP */

#define PI_ALGPREF_SCTP	0	/* SCTP is  attempted first (default) */
#define PI_ALGPREF_TCP	1	/* TCP is attempted first */

#define PI_SEC_DEFAULT	0	/* New TLS security (handshake after connection, protecting also CER/CEA) */
#define PI_SEC_NONE	1	/* Transparent security with this peer (IPsec) */
#define PI_SEC_TLS_OLD	2	/* Old TLS security (use Inband-Security-Id AVP during CER/CEA) */
				/* Set sec = 3 to authorize use of (Inband-Security-Id == NONE) with this peer, sec = 2 only authorizing TLS */

#define PI_EXP_NONE	0	/* the peer entry does not expire */
#define PI_EXP_INACTIVE	1	/* the peer entry expires (i.e. is deleted) after pi_lft seconds without activity */

#define PI_PRST_NONE	0	/* the peer entry is deleted after disconnection / error */
#define PI_PRST_ALWAYS	1	/* the peer entry is persistant (will be kept as ZOMBIE in case of error) */
			
/* Information about a remote peer */
struct peer_info {
	
	DiamId_t	pi_diamid;	/* (supposedly) UTF-8, \0 terminated. The Diameter Identity of the remote peer. */
	size_t		pi_diamidlen;	/* cached length of pi_diamid */
	
	struct {
		struct {
			unsigned	pro3 :2;	/* PI_P3_* */
			unsigned	pro4 :2;	/* PI_P4_* */
			unsigned	alg :1;		/* PI_ALGPREF_* */
			unsigned	sec :2;		/* PI_SEC_* */
			unsigned	exp :1;		/* PI_EXP_* */
			unsigned	persist :1;	/* PI_PRST_* */
			
		}		pic_flags;	/* Flags influencing the connection to the remote peer */
		
		DiamId_t	pic_realm;	/* If configured, the daemon will check the received realm in CER/CEA matches this. */
		uint16_t	pic_port; 	/* port to connect to. 0: default. */
		
		uint32_t 	pic_lft;	/* lifetime of this peer when inactive (see pic_flags.exp definition) */
		int		pic_tctimer; 	/* use this value for TcTimer instead of global, if != 0 */
		int		pic_twtimer; 	/* use this value for TwTimer instead of global, if != 0 */
		
		char *		pic_priority;	/* Priority string for GnuTLS if we don't use the default */
		
	} config;	/* Configured data (static for this peer entry) */
	
	struct {
		
		/* enum peer_state	pir_state; */ 
		/* Since 1.1.0, read the state with fd_peer_getstate(peer). */
		
		DiamId_t	pir_realm;	/* The received realm in CER/CEA. */
		size_t		pir_realmlen;	/* length of the realm */
		
		uint32_t	pir_vendorid;	/* Content of the Vendor-Id AVP, or 0 by default */
		uint32_t	pir_orstate;	/* Origin-State-Id value */
		os0_t		pir_prodname;	/* copy of Product-Name AVP (\0 terminated) */
		uint32_t	pir_firmrev;	/* Content of the Firmware-Revision AVP */
		int		pir_relay;	/* The remote peer advertized the relay application */
		struct fd_list	pir_apps;	/* applications advertised by the remote peer, except relay (pi_flags.relay) */
		int		pir_isi;	/* Inband-Security-Id advertised (PI_SEC_* bits) */
		
		uint32_t	pir_lastDC;	/* The last Disconnect-Cause value received */
		
		int		pir_proto;	/* The L4 protocol currently used with the peer (IPPROTO_TCP or IPPROTO_SCTP) */
		const gnutls_datum_t 	*pir_cert_list; 	/* The (valid) credentials that the peer has presented, or NULL if TLS is not used */
								/* This is inspired from http://www.gnu.org/software/gnutls/manual/gnutls.html#ex_003ax509_002dinfo 
								   see there for example of using this data */
		unsigned int 	pir_cert_list_size;		/* Number of certificates in the list */
		
	} runtime;	/* Data populated after connection, may change between 2 connections -- not used by fd_peer_add */
	
	struct fd_list	pi_endpoints;	/* Endpoint(s) of the remote peer (configured, discovered, or advertized). list of struct fd_endpoint. DNS resolved if empty. */
};


struct peer_hdr {
	struct fd_list	 chain;	/* Link into the list of all the peers, ordered by their Diameter Id (fd_os_cmp) */
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
 * FUNCTION:	fd_peer_getbyid
 *
 * PARAMETERS:
 *  diamid 	: an UTF8 string describing the diameter Id of the peer to seek
 *  diamidlen	: length of the diamid
 *  igncase	: perform an almost-case-insensitive search? (slower)
 *  peer	: The peer is stored here if it exists.
 *
 * DESCRIPTION: 
 *   Search a peer by its Diameter-Id.
 *
 * RETURN VALUE:
 *  0   : *peer has been updated (to NULL if the peer is not found).
 * !0	: An error occurred.
 */
int fd_peer_getbyid( DiamId_t diamid, size_t diamidlen, int igncase, struct peer_hdr ** peer );

/* 
 * FUNCTION:	fd_peer_get_state
 *
 * PARAMETERS:
 *  peer	: The peer which state to read
 *
 * DESCRIPTION: 
 *   Returns the current state of the peer.
 *
 * RETURN VALUE:
 *  -1  : peer is invalid
 * >=0	: the state of the peer at the time of reading.
 */
int fd_peer_get_state(struct peer_hdr *peer);

/*
 * FUNCTION:	fd_peer_validate_register
 *
 * PARAMETERS:
 *  peer_validate 	: Callback as defined bellow.
 *
 * DESCRIPTION: 
 *  Add a callback to authorize / reject incoming peer connections.
 * All registered callbacks are called until a callback sets auth = -1 or auth = 1.
 * If no callback returns a clear decision, the default behavior is applied (reject unknown connections)
 * The callbacks are called in FILO order of their registration.
 *
 * RETURN VALUE:
 *  0   : The callback is added.
 * !0	: An error occurred.
 */
int fd_peer_validate_register ( int (*peer_validate)(struct peer_info * /* info */, int * /* auth */, int (**cb2)(struct peer_info *)) );
/*
 * CALLBACK:	peer_validate
 *
 * PARAMETERS:
 *   info     : Structure containing information about the peer attempting the connection.
 *   auth     : Store there the result if the peer is accepted (1), rejected (-1), or unknown (0).
 *   cb2      : If != NULL and in case of PI_SEC_TLS_OLD, another callback to call after handshake (if auth = 1).
 *
 * DESCRIPTION: 
 *   This callback is called when a new connection is being established from an unknown peer,
 * after the CER is received. An extension must register such callback with peer_validate_register.
 *
 *   The callback can learn if the peer has sent Inband-Security-Id AVPs in runtime.pir_isi fields.
 * It can also learn if a handshake has already been performed in runtime.pir_cert_list field.
 * The callback must set the value of config.pic_flags.sec appropriately to allow a connection without TLS.
 *
 *   If the old TLS mechanism is used,
 * the extension may also need to check the credentials provided during the TLS
 * exchange (remote certificate). For this purpose, it may set the address of a new callback
 * to be called once the handshake is completed. This new callback receives the information
 * structure as parameter (with pir_cert_list set) and returns 0 if the credentials are correct,
 * or an error code otherwise. If the error code is received, the connection is closed and the 
 * peer is destroyed.
 * Note that freeDiameter already achieves some usual checks. The callback may be used to enforce
 * additional restrictions.
 *
 * RETURN VALUE:
 *  0      	: The authorization decision has been written in the location pointed by auth.
 *  !0 		: An error occurred.
 */



/*============================================================*/
/*                         MESSAGES                           */
/*============================================================*/

/*
 * FUNCTION:	fd_msg_send, fd_msg_send_timeout  
 *
 * PARAMETERS:
 *  pmsg 	: Location of the message to be sent on the network (set to NULL on function return to avoid double deletion).
 *  anscb	: A callback to be called when answer is received, if msg is a request (optional for fd_msg_send)
 *  anscb_data	: opaque data to be passed back to the anscb when it is called.
 *  timeout     : (only for fd_msg_send_timeout) sets the absolute time until when to wait for an answer. Past this time,
 *                the anscb is called with the request as parameter and the answer will be discarded when received.
 *
 * DESCRIPTION: 
 *   Sends a message on the network. (actually simply queues it in a global queue, to be picked by a daemon's thread)
 * For requests, the end-to-end id must be set (see fd_msg_get_eteid / MSGFL_ALLOC_ETEID).
 * For answers, the message must be created with function fd_msg_new_answ.
 *
 * The routing module will handle sending to the correct peer, usually based on the Destination-Realm / Destination-Host AVP.
 *
 * If the msg is a request, there are two ways of receiving the answer:
 *  - either having registered a callback in the dispatch module (see fd_disp_register)
 *  - or provide a callback as parameter here. If such callback is provided, it is called before the dispatch callbacks.
 *    The prototype for this callback function is:
 *     void anscb(void * data, struct msg ** answer)
 *	where:
 *		data   : opaque data that was registered along with the callback.
 *		answer : location of the pointer to the answer.
 *      note1: on function return, if *answer is not NULL, the message is passed to the dispatch module for regular callbacks.
 *	       otherwise, the callback must take care of freeing the message (fd_msg_free).
 *	note2: the opaque data is not freed by the daemon in any case, extensions should ensure clean handling in fd_ext_fini.
 * 
 * If no callback is registered to handle an answer, the message is discarded and an error is logged.
 *
 *  fd_msg_send_timeout is similar to fd_msg_send, except that it takes an additional argument "timeout" and can be called
 * only with requests as parameters, and an anscb callback.
 * If the matching answer or error is received before the timeout date passes, everything occurs as with fd_msg_send. Otherwise,
 * the request is removed from the queue (meaning the matching answer will be discarded upon reception) and passed to the answcb 
 * function. This function can easily distinguish between timeout case and answer case by checking if the message received is 
 * a request. Upon return, if the *msg parameter is not NULL, it is freed (not passed to other callbacks).
 *
 * RETURN VALUE:
 *  0      	: The message has been queued for sending (sending may fail asynchronously).
 *  EINVAL 	: A parameter is invalid (ex: anscb provided but message is not a request).
 *  ...
 */
int fd_msg_send ( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data );
int fd_msg_send_timeout ( struct msg ** pmsg, void (*anscb)(void *, struct msg **), void * data, const struct timespec *timeout );

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

/* Generate a new Session-Id and add it at the beginning of the message (opt is added at the end of the sid if provided) */
int fd_msg_new_session( struct msg * msg, os0_t opt, size_t optlen );


/* Parse a message against our dictionary, and in case of error log and eventually build the error reply (on return and EBADMSG, *msg == NULL or *msg is the error message ready to send) */
int fd_msg_parse_or_error( struct msg ** msg );




/*============================================================*/
/*                         DISPATCH                           */
/*============================================================*/

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


/*============================================================*/
/*                         ROUTING                            */
/*============================================================*/

/* This file contains the definitions of types and functions involved in the routing decisions in freeDiameter,
 * and that can be called by extensions. 
 *
 * Three different type of messages must be distinguished:
 *  - Messages received, and the peer is final recipient (IN messages)
 *  - Messages received, and the peer is not final recipient (FWD messages)
 *  - Message is locally generated (OUT messages)
 *
 * There are three global message queues (in queues.c) and also peers-specific queues (in struct fd_peer).
 *
 * (*) IN messages processing details:
 *   - the message is received from the remote peer, a FDEVP_CNX_MSG_RECV event is generated for the peer.
 *   - the PSM thread parses the buffer, does some verifications, handles non routable messages (fd_msg_is_routable)
 *   - routable messages are queued in the fd_g_incoming global queue.
 *   - a thread (routing-in) picks the message and takes the decision if it is handled locally or forwarded, 
 *       based on local capabilities (registered by extensions with fd_disp_app_support).
 *   - If the message is handled locally, it is queued in fd_g_local.
 *   - Another thread (dispatch.c) will handle this message and pass it to registered callbacks (see fd_disp_register in libfreeDiameter.h).
 *
 * (*) FWD messages details:
 *   - The process is the same as for IN messages, until the routing-in threads makes its decision that the message is not handled locally.
 *   - If the local peer does not relay message, an error DIAMETER_APPLICATION_UNSUPPORTED is returned.
 *   - All callbacks registered with fd_rt_fwd_register are called for the message (see bellow).
 *     - these callbacks will typically do proxying work. Note that adding the route-record is handled by the daemon.
 *   - Once all callbacks have been called, the message is queued in the global fd_g_outgoing queue.
 *   - The remaining processing is the same as for OUT messages, as described bellow.
 *
 * (*) OUT messages details:
 *   - The message are picked from fd_g_outgoing (they are queued there as result of forwarding process or call to fd_msg_send.)
 *   - The (routing-out) thread builds a list of possible destinations for the message, as follow:
 *      - create a list of all known peers in the "OPEN" state.
 *      - remove from that list all peers that are in a Route-Record AVP of the message, to avoid routing loops.
 *      - remove also all peers that have previously replied an error message for this message.
 *   - If the list is empty, create an error UNABLE_TO_DELIVER (note: should we trig dynamic discovery here???) and reply.
 *   - Otherwise, call all callbacks registered by function fd_rt_out_register, with the list of peers and the message.
 *   - Order the resulting list of peers by score (see bellow), and sent the message to the peer with highest (positive) score.
 *    - in case the peer is no longer in the "OPEN" state, send the message to the second peer in the list.
 *      - if no peer is in OPEN state anymore, restart the process of creating the list.
 *   - Once a peer has been selected, the message is queued into that peer's outgoing queue.
 *
 * The following functions allow an extension to register or remove a callback as described above.
 */

/********** Forwarding callbacks: for Proxy operations ***********/

/* Handle to registered callback */
struct fd_rt_fwd_hdl;

/* Message direction for the callback */
enum fd_rt_fwd_dir {
	RT_FWD_REQ = 1,	/* The callback will be called on forwarded requests only */
	RT_FWD_ALL = 2,	/* The callback will be called on all forwarded messages (requests and answers )*/
	RT_FWD_ANS = 3	/* The callback will be called on answers and errors only */
};	

/*
 * FUNCTION:	fd_rt_fwd_register
 *
 * PARAMETERS:
 *  rt_fwd_cb	  : The callback function to register (see prototype bellow).
 *  cbdata	  : Pointer to pass to the callback when it is called. The data is opaque to the daemon.
 *  dir           : One of the RT_FWD_* directions defined above.
 *  handler       : On success, a handler to the registered callback is stored here. 
 *		   This handler will be used to unregister the cb.
 *
 * DESCRIPTION: 
 *   Register a new callback for forwarded messages. See explanations above. 
 * Note that there is no guaranteed order for the callbacks calls.
 *
 * RETURN VALUE:
 *  0      	: The callback is registered.
 *  EINVAL 	: A parameter is invalid.
 *  ENOMEM	: Not enough memory to complete the operation
 */
int fd_rt_fwd_register ( int (*rt_fwd_cb)(void * cbdata, struct msg ** msg), void * cbdata, enum fd_rt_fwd_dir dir, struct fd_rt_fwd_hdl ** handler );
/*
 * CALLBACK:	rt_fwd_cb
 *
 * PARAMETERS:
 *  data	: pointer to some data that was passed when the callback was registered (optional).
 *  msg 	: The message that is being forwarded.
 *
 * DESCRIPTION: 
 *   This callback is called when a message is forwarded to another peer. It may for example add a Proxy-Info AVP.
 *  The callback may also choose to handle the message in a more complex form. In that case, it must set *msg = NULL
 *  and handle it differently. In such case, the forwarding thread will stop processing this message.
 *
 * RETURN VALUE:
 *  0      	: Operation complete.
 *  !0 		: An error occurred -- will result in daemon's termination.
 */

/*
 * FUNCTION:	fd_rt_fwd_unregister
 *
 * PARAMETERS:
 *  handler     : The handler of the callback that must be unregistered.
 *  cbdata	: Will receive the data registered with the callback, that can be freed if needed.
 *
 * DESCRIPTION: 
 *   Removes a callback from the list of registered callbacks.
 *
 * RETURN VALUE:
 *  0      	: The callback is unregistered.
 *  EINVAL 	: A parameter is invalid.
 */
int fd_rt_fwd_unregister ( struct fd_rt_fwd_hdl * handler, void ** cbdata );


/********** Out callbacks: for next hop routing decision operations ***********/

/* Handle to registered callback */
struct fd_rt_out_hdl;

enum fd_rt_out_score {
	FD_SCORE_NO_DELIVERY	 = -70,	/* We should not send this message to this candidate */
	FD_SCORE_SENT_REDIRECT	 = -60, /* If this peer previously sent a Redirect indication that applies to this message */
	FD_SCORE_INI		 =  -2, /* All candidates are initialized with this value */
	FD_SCORE_LOAD_BALANCE	 =   1,	/* Use this to differentiate between several peers with the same score */
	FD_SCORE_DEFAULT	 =   5,	/* The peer is a default route for all messages */
	FD_SCORE_DEFAULT_REALM	 =  10,	/* The peer is a default route for this realm */
	FD_SCORE_REALM		 =  15,	/* The peer belongs to Destination-Realm of the message */
	FD_SCORE_REDIR_HOST	 =  25,	/* If there is a redirect rule with ALL_HOST for these message and peer */
	FD_SCORE_REDIR_APP	 =  30,	/* If there is a redirect rule with ALL_APPLICATION for these message and peer */
	FD_SCORE_REDIR_REALM	 =  35,	/* If there is a redirect rule with ALL_REALM for these message and peer */
	FD_SCORE_REDIR_REALM_APP =  40,	/* If there is a redirect rule with REALM_AND_APPLICATION for these message and peer */
	FD_SCORE_REDIR_USER	 =  45,	/* If there is a redirect rule with ALL_USER for these message and peer */
	FD_SCORE_REDIR_SESSION	 =  50,	/* If there is a redirect rule with ALL_SESSION for these message and peer */
	FD_SCORE_REDIR_ONCE	 =  55,	/* If there is a redirect rule with DONT_CACHE for these message and peer */
	FD_SCORE_FINALDEST	 = 100	/* If the peer is the final recipient of the message (i.e. matching Destination-Host), it receives a big score. */
};

/*
 * FUNCTION:	fd_rt_out_register
 *
 * PARAMETERS:
 *  rt_out_cb	  : The callback function to register (see prototype bellow).
 *  cbdata	  : Pointer to pass to the callback when it is called. The data is opaque to the daemon.
 *  priority      : Order for calling this callback. The callbacks are called in reverse priority order (higher priority = called sooner).
 *  handler       : On success, a handler to the registered callback is stored here. 
 *		   This handler will be used to unregister the cb.
 *
 * DESCRIPTION: 
 *   Register a new callback to handle OUT routing decisions. See explanations above. 
 *
 * RETURN VALUE:
 *  0      	: The callback is registered.
 *  EINVAL 	: A parameter is invalid.
 *  ENOMEM	: Not enough memory to complete the operation
 */
int fd_rt_out_register ( int (*rt_out_cb)(void * cbdata, struct msg * msg, struct fd_list * candidates), void * cbdata, int priority, struct fd_rt_out_hdl ** handler );
/*
 * CALLBACK:	rt_out_cb
 *
 * PARAMETERS:
 *  cbdata	: pointer to some data that was registered with the callback.
 *  msg 	: The message that must be sent.
 *  list        : The list of peers to which the message may be sent to, as returned by fd_rtd_candidate_extract
 *
 * DESCRIPTION: 
 *   This callback must attribute a score (preferably from FD_SCORE_*) to each candidate peer in the list.
 *  Once all registered callbacks have been called, the message is sent to the candidate with the highest score.
 *  Note that each callback must *add* its locally-attributed score to the candidate current "score" parameter, not replace it!
 *  Note also that this callback must be re-entrant since it may be called by several threads at the same time 
 *  (for different messages)
 *
 * RETURN VALUE:
 *  0      	: Operation complete.
 *  !0 		: An error occurred.
 */

/*
 * FUNCTION:	fd_rt_out_unregister
 *
 * PARAMETERS:
 *  handler     : The handler of the callback that must be unregistered.
 *  cbdata	: Will receive the data registered with the callback, that can be freed if needed.
 *
 * DESCRIPTION: 
 *   Removes a callback from the list of registered callbacks.
 *
 * RETURN VALUE:
 *  0      	: The callback is unregistered.
 *  EINVAL 	: A parameter is invalid.
 */
int fd_rt_out_unregister ( struct fd_rt_out_hdl * handler, void ** cbdata );


/*============================================================*/
/*                         EVENTS                             */
/*============================================================*/

struct fd_event {
	int	 code; /* codespace depends on the queue */
	size_t 	 size;
	void    *data;
};

/* Daemon's codespace: 1000->1999 (1500->1999 defined in fdcore-internal.h) */
enum {
	 FDEV_TERMINATE	= 1000	/* request to terminate */
	,FDEV_DUMP_DICT		/* Dump the content of the dictionary */
	,FDEV_DUMP_EXT		/* Dump state of extensions */
	,FDEV_DUMP_SERV		/* Dump the server socket status */
	,FDEV_DUMP_QUEUES	/* Dump the message queues */
	,FDEV_DUMP_CONFIG	/* Dump the configuration */
	,FDEV_DUMP_PEERS	/* Dump the list of peers */
	,FDEV_TRIGGER		/* Trigger available for extensions. size is sizeof(int), data is int * */
};

int fd_event_send(struct fifo *queue, int code, size_t datasz, void * data);
int fd_event_get(struct fifo *queue, int * code, size_t * datasz, void ** data);
int fd_event_timedget(struct fifo *queue, struct timespec * timeout, int timeoutcode, int * code, size_t * datasz, void ** data);
void fd_event_destroy(struct fifo **queue, void (*free_cb)(void * data));
const char * fd_ev_str(int event);

/* for extensions */
int fd_event_trig_regcb(int trigger_val, const char * module, void (*cb)(void));
void fd_event_trig_dump();


/*============================================================*/
/*                         ENDPOINTS                          */
/*============================================================*/

struct fd_endpoint {
	struct fd_list  chain;	/* link in cnf_endpoints list */
	
	union {
		sSS		ss;	/* the socket information. List is always ordered by ss value (memcmp) -- see fd_ep_add_merge */
		sSA4		sin;
		sSA6		sin6;
		sSA		sa;
	}
#ifdef SWIG /* nested anonymous unions are not supported yet */
			s
#endif /* SWIG */
	;
	
#define	EP_FL_CONF	(1 << 0)	/* This endpoint is statically configured in a configuration file */
#define	EP_FL_DISC	(1 << 1)	/* This endpoint was resolved from the Diameter Identity or other DNS query */
#define	EP_FL_ADV	(1 << 2)	/* This endpoint was advertized in Diameter CER/CEA exchange */
#define	EP_FL_LL	(1 << 3)	/* Lower layer mechanism provided this endpoint */
#define	EP_FL_PRIMARY	(1 << 4)	/* This endpoint is primary in a multihomed SCTP association */
#define	EP_ACCEPTALL	(1 << 15)	/* This flag allows bypassing the address filter in fd_ep_add_merge. */
	uint32_t	flags;		/* Additional information about the endpoint */
		
	/* To add: a validity timestamp for DNS records ? How do we retrieve this lifetime from DNS ? */
};

int fd_ep_add_merge( struct fd_list * list, sSA * sa, socklen_t sl, uint32_t flags );
int fd_ep_filter( struct fd_list * list, uint32_t flags );
int fd_ep_filter_family( struct fd_list * list, int af );
int fd_ep_filter_list( struct fd_list * list, struct fd_list * exclude_list );
int fd_ep_clearflags( struct fd_list * list, uint32_t flags );
void fd_ep_dump_one( char * prefix, struct fd_endpoint * ep, char * suffix );
void fd_ep_dump( int indent, struct fd_list * eps );


/*============================================================*/
/*                         APPLICATIONS IDs                   */
/*============================================================*/

struct fd_app {
	struct fd_list	 chain;	/* link in cnf_apps list. List ordered by appid. */
	struct {
		unsigned auth   : 1;
		unsigned acct   : 1;
	}		 flags;
	vendor_id_t	 vndid; /* if not 0, Vendor-Specific-App-Id AVP will be used */
	application_id_t appid;	/* The identifier of the application */
};
	
int fd_app_merge(struct fd_list * list, application_id_t aid, vendor_id_t vid, int auth, int acct);
int fd_app_check(struct fd_list * list, application_id_t aid, struct fd_app **detail);
int fd_app_check_common(struct fd_list * list1, struct fd_list * list2, int * common_found);
int fd_app_empty(struct fd_list * list);

#endif /* _LIBFDCORE_H */
