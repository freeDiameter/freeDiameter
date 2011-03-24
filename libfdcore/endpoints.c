/*********************************************************************************************************
* Software License Agreement (BSD License)                                                               *
* Author: Sebastien Decugis <sdecugis@freediameter.net>							 *
*													 *
* Copyright (c) 2011, WIDE Project and NICT								 *
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

#include "fdcore-internal.h"


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
	in_port_t * port;
	int cmp = -1;
	
	TRACE_ENTRY("%p %p %u %x", list, sa, sl, flags);
	CHECK_PARAMS( list && sa && (sl <= sizeof(sSS)) );
	
	if (list->next == NULL) {
		/* the list is not initialized yet, do it */
		fd_list_init(list, NULL);
	}
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_add_merge  Current list:");
		fd_ep_dump( 4, list );
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_add_merge  Adding:");
		fd_log_debug("    ");
		sSA_DUMP_NODE_SERV( sa, NI_NUMERICHOST | NI_NUMERICSERV );
		fd_log_debug(" {%s%s%s%s}\n", 
				(flags & EP_FL_CONF) 	? "C" : "-",
				(flags & EP_FL_DISC)	    ? "D" : "-",
				(flags & EP_FL_ADV)	    ? "A" : "-",
				(flags & EP_FL_LL)	    ? "L" : "-",
				(flags & EP_FL_PRIMARY)     ? "P" : "-");
	}
	ptr.sa = sa;
	
	/* Filter out a bunch of invalid addresses */
	switch (sa->sa_family) {
		case AF_INET:
			if (! (flags & EP_ACCEPTALL)) {
				if (IN_IS_ADDR_UNSPECIFIED(&ptr.sin->sin_addr) 
				 || IN_IS_ADDR_LOOPBACK(&ptr.sin->sin_addr)
				    /* the next one filters both EXPERIMENTAL, BADCLASS and MULTICAST. */
				 || ((ntohl(ptr.sin->sin_addr.s_addr) & 0xe0000000) == 0xe0000000)
				 || (ptr.sin->sin_addr.s_addr == INADDR_BROADCAST)) {
					if (TRACE_BOOL(ANNOYING + 1)) {
						TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_add_merge  Address was ignored, not added.");
					}
					return 0;
				}
			}
			port = &ptr.sin->sin_port;
			break;

		case AF_INET6:
			if (! (flags & EP_ACCEPTALL)) {
				if (IN6_IS_ADDR_UNSPECIFIED(&ptr.sin6->sin6_addr) 
				 || IN6_IS_ADDR_LOOPBACK(&ptr.sin6->sin6_addr)
				 || IN6_IS_ADDR_MULTICAST(&ptr.sin6->sin6_addr)
				 || IN6_IS_ADDR_LINKLOCAL(&ptr.sin6->sin6_addr)
				 || IN6_IS_ADDR_SITELOCAL(&ptr.sin6->sin6_addr)) {
					if (TRACE_BOOL(ANNOYING + 1)) {
						TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_add_merge  Address was ignored, not added.");
					}
					return 0;
				}
			}
			port = &ptr.sin6->sin6_port;
			break;

		default:
			if (TRACE_BOOL(ANNOYING + 1)) {
				TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_add_merge  Address family was unknown, not added.");
			}
			return 0;
	}

	/* remove the ACCEPTALL flag */
	flags &= ~EP_ACCEPTALL;
	
	/* Search place in the list */
	for (li = list->next; li != list; li = li->next) {
		ep = (struct fd_endpoint *)li;
		in_port_t * ep_port;
		
		/* First, compare the address family */
		if (ep->sa.sa_family < sa->sa_family)
			continue;
		if (ep->sa.sa_family > sa->sa_family)
			break;
		
		/* Then compare the address field */
		switch (sa->sa_family) {
			case AF_INET:
				cmp = memcmp(&ep->sin.sin_addr, &ptr.sin->sin_addr, sizeof(struct in_addr));
				ep_port = &ep->sin.sin_port;
				break;
			case AF_INET6:
				cmp = memcmp(&ep->sin6.sin6_addr, &ptr.sin6->sin6_addr, sizeof(struct in6_addr));
				ep_port = &ep->sin6.sin6_port;
				break;
			default:
				ASSERT( 0 ); /* we got a different value previously in this same function */
		}
		if (cmp < 0)
			continue;
		if (cmp > 0)
			break;
		
		/* Finally compare the port, only if not 0 */
		if (*port == 0)
			break;
		if (*ep_port == 0) {
			/* save the port information in the list, and break */
			*ep_port = *port;
			break;
		}
		if (*ep_port < *port)
			continue;
		if (*ep_port > *port)
			cmp = 1;
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
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_add_merge  New list:");
		fd_ep_dump( 4, list );
	}
	return 0;
}

/* Delete endpoints that do not have a matching flag from a list (0: delete all endpoints) */
int fd_ep_filter( struct fd_list * list, uint32_t flags )
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %x", list, flags);
	CHECK_PARAMS(list);
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter  Filter this list for flags %x:", flags);
		fd_ep_dump( 4, list );
	}
	for (li = list->next; li != list; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *)li;
		
		if (! (ep->flags & flags)) {
			li = li->prev;
			fd_list_unlink(&ep->chain);
			free(ep);
		}
	}
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter  Resulting list:");
		fd_ep_dump( 4, list );
	}
	return 0;
}

/* Keep only endpoints of the same family as af */
int fd_ep_filter_family( struct fd_list * list, int af )
{
	struct fd_list * li;
	
	TRACE_ENTRY("%p %d", list, af);
	CHECK_PARAMS(list);
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter_family  Filter this list for family %d:", af);
		fd_ep_dump( 4, list );
	}
	for (li = list->next; li != list; li = li->next) {
		struct fd_endpoint * ep = (struct fd_endpoint *)li;
		
		if (ep->sa.sa_family != af) {
			li = li->prev;
			fd_list_unlink(&ep->chain);
			free(ep);
		}
	}
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter_family  Resulting list:");
		fd_ep_dump( 4, list );
	}
	return 0;
}

/* Remove any endpoint from the exclude list in the list */
int fd_ep_filter_list( struct fd_list * list, struct fd_list * exclude_list )
{
	struct fd_list * li_out, *li_ex, *li;
	struct fd_endpoint * out, * ex;
	
	TRACE_ENTRY("%p %p", list, exclude_list);
	CHECK_PARAMS(list && exclude_list);
	
	/* initialize. Both lists are ordered */
	li_out = list->next;
	li_ex = exclude_list->next;
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter_list  Filter this list ");
		fd_ep_dump( 4, list );
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter_list  Removing from list");
		fd_ep_dump( 6, exclude_list );
	}
	/* Now browse both lists in parallel */
	while ((li_out != list) && (li_ex != exclude_list)) {
		int cmp;

		out = (struct fd_endpoint *)li_out;
		ex = (struct fd_endpoint *)li_ex;

		/* Compare the next elements families */
		if (out->sa.sa_family < ex->sa.sa_family) {
			li_out = li_out->next;
			continue;
		}
		if (out->sa.sa_family > ex->sa.sa_family) {
			li_ex = li_ex->next;
			continue;
		}

		/* Then compare the address fields */
		switch (out->sa.sa_family) {
			case AF_INET:
				cmp = memcmp(&out->sin.sin_addr, &ex->sin.sin_addr, sizeof(struct in_addr));
				break;
			case AF_INET6:
				cmp = memcmp(&out->sin6.sin6_addr, &ex->sin6.sin6_addr, sizeof(struct in6_addr));
				break;
			default:
				/* Filter this out */
				cmp = 0;
		}
		if (cmp < 0) {
			li_out = li_out->next;
			continue;
		}
		if (cmp > 0) {
			li_ex = li_ex->next;
			continue;
		}
	
		/* We remove this element then loop */
		li = li_out;
		li_out = li->next;
		fd_list_unlink(li);
		free(li);
	}
	
	if (TRACE_BOOL(ANNOYING + 1)) {
		TRACE_DEBUG(ANNOYING, "  DEBUG:fd_ep_filter_list  Resulting list:");
		fd_ep_dump( 4, list );
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
	
