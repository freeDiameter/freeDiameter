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

/* Manage the list of RADIUS clients, along with their shared secrets. */

/* Probably some changes are needed to support RADIUS Proxies */

#include "rgw.h"

#define REVERSE_DNS_SIZE_MAX	512 /* length of our buffer for reverse DNS */

/* Ordered lists of clients. The order relationship is a memcmp on the address zone. 
   For same addresses, the port is compared.
   The same address cannot be added twice, once with a 0-port and once with another port value.
 */
static struct fd_list cli_ip = FD_LIST_INITIALIZER(cli_ip);
static struct fd_list cli_ip6 = FD_LIST_INITIALIZER(cli_ip6);

/* Mutex to protect the previous lists */
static pthread_mutex_t cli_mtx = PTHREAD_MUTEX_INITIALIZER;

/* Structure describing one client */
struct rgw_client {
	/* Link information in global list */
	struct fd_list		chain;
	
	/* Reference count */
	int			refcount;
	
	/* The address and optional port (alloc'd during configuration file parsing). */
	union {
		struct sockaddr		*sa; /* generic pointer */
		struct sockaddr_in	*sin;
		struct sockaddr_in6	*sin6;
	};
	
	/* The FQDN, realm, and optional aliases */
	int			 is_local; /* true if the RADIUS client runs on the same host -- we use Diameter Identity in that case */
	enum rgw_cli_type 	 type; /* is it a proxy ? */
	char 			*fqdn;
	size_t			 fqdn_len;
	char			*realm;
	char			**aliases;
	size_t			 aliases_nb;
	
	/* The secret key data. */
	struct {
		unsigned char * data;
		size_t		len;
	} 			key;
	
	/* information of previous msg received, for duplicate checks. */
	struct {
		uint16_t		port;
		uint8_t			id;
		uint8_t			auth[16]; /* we also compare the request authenticator to avoid buggy NASes */
		struct radius_msg * 	ans; /* to be able to resend a lost answer */
	} last[2]; /*[0] for auth, [1] for acct. */
};



/* create a new rgw_client. the arguments are moved into the structure (to limit malloc & free calls). */
static int client_create(struct rgw_client ** res, struct sockaddr ** ip_port, unsigned char ** key, size_t keylen, enum rgw_cli_type type )
{
	struct rgw_client *tmp = NULL;
	char buf[255];
	int ret;
	int loc = 0;
	
	/* Check if the IP address is local */
	if (   ( ((*ip_port)->sa_family == AF_INET ) && (   IN_IS_ADDR_LOOPBACK( &((struct sockaddr_in  *)(*ip_port))->sin_addr ) ) )
	     ||( ((*ip_port)->sa_family == AF_INET6) && (  IN6_IS_ADDR_LOOPBACK( &((struct sockaddr_in6 *)(*ip_port))->sin6_addr) ) )) {
		/* The client is local */
		loc = 1;
	} else {
	
		/* Search FQDN for the client */
		ret = getnameinfo( *ip_port, sizeof(struct sockaddr_storage), &buf[0], sizeof(buf), NULL, 0, 0 );
		if (ret) {
			TRACE_DEBUG(INFO, "Unable to resolve peer name: %s", gai_strerror(ret));
			return EINVAL;
		}
	}
	
	/* Create the new object */
	CHECK_MALLOC( tmp = malloc(sizeof (struct rgw_client)) );
	memset(tmp, 0, sizeof(struct rgw_client));
	fd_list_init(&tmp->chain, NULL);
	
	tmp->type = type;
	
	if (loc) {
		tmp->is_local = 1;
	} else {
		/* Copy the fqdn */
		CHECK_MALLOC( tmp->fqdn = strdup(buf) );
		tmp->fqdn_len = strlen(tmp->fqdn);
		/* Find an appropriate realm */
		tmp->realm = strchr(tmp->fqdn, '.');
		if (tmp->realm)
			tmp->realm += 1;
		if ((!tmp->realm) || (*tmp->realm == '\0')) /* in case the fqdn was "localhost." for example, if it is possible... */
			tmp->realm = fd_g_config->cnf_diamrlm;
	}
	
	/* move the sa info reference */
	tmp->sa = *ip_port;
	*ip_port = NULL;
	
	/* move the key material */
	tmp->key.data = *key;
	tmp->key.len = keylen;
	*key = NULL;
	
	/* Done! */
	*res = tmp;
	return 0;
}


/* Decrease refcount on a client; the lock must be held when this function is called. */
static void client_unlink(struct rgw_client * client)
{
	client->refcount -= 1;
	
	if (client->refcount <= 0) {
		int idx;
		/* to be sure: the refcount should be 0 only when client_fini is called */
		ASSERT( FD_IS_LIST_EMPTY(&client->chain) );
		
		/* Free the data */
		for (idx = 0; idx < client->aliases_nb; idx++)
			free(client->aliases[idx]);
		free(client->aliases);
		free(client->fqdn);
		free(client->sa);
		free(client->key.data);
		free(client);
	}
}


/* Macro to avoid duplicating the code in the next function */
#define client_search_family( _family_ )												\
		case AF_INET##_family_: {												\
			struct sockaddr_in##_family_ * sin##_family_ = (struct sockaddr_in##_family_ *)ip_port;				\
			for (ref = cli_ip##_family_.next; ref != &cli_ip##_family_; ref = ref->next) {					\
				cmp = memcmp(&sin##_family_->sin##_family_##_addr, 							\
					     &((struct rgw_client *)ref)->sin##_family_->sin##_family_##_addr, 				\
					     sizeof(struct in##_family_##_addr));							\
				if (cmp > 0) continue; /* search further in the list */							\
				if (cmp < 0) break; /* this IP is not in the list */							\
				/* Now compare the ports as follow: */									\
				     /* If the ip_port we are searching does not contain a port, just return the first match result */	\
				if ( (sin##_family_->sin##_family_##_port == 0) 							\
				     /* If the entry in the list does not contain a port, return it as a match */			\
				  || (((struct rgw_client *)ref)->sin##_family_->sin##_family_##_port == 0) 				\
				     /* If both ports are equal, it is a match */							\
				  || (sin##_family_->sin##_family_##_port == 								\
				  		((struct rgw_client *)ref)->sin##_family_->sin##_family_##_port)) {			\
					*res = (struct rgw_client *)ref;								\
					return EEXIST;											\
				}													\
				/* Otherwise, the list is ordered by port value (byte order does not matter */				\
				if (sin##_family_->sin##_family_##_port 								\
					> ((struct rgw_client *)ref)->sin##_family_->sin##_family_##_port) continue;			\
				else break;												\
			}														\
			*res = (struct rgw_client *)(ref->prev);									\
			return ENOENT;													\
		}
/* Function to look for an existing rgw_client, or the previous element. 
   The cli_mtx must be held when calling this function. 
   Returns ENOENT if the matching client does not exist, and res points to the previous element in the list. 
   Returns EEXIST if the matching client is found, and res points to this element. 
   Returns other error code on other error. */
static int client_search(struct rgw_client ** res, struct sockaddr * ip_port )
{
	int cmp;
	struct fd_list *ref = NULL;
	
	CHECK_PARAMS(res && ip_port);
	
	switch (ip_port->sa_family) {
		client_search_family()
				break;
		
		client_search_family( 6 )
				break;
	}
	
	/* We're never supposed to reach this point */
	ASSERT(0);
	return EINVAL;
}

int rgw_clients_getkey(struct rgw_client * cli, unsigned char **key, size_t *key_len)
{
	CHECK_PARAMS( cli && key && key_len );
	*key = cli->key.data;
	*key_len = cli->key.len;
	return 0;
}

int rgw_clients_gettype(struct rgw_client * cli, enum rgw_cli_type *type)
{
	CHECK_PARAMS( cli && type );
	*type = cli->type;
	return 0;
}


int rgw_clients_search(struct sockaddr * ip_port, struct rgw_client ** ref)
{
	int ret = 0;
	
	TRACE_ENTRY("%p %p", ip_port, ref);
	
	CHECK_PARAMS(ip_port && ref);
	
	CHECK_POSIX( pthread_mutex_lock(&cli_mtx) );

	ret = client_search(ref, ip_port);
	if (ret == EEXIST) {
		(*ref)->refcount ++;
		ret = 0;
	} else {
		*ref = NULL;
	}
	
	CHECK_POSIX( pthread_mutex_unlock(&cli_mtx) );
	
	return ret;
}

int rgw_clients_check_dup(struct rgw_radius_msg_meta **msg, struct rgw_client *cli)
{
	int idx;
	
	TRACE_ENTRY("%p %p", msg, cli);
	
	CHECK_PARAMS( msg && cli );
	
	if ((*msg)->serv_type == RGW_PLG_TYPE_AUTH)
		idx = 0;
	else
		idx = 1;
	
	if ((cli->last[idx].id == (*msg)->radius.hdr->identifier) 
	 && (cli->last[idx].port == (*msg)->port) 
	 && !memcmp(&cli->last[idx].auth[0], &(*msg)->radius.hdr->authenticator[0], 16)) {
		/* Duplicate! */
		TRACE_DEBUG(INFO, "Received duplicated RADIUS message (id: %02hhx, port: %hu).", (*msg)->radius.hdr->identifier, ntohs((*msg)->port));
		if (cli->last[idx].ans) {
			/* Resend the answer */
			CHECK_FCT_DO( rgw_servers_send((*msg)->serv_type, cli->last[idx].ans->buf, cli->last[idx].ans->buf_used, cli->sa, (*msg)->port),  );
		}
		rgw_msg_free(msg);
	} else {
		/* We have not just received this message already */
		if (cli->last[idx].port == 0) { /* first message from this client */
			/* Just add the new information */
			ASSERT(cli->last[idx].ans == NULL);
			cli->last[idx].id = (*msg)->radius.hdr->identifier;
			cli->last[idx].port = (*msg)->port;
			memcpy(&cli->last[idx].auth[0], &(*msg)->radius.hdr->authenticator[0], 16);
		} else { 
			/* We have got previous message(s), update the info only if answered already */
			if (cli->last[idx].ans) {
				cli->last[idx].id = (*msg)->radius.hdr->identifier;
				cli->last[idx].port = (*msg)->port;
				memcpy(&cli->last[idx].auth[0], &(*msg)->radius.hdr->authenticator[0], 16);
				/* Free the previous answer */
				radius_msg_free(cli->last[idx].ans);
				free(cli->last[idx].ans);
				cli->last[idx].ans = NULL;
			} 
		}
	}
	
	return 0;
}

/* Check that the NAS-IP-Adress or NAS-Identifier is coherent with the IP the packet was received from */
/* Also update the client list of aliases if needed */
/* NOTE: This function does nothing if the client is a RADIUS Proxy... */
/* Check if the message has a valid authenticator, and update the meta-data accordingly */
int rgw_clients_auth_check(struct rgw_radius_msg_meta * msg, struct rgw_client * cli, uint8_t * req_auth)
{
	unsigned char * key;
	size_t keylen;
	int count;
	
	TRACE_ENTRY("%p %p %p", msg, cli, req_auth);
	
	CHECK_PARAMS(msg && cli);
	
	CHECK_FCT(rgw_clients_getkey(cli, &key, &keylen));
	
	count = radius_msg_count_attr(&msg->radius, RADIUS_ATTR_MESSAGE_AUTHENTICATOR, 0);
	if (count > 1) {
		TRACE_DEBUG(INFO, "Too many Message-Authenticator attributes (%d), discarding message.", count);
		return EINVAL;
	}
	if (count == 0) {
		TRACE_DEBUG(FULL, "Message does not contain a Message-Authenticator attributes.");
		msg->valid_mac = 0;
	} else {
		if (radius_msg_verify_msg_auth( &msg->radius, key, keylen, req_auth )) {
			TRACE_DEBUG(INFO, "Invalid Message-Authenticator received, discarding message.");
			return EINVAL;
		}
		msg->valid_mac = 1;
	}
	
	return 0;
}

static struct dict_object * cache_orig_host = NULL;
static struct dict_object * cache_orig_realm = NULL;
static struct dict_object * cache_route_record = NULL;

int rgw_clients_init(void)
{
	TRACE_ENTRY();
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Host", &cache_orig_host, ENOENT) );
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Origin-Realm", &cache_orig_realm, ENOENT) );
	CHECK_FCT( fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, "Route-Record", &cache_route_record, ENOENT) );
	return 0;
}


/* The following function checks if a RADIUS message contains a valid NAS identifier, and initializes an empty Diameter
 message with the appropriate routing information */
int rgw_clients_create_origin(struct rgw_radius_msg_meta *msg, struct rgw_client * cli, struct msg ** diam)
{
	int idx;
	int valid_nas_info = 0;
	struct radius_attr_hdr *nas_ip = NULL, *nas_ip6 = NULL, *nas_id = NULL;
	char * oh_str = NULL;
	char * or_str = NULL;
	char * rr_str = NULL;
	char buf[REVERSE_DNS_SIZE_MAX]; /* to store DNS lookups results */
	
	struct avp *avp = NULL;
	union avp_value avp_val;
	
	TRACE_ENTRY("%p %p %p", msg, cli, diam);
	CHECK_PARAMS(msg && cli && diam && (*diam == NULL));
	
	/* Find the relevant attributes, if any */
	for (idx = 0; idx < msg->radius.attr_used; idx++) {
		struct radius_attr_hdr * attr = (struct radius_attr_hdr *)(msg->radius.buf + msg->radius.attr_pos[idx]);
		size_t attr_len = attr->length - sizeof(struct radius_attr_hdr);
		
		if ((attr->type == RADIUS_ATTR_NAS_IP_ADDRESS) && (attr_len = 4)) {
			nas_ip = attr;
			continue;
		}
			
		if ((attr->type == RADIUS_ATTR_NAS_IDENTIFIER) && (attr_len > 0)) {
			nas_id = attr;
			continue;
		}
			
		if ((attr->type == RADIUS_ATTR_NAS_IPV6_ADDRESS) && (attr_len = 16)) {
			nas_ip6 = attr;
			continue;
		}
	}
		
	if (!nas_ip && !nas_ip6 && !nas_id) {
		TRACE_DEBUG(FULL, "The message does not contain any NAS identification attribute.");
		
		/* Get information on this peer */
		CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );
		
		goto diameter;
	}
	
	/* Check if the message was received from the IP in NAS-IP-Address attribute */
	if (nas_ip && (cli->sa->sa_family == AF_INET) && !memcmp(nas_ip+1, &cli->sin->sin_addr, sizeof(struct in_addr))) {
		TRACE_DEBUG(FULL, "NAS-IP-Address contains the same address as the message was received from.");
		valid_nas_info |= 1;
	}
	if (nas_ip6 && (cli->sa->sa_family == AF_INET6) && !memcmp(nas_ip6+1, &cli->sin6->sin6_addr, sizeof(struct in6_addr))) {
		TRACE_DEBUG(FULL, "NAS-IPv6-Address contains the same address as the message was received from.");
		valid_nas_info |= 2;
	}
	
	
	/*
			In RADIUS it would be possible for a rogue NAS to forge the NAS-IP-
			Address attribute value.  Diameter/RADIUS translation agents MUST
			check a received NAS-IP-Address or NAS-IPv6-Address attribute against
			the source address of the RADIUS packet.  If they do not match and
			the Diameter/RADIUS translation agent does not know whether the
			packet was sent by a RADIUS proxy or NAS (e.g., no Proxy-State
			attribute), then by default it is assumed that the source address
			corresponds to a RADIUS proxy, and that the NAS Address is behind
			that proxy, potentially with some additional RADIUS proxies in
			between.  The Diameter/RADIUS translation agent MUST insert entries
			in the Route-Record AVP corresponding to the apparent route.  This
			implies doing a reverse lookup on the source address and NAS-IP-
			Address or NAS-IPv6-Address attributes to determine the corresponding
			FQDNs.

			If the source address and the NAS-IP-Address or NAS-IPv6-Address do
			not match, and the Diameter/RADIUS translation agent knows that it is
			talking directly to the NAS (e.g., there are no RADIUS proxies
			between it and the NAS), then the error should be logged, and the
			packet MUST be discarded.

			Diameter agents and servers MUST check whether the NAS-IP-Address AVP
			corresponds to an entry in the Route-Record AVP.  This is done by
			doing a reverse lookup (PTR RR) for the NAS-IP-Address to retrieve
			the corresponding FQDN, and by checking for a match with the Route-
			Record AVP.  If no match is found, then an error is logged, but no
			other action is taken.
	*/
	if (nas_ip || nas_ip6) {
		if (!valid_nas_info) {
			if ((!cli->is_local) && (cli->type == RGW_CLI_NAS)) {
				TRACE_DEBUG(INFO, "Message received with a NAS-IP-Address or NAS-IPv6-Address different \nfrom the sender's. Please configure as Proxy if this is expected.\n Message discarded.");
				return EINVAL;
			} else {
				/* the peer is configured as a proxy, or running on localhost, so accept the message */
				sSS ss;
				
				/* In that case, the cli will be stored as Route-Record and the NAS-IP-Address as origin */
				if (!cli->is_local) {
					rr_str = cli->fqdn;
				}
				
				/* We must DNS-reverse the NAS-IP*-Address */
				memset(&ss, 0 , sizeof(sSS));
				if (nas_ip) {
					sSA4 * sin = (sSA4 *)&ss;
					sin->sin_family = AF_INET;
					memcpy(&sin->sin_addr, nas_ip + 1, sizeof(struct in_addr));
				} else {
					sSA6 * sin6 = (sSA6 *)&ss;
					sin6->sin6_family = AF_INET6;
					memcpy(&sin6->sin6_addr, nas_ip6 + 1, sizeof(struct in6_addr));
				}
				CHECK_SYS_DO( getnameinfo( (sSA *)&ss, sSAlen(&ss), &buf[0], sizeof(buf), NULL, 0, NI_NAMEREQD),
					{
						if (cli->is_local) {
							CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );
							goto diameter;
						}
						
						TRACE_DEBUG(INFO, "The NAS-IP*-Address cannot be DNS reversed in order to create the Origin-Host AVP; rejecting the message (translation is impossible).");
						return EINVAL;
					} );
				
				oh_str = &buf[0];
				or_str = strchr(oh_str, '.');
				if (or_str) {
					or_str ++; /* move after the first dot */
					if (*or_str == '\0')
						or_str = NULL; /* Discard this realm, we will use the local realm later */
				}
			}
		} else {
			/* The attribute matches the source address, just use this in origin-host */
			CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );
		}
		
		goto diameter; /* we ignore the nas_id in that case */
	}
	
	/* We don't have a NAS-IP*-Address attribute if we are here */
	if (cli->is_local) {
		/* Simple: we use our own configuration */
		CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );
		goto diameter;
	}
	
	/* At this point, we only have nas_id, and the client is not local */
	ASSERT(nas_id);
	
	{
		int found, ret;
		struct addrinfo hint, *res, *ptr;
		
		/*
			In RADIUS it would be possible for a rogue NAS to forge the NAS-
			Identifier attribute.  Diameter/RADIUS translation agents SHOULD
			attempt to check a received NAS-Identifier attribute against the
			source address of the RADIUS packet, by doing an A/AAAA RR query.  If
			the NAS-Identifier attribute contains an FQDN, then such a query
			would resolve to an IP address matching the source address.  However,
			the NAS-Identifier attribute is not required to contain an FQDN, so
			such a query could fail.  If it fails, an error should be logged, but
			no action should be taken, other than a reverse lookup on the source
			address and insert the resulting FQDN into the Route-Record AVP.

			Diameter agents and servers SHOULD check whether a NAS-Identifier AVP
			corresponds to an entry in the Route-Record AVP.  If no match is
			found, then an error is logged, but no other action is taken.
		*/
		
		/* first, check if the nas_id is the fqdn of the peer or a known alias */
		if ((cli->fqdn_len == (nas_id->length - sizeof(struct radius_attr_hdr))) 
		&& (!strncasecmp((char *)(nas_id + 1), cli->fqdn, nas_id->length - sizeof(struct radius_attr_hdr)))) {
			TRACE_DEBUG(FULL, "NAS-Identifier contains the fqdn of the client");
			found = 1;
		} else {
			for (idx = 0; idx < cli->aliases_nb; idx++) {
				if (((nas_id->length - sizeof(struct radius_attr_hdr)) == strlen(cli->aliases[idx])) 
				&& (!strncasecmp((char *)(nas_id + 1), cli->aliases[idx], nas_id->length - sizeof(struct radius_attr_hdr)))) {
					TRACE_DEBUG(FULL, "NAS-Identifier valid value found in the cache");
					found = 1;
					break;
				}
			}
		}
		
		if (found) {
			/* The NAS-Identifier matches the source IP */
			CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );

			goto diameter;
		}
		
		/* Attempt DNS resolution of the identifier */
		ASSERT( nas_id->length - sizeof(struct radius_attr_hdr) < sizeof(buf) );
		memcpy(buf, nas_id + 1, nas_id->length - sizeof(struct radius_attr_hdr));
		buf[nas_id->length - sizeof(struct radius_attr_hdr)] = '\0';
		
		/* Now check if this alias is valid for this peer */
		memset(&hint, 0, sizeof(hint));
		hint.ai_flags  = AI_CANONNAME;
		ret = getaddrinfo(buf, NULL, &hint, &res);
		if (ret == 0) {
			strncpy(buf, res->ai_canonname, sizeof(buf));
			/* The name was resolved correctly, does it match the IP of the client? */
			for (ptr = res; ptr != NULL; ptr = ptr->ai_next) {
				if (cli->sa->sa_family != ptr->ai_family)
					continue;
				if (memcmp(cli->sa, ptr->ai_addr, sSAlen(cli->sa)))
					continue;
				
				found = 1;
				break;
			}
			freeaddrinfo(res);
			
			if (!found) {
				if (cli->type == RGW_CLI_NAS) {
					TRACE_DEBUG(INFO, "The NAS-Identifier value '%.*s' resolves to a different IP than the client's, discarding the message. \nConfigure this client as a Proxy if this message should be valid.", 
						nas_id->length - sizeof(struct radius_attr_hdr), nas_id + 1);
					return EINVAL;
				} else {
					/* This identifier matches a different IP, assume it is a proxied message */
					if (!cli->is_local) {
						rr_str = cli->fqdn;
					}
					oh_str = &buf[0]; /* The canonname resolved */
					or_str = strchr(oh_str, '.');
					if (or_str) {
						or_str ++; /* move after the first dot */
						if (*or_str == '\0')
							or_str = NULL; /* Discard this realm, we will use the local realm later */
					}
				}
			} else {
				/* It is a valid alias, save it */
				CHECK_MALLOC( cli->aliases = realloc(cli->aliases, (cli->aliases_nb + 1) * sizeof(char *)) );
				CHECK_MALLOC( cli->aliases[cli->aliases_nb + 1] = malloc( 1 + nas_id->length - sizeof(struct radius_attr_hdr) ));
				memcpy( cli->aliases[cli->aliases_nb + 1], nas_id + 1, nas_id->length - sizeof(struct radius_attr_hdr));
				*(cli->aliases[cli->aliases_nb + 1] + nas_id->length - sizeof(struct radius_attr_hdr)) = '\0';
				cli->aliases_nb ++;
				TRACE_DEBUG(FULL, "Saved valid alias for client: '%s' -> '%s'", cli->aliases[cli->aliases_nb + 1], cli->fqdn);
				CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );
			}
		} else {
			/* Error resolving the name */
			TRACE_DEBUG(INFO, "NAS-Identifier '%s' cannot be resolved: %s. Ignoring...", buf, gai_strerror(ret));
			/* Assume this is a valid identifier for the client */
			CHECK_FCT( rgw_clients_get_origin(cli, &oh_str, &or_str) );
		}
	}
	
	/* Now, let's create the empty Diameter message with Origin-Host, -Realm, and Route-Record if needed. */
diameter:
	ASSERT(oh_str); /* If it is not defined here, there is a bug... */
	if (!or_str)
		or_str = fd_g_config->cnf_diamrlm; /* Use local realm in that case */
	
	/* Create an empty Diameter message so that extensions can store their AVPs */
	CHECK_FCT(  fd_msg_new ( NULL, MSGFL_ALLOC_ETEID, diam )  );
	
	/* Add the Origin-Host as next AVP */
	CHECK_FCT( fd_msg_avp_new ( cache_orig_host, 0, &avp ) );
	memset(&avp_val, 0, sizeof(avp_val));
	avp_val.os.data = (unsigned char *)oh_str;
	avp_val.os.len = strlen(oh_str);
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
	CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	
	/* Add the Origin-Realm as next AVP */
	CHECK_FCT( fd_msg_avp_new ( cache_orig_realm, 0, &avp ) );
	memset(&avp_val, 0, sizeof(avp_val));
	avp_val.os.data = (unsigned char *)or_str;
	avp_val.os.len = strlen(or_str);
	CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
	CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	
	if (rr_str) {
		CHECK_FCT( fd_msg_avp_new ( cache_route_record, 0, &avp ) );
		memset(&avp_val, 0, sizeof(avp_val));
		avp_val.os.data = (unsigned char *)rr_str;
		avp_val.os.len = strlen(rr_str);
		CHECK_FCT( fd_msg_avp_setvalue ( avp, &avp_val ) );
		CHECK_FCT( fd_msg_avp_add ( *diam, MSG_BRW_LAST_CHILD, avp) );
	}
	
	/* Done! */
	return 0;
}

int rgw_clients_get_origin(struct rgw_client *cli, char **fqdn, char **realm)
{
	TRACE_ENTRY("%p %p %p", cli, fqdn, realm);
	CHECK_PARAMS(cli && fqdn);
	
	if (cli->is_local) {
		*fqdn = fd_g_config->cnf_diamid;
		if (realm)
			*realm= fd_g_config->cnf_diamrlm;
	} else {
		*fqdn = cli->fqdn;
		if (realm)
			*realm= cli->realm;
	}
		
	return 0;
}

char * rgw_clients_id(struct rgw_client *cli)
{
	return cli->is_local ? "(local)" : cli->fqdn;
}


void rgw_clients_dispose(struct rgw_client ** ref)
{
	TRACE_ENTRY("%p", ref);
	CHECK_PARAMS_DO(ref, return);
	
	CHECK_POSIX_DO( pthread_mutex_lock(&cli_mtx),  );
	client_unlink(*ref);
	*ref = NULL;
	CHECK_POSIX_DO( pthread_mutex_unlock(&cli_mtx), );
}

int rgw_clients_add( struct sockaddr * ip_port, unsigned char ** key, size_t keylen, enum rgw_cli_type type )
{
	struct rgw_client * prev = NULL, *new = NULL;
	int ret;
	
	TRACE_ENTRY("%p %p %lu", ip_port, key, keylen);
	
	CHECK_PARAMS( ip_port && key && *key && keylen );
	CHECK_PARAMS( (ip_port->sa_family == AF_INET) || (ip_port->sa_family == AF_INET6) );
	CHECK_PARAMS( (type == RGW_CLI_NAS) || (type == RGW_CLI_PXY) );
	
	/* Dump the entry in debug mode */
	if (TRACE_BOOL(FULL + 1 )) {
		TRACE_DEBUG(FULL, "Adding %s:", (type == RGW_CLI_NAS) ? "NAS" : "PROXY"  );
		TRACE_DEBUG_sSA(FULL, 	 "\tIP : ", ip_port, NI_NUMERICHOST | NI_NUMERICSERV, "" );
		TRACE_DEBUG_BUFFER(FULL, "\tKey: [", *key, keylen, "]" );
	}
	
	/* Lock the lists */
	CHECK_POSIX( pthread_mutex_lock(&cli_mtx) );
	
	/* Check if the same entry does not already exist */
	ret = client_search(&prev, ip_port );
	if (ret == ENOENT) {
		/* No duplicate found, Ok to add */
		CHECK_FCT_DO( ret = client_create( &new, &ip_port, key, keylen, type ), goto end );
		fd_list_insert_after(&prev->chain, &new->chain);
		new->refcount++;
		ret = 0;
		goto end;
	}
	
	if (ret == EEXIST) {
		/* Check if the key is the same, then skip or return an error */
		if ((keylen == prev->key.len ) && ( ! memcmp(*key, prev->key.data, keylen) ) && (type == prev->type)) {
			TRACE_DEBUG(INFO, "Skipping duplicate client description");
			ret = 0;
			goto end;
		}
		
		fd_log_debug("ERROR: Conflicting RADIUS clients descriptions!\n");
		TRACE_DEBUG(NONE, "Previous entry: %s", (prev->type == RGW_CLI_NAS) ? "NAS" : "PROXY");
		TRACE_DEBUG_sSA(NONE, 	 "\tIP : ", prev->sa, NI_NUMERICHOST | NI_NUMERICSERV, "" );
		TRACE_DEBUG_BUFFER(NONE, "\tKey: [", prev->key.data, prev->key.len, "]" );
		TRACE_DEBUG(NONE, "Conflicting entry: %s", (type == RGW_CLI_NAS) ? "NAS" : "PROXY");
		TRACE_DEBUG_sSA(NONE, 	 "\tIP : ", ip_port, NI_NUMERICHOST | NI_NUMERICSERV, "" );
		TRACE_DEBUG_BUFFER(NONE, "\tKey: [", *key, keylen, "]" );
	}
end:
	/* release the lists */
	CHECK_POSIX( pthread_mutex_unlock(&cli_mtx) );
	
	return ret;
}

static void dump_cli_list(struct fd_list *senti)
{
	struct rgw_client * client = NULL;
	struct fd_list *ref = NULL;
	
	for (ref = senti->next; ref != senti; ref = ref->next) {
		client = (struct rgw_client *)ref;
		TRACE_DEBUG_sSA(NONE, 	 "  - ", client->sa, NI_NUMERICHOST | NI_NUMERICSERV, (type == RGW_CLI_NAS) ? "" : " [PROXY]" );
	}
}

void rgw_clients_dump(void)
{
	if ( ! TRACE_BOOL(FULL) )
		return;
	
	CHECK_POSIX_DO( pthread_mutex_lock(&cli_mtx), /* ignore error */ );
	
	if (!FD_IS_LIST_EMPTY(&cli_ip))
		fd_log_debug(" RADIUS IP clients list:\n");
	dump_cli_list(&cli_ip);
		
	if (!FD_IS_LIST_EMPTY(&cli_ip6))
		fd_log_debug(" RADIUS IPv6 clients list:\n");
	dump_cli_list(&cli_ip6);
		
	CHECK_POSIX_DO( pthread_mutex_unlock(&cli_mtx), /* ignore error */ );
}

void rgw_clients_fini(void)
{
	struct fd_list * client;
	
	TRACE_ENTRY();
	
	CHECK_POSIX_DO( pthread_mutex_lock(&cli_mtx), /* ignore error */ );
	
	/* empty the lists */
	while ( ! FD_IS_LIST_EMPTY(&cli_ip) ) {
		client = cli_ip.next;
		fd_list_unlink(client);
		client_unlink((struct rgw_client *)client);
	}
	while (! FD_IS_LIST_EMPTY(&cli_ip6)) {
		client = cli_ip6.next;
		fd_list_unlink(client);
		client_unlink((struct rgw_client *)client);
	}
	
	CHECK_POSIX_DO( pthread_mutex_unlock(&cli_mtx), /* ignore error */ );
	
}

int rgw_client_finish_send(struct radius_msg ** msg, struct rgw_radius_msg_meta * req, struct rgw_client * cli)
{
	int idx;
	
	TRACE_ENTRY("%p %p %p", msg, req, cli);
	CHECK_PARAMS( msg && *msg && cli );
	
	if (!req) {
		/* We don't support this case yet */
		ASSERT(0);
		return ENOTSUP;
	}
	
	if (radius_msg_finish_srv(*msg, cli->key.data, cli->key.len, req->radius.hdr->authenticator)) {
		TRACE_DEBUG(INFO, "An error occurred while preparing the RADIUS answer");
		radius_msg_free(*msg);
		free(*msg);
		*msg = NULL;
		return EINVAL;
	}
	
	/* Debug */
	TRACE_DEBUG(FULL, "RADIUS message ready for sending:");
	rgw_msg_dump((struct rgw_radius_msg_meta *)*msg);

	/* Send the message */
	CHECK_FCT( rgw_servers_send(req->serv_type, (*msg)->buf, (*msg)->buf_used, cli->sa, req->port) );

	/* update the duplicate cache in rgw_clients */
	if (req->serv_type == RGW_PLG_TYPE_AUTH)
		idx = 0;
	else
		idx = 1;
	if (cli->last[idx].ans) {
		/* Free it */
		radius_msg_free(cli->last[idx].ans);
		free(cli->last[idx].ans);
	}
	cli->last[idx].ans = *msg;
	cli->last[idx].id = req->radius.hdr->identifier;
	cli->last[idx].port = req->port;
	*msg = NULL;
	
	/* Finished */
	return 0;
}

