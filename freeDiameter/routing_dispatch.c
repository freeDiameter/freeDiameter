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

#include "fD.h"

/********************************************************************************/
/*              First part : handling the extensions callbacks                  */
/********************************************************************************/

/* Lists of the callbacks, and locks to protect them */
static pthread_rwlock_t rt_fwd_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct fd_list 	rt_fwd_list = FD_LIST_INITIALIZER_O(rt_fwd_list, &rt_fwd_lock);

static pthread_rwlock_t rt_out_lock = PTHREAD_RWLOCK_INITIALIZER;
static struct fd_list 	rt_out_list = FD_LIST_INITIALIZER_O(rt_out_list, &rt_out_lock);

/* Items in the lists are the same */
struct rt_hdl {
	struct fd_list	chain;	/* link in the rt_fwd_list or rt_out_list */
	void *		cbdata;	/* the registered data */
	union {
		int	order;	/* This value is used to sort the list */
		int 	dir;	/* It is the direction for FWD handlers */
		int	prio;	/* and the priority for OUT handlers */
	};
	union {
		int (*rt_fwd_cb)(void * cbdata, struct msg ** msg);
		int (*rt_out_cb)(void * cbdata, struct msg * msg, struct fd_list * candidates);
	};
};	

/* Add a new entry in the list */
static int add_ordered(struct rt_hdl * new, struct fd_list * list)
{
	/* The list is ordered by prio parameter */
	struct fd_list * li;
	
	CHECK_POSIX( pthread_rwlock_wrlock(list->o) );
	
	for (li = list->next; li != list; li = li->next) {
		struct rt_hdl * h = (struct rt_hdl *) li;
		if (new->order <= h->order)
			break;
	}
	
	fd_list_insert_before(li, &new->chain);
	
	CHECK_POSIX( pthread_rwlock_unlock(list->o) );
	
	return 0;
}

/* Register a new FWD callback */
int fd_rt_fwd_register ( int (*rt_fwd_cb)(void * cbdata, struct msg ** msg), void * cbdata, enum fd_rt_fwd_dir dir, struct fd_rt_fwd_hdl ** handler )
{
	struct rt_hdl * new;
	
	TRACE_ENTRY("%p %p %d %p", rt_fwd_cb, cbdata, dir, handler);
	CHECK_PARAMS( rt_fwd_cb );
	CHECK_PARAMS( (dir >= RT_FWD_REQ) && ( dir <= RT_FWD_ANS) );
	
	/* Create a new container */
	CHECK_MALLOC(new = malloc(sizeof(struct rt_hdl)));
	memset(new, 0, sizeof(struct rt_hdl));
	
	/* Write the content */
	fd_list_init(&new->chain, NULL);
	new->cbdata 	= cbdata;
	new->dir    	= dir;
	new->rt_fwd_cb 	= rt_fwd_cb;
	
	/* Save this in the list */
	CHECK_FCT( add_ordered(new, &rt_fwd_list) );
	
	/* Give it back to the extension if needed */
	if (handler)
		*handler = (void *)new;
	
	return 0;
}

/* Remove it */
int fd_rt_fwd_unregister ( struct fd_rt_fwd_hdl * handler, void ** cbdata )
{
	struct rt_hdl * del;
	TRACE_ENTRY( "%p %p", handler, cbdata);
	CHECK_PARAMS( handler );
	
	del = (struct rt_hdl *)handler;
	CHECK_PARAMS( del->chain.head == &rt_fwd_list );
	
	/* Unlink */
	CHECK_POSIX( pthread_rwlock_wrlock(&rt_fwd_lock) );
	fd_list_unlink(&del->chain);
	CHECK_POSIX( pthread_rwlock_unlock(&rt_fwd_lock) );
	
	if (cbdata)
		*cbdata = del->cbdata;
	
	free(del);
	return 0;
}

/* Register a new OUT callback */
int fd_rt_out_register ( int (*rt_out_cb)(void * cbdata, struct msg * msg, struct fd_list * candidates), void * cbdata, int priority, struct fd_rt_out_hdl ** handler )
{
	struct rt_hdl * new;
	
	TRACE_ENTRY("%p %p %d %p", rt_out_cb, cbdata, priority, handler);
	CHECK_PARAMS( rt_out_cb );
	
	/* Create a new container */
	CHECK_MALLOC(new = malloc(sizeof(struct rt_hdl)));
	memset(new, 0, sizeof(struct rt_hdl));
	
	/* Write the content */
	fd_list_init(&new->chain, NULL);
	new->cbdata 	= cbdata;
	new->prio    	= priority;
	new->rt_out_cb 	= rt_out_cb;
	
	/* Save this in the list */
	CHECK_FCT( add_ordered(new, &rt_out_list) );
	
	/* Give it back to the extension if needed */
	if (handler)
		*handler = (void *)new;
	
	return 0;
}

/* Remove it */
int fd_rt_out_unregister ( struct fd_rt_out_hdl * handler, void ** cbdata )
{
	struct rt_hdl * del;
	TRACE_ENTRY( "%p %p", handler, cbdata);
	CHECK_PARAMS( handler );
	
	del = (struct rt_hdl *)handler;
	CHECK_PARAMS( del->chain.head == &rt_out_list );
	
	/* Unlink */
	CHECK_POSIX( pthread_rwlock_wrlock(&rt_out_lock) );
	fd_list_unlink(&del->chain);
	CHECK_POSIX( pthread_rwlock_unlock(&rt_out_lock) );
	
	if (cbdata)
		*cbdata = del->cbdata;
	
	free(del);
	return 0;
}

/********************************************************************************/
/*                      Some default OUT routing callbacks                      */
/********************************************************************************/

/* Prevent sending to peers that do not support the message application */
static int dont_send_if_no_common_app(void * cbdata, struct msg * msg, struct fd_list * candidates)
{
	struct fd_list * li;
	struct msg_hdr * hdr;
	
	TRACE_ENTRY("%p %p %p", cbdata, msg, candidates);
	CHECK_PARAMS(msg && candidates);
	
	CHECK_FCT( fd_msg_hdr(msg, &hdr) );
	
	/* For Base Diameter Protocol, every peer is supposed to support it, so skip */
	if (hdr->msg_appl == 0)
		return 0;
	
	/* Otherwise, check that the peers support the application */
	for (li = candidates->next; li != candidates; li = li->next) {
		struct rtd_candidate *c = (struct rtd_candidate *) li;
		struct fd_peer * peer;
		struct fd_app *found;
		CHECK_FCT( fd_peer_getbyid( c->diamid, (void *)&peer ) );
		if (peer && (peer->p_hdr.info.runtime.pir_relay == 0)) {
			/* Check if the remote peer advertised the message's appli */
			CHECK_FCT( fd_app_check(&peer->p_hdr.info.runtime.pir_apps, hdr->msg_appl, &found) );
			if (!found)
				c->score += FD_SCORE_NO_DELIVERY;
		}
	}

	return 0;
}

/* Detect if the Destination-Host and Destination-Realm match the peer */
static int score_destination_avp(void * cbdata, struct msg * msg, struct fd_list * candidates)
{
	struct fd_list * li;
	struct avp * avp;
	union avp_value *dh = NULL, *dr = NULL;
	
	TRACE_ENTRY("%p %p %p", cbdata, msg, candidates);
	CHECK_PARAMS(msg && candidates);
	
	/* Search the Destination-Host and Destination-Realm AVPs -- we could also use fd_msg_search_avp here, but this one is slightly more efficient */
	CHECK_FCT(  fd_msg_browse(msg, MSG_BRW_FIRST_CHILD, &avp, NULL) );
	while (avp) {
		struct avp_hdr * ahdr;
		CHECK_FCT(  fd_msg_avp_hdr( avp, &ahdr ) );

		if (! (ahdr->avp_flags & AVP_FLAG_VENDOR)) {
			switch (ahdr->avp_code) {
				case AC_DESTINATION_HOST:
					/* Parse this AVP */
					CHECK_FCT( fd_msg_parse_dict ( avp, fd_g_config->cnf_dict, NULL ) );
					ASSERT( ahdr->avp_value );
					dh = ahdr->avp_value;
					break;

				case AC_DESTINATION_REALM:
					/* Parse this AVP */
					CHECK_FCT( fd_msg_parse_dict ( avp, fd_g_config->cnf_dict, NULL ) );
					ASSERT( ahdr->avp_value );
					dr = ahdr->avp_value;
					break;
			}
		}

		if (dh && dr)
			break;

		/* Go to next AVP */
		CHECK_FCT(  fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL) );
	}
	
	/* Now, check each candidate against these AVP values */
	for (li = candidates->next; li != candidates; li = li->next) {
		struct rtd_candidate *c = (struct rtd_candidate *) li;
		struct fd_peer * peer;
		CHECK_FCT( fd_peer_getbyid( c->diamid, (void *)&peer ) );
		if (peer) {
			if (dh 
				&& (dh->os.len == strlen(peer->p_hdr.info.pi_diamid)) 
				/* Here again we use strncasecmp on UTF8 data... This should probably be changed. */
				&& (strncasecmp(peer->p_hdr.info.pi_diamid, (char *)dh->os.data, dh->os.len) == 0)) {
				/* The candidate is the Destination-Host */
				c->score += FD_SCORE_FINALDEST;
			} else {
				if (dr  && peer->p_hdr.info.runtime.pir_realm 
					&& (dr->os.len == strlen(peer->p_hdr.info.runtime.pir_realm)) 
					/* Yet another case where we use strncasecmp on UTF8 data... Hmmm :-( */
					&& (strncasecmp(peer->p_hdr.info.runtime.pir_realm, (char *)dr->os.data, dr->os.len) == 0)) {
					/* The candidate's realm matchs the Destination-Realm */
					c->score += FD_SCORE_REALM;
				}
			}
		}
	}

	return 0;
}

/********************************************************************************/
/*                        Helper functions                                      */
/********************************************************************************/

/* Find (first) '!' and '@' positions in a UTF-8 encoded string (User-Name AVP value) */
static void nai_get_indexes(union avp_value * un, int * excl_idx, int * at_idx)
{
	int i;
	
	TRACE_ENTRY("%p %p %p", un, excl_idx, at_idx);
	CHECK_PARAMS_DO( un && excl_idx, return );
	*excl_idx = 0;
	
	/* Search if there is a '!' before any '@' -- do we need to check it contains a '.' ? */
	for (i = 0; i < un->os.len; i++) {
		/* The '!' marks the decorated NAI */
		if ( un->os.data[i] == (unsigned char) '!' ) {
			if (!*excl_idx)
				*excl_idx = i;
			if (!at_idx)
				return;
		}
		/* If we reach the realm part, we can stop */
		if ( un->os.data[i] == (unsigned char) '@' ) {
			if (at_idx)
				*at_idx = i;
			break;
		}
		/* Skip escaped characters */
		if ( un->os.data[i] == (unsigned char) '\\' ) {
			i++;
			continue;
		}
		/* Skip UTF-8 characters spanning on several bytes */
		if ( (un->os.data[i] & 0xF8) == 0xF0 ) { /* 11110zzz */
			i += 3;
			continue;
		}
		if ( (un->os.data[i] & 0xF0) == 0xE0 ) { /* 1110yyyy */
			i += 2;
			continue;
		}
		if ( (un->os.data[i] & 0xE0) == 0xC0 ) { /* 110yyyxx */
			i += 1;
			continue;
		}
	}
	
	return;
}	

/* Test if a User-Name AVP contains a Decorated NAI -- RFC4282, RFC5729 */
static int is_decorated_NAI(union avp_value * un)
{
	int i;
	TRACE_ENTRY("%p", un);
	
	/* If there was no User-Name, we return false */
	if (un == NULL)
		return 0;
	
	nai_get_indexes(un, &i, NULL);
	
	return i;
}

/* Create new User-Name and Destination-Realm values */
static int process_decorated_NAI(union avp_value * un, union avp_value * dr)
{
	int at_idx = 0, sep_idx = 0;
	unsigned char * old_un;
	TRACE_ENTRY("%p %p", un, dr);
	CHECK_PARAMS(un && dr);
	
	/* Save the decorated User-Name, for example 'homerealm.example.net!user@otherrealm.example.net' */
	old_un = un->os.data;
	
	/* Search the positions of the first '!' and the '@' in the string */
	nai_get_indexes(un, &sep_idx, &at_idx);
	CHECK_PARAMS( (0 < sep_idx) && (sep_idx < at_idx) && (at_idx < un->os.len));
	
	/* Create the new User-Name value */
	CHECK_MALLOC( un->os.data = malloc( at_idx ) );
	memcpy( un->os.data, old_un + sep_idx + 1, at_idx - sep_idx ); /* user@ */
	memcpy( un->os.data + at_idx - sep_idx, old_un, sep_idx ); /* homerealm.example.net */
	
	/* Create the new Destination-Realm value */
	CHECK_MALLOC( dr->os.data = realloc(dr->os.data, sep_idx) );
	memcpy( dr->os.data, old_un, sep_idx );
	dr->os.len = sep_idx;
	
	TRACE_DEBUG(FULL, "Processed Decorated NAI : '%.*s' became '%.*s' (%.*s)",
				un->os.len, old_un,
				at_idx, un->os.data,
				dr->os.len, dr->os.data);
	
	un->os.len = at_idx;
	free(old_un);
	
	return 0;
}

/* Function to return an error to an incoming request */
static int return_error(struct msg ** pmsg, char * error_code, char * error_message, struct avp * failedavp)
{
	struct fd_peer * peer;
	int is_loc = 0;

	/* Get the source of the message */
	{
		char * id;
		CHECK_FCT( fd_msg_source_get( *pmsg, &id ) );
		
		if (id == NULL) {
			is_loc = 1; /* The message was issued locally */
		} else {
		
			/* Search the peer with this id */
			CHECK_FCT( fd_peer_getbyid( id, (void *)&peer ) );

			if (!peer) {
				TRACE_DEBUG(INFO, "Unable to send error '%s' to deleted peer '%s' in reply to:", error_code, id);
				fd_msg_dump_walk(INFO, *pmsg);
				fd_msg_free(*pmsg);
				*pmsg = NULL;
				return 0;
			}
		}
	}
	
	/* Create the error message */
	CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, pmsg, MSGFL_ANSW_ERROR ) );

	/* Set the error code */
	CHECK_FCT( fd_msg_rescode_set(*pmsg, error_code, error_message, failedavp, 1 ) );

	/* Send the answer */
	if (is_loc) {
		CHECK_FCT( fd_fifo_post(fd_g_incoming, pmsg) );
	} else {
		CHECK_FCT( fd_out_send(pmsg, NULL, peer, 0) );
	}
	
	/* Done */
	return 0;
}


/****************************************************************************/
/*         Second part : threads moving messages in the daemon              */
/****************************************************************************/

/* These are the functions of each threads: dispatch & routing */
/* The DISPATCH message processing */
static int msg_dispatch(struct msg ** pmsg)
{
	struct msg_hdr * hdr;
	int is_req = 0, ret;
	struct session * sess;
	enum disp_action action;
	const char * ec = NULL;
	const char * em = NULL;

	/* Read the message header */
	CHECK_FCT( fd_msg_hdr(*pmsg, &hdr) );
	is_req = hdr->msg_flags & CMD_FLAG_REQUEST;
	
	/* Note: if the message is for local delivery, we should test for duplicate
	  (draft-asveren-dime-dupcons-00). This may conflict with path validation decisions, no clear answer yet */

	/* At this point, we need to understand the message content, so parse it */
	CHECK_FCT_DO( ret = fd_msg_parse_or_error( pmsg ),
		{
			/* in case of error, the message is already dump'd */
			if ((ret == EBADMSG) && (*pmsg != NULL)) {
				/* msg now contains the answer message to send back */
				CHECK_FCT( fd_fifo_post(fd_g_outgoing, pmsg) );
			}
			if (*pmsg) {	/* another error happen'd */
				TRACE_DEBUG(INFO, "An unexpected error occurred (%s), discarding a message:", strerror(ret));
				fd_msg_dump_walk(INFO, *pmsg);
				CHECK_FCT_DO( fd_msg_free(*pmsg), /* continue */);
				*pmsg = NULL;
			}
			/* We're done with this one */
			return 0;
		} );

	/* First, if the original request was registered with a callback and we receive the answer, call it. */
	if ( ! is_req ) {
		struct msg * qry;
		void (*anscb)(void *, struct msg **) = NULL;
		void * data = NULL;

		/* Retrieve the corresponding query */
		CHECK_FCT( fd_msg_answ_getq( *pmsg, &qry ) );

		/* Retrieve any registered handler */
		CHECK_FCT( fd_msg_anscb_get( qry, &anscb, &data ) );

		/* If a callback was registered, pass the message to it */
		if (anscb != NULL) {

			TRACE_DEBUG(FULL, "Calling callback registered when query was sent (%p, %p)", anscb, data);
			(*anscb)(data, pmsg);
			
			/* If the message is processed, we're done */
			if (*pmsg == NULL) {
				return 0;
			}
		}
	}
	
	/* Retrieve the session of the message */
	CHECK_FCT( fd_msg_sess_get(fd_g_config->cnf_dict, *pmsg, &sess, NULL) );

	/* Now, call any callback registered for the message */
	CHECK_FCT( fd_msg_dispatch ( pmsg, sess, &action, &ec) );

	/* Now, act depending on msg and action and ec */
	if (*pmsg)
		switch ( action ) {
			case DISP_ACT_CONT:
				/* No callback has handled the message, let's reply with a generic error */
				em = "The message was not handled by any extension callback";
				ec = "DIAMETER_COMMAND_UNSUPPORTED";
			
			case DISP_ACT_ERROR:
				/* We have a problem with delivering the message */
				if (ec == NULL) {
					ec = "DIAMETER_UNABLE_TO_COMPLY";
				}
				
				if (!is_req) {
					TRACE_DEBUG(INFO, "Received an answer to a localy issued query, but no handler processed this answer!");
					fd_msg_dump_walk(INFO, *pmsg);
					fd_msg_free(*pmsg);
					*pmsg = NULL;
					break;
				}
				
				/* Create an answer with the error code and message */
				CHECK_FCT( fd_msg_new_answer_from_req ( fd_g_config->cnf_dict, pmsg, 0 ) );
				CHECK_FCT( fd_msg_rescode_set(*pmsg, (char *)ec, (char *)em, NULL, 1 ) );
				
			case DISP_ACT_SEND:
				/* Now, send the message */
				CHECK_FCT( fd_fifo_post(fd_g_outgoing, pmsg) );
		}
	
	/* We're done with dispatching this message */
	return 0;
}

/* The ROUTING-IN message processing */
static int msg_rt_in(struct msg ** pmsg)
{
	struct msg_hdr * hdr;
	int is_req = 0;
	int is_err = 0;
	char * qry_src = NULL;

	/* Read the message header */
	CHECK_FCT( fd_msg_hdr(*pmsg, &hdr) );
	is_req = hdr->msg_flags & CMD_FLAG_REQUEST;
	is_err = hdr->msg_flags & CMD_FLAG_ERROR;

	/* Handle incorrect bits */
	if (is_req && is_err) {
		CHECK_FCT( return_error( pmsg, "DIAMETER_INVALID_HDR_BITS", "R & E bits were set", NULL) );
		return 0;
	}
	
	/* If it is a request, we must analyze its content to decide what we do with it */
	if (is_req) {
		struct avp * avp, *un = NULL;
		union avp_value * un_val = NULL, *dr_val = NULL;
		enum status { UNKNOWN, YES, NO };
		/* Are we Destination-Host? */
		enum status is_dest_host = UNKNOWN;
		/* Are we Destination-Realm? */
		enum status is_dest_realm = UNKNOWN;
		/* Do we support the application of the message? */
		enum status is_local_app = UNKNOWN;

		/* Check if we have local support for the message application */
		if ( (hdr->msg_appl == 0) || (hdr->msg_appl == AI_RELAY) ) {
			TRACE_DEBUG(INFO, "Received a routable message with application id 0, returning DIAMETER_APPLICATION_UNSUPPORTED");
			CHECK_FCT( return_error( pmsg, "DIAMETER_APPLICATION_UNSUPPORTED", "Routable message with application id 0 or relay", NULL) );
			return 0;
		} else {
			struct fd_app * app;
			CHECK_FCT( fd_app_check(&fd_g_config->cnf_apps, hdr->msg_appl, &app) );
			is_local_app = (app ? YES : NO);
		}

		/* Parse the message for Dest-Host and Dest-Realm */
		CHECK_FCT(  fd_msg_browse(*pmsg, MSG_BRW_FIRST_CHILD, &avp, NULL)  );
		while (avp) {
			struct avp_hdr * ahdr;
			struct fd_pei error_info;
			int ret;
			CHECK_FCT(  fd_msg_avp_hdr( avp, &ahdr )  );

			if (! (ahdr->avp_flags & AVP_FLAG_VENDOR)) {
				switch (ahdr->avp_code) {
					case AC_DESTINATION_HOST:
						/* Parse this AVP */
						CHECK_FCT_DO( ret = fd_msg_parse_dict ( avp, fd_g_config->cnf_dict, &error_info ),
							{
								if (error_info.pei_errcode) {
									CHECK_FCT( return_error( pmsg, error_info.pei_errcode, error_info.pei_message, error_info.pei_avp) );
									return 0;
								} else {
									return ret;
								}
							} );
						ASSERT( ahdr->avp_value );
						/* Compare the Destination-Host AVP of the message with our identity */
						if (ahdr->avp_value->os.len != fd_g_config->cnf_diamid_len) {
							is_dest_host = NO;
						} else {
							is_dest_host = (strncasecmp(fd_g_config->cnf_diamid, (char *)ahdr->avp_value->os.data, fd_g_config->cnf_diamid_len) 
										? NO : YES);
						}
						break;

					case AC_DESTINATION_REALM:
						/* Parse this AVP */
						CHECK_FCT_DO( ret = fd_msg_parse_dict ( avp, fd_g_config->cnf_dict, &error_info ),
							{
								if (error_info.pei_errcode) {
									CHECK_FCT( return_error( pmsg, error_info.pei_errcode, error_info.pei_message, error_info.pei_avp) );
									return 0;
								} else {
									return ret;
								}
							} );
						ASSERT( ahdr->avp_value );
						dr_val = ahdr->avp_value;
						/* Compare the Destination-Realm AVP of the message with our identity */
						if (ahdr->avp_value->os.len != fd_g_config->cnf_diamrlm_len) {
							is_dest_realm = NO;
						} else {
							is_dest_realm = (strncasecmp(fd_g_config->cnf_diamrlm, (char *)ahdr->avp_value->os.data, fd_g_config->cnf_diamrlm_len) 
										? NO : YES);
						}
						break;

					case AC_USER_NAME:
						/* Parse this AVP */
						CHECK_FCT_DO( ret = fd_msg_parse_dict ( avp, fd_g_config->cnf_dict, &error_info ),
							{
								if (error_info.pei_errcode) {
									CHECK_FCT( return_error( pmsg, error_info.pei_errcode, error_info.pei_message, error_info.pei_avp) );
									return 0;
								} else {
									return ret;
								}
							} );
						ASSERT( ahdr->avp_value );
						un = avp;
						un_val = ahdr->avp_value;
						break;
				}
			}

			if ((is_dest_host != UNKNOWN) && (is_dest_realm != UNKNOWN) && un)
				break;

			/* Go to next AVP */
			CHECK_FCT(  fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL)  );
		}

		/* OK, now decide what we do with the request */

		/* Handle the missing routing AVPs first */
		if ( is_dest_realm == UNKNOWN ) {
			CHECK_FCT( return_error( pmsg, "DIAMETER_COMMAND_UNSUPPORTED", "Non-routable message not supported (invalid bit ? missing Destination-Realm ?)", NULL) );
			return 0;
		}

		/* If we are listed as Destination-Host */
		if (is_dest_host == YES) {
			if (is_local_app == YES) {
				/* Ok, give the message to the dispatch thread */
				CHECK_FCT( fd_fifo_post(fd_g_local, pmsg) );
			} else {
				/* We don't support the application, reply an error */
				CHECK_FCT( return_error( pmsg, "DIAMETER_APPLICATION_UNSUPPORTED", NULL, NULL) );
			}
			return 0;
		}

		/* If the message is explicitely for someone else */
		if ((is_dest_host == NO) || (is_dest_realm == NO)) {
			if (fd_g_config->cnf_flags.no_fwd) {
				CHECK_FCT( return_error( pmsg, "DIAMETER_UNABLE_TO_DELIVER", "This peer is not an agent", NULL) );
				return 0;
			}
		} else {
		/* Destination-Host was not set, and Destination-Realm is matching : we may handle or pass to a fellow peer */

			/* test for decorated NAI  (RFC5729 section 4.4) */
			if (is_decorated_NAI(un_val)) {
				/* Handle the decorated NAI */
				CHECK_FCT_DO( process_decorated_NAI(un_val, dr_val),
					{
						/* If the process failed, we assume it is because of the AVP format */
						CHECK_FCT( return_error( pmsg, "DIAMETER_INVALID_AVP_VALUE", "Failed to process decorated NAI", un) );
						return 0;
					} );

				/* We have transformed the AVP, now submit it again in the queue */
				CHECK_FCT(fd_fifo_post(fd_g_incoming, pmsg) );
				return 0;
			}

			if (is_local_app == YES) {
				/* Handle localy since we are able to */
				CHECK_FCT(fd_fifo_post(fd_g_local, pmsg) );
				return 0;
			}

			if (fd_g_config->cnf_flags.no_fwd) {
				/* We return an error */
				CHECK_FCT( return_error( pmsg, "DIAMETER_APPLICATION_UNSUPPORTED", NULL, NULL) );
				return 0;
			}
		}

		/* From that point, for requests, we will call the registered callbacks, then forward to another peer */

	} else {
		/* The message is an answer */
		struct msg * qry;

		/* Retrieve the corresponding query and its origin */
		CHECK_FCT( fd_msg_answ_getq( *pmsg, &qry ) );
		CHECK_FCT( fd_msg_source_get( qry, &qry_src ) );

		if ((!qry_src) && (!is_err)) {
			/* The message is a normal answer to a request issued localy, we do not call the callbacks chain on it. */
			CHECK_FCT(fd_fifo_post(fd_g_local, pmsg) );
			return 0;
		}

		/* From that point, for answers, we will call the registered callbacks, then pass it to the dispatch module or forward it */
	}

	/* Call all registered callbacks for this message */
	{
		struct fd_list * li;

		CHECK_FCT( pthread_rwlock_rdlock( &rt_fwd_lock ) );
		pthread_cleanup_push( fd_cleanup_rwlock, &rt_fwd_lock );

		/* requests: dir = 1 & 2 => in order; answers = 3 & 2 => in reverse order */
		for (	li = (is_req ? rt_fwd_list.next : rt_fwd_list.prev) ; *pmsg && (li != &rt_fwd_list) ; li = (is_req ? li->next : li->prev) ) {
			struct rt_hdl * rh = (struct rt_hdl *)li;

			if (is_req && (rh->dir > RT_FWD_ALL))
				break;
			if ((!is_req) && (rh->dir < RT_FWD_ALL))
				break;

			/* Ok, call this cb */
			TRACE_DEBUG(ANNOYING, "Calling next FWD callback on %p : %p", *pmsg, rh->rt_fwd_cb);
			CHECK_FCT_DO( (*rh->rt_fwd_cb)(rh->cbdata, pmsg),
				{
					TRACE_DEBUG(INFO, "A FWD routing callback returned an error, message discarded.");
					fd_msg_dump_walk(INFO, *pmsg);
					fd_msg_free(*pmsg);
					*pmsg = NULL;
				} );
		}

		pthread_cleanup_pop(0);
		CHECK_FCT( pthread_rwlock_unlock( &rt_fwd_lock ) );

		/* If a callback has handled the message, we stop now */
		if (!*pmsg)
			return 0;
	}

	/* Now pass the message to the next step: either forward to another peer, or dispatch to local extensions */
	if (is_req || qry_src) {
		CHECK_FCT(fd_fifo_post(fd_g_outgoing, pmsg) );
	} else {
		CHECK_FCT(fd_fifo_post(fd_g_local, pmsg) );
	}

	/* We're done with this message */
	return 0;
}
		

/* The ROUTING-OUT message processing */
static int msg_rt_out(struct msg ** pmsg)
{
	struct rt_data * rtd = NULL;
	struct msg_hdr * hdr;
	int is_req = 0;
	int ret;
	struct fd_list * li, *candidates;
	struct avp * avp;
	struct rtd_candidate * c;
	
	/* Read the message header */
	CHECK_FCT( fd_msg_hdr(*pmsg, &hdr) );
	is_req = hdr->msg_flags & CMD_FLAG_REQUEST;
	
	/* For answers, the routing is very easy */
	if ( ! is_req ) {
		struct msg * qry;
		char * qry_src = NULL;
		struct msg_hdr * qry_hdr;
		struct fd_peer * peer = NULL;

		/* Retrieve the corresponding query and its origin */
		CHECK_FCT( fd_msg_answ_getq( *pmsg, &qry ) );
		CHECK_FCT( fd_msg_source_get( qry, &qry_src ) );

		ASSERT( qry_src ); /* if it is NULL, the message should have been in the LOCAL queue! */

		/* Find the peer corresponding to this name */
		CHECK_FCT( fd_peer_getbyid( qry_src, (void *) &peer ) );
		fd_cpu_flush_cache();
		if ((!peer) || (peer->p_hdr.info.runtime.pir_state != STATE_OPEN)) {
			TRACE_DEBUG(INFO, "Unable to forward answer message to peer '%s', deleted or not in OPEN state.", qry_src);
			fd_msg_dump_walk(INFO, *pmsg);
			fd_msg_free(*pmsg);
			*pmsg = NULL;
			return 0;
		}

		/* We must restore the hop-by-hop id */
		CHECK_FCT( fd_msg_hdr(qry, &qry_hdr) );
		hdr->msg_hbhid = qry_hdr->msg_hbhid;

		/* Push the message into this peer */
		CHECK_FCT( fd_out_send(pmsg, NULL, peer, 0) );

		/* We're done with this answer */
		return 0;
	}
	
	/* From that point, the message is a request */

	/* Get the routing data out of the message if any (in case of re-transmit) */
	CHECK_FCT( fd_msg_rt_get ( *pmsg, &rtd ) );

	/* If there is no routing data already, let's create it */
	if (rtd == NULL) {
		CHECK_FCT( fd_rtd_init(&rtd) );

		/* Add all peers currently in OPEN state */
		CHECK_FCT( pthread_rwlock_rdlock(&fd_g_activ_peers_rw) );
		for (li = fd_g_activ_peers.next; li != &fd_g_activ_peers; li = li->next) {
			struct fd_peer * p = (struct fd_peer *)li->o;
			CHECK_FCT_DO( ret = fd_rtd_candidate_add(rtd, p->p_hdr.info.pi_diamid, p->p_hdr.info.runtime.pir_realm), { CHECK_FCT_DO( pthread_rwlock_unlock(&fd_g_activ_peers_rw), ); return ret; } );
		}
		CHECK_FCT( pthread_rwlock_unlock(&fd_g_activ_peers_rw) );

		/* Now let's remove all peers from the Route-Records */
		CHECK_FCT(  fd_msg_browse(*pmsg, MSG_BRW_FIRST_CHILD, &avp, NULL)  );
		while (avp) {
			struct avp_hdr * ahdr;
			struct fd_pei error_info;
			CHECK_FCT(  fd_msg_avp_hdr( avp, &ahdr )  );

			if ((ahdr->avp_code == AC_ROUTE_RECORD) && (! (ahdr->avp_flags & AVP_FLAG_VENDOR)) ) {
				/* Parse this AVP */
				CHECK_FCT_DO( ret = fd_msg_parse_dict ( avp, fd_g_config->cnf_dict, &error_info ),
					{
						if (error_info.pei_errcode) {
							CHECK_FCT( return_error( pmsg, error_info.pei_errcode, error_info.pei_message, error_info.pei_avp) );
							return 0;
						} else {
							return ret;
						}
					} );
				ASSERT( ahdr->avp_value );
				/* Remove this value from the list */
				fd_rtd_candidate_del(rtd, (char *)ahdr->avp_value->os.data, ahdr->avp_value->os.len);
			}

			/* Go to next AVP */
			CHECK_FCT(  fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL)  );
		}
	}

	/* Note: we reset the scores and pass the message to the callbacks, maybe we could re-use the saved scores when we have received an error ? */

	/* Ok, we have our list in rtd now, let's (re)initialize the scores */
	fd_rtd_candidate_extract(rtd, &candidates, FD_SCORE_INI);

	/* Pass the list to registered callbacks (even if it is empty) */
	{
		CHECK_FCT( pthread_rwlock_rdlock( &rt_out_lock ) );
		pthread_cleanup_push( fd_cleanup_rwlock, &rt_out_lock );

		/* We call the cb by reverse priority order */
		for (	li = rt_out_list.prev ; li != &rt_out_list ; li = li->prev ) {
			struct rt_hdl * rh = (struct rt_hdl *)li;

			TRACE_DEBUG(ANNOYING, "Calling next OUT callback on %p : %p (prio %d)", *pmsg, rh->rt_out_cb, rh->prio);
			CHECK_FCT_DO( ret = (*rh->rt_out_cb)(rh->cbdata, *pmsg, candidates),
				{
					TRACE_DEBUG(INFO, "An OUT routing callback returned an error (%s) ! Message discarded.", strerror(ret));
					fd_msg_dump_walk(INFO, *pmsg);
					fd_msg_free(*pmsg);
					*pmsg = NULL;
					break;
				} );
		}

		pthread_cleanup_pop(0);
		CHECK_FCT( pthread_rwlock_unlock( &rt_out_lock ) );

		/* If an error occurred, skip to the next message */
		if (! *pmsg) {
			if (rtd)
				fd_rtd_free(&rtd);
			return 0;
		}
	}
	
	/* Order the candidate peers by score attributed by the callbacks */
	CHECK_FCT( fd_rtd_candidate_reorder(candidates) );

	/* Save the routing information in the message */
	CHECK_FCT( fd_msg_rt_associate ( *pmsg, &rtd ) );

	/* Now try sending the message */
	for (li = candidates->prev; li != candidates; li = li->prev) {
		struct fd_peer * peer;

		c = (struct rtd_candidate *) li;

		/* Stop when we have reached the end of valid candidates */
		if (c->score < 0)
			break;

		/* Search for the peer */
		CHECK_FCT( fd_peer_getbyid( c->diamid, (void *)&peer ) );

		fd_cpu_flush_cache();
		if (peer && (peer->p_hdr.info.runtime.pir_state == STATE_OPEN)) {
			/* Send to this one */
			CHECK_FCT_DO( fd_out_send(pmsg, NULL, peer, 0), continue );
			
			/* If the sending was successful */
			break;
		}
	}

	/* If the message has not been sent, return an error */
	if (*pmsg) {
		TRACE_DEBUG(INFO, "Could not send the following message, replying with UNABLE_TO_DELIVER");
		fd_msg_dump_walk(INFO, *pmsg);
		return_error( pmsg, "DIAMETER_UNABLE_TO_DELIVER", "No suitable candidate to route the message to", NULL);
	}

	/* We're done with this message */
	
	return 0;
}


/********************************************************************************/
/*                     Management of the threads                                */
/********************************************************************************/

/* Note: in the first version, we only create one thread of each kind.
 We could improve the scalability by using the threshold feature of the queues
 to create additional threads if a queue is filling up, or at least giving a configurable
 number of threads of each kind.
 */

/* Control of the threads */
static enum { RUN = 0, STOP = 1 } order_val = RUN;
static pthread_mutex_t order_lock = PTHREAD_MUTEX_INITIALIZER;

/* Threads report their status */
enum thread_state { INITIAL = 0, RUNNING = 1, TERMINATED = 2 };
static void cleanup_state(void * state_loc)
{
	if (state_loc)
		*(enum thread_state *)state_loc = TERMINATED;
}

/* This is the common thread code (same for routing and dispatching) */
static void * process_thr(void * arg, int (*action_cb)(struct msg ** pmsg), struct fifo * queue, char * action_name)
{
	TRACE_ENTRY("%p %p %p %p", arg, action_cb, queue, action_name);
	
	/* Set the thread name */
	{
		char buf[48];
		snprintf(buf, sizeof(buf), "%s (%p)", action_name, arg);
		fd_log_threadname ( buf );
	}
	
	/* The thread reports its status when canceled */
	CHECK_PARAMS_DO(arg, return NULL);
	pthread_cleanup_push( cleanup_state, arg );
	
	/* Mark the thread running */
	*(enum thread_state *)arg = RUNNING;
	fd_cpu_flush_cache();
	
	do {
		struct msg * msg;
	
		/* Test the current order */
		{
			int must_stop;
			CHECK_POSIX_DO( pthread_mutex_lock(&order_lock), goto end ); /* we lock to flush the caches */
			must_stop = (order_val == STOP);
			CHECK_POSIX_DO( pthread_mutex_unlock(&order_lock), goto end );
			if (must_stop)
				goto end;
			
			pthread_testcancel();
		}
		
		/* Ok, we are allowed to run */
		
		/* Get the next message from the queue */
		{
			int ret;
			ret = fd_fifo_get ( queue, &msg );
			if (ret == EPIPE)
				/* The queue was destroyed, we are probably exiting */
				goto end;
			
			/* check if another error occurred */
			CHECK_FCT_DO( ret, goto fatal_error );
		}
		
		if (TRACE_BOOL(FULL)) {
			TRACE_DEBUG(FULL, "Picked next message");
			fd_msg_dump_one(ANNOYING, msg);
		}
		
		/* Now process the message */
		CHECK_FCT_DO( (*action_cb)(&msg), goto fatal_error);

		/* We're done with this message */
	
	} while (1);
	
fatal_error:
	TRACE_DEBUG(INFO, "An unrecoverable error occurred, %s thread is terminating...", action_name);
	CHECK_FCT_DO(fd_event_send(fd_g_config->cnf_main_ev, FDEV_TERMINATE, 0, NULL), );
	
end:	
	; /* noop so that we get rid of "label at end of compund statement" warning */
	/* Mark the thread as terminated */
	pthread_cleanup_pop(1);
	return NULL;
}

/* The dispatch thread */
static void * dispatch_thr(void * arg)
{
	return process_thr(arg, msg_dispatch, fd_g_local, "Dispatch");
}

/* The (routing-in) thread -- see description in freeDiameter.h */
static void * routing_in_thr(void * arg)
{
	return process_thr(arg, msg_rt_in, fd_g_incoming, "Routing-IN");
}

/* The (routing-out) thread -- see description in freeDiameter.h */
static void * routing_out_thr(void * arg)
{
	return process_thr(arg, msg_rt_out, fd_g_outgoing, "Routing-OUT");
}


/********************************************************************************/
/*                     The functions for the other files                        */
/********************************************************************************/

static pthread_t * dispatch = NULL;
static enum thread_state * disp_state = NULL;

/* Later: make this more dynamic */
static pthread_t rt_out = (pthread_t)NULL;
static enum thread_state out_state = INITIAL;

static pthread_t rt_in  = (pthread_t)NULL;
static enum thread_state in_state = INITIAL;

/* Initialize the routing and dispatch threads */
int fd_rtdisp_init(void)
{
	int i;
	
	/* Prepare the array for dispatch */
	CHECK_MALLOC( dispatch = calloc(fd_g_config->cnf_dispthr, sizeof(pthread_t)) );
	CHECK_MALLOC( disp_state = calloc(fd_g_config->cnf_dispthr, sizeof(enum thread_state)) );
	
	/* Create the threads */
	for (i=0; i < fd_g_config->cnf_dispthr; i++) {
		CHECK_POSIX( pthread_create( &dispatch[i], NULL, dispatch_thr, &disp_state[i] ) );
	}
	CHECK_POSIX( pthread_create( &rt_out, NULL, routing_out_thr, &out_state) );
	CHECK_POSIX( pthread_create( &rt_in,  NULL, routing_in_thr,  &in_state) );
	
	/* Later: TODO("Set the thresholds for the queues to create more threads as needed"); */
	
	/* Register the built-in callbacks */
	CHECK_FCT( fd_rt_out_register( dont_send_if_no_common_app, NULL, 10, NULL ) );
	CHECK_FCT( fd_rt_out_register( score_destination_avp, NULL, 10, NULL ) );
	return 0;
}

/* Ask the thread to terminate after next iteration */
int fd_rtdisp_cleanstop(void)
{
	CHECK_POSIX( pthread_mutex_lock(&order_lock) );
	order_val = STOP;
	CHECK_POSIX( pthread_mutex_unlock(&order_lock) );

	return 0;
}

static void stop_thread_delayed(enum thread_state *st, pthread_t * thr, char * th_name)
{
	TRACE_ENTRY("%p %p", st, thr);
	CHECK_PARAMS_DO(st && thr, return);

	/* Wait for a second for the thread to complete, by monitoring my_state */
	fd_cpu_flush_cache();
	if (*st != TERMINATED) {
		TRACE_DEBUG(INFO, "Waiting for the %s thread to have a chance to terminate", th_name);
		do {
			struct timespec	 ts, ts_final;

			CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &ts), break );
			
			ts_final.tv_sec = ts.tv_sec + 1;
			ts_final.tv_nsec = ts.tv_nsec;
			
			while (TS_IS_INFERIOR( &ts, &ts_final )) {
				if (*st == TERMINATED)
					break;
				
				usleep(100000);
				CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &ts), break );
			}
		} while (0);
	}

	/* Now stop the thread and reclaim its resources */
	CHECK_FCT_DO( fd_thr_term(thr ), /* continue */);
	
}

/* Stop the thread after up to one second of wait */
int fd_rtdisp_fini(void)
{
	int i;
	
	/* Destroy the incoming queue */
	CHECK_FCT_DO( fd_queues_fini(&fd_g_incoming), /* ignore */);
	
	/* Stop the routing IN thread */
	stop_thread_delayed(&in_state, &rt_in, "IN routing");
	
	/* Destroy the outgoing queue */
	CHECK_FCT_DO( fd_queues_fini(&fd_g_outgoing), /* ignore */);
	
	/* Stop the routing OUT thread */
	stop_thread_delayed(&out_state, &rt_out, "OUT routing");
	
	/* Destroy the local queue */
	CHECK_FCT_DO( fd_queues_fini(&fd_g_local), /* ignore */);
	
	/* Stop the Dispatch thread */
	for (i=0; i < fd_g_config->cnf_dispthr; i++) {
		stop_thread_delayed(&disp_state[i], &dispatch[i], "Dispatching");
	}
	
	return 0;
}

/* Cleanup handlers */
int fd_rtdisp_cleanup(void)
{
	/* Cleanup all remaining handlers */
	while (!FD_IS_LIST_EMPTY(&rt_fwd_list)) {
		CHECK_FCT_DO( fd_rt_fwd_unregister ( (void *)rt_fwd_list.next, NULL ), /* continue */ );
	}
	while (!FD_IS_LIST_EMPTY(&rt_out_list)) {
		CHECK_FCT_DO( fd_rt_out_unregister ( (void *)rt_out_list.next, NULL ), /* continue */ );
	}
	
	fd_disp_unregister_all(); /* destroy remaining handlers */

	return 0;
}


/********************************************************************************/
/*                     For extensiosn to register a new appl                    */
/********************************************************************************/

/* Add an application into the peer's supported apps */
int fd_disp_app_support ( struct dict_object * app, struct dict_object * vendor, int auth, int acct )
{
	application_id_t aid = 0;
	vendor_id_t	 vid = 0;
	
	TRACE_ENTRY("%p %p %d %d", app, vendor, auth, acct);
	CHECK_PARAMS( app && (auth || acct) );
	
	{
		enum dict_object_type type = 0;
		struct dict_application_data data;
		CHECK_FCT( fd_dict_gettype(app, &type) );
		CHECK_PARAMS( type == DICT_APPLICATION );
		CHECK_FCT( fd_dict_getval(app, &data) );
		aid = data.application_id;
	}

	if (vendor) {
		enum dict_object_type type = 0;
		struct dict_vendor_data data;
		CHECK_FCT( fd_dict_gettype(vendor, &type) );
		CHECK_PARAMS( type == DICT_VENDOR );
		CHECK_FCT( fd_dict_getval(vendor, &data) );
		vid = data.vendor_id;
	}
	
	return fd_app_merge(&fd_g_config->cnf_apps, aid, vid, auth, acct);
}



