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

/* Sessions module.
 * 
 * Basic functionalities to help implementing User sessions state machines from RFC3588.
 */

#include "libfD.h"

/*********************** Parameters **********************/

/* Size of the hash table containing the session objects (pow of 2. ex: 6 => 2^6 = 64). must be between 0 and 31. */
#ifndef SESS_HASH_SIZE
#define SESS_HASH_SIZE	6
#endif /* SESS_HASH_SIZE */

/* Default lifetime of a session, in seconds. (31 days = 2678400 seconds) */
#ifndef SESS_DEFAULT_LIFETIME
#define SESS_DEFAULT_LIFETIME	2678400
#endif /* SESS_DEFAULT_LIFETIME */

/********************** /Parameters **********************/

/* Eyescatchers definitions */
#define SH_EYEC 0x53554AD1
#define SD_EYEC 0x5355D474
#define SI_EYEC 0x53551D

/* Macro to check an object is valid */
#define VALIDATE_SH( _obj ) ( ((_obj) != NULL) && ( ((struct session_handler *)(_obj))->eyec == SH_EYEC) )
#define VALIDATE_SI( _obj ) ( ((_obj) != NULL) && ( ((struct session         *)(_obj))->eyec == SI_EYEC) )


/* Handlers registered by users of the session module */
struct session_handler {
	int		  eyec;	/* An eye catcher also used to ensure the object is valid, must be SH_EYEC */
	int		  id;	/* A unique integer to identify this handler */
	void 		(*cleanup)(char *, session_state *); /* The cleanup function to be called for cleaning a state */
};

static int 		hdl_id = 0;				/* A global counter to initialize the id field */
static pthread_mutex_t	hdl_lock = PTHREAD_MUTEX_INITIALIZER;	/* lock to protect hdl_id; we could use atomic operations otherwise (less portable) */


/* Data structures linked from the sessions, containing the applications states */
struct state {
	int			 eyec;	/* Must be SD_EYEC */
	session_state		*state;	/* The state registered by the application, never NULL (or the whole object is deleted) */
	struct fd_list		 chain;	/* Chaining in the list of session's states ordered by hdl->id */
	union {
		struct session_handler	*hdl;	/* The handler for which this state was registered */
		char 			*sid;	/* For deleted state, the sid of the session it belong to */
	};
};

/* Session object, one for each value of Session-Id AVP */
struct session {
	int 		eyec;	/* Eyecatcher, SI_EYEC */
	
	char *		sid;	/* The \0-terminated Session-Id */
	uint32_t	hash;	/* computed hash of sid */
	struct fd_list	chain_h;/* chaining in the hash table of sessions. */
	
	struct timespec	timeout;/* Timeout date for the session */
	struct fd_list	expire;	/* List of expiring sessions, ordered by timeouts. */
	
	pthread_mutex_t stlock;	/* A lock to protect the list of states associated with this session */
	struct fd_list	states;	/* Sentinel for the list of states of this session. */
	int		msg_cnt;/* Reference counter for the messages pointing to this session */
};

/* Sessions hash table, to allow fast sid to session retrieval */
static struct {
	struct fd_list	sentinel;	/* sentinel element for this sublist */
	pthread_mutex_t lock;		/* the mutex for this sublist -- we might probably change it to rwlock for a little optimization */
} sess_hash [ 1 << SESS_HASH_SIZE ] ;
#define H_MASK( __hash ) ((__hash) & (( 1 << SESS_HASH_SIZE ) - 1))
#define H_LIST( _hash ) (&(sess_hash[H_MASK(_hash)].sentinel))
#define H_LOCK( _hash ) (&(sess_hash[H_MASK(_hash)].lock    ))

/* The following are used to generate sid values that are eternaly unique */
static uint32_t   	sid_h;	/* initialized to the current time in fd_sess_init */
static uint32_t   	sid_l;	/* incremented each time a session id is created */
static pthread_mutex_t 	sid_lock = PTHREAD_MUTEX_INITIALIZER;

/* Expiring sessions management */
static struct fd_list	exp_sentinel = FD_LIST_INITIALIZER(exp_sentinel);	/* list of sessions ordered by their timeout date */
static pthread_mutex_t	exp_lock = PTHREAD_MUTEX_INITIALIZER;	/* lock protecting the list. */
static pthread_cond_t	exp_cond = PTHREAD_COND_INITIALIZER;	/* condvar used by the expiry mecahinsm. */
static pthread_t	exp_thr; 	/* The expiry thread that handles cleanup of expired sessions */

/* Hierarchy of the locks, to avoid deadlocks:
 *  hash lock > state lock > expiry lock
 * i.e. state lock can be taken while holding the hash lock, but not while holding the expiry lock.
 * As well, the hash lock cannot be taken while holding a state lock.
 */

/********************************************************************************************************/

/* Initialize a session object. It is not linked now. sid must be already malloc'ed. */
static struct session * new_session(char * sid, size_t sidlen)
{
	struct session * sess;
	
	TRACE_ENTRY("%p %d", sid, sidlen);
	CHECK_PARAMS_DO( sid && sidlen, return NULL );
	
	CHECK_MALLOC_DO( sess = malloc(sizeof(struct session)), return NULL );
	memset(sess, 0, sizeof(struct session));
	
	sess->eyec = SI_EYEC;
	
	sess->sid  = sid;
	sess->hash = fd_hash(sid, sidlen);
	fd_list_init(&sess->chain_h, sess);
	
	CHECK_SYS_DO( clock_gettime(CLOCK_REALTIME, &sess->timeout), return NULL );
	sess->timeout.tv_sec += SESS_DEFAULT_LIFETIME;
	fd_list_init(&sess->expire, sess);
	
	CHECK_POSIX_DO( pthread_mutex_init(&sess->stlock, NULL), return NULL );
	fd_list_init(&sess->states, sess);
	
	return sess;
}
	
/* The expiry thread */
static void * exp_fct(void * arg)
{
	fd_log_threadname ( "Session/expire" );
	TRACE_ENTRY( "" );
	
	
	do {
		struct timespec	now;
		struct session * first;
		
		CHECK_POSIX_DO( pthread_mutex_lock(&exp_lock),  break );
		pthread_cleanup_push( fd_cleanup_mutex, &exp_lock );
again:		
		/* Check if there are expiring sessions available */
		if (FD_IS_LIST_EMPTY(&exp_sentinel)) {
			/* Just wait for a change or cancelation */
			CHECK_POSIX_DO( pthread_cond_wait( &exp_cond, &exp_lock ), break );
			/* Restart the loop on wakeup */
			goto again;
		}
		
		/* Get the pointer to the session that expires first */
		first = (struct session *)(exp_sentinel.next->o);
		ASSERT( VALIDATE_SI(first) );
		
		/* Get the current time */
		CHECK_SYS_DO(  clock_gettime(CLOCK_REALTIME, &now),  break  );

		/* If first session is not expired, we just wait until it happens */
		if ( TS_IS_INFERIOR( &now, &first->timeout ) ) {
			
			CHECK_POSIX_DO2(  pthread_cond_timedwait( &exp_cond, &exp_lock, &first->timeout ),  
					ETIMEDOUT, /* ETIMEDOUT is a normal error, continue */,
					/* on other error, */ break );
	
			/* on wakeup, loop */
			goto again;
		}
		
		/* Now, the first session in the list is expired; destroy it */
		pthread_cleanup_pop( 0 );
		CHECK_POSIX_DO( pthread_mutex_unlock(&exp_lock),  break );
		
		CHECK_FCT_DO( fd_sess_destroy( &first ), break );
		
	} while (1);
	
	TRACE_DEBUG(INFO, "An error occurred in session module! Expiry thread is terminating...");
	ASSERT(0);
	return NULL;
}
	
	

/********************************************************************************************************/

/* Initialize the session module */
int fd_sess_init(void)
{
	int i;
	
	TRACE_ENTRY( "" );
	
	/* Initialize the global counters */
	sid_h = (uint32_t) time(NULL);
	sid_l = 0;
	
	/* Initialize the hash table */
	for (i = 0; i < sizeof(sess_hash) / sizeof(sess_hash[0]); i++) {
		fd_list_init( &sess_hash[i].sentinel, NULL );
		CHECK_POSIX(  pthread_mutex_init(&sess_hash[i].lock, NULL)  );
	}
	
	/* Start session garbage collector (expiry) */
	CHECK_POSIX(  pthread_create(&exp_thr, NULL, exp_fct, NULL)  );
	
	return 0;
}

/* Terminate */
void fd_sess_fini(void)
{
	TRACE_ENTRY("");
	CHECK_FCT_DO( fd_thr_term(&exp_thr), /* continue */ );
	return;
}

/* Create a new handler */
int fd_sess_handler_create_internal ( struct session_handler ** handler, void (*cleanup)(char * sid, session_state * state) )
{
	struct session_handler *new;
	
	TRACE_ENTRY("%p %p", handler, cleanup);
	
	CHECK_PARAMS( handler && cleanup );
	
	CHECK_MALLOC( new = malloc(sizeof(struct session_handler)) );
	memset(new, 0, sizeof(struct session_handler));
	
	CHECK_POSIX( pthread_mutex_lock(&hdl_lock) );
	new->id = ++hdl_id;
	CHECK_POSIX( pthread_mutex_unlock(&hdl_lock) );
	
	new->eyec = SH_EYEC;
	new->cleanup = cleanup;
	
	*handler = new;
	return 0;
}

/* Destroy a handler, and all states attached to this handler. This operation is very slow but we don't care since it's rarely used. 
 * Note that it's better to call this function after all sessions have been deleted... */
int fd_sess_handler_destroy ( struct session_handler ** handler )
{
	struct session_handler * del;
	/* place to save the list of states to be cleaned up. We do it after finding them to avoid deadlocks. the "o" field becomes a copy of the sid. */
	struct fd_list deleted_states = FD_LIST_INITIALIZER( deleted_states );
	int i;
	
	TRACE_ENTRY("%p", handler);
	CHECK_PARAMS( handler && VALIDATE_SH(*handler) );
	
	del = *handler;
	*handler = NULL;
	
	del->eyec = 0xdead; /* The handler is not valid anymore for any other operation */
	
	/* Now find all sessions with data registered for this handler, and move this data to the deleted_states list. */
	for (i = 0; i < sizeof(sess_hash) / sizeof(sess_hash[0]); i++) {
		struct fd_list * li_si;
		CHECK_POSIX(  pthread_mutex_lock(&sess_hash[i].lock)  );
		
		for (li_si = sess_hash[i].sentinel.next; li_si != &sess_hash[i].sentinel; li_si = li_si->next) {
			struct fd_list * li_st;
			struct session * sess = (struct session *)(li_si->o);
			CHECK_POSIX(  pthread_mutex_lock(&sess->stlock)  );
			for (li_st = sess->states.next; li_st != &sess->states; li_st = li_st->next) {
				struct state * st = (struct state *)(li_st->o);
				char * sid_cpy;
				/* The list is ordered */
				if (st->hdl->id < del->id)
					continue;
				if (st->hdl->id == del->id) {
					/* This state belongs to the handler we are deleting, move the item to the deleted_states list */
					fd_list_unlink(&st->chain);
					CHECK_MALLOC( st->sid = strdup(sess->sid) );
					fd_list_insert_before(&deleted_states, &st->chain);
				}
				break;
			}
			CHECK_POSIX(  pthread_mutex_unlock(&sess->stlock)  );
		}
		CHECK_POSIX(  pthread_mutex_unlock(&sess_hash[i].lock)  );
	}
	
	/* Now, delete all states after calling their cleanup handler */
	while (!FD_IS_LIST_EMPTY(&deleted_states)) {
		struct state * st = (struct state *)(deleted_states.next->o);
		TRACE_DEBUG(FULL, "Calling cleanup handler for session '%s' and data %p", st->sid, st->state);
		(*del->cleanup)(st->sid, st->state);
		free(st->sid);
		fd_list_unlink(&st->chain);
		free(st);
	}
	
	return 0;
}



/* Create a new session object with the default timeout value, and link it */
int fd_sess_new ( struct session ** session, char * diamId, char * opt, size_t optlen )
{
	char * sid = NULL;
	size_t sidlen;
	uint32_t hash;
	struct session * sess;
	struct fd_list * li;
	int found = 0;
	
	TRACE_ENTRY("%p %p %p %d", session, diamId, opt, optlen);
	CHECK_PARAMS( session && (diamId || opt) );
	
	/* Ok, first create the identifier for the string */
	if (diamId == NULL) {
		/* opt is the full string */
		if (optlen) {
			CHECK_MALLOC( sid = malloc(optlen + 1) );
			strncpy(sid, opt, optlen);
			sid[optlen] = '\0';
			sidlen = optlen;
		} else {
			CHECK_MALLOC( sid = strdup(opt) );
			sidlen = strlen(sid);
		}
	} else {
		uint32_t sid_h_cpy;
		uint32_t sid_l_cpy;
		/* "<diamId>;<high32>;<low32>[;opt]" */
		sidlen = strlen(diamId);
		sidlen += 22; /* max size of ';<high32>;<low32>' */
		if (opt)
			sidlen += 1 + (optlen ?: strlen(opt)) ;
		sidlen++; /* space for the final \0 also */
		CHECK_MALLOC( sid = malloc(sidlen) );
		
		CHECK_POSIX( pthread_mutex_lock(&sid_lock) );
		if ( ++sid_l == 0 ) /* overflow */
			++sid_h;
		sid_h_cpy = sid_h;
		sid_l_cpy = sid_l;
		CHECK_POSIX( pthread_mutex_unlock(&sid_lock) );
		
		if (opt) {
			if (optlen)
				sidlen = snprintf(sid, sidlen, "%s;%u;%u;%.*s", diamId, sid_h_cpy, sid_l_cpy, (int)optlen, opt);
			else
				sidlen = snprintf(sid, sidlen, "%s;%u;%u;%s", diamId, sid_h_cpy, sid_l_cpy, opt);
		} else {
			sidlen = snprintf(sid, sidlen, "%s;%u;%u", diamId, sid_h_cpy, sid_l_cpy);
		}
	}
	
	/* Initialize the session object now, to spend less time inside locked section later. 
	 * Cons: we malloc then free if there is already a session with same SID; we could malloc later to avoid this. */
	CHECK_MALLOC( sess = new_session(sid, sidlen) );
	
	/* Now find the place to add this object in the hash table. */
	CHECK_POSIX( pthread_mutex_lock( H_LOCK(sess->hash) ) );
	pthread_cleanup_push( fd_cleanup_mutex, H_LOCK(sess->hash) );
	
	for (li = H_LIST(sess->hash)->next; li != H_LIST(sess->hash); li = li->next) {
		int cmp;
		struct session * s = (struct session *)(li->o);
		
		/* The list is ordered by hash and sid (in case of collisions) */
		if (s->hash < sess->hash)
			continue;
		if (s->hash > sess->hash)
			break;
		
		cmp = strcasecmp(s->sid, sess->sid);
		if (cmp < 0)
			continue;
		if (cmp > 0)
			break;
		
		/* A session with the same sid was already in the hash table */
		found = 1;
		*session = s;
		break;
	}
	
	/* If the session did not exist, we can link it in global tables */
	if (!found) {
		fd_list_insert_before(li, &sess->chain_h); /* hash table */
		
		/* We must also insert in the expiry list */
		CHECK_POSIX( pthread_mutex_lock( &exp_lock ) );
		pthread_cleanup_push( fd_cleanup_mutex, &exp_lock );
		
		/* Find the position in that list. We take it in reverse order */
		for (li = exp_sentinel.prev; li != &exp_sentinel; li = li->prev) {
			struct session * s = (struct session *)(li->o);
			if (TS_IS_INFERIOR( &s->timeout, &sess->timeout ) )
				break;
		}
		fd_list_insert_after( li, &sess->expire );

		/* We added a new expiring element, we must signal */
		if (li == &exp_sentinel) {
			CHECK_POSIX( pthread_cond_signal(&exp_cond) );
		}
		
		#if 0
		if (TRACE_BOOL(ANNOYING)) {	
			TRACE_DEBUG(FULL, "-- Updated session expiry list --");
			for (li = exp_sentinel.next; li != &exp_sentinel; li = li->next) {
				struct session * s = (struct session *)(li->o);
				fd_sess_dump(FULL, s);
			}
			TRACE_DEBUG(FULL, "-- end of expiry list --");
		}
		#endif
		
		/* We're done */
		pthread_cleanup_pop(0);
		CHECK_POSIX( pthread_mutex_unlock( &exp_lock ) );
	}
	
	pthread_cleanup_pop(0);
	CHECK_POSIX( pthread_mutex_unlock( H_LOCK(sess->hash) ) );
	
	/* If a session already existed, we must destroy the new element */
	if (found) {
		CHECK_FCT( fd_sess_destroy( &sess ) ); /* we could avoid locking this time for optimization */
		return EALREADY;
	}
	
	*session = sess;
	return 0;
}

/* Find or create a session */
int fd_sess_fromsid ( char * sid, size_t len, struct session ** session, int * new)
{
	int ret;
	
	TRACE_ENTRY("%p %d %p %p", sid, len, session, new);
	CHECK_PARAMS( sid && session );
	
	/* All the work is done in sess_new */
	ret = fd_sess_new ( session, NULL, sid, len );
	switch (ret) {
		case 0:
		case EALREADY:
			break;
		
		default:
			CHECK_FCT(ret);
	}
	
	if (new)
		*new = ret ? 0 : 1;
	
	return 0;
}

/* Get the sid of a session */
int fd_sess_getsid ( struct session * session, char ** sid )
{
	TRACE_ENTRY("%p %p", session, sid);
	
	CHECK_PARAMS( VALIDATE_SI(session) && sid );
	
	*sid = session->sid;
	
	return 0;
}

/* Change the timeout value of a session */
int fd_sess_settimeout( struct session * session, const struct timespec * timeout )
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %p", session, timeout);
	CHECK_PARAMS( VALIDATE_SI(session) && timeout );
	
	/* Lock -- do we need to lock the hash table as well? I don't think so... */
	CHECK_POSIX( pthread_mutex_lock( &exp_lock ) );
	pthread_cleanup_push( fd_cleanup_mutex, &exp_lock );
	
	/* Update the timeout */
	fd_list_unlink(&session->expire);
	memcpy(&session->timeout, timeout, sizeof(struct timespec));
	
	/* Find the new position in expire list. We take it in normal order */
	for (li = exp_sentinel.next; li != &exp_sentinel; li = li->next) {
		struct session * s = (struct session *)(li->o);

		if (TS_IS_INFERIOR( &s->timeout, &session->timeout ) )
			continue;

		break;
	}
	fd_list_insert_before( li, &session->expire );

	/* We added a new expiring element, we must signal if it was in first position */
	if (session->expire.prev == &exp_sentinel) {
		CHECK_POSIX( pthread_cond_signal(&exp_cond) );
	}

	#if 0
	if (TRACE_BOOL(ANNOYING)) {	
		TRACE_DEBUG(FULL, "-- Updated session expiry list --");
		for (li = exp_sentinel.next; li != &exp_sentinel; li = li->next) {
			struct session * s = (struct session *)(li->o);
			fd_sess_dump(FULL, s);
		}
		TRACE_DEBUG(FULL, "-- end of expiry list --");
	}
	#endif

	/* We're done */
	pthread_cleanup_pop(0);
	CHECK_POSIX( pthread_mutex_unlock( &exp_lock ) );
	
	return 0;
}

/* Destroy a session immediatly */
int fd_sess_destroy ( struct session ** session )
{
	struct session * sess;
	
	TRACE_ENTRY("%p", session);
	CHECK_PARAMS( session && VALIDATE_SI(*session) );
	
	sess = *session;
	*session = NULL;
	
	/* Unlink and invalidate */
	CHECK_FCT( pthread_mutex_lock( H_LOCK(sess->hash) ) );
	pthread_cleanup_push( fd_cleanup_mutex, H_LOCK(sess->hash) );
	CHECK_FCT( pthread_mutex_lock( &exp_lock ) );
	fd_list_unlink( &sess->chain_h );
	fd_list_unlink( &sess->expire ); /* no need to signal the condition here */
	sess->eyec = 0xdead;
	CHECK_FCT( pthread_mutex_unlock( &exp_lock ) );
	pthread_cleanup_pop(0);
	CHECK_FCT( pthread_mutex_unlock( H_LOCK(sess->hash) ) );
	
	/* Now destroy all states associated -- we don't take the lock since nobody can access this session anymore (in theory) */
	while (!FD_IS_LIST_EMPTY(&sess->states)) {
		struct state * st = (struct state *)(sess->states.next->o);
		fd_list_unlink(&st->chain);
		TRACE_DEBUG(FULL, "Calling handler %p cleanup for state registered with session '%s'", st->hdl, sess->sid);
		(*st->hdl->cleanup)(sess->sid, st->state);
		free(st);
	}
	
	/* Finally, destroy the session itself */
	free(sess->sid);
	free(sess);
	
	return 0;
}

/* Destroy a session if it is not used */
int fd_sess_reclaim ( struct session ** session )
{
	struct session * sess;
	
	TRACE_ENTRY("%p", session);
	CHECK_PARAMS( session && VALIDATE_SI(*session) );
	
	sess = *session;
	*session = NULL;
	
	CHECK_FCT( pthread_mutex_lock( H_LOCK(sess->hash) ) );
	pthread_cleanup_push( fd_cleanup_mutex, H_LOCK(sess->hash) );
	CHECK_FCT( pthread_mutex_lock( &exp_lock ) );
	if (FD_IS_LIST_EMPTY(&sess->states)) {
		fd_list_unlink( &sess->chain_h );
		fd_list_unlink( &sess->expire );
		sess->eyec = 0xdead;
		free(sess->sid);
		free(sess);
	}
	CHECK_FCT( pthread_mutex_unlock( &exp_lock ) );
	pthread_cleanup_pop(0);
	CHECK_FCT( pthread_mutex_unlock( H_LOCK(sess->hash) ) );
	
	return 0;
}

/* Save a state information with a session */
int fd_sess_state_store_internal ( struct session_handler * handler, struct session * session, session_state ** state )
{
	struct state *new;
	struct fd_list * li;
	int already = 0;
	
	TRACE_ENTRY("%p %p %p", handler, session, state);
	CHECK_PARAMS( handler && VALIDATE_SH(handler) && session && VALIDATE_SI(session) && state );
	
	/* Lock the session state list */
	CHECK_POSIX( pthread_mutex_lock(&session->stlock) );
	pthread_cleanup_push( fd_cleanup_mutex, &session->stlock );
			
	/* Create the new state object */
	CHECK_MALLOC(new = malloc(sizeof(struct state)) );
	memset(new, 0, sizeof(struct state));
	
	new->eyec = SD_EYEC;
	new->state= *state;
	fd_list_init(&new->chain, new);
	new->hdl = handler;
	
	/* find place for this state in the list */
	for (li = session->states.next; li != &session->states; li = li->next) {
		struct state * st = (struct state *)(li->o);
		/* The list is ordered by handler's id */
		if (st->hdl->id < handler->id)
			continue;
		
		if (st->hdl->id == handler->id) {
			TRACE_DEBUG(INFO, "A state was already stored for session '%s' and handler '%p', at location %p", session->sid, st->hdl, st->state);
			already = 1;
		}
		
		break;
	}
	
	if (!already) {
		fd_list_insert_before(li, &new->chain);
		*state = NULL;
	} else {
		free(new);
	}
	
	pthread_cleanup_pop(0);
	CHECK_POSIX( pthread_mutex_unlock(&session->stlock) );
	
	return already ? EALREADY : 0;
}

/* Get the data back */
int fd_sess_state_retrieve_internal ( struct session_handler * handler, struct session * session, session_state ** state )
{
	struct fd_list * li;
	struct state * st = NULL;
	
	TRACE_ENTRY("%p %p %p", handler, session, state);
	CHECK_PARAMS( handler && VALIDATE_SH(handler) && session && VALIDATE_SI(session) && state );
	
	*state = NULL;
	
	/* Lock the session state list */
	CHECK_POSIX( pthread_mutex_lock(&session->stlock) );
	pthread_cleanup_push( fd_cleanup_mutex, &session->stlock );
	
	/* find the state in the list */
	for (li = session->states.next; li != &session->states; li = li->next) {
		st = (struct state *)(li->o);
		
		/* The list is ordered by handler's id */
		if (st->hdl->id > handler->id)
			break;
	}
	
	/* If we found the state */
	if (st && (st->hdl == handler)) {
		fd_list_unlink(&st->chain);
		*state = st->state;
		free(st);
	}
	
	pthread_cleanup_pop(0);
	CHECK_POSIX( pthread_mutex_unlock(&session->stlock) );
	
	return 0;
}

/* For the messages module */
int fd_sess_fromsid_msg ( unsigned char * sid, size_t len, struct session ** session, int * new)
{
	TRACE_ENTRY("%p %zd %p %p", sid, len, session, new);
	CHECK_PARAMS( sid && len && session );
	
	/* Get the session object */
	CHECK_FCT( fd_sess_fromsid ( (char *) sid, len, session, new) );
	
	/* Update the msg refcount */
	CHECK_POSIX( pthread_mutex_lock(&(*session)->stlock) );
	(*session)->msg_cnt++;
	CHECK_POSIX( pthread_mutex_unlock(&(*session)->stlock) );
	
	/* Done */
	return 0;
}

int fd_sess_reclaim_msg ( struct session ** session )
{
	int reclaim;
	
	TRACE_ENTRY("%p", session);
	CHECK_PARAMS( session && VALIDATE_SI(*session) );
	
	/* Update the msg refcount */
	CHECK_POSIX( pthread_mutex_lock(&(*session)->stlock) );
	reclaim = (*session)->msg_cnt;
	(*session)->msg_cnt = reclaim - 1;
	CHECK_POSIX( pthread_mutex_unlock(&(*session)->stlock) );
	
	if (reclaim == 1) {
		CHECK_FCT(fd_sess_reclaim ( session ));
	} else {
		*session = NULL;
	}
	return 0;
}



/* Dump functions */
void fd_sess_dump(int level, struct session * session)
{
	struct fd_list * li;
	char buf[30];
	struct tm tm;
	
	if (!TRACE_BOOL(level))
		return;
	
	fd_log_debug("\t  %*s -- Session @%p --\n", level, "", session);
	if (!VALIDATE_SI(session)) {
		fd_log_debug("\t  %*s  Invalid session object\n", level, "");
	} else {
		
		fd_log_debug("\t  %*s  sid '%s', hash %x\n", level, "", session->sid, session->hash);

		strftime(buf, sizeof(buf), "%D,%T", localtime_r( &session->timeout.tv_sec , &tm ));
		fd_log_debug("\t  %*s  timeout %s.%09ld\n", level, "", buf, session->timeout.tv_nsec);

		CHECK_POSIX_DO( pthread_mutex_lock(&session->stlock), /* ignore */ );
		pthread_cleanup_push( fd_cleanup_mutex, &session->stlock );
		for (li = session->states.next; li != &session->states; li = li->next) {
			struct state * st = (struct state *)(li->o);
			fd_log_debug("\t  %*s    handler %d registered data %p\n", level, "", st->hdl->id, st->state);
		}
		pthread_cleanup_pop(0);
		CHECK_POSIX_DO( pthread_mutex_unlock(&session->stlock), /* ignore */ );
	}
	fd_log_debug("\t  %*s -- end of session @%p --\n", level, "", session);
}

void fd_sess_dump_hdl(int level, struct session_handler * handler)
{
	if (!TRACE_BOOL(level))
		return;
	
	fd_log_debug("\t  %*s -- Handler @%p --\n", level, "", handler);
	if (!VALIDATE_SH(handler)) {
		fd_log_debug("\t  %*s  Invalid session handler object\n", level, "");
	} else {
		fd_log_debug("\t  %*s  id %d, cleanup %p\n", level, "", handler->id, handler->cleanup);
	}
	fd_log_debug("\t  %*s -- end of handler @%p --\n", level, "", handler);
}	
