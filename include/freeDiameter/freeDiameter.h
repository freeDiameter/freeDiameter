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
	int		 eyec;		/* Eye catcher: EYEC_CONFIG */
	char		*conf_file;	/* Configuration file to parse, default is DEFAULT_CONF_FILE */
	
	char   		*diam_id;	/* Diameter Identity of the local peer (FQDN -- UTF-8) */
	size_t		 diam_id_len;	/* length of the previous string */
	char		*diam_realm;	/* Diameter realm of the local peer, default to realm part of diam_id */
	size_t		 diam_realm_len;/* length of the previous string */
	
	uint16_t	 loc_port;	/* the local port for legacy Diameter (default: 3868) in host byte order */
	uint16_t	 loc_port_tls;	/* the local port for Diameter/TLS (default: 3869) in host byte order */
	uint16_t	 loc_sctp_str;	/* default max number of streams for SCTP associations (def: 30) */
	struct fd_list	 loc_endpoints;	/* the local endpoints to bind the server to. list of struct fd_endpoint. default is empty (bind all) */
	struct {
		unsigned no_ip4 : 1;	/* disable IP */
		unsigned no_ip6 : 1;	/* disable IPv6 */
		unsigned no_tcp : 1;	/* disable use of TCP */
		unsigned no_sctp: 1;	/* disable the use of SCTP */
		unsigned pr_tcp	: 1;	/* prefer TCP over SCTP */
		unsigned tls_alg: 1;	/* TLS algorithm for initiated cnx. 0: separate port. 1: inband-security (old) */
		unsigned no_fwd : 1;	/* the peer does not relay messages (0xffffff app id) */
	} 		 flags;
	
	unsigned int	 timer_tc;	/* The value in seconds of the default Tc timer */
	unsigned int 	 timer_tw;	/* The value in seconds of the default Tw timer */
	
	uint32_t	 or_state_id;	/* The value to use in Origin-State-Id, default to random value */
	struct dictionary *g_dict;	/* pointer to the global dictionary */
	struct fifo	  *g_fifo_main;	/* FIFO queue of events in the daemon main (struct fd_event items) */
};

#define	EYEC_CONFIG	0xC011F16

/* The pointer to access the global configuration, initalized in main */
extern struct fd_config *fd_g_config;

/* Endpoints */
struct fd_endpoint {
	struct fd_list  chain;	/* link in loc_endpoints list */
	sSS		ss;	/* the socket information. */
};

/* Events */
struct fd_event {
	int	 code; /* codespace depends on the queue */
	void    *data;
};

/* send an event */
static __inline__ int fd_event_send(struct fifo *queue, int code, void * data)
{
	struct fd_event * ev;
	CHECK_MALLOC( ev = malloc(sizeof(struct fd_event)) );
	ev->code = code;
	ev->data = data;
	CHECK_FCT( fd_fifo_post(queue, &ev) );
	return 0;
}
/* receive an event */
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
 *  dict	: dictionary to use for AVP definitions
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
int fd_msg_rescode_set( struct msg * msg, struct dictionary * dict, char * rescode, char * errormsg, struct avp * optavp, int type_id );

/* The following functions are used to achieve frequent operations on the messages */
int fd_msg_add_origin ( struct msg * msg, struct dictionary * dict, int osi ); /* Add Origin-Host, Origin-Realm, (if osi) Origin-State-Id AVPS at the end of the message */



/***************************************/
/*   Dispatch module, daemon's part    */
/***************************************/

enum {
	DISP_APP_AUTH	= 1,
	DISP_APP_ACCT	= 2
};
/*
 * FUNCTION:	fd_disp_app_support
 *
 * PARAMETERS:
 *  app		: The dictionary object corresponding to the Application.
 *  vendor	: (Optional) the dictionary object of a Vendor to claim support in Vendor-Specific-Application-Id
 *  flags	: Combination of DISP_APP_* flags.
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
int fd_disp_app_support ( struct dict_object * app, struct dict_object * vendor, int flags );

/* Note: if we want to support capabilities updates, we'll have to add possibility to remove an app as well... */


#endif /* _FREEDIAMETER_H */
