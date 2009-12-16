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


/* Add an endpoint information in a list */
int fd_ep_add_merge( struct fd_list * list, sSA * sa, socklen_t sl, uint32_t flags )
{
	struct fd_endpoint * ep;
	struct fd_list * li;
	union {
		sSA * sa;
		sSA4 *sin;
		sSA6 *sin6;
	} ptr;
	int cmp = -1;
	
	TRACE_ENTRY("%p %p %u %x", list, sa, sl, flags);
	CHECK_PARAMS( list && sa && (sl <= sizeof(sSS)) );
	
	/* Filter out loopback addresses, unspecified addresses, and invalid families */
	if (! (flags & EP_ACCEPTALL)) {
		ptr.sa = sa;
		switch (sa->sa_family) {
			case AF_INET:
				if (IN_IS_ADDR_UNSPECIFIED(&ptr.sin->sin_addr) || IN_IS_ADDR_LOOPBACK(&ptr.sin->sin_addr))
					return 0;
				break;

			case AF_INET6:
				if (IN6_IS_ADDR_UNSPECIFIED(&ptr.sin6->sin6_addr) || IN6_IS_ADDR_LOOPBACK(&ptr.sin6->sin6_addr))
					return 0;
				break;

			default:
				return 0;
		}
	} else {
		/* remove it */
		flags &= ~EP_ACCEPTALL;
	}
	
	/* Search place in the list */
	for (li = list->next; li != list; li = li->next) {
		ep = (struct fd_endpoint *)li;
		
		cmp = memcmp(&ep->ss, sa, sl);
		if (cmp >= 0)
			break;
	}
	
	if (cmp) {
		/* new item to be added */
		CHECK_MALLOC( ep = malloc(sizeof(struct fd_endpoint)) );
		memset(ep, 0, sizeof(struct fd_endpoint));
		fd_list_init(&ep->chain, NULL);
		memcpy(&ep->ss, sa, sl);
		
		/* Insert in the list */
		fd_list_insert_before(li, &ep->chain);
	}
	
	/* Merge the flags */
	ep->flags |= flags;
	
	return 0;
}

/* Delete endpoints that do not have a matching flag from a list (0: delete all endpoints) */
int fd_ep_filter( struct fd_list * list, uint32_t flags )
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %x", list, flags);
	CHECK_PARAMS(list);
	
	for (li = list->next; li != list; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *)li;
		
		if (! (ep->flags & flags)) {
			li = li->prev;
			fd_list_unlink(&ep->chain);
			free(ep);
		}
	}
	
	return 0;
}

/* Keep only endpoints of the same family as af */
int fd_ep_filter_family( struct fd_list * list, int af )
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %d", list, af);
	CHECK_PARAMS(list);
	
	for (li = list->next; li != list; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *)li;
		
		if (ep->sa.sa_family != af) {
			li = li->prev;
			fd_list_unlink(&ep->chain);
			free(ep);
		}
	}
	
	return 0;
}

/* Reset the given flag(s) from all items in the list */
int fd_ep_clearflags( struct fd_list * list, uint32_t flags )
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %x", list, flags);
	CHECK_PARAMS(list);
	
	for (li = list->next; li != list; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *)li;
		ep->flags &= ~flags;
		if (ep->flags == 0) {
			li = li->prev;
			fd_list_unlink(&ep->chain);
			free(ep);
		}
	}
	
	return 0;
}

void fd_ep_dump_one( char * prefix, struct fd_endpoint * ep, char * suffix )
{
	if (prefix)
		fd_log_debug("%s", prefix);
	
	sSA_DUMP_NODE_SERV( &ep->sa, NI_NUMERICHOST | NI_NUMERICSERV );
	fd_log_debug(" {%s%s%s%s}", 
			(ep->flags & EP_FL_CONF) 	? "C" : "-",
			(ep->flags & EP_FL_DISC) 	? "D" : "-",
			(ep->flags & EP_FL_ADV) 	? "A" : "-",
			(ep->flags & EP_FL_LL) 		? "L" : "-",
			(ep->flags & EP_FL_PRIMARY) 	? "P" : "-");
	if (suffix)
		fd_log_debug("%s", suffix);
}

void fd_ep_dump( int indent, struct fd_list * eps )
{
	struct fd_list * li;
	for (li = eps->next; li != eps; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *)li;
		fd_log_debug("%*s", indent, "");
		fd_ep_dump_one( NULL, ep, "\n" );
	}
}
	
