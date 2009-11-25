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
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

/* GNUTLS version */
#ifndef GNUTLS_VERSION
#define GNUTLS_VERSION LIBGNUTLS_VERSION
#endif /* GNUTLS_VERSION */

/* Check the return value of a GNUTLS function, log and propagate */
#define CHECK_GNUTLS_DO( __call__, __fallback__ ) {						\
	int __ret__;										\
	TRACE_DEBUG_ALL( "Check FCT: " #__call__ );						\
	__ret__ = (__call__);									\
	if (__ret__ < 0) {									\
		TRACE_DEBUG(INFO, "Error in '" #__call__ "':\t%s", gnutls_strerror(__ret__));	\
		__fallback__;									\
	}											\
}


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
	
	struct {
		/* Credentials parameters (backup) */
		char *  			 cert_file;
		char *				 key_file;
		
		char *  			 ca_file;
		char *  			 crl_file;
		
		char *				 prio_string;
		unsigned int 			 dh_bits;
		
		/* GNUTLS parameters */
		gnutls_priority_t 		 prio_cache;
		gnutls_dh_params_t 		 dh_cache;
		
		/* GNUTLS server credential(s) */
		gnutls_certificate_credentials_t credentials;
		
	} 		 cnf_sec_data;
	
	uint32_t	 cnf_orstateid;	/* The value to use in Origin-State-Id, default to random value */
	struct dictionary *cnf_dict;	/* pointer to the global dictionary */
	struct fifo	  *cnf_main_ev;	/* events for the daemon's main (struct fd_event items) */
};
extern struct fd_config *fd_g_config; /* The pointer to access the global configuration, initalized in main */

/* Endpoints */
struct fd_endpoint {
	struct fd_list  chain;	/* link in cnf_endpoints list */
	
	union {
		sSS		ss;	/* the socket information. List is always ordered by ss value (memcmp) -- see fd_ep_add_merge */
		sSA4		sin;
		sSA6		sin6;
		sSA		sa;
	};
	
#define	EP_FL_CONF	(1 << 0)	/* This endpoint is statically configured in a configuration file */
#define	EP_FL_DISC	(1 << 1)	/* This endpoint was resolved from the Diameter Identity or other DNS query */
#define	EP_FL_ADV	(1 << 2)	/* This endpoint was advertized in Diameter CER/CEA exchange */
#define	EP_FL_LL	(1 << 3)	/* Lower layer mechanism provided this endpoint */
#define	EP_FL_PRIMARY	(1 << 4)	/* This endpoint is primary in a multihomed SCTP association */
	uint32_t	flags;		/* Additional information about the endpoint */
		
	/* To add: a validity timestamp for DNS records ? How do we retrieve this lifetime from DNS ? */
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
	size_t 	 size;
	void    *data;
};

/* Daemon's codespace: 1000->1999 */
enum {
	 FDEV_TERMINATE	= 1000	/* request to terminate */
	,FDEV_DUMP_DICT		/* Dump the content of the dictionary */
	,FDEV_DUMP_EXT		/* Dump state of extensions */
	,FDEV_DUMP_SERV		/* Dump the server socket status */
	,FDEV_DUMP_QUEUES	/* Dump the message queues */
	,FDEV_DUMP_CONFIG	/* Dump the configuration */
	,FDEV_DUMP_PEERS	/* Dump the list of peers */
};

static __inline__ int fd_event_send(struct fifo *queue, int code, size_t datasz, void * data)
{
	struct fd_event * ev;
	CHECK_MALLOC( ev = malloc(sizeof(struct fd_event)) );
	ev->code = code;
	ev->size = datasz;
	ev->data = data;
	CHECK_FCT( fd_fifo_post(queue, &ev) );
	return 0;
}
static __inline__ int fd_event_get(struct fifo *queue, int *code, size_t *datasz, void ** data)
{
	struct fd_event * ev;
	CHECK_FCT( fd_fifo_get(queue, &ev) );
	if (code)
		*code = ev->code;
	if (datasz)
		*datasz = ev->size;
	if (data)
		*data = ev->data;
	free(ev);
	return 0;
}
static __inline__ int fd_event_timedget(struct fifo *queue, struct timespec * timeout, int timeoutcode, int *code, size_t *datasz, void ** data)
{
	struct fd_event * ev;
	int ret = 0;
	ret = fd_fifo_timedget(queue, &ev, timeout);
	if (ret == ETIMEDOUT) {
		if (code)
			*code = timeoutcode;
		if (datasz)
			*datasz = 0;
		if (data)
			*data = NULL;
	} else {
		CHECK_FCT( ret );
		if (code)
			*code = ev->code;
		if (datasz)
			*datasz = ev->size;
		if (data)
			*data = ev->data;
		free(ev);
	}
	return 0;
}
static __inline__ void fd_event_destroy(struct fifo **queue, void (*free_cb)(void * data))
{
	struct fd_event * ev;
	/* Purge all events, and free the associated data if any */
	while (fd_fifo_tryget( *queue, &ev ) == 0) {
		(*free_cb)(ev->data);
		free(ev);
	}
	CHECK_FCT_DO( fd_fifo_del(queue), /* continue */ );
	return ;
}  
const char * fd_ev_str(int event); /* defined in freeDiameter/main.c */


/***************************************/
/*   Peers information                 */
/***************************************/

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
	, "STATE_ZOMBIE"		\
	};
extern const char *peer_state_str[];
#define STATE_STR(state) \
	(((unsigned)(state)) <= STATE_MAX ? peer_state_str[((unsigned)(state)) ] : "<Invalid>")

/* Information about a remote peer */
struct peer_info {
	
	char * 		pi_diamid;	/* UTF-8, \0 terminated. The Diameter Identity of the remote peer. */
	
	struct {
		struct {
			#define PI_P3_DEFAULT	0	/* Use any available protocol */
			#define PI_P3_IP	1	/* Use only IP to connect to this peer */
			#define PI_P3_IPv6	2	/* resp, IPv6 */
			unsigned	pro3 :2;

			#define PI_P4_DEFAULT	0	/* Attempt any available protocol */
			#define PI_P4_TCP	1	/* Only use TCP */
			#define PI_P4_SCTP	2	/* Only use SCTP */
			unsigned	pro4 :2;

			#define PI_ALGPREF_SCTP	0	/* SCTP is  attempted first (default) */
			#define PI_ALGPREF_TCP	1	/* TCP is attempted first */
			unsigned	alg :1;

			#define PI_SEC_DEFAULT	0	/* New TLS security (handshake after connection, protecting also CER/CEA) */
			#define PI_SEC_NONE	1	/* Transparent security with this peer (IPsec) */
			#define PI_SEC_TLS_OLD	2	/* Old TLS security (use Inband-Security-Id AVP during CER/CEA) */
			unsigned	sec :2;		/* Set sec = 3 to authorize use of (Inband-Security-Id == NONE) with this peer, sec = 2 only authorizing TLS */

			#define PI_EXP_NONE	0	/* the peer entry does not expire */
			#define PI_EXP_INACTIVE	1	/* the peer entry expires (i.e. is deleted) after pi_lft seconds without activity */
			unsigned	exp :1;

			#define PI_PRST_NONE	0	/* the peer entry is deleted after disconnection / error */
			#define PI_PRST_ALWAYS	1	/* the peer entry is persistant (will be kept as ZOMBIE in case of error) */
			unsigned	persist :1;
			
		}		pic_flags;	/* Flags influencing the connection to the remote peer */
		
		char * 		pic_realm;	/* If configured, the daemon will match the received realm in CER/CEA matches this. */
		uint16_t	pic_port; 	/* port to connect to. 0: default. */
		
		uint32_t 	pic_lft;	/* lifetime of this peer when inactive (see pic_flags.exp definition) */
		int		pic_tctimer; 	/* use this value for TcTimer instead of global, if != 0 */
		int		pic_twtimer; 	/* use this value for TwTimer instead of global, if != 0 */
		
		char *		pic_priority;	/* Priority string for GnuTLS if we don't use the default */
		
	} config;	/* Configured data (static for this peer entry) */
	
	struct {
		
		enum peer_state	pir_state;	/* Current state of the peer in the state machine */
		
		char * 		pir_realm;	/* The received realm in CER/CEA. */
		
		uint32_t	pir_vendorid;	/* Content of the Vendor-Id AVP, or 0 by default */
		uint32_t	pir_orstate;	/* Origin-State-Id value */
		char *		pir_prodname;	/* copy of UTF-8 Product-Name AVP (\0 terminated) */
		uint32_t	pir_firmrev;	/* Content of the Firmware-Revision AVP */
		int		pir_relay;	/* The remote peer advertized the relay application */
		struct fd_list	pir_apps;	/* applications advertised by the remote peer, except relay (pi_flags.relay) */
		int		pir_isi;	/* Inband-Security-Id advertised (PI_SEC_* bits) */
		
		int		pir_proto;	/* The L4 protocol currently used with the peer (IPPROTO_TCP or IPPROTO_SCTP) */
		const gnutls_datum_t 	*pir_cert_list; 	/* The (valid) credentials that the peer has presented, or NULL if TLS is not used */
								/* This is inspired from http://www.gnu.org/software/gnutls/manual/gnutls.html#ex_003ax509_002dinfo 
								   see there for example of using this data */
		unsigned int 	pir_cert_list_size;		/* Number of certificates in the list */
		
	} runtime;	/* Data populated after connection, may change between 2 connections -- not used by fd_peer_add */
	
	struct fd_list	pi_endpoints;	/* Endpoint(s) of the remote peer (configured, discovered, or advertized). list of struct fd_endpoint. DNS resolved if empty. */
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

/* Parse a message against our dictionary, and in case of error log and eventually build the error reply (on return and EBADMSG, *msg == NULL or *msg is the error message ready to send) */
int fd_msg_parse_or_error( struct msg ** msg );


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


/***************************************/
/*   Endpoints lists helpers           */
/***************************************/

int fd_ep_add_merge( struct fd_list * list, sSA * sa, socklen_t sl, uint32_t flags );
int fd_ep_filter( struct fd_list * list, uint32_t flags );
int fd_ep_filter_family( struct fd_list * list, int af );
int fd_ep_clearflags( struct fd_list * list, uint32_t flags );
void fd_ep_dump_one( char * prefix, struct fd_endpoint * ep, char * suffix );
void fd_ep_dump( int indent, struct fd_list * eps );

/***************************************/
/*   Applications lists helpers        */
/***************************************/

int fd_app_merge(struct fd_list * list, application_id_t aid, vendor_id_t vid, int auth, int acct);
int fd_app_find_common(struct fd_list * target, struct fd_list * reference);
int fd_app_gotcommon(struct fd_list * apps);

#endif /* _FREEDIAMETER_H */
