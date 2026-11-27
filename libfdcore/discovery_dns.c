/* Internal DNS-only S-NAPTR/SRV discovery; see discovery_dns.h. */
#include "discovery_dns.h"

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef FD_DISC_HAVE_RES_NQUERY
#include <netdb.h>
#include <resolv.h>
#endif

#define DNS_A 1
#define DNS_CNAME 5
#define DNS_AAAA 28
#define DNS_SRV 33
#define DNS_NAPTR 35
#define DNS_PACKET_MAX 65535
#define DNS_RR_MAX 256
#define DNS_DEPTH_MAX 12
#define DNS_QUERY_MAX 512

struct rr {
	char owner[FD_DISC_DNS_NAME_MAX + 1];
	uint16_t type, cls, len;
	uint32_t ttl;
	size_t data;
};

struct packet {
	unsigned char bytes[DNS_PACKET_MAX];
	size_t len, count;
	struct rr rr[DNS_RR_MAX];
};

struct path {
	size_t count;
	char names[FD_DISC_DNS_CHAIN_MAX][FD_DISC_DNS_NAME_MAX + 1];
	uint32_t ttl;
};

struct context {
	struct fd_disc_dns_options opt;
	struct fd_disc_dns_result *result;
	unsigned int queries;
	uint32_t seed;
	int extended;
};

struct naptr {
	uint16_t order, preference;
	unsigned int protocols;
	uint32_t application_id;
	int service; /* 0: unsupported, 1: legacy, 2: extended */
	char flag;
	char target[FD_DISC_DNS_NAME_MAX + 1];
	uint32_t ttl;
};

struct srv {
	uint16_t priority, weight, port;
	char target[FD_DISC_DNS_NAME_MAX + 1];
	uint32_t ttl;
	int used;
};

static int cancelled(struct context *ctx)
{
	return ctx->opt.cancelled &&
		ctx->opt.cancelled(ctx->opt.cancel_context);
}

static uint16_t u16(const unsigned char *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t u32(const unsigned char *p)
{
	return ((uint32_t)u16(p) << 16) | u16(p + 2);
}

static int name_equal(const char *a, const char *b)
{
	while (*a && *b) {
		if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++))
			return 0;
	}
	return *a == *b;
}

static int label_char(unsigned char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '-' || c == '_';
}

/* Restrict presentation names to unambiguous ASCII DNS labels. */
static int valid_name(const char *name)
{
	size_t label = 0, len = strlen(name), i;
	if (!len || len > 253)
		return 0;
	for (i = 0; i < len; i++) {
		unsigned char c = (unsigned char)name[i];
		if (c == '.') {
			if (!label || label > 63)
				return 0;
			label = 0;
		} else {
			if (!label_char(c))
				return 0;
			label++;
		}
	}
	return label > 0 && label <= 63;
}

/* Decompress a DNS name while bounding both the wire field and pointers. */
static int read_name(const struct packet *p, size_t *offset, size_t end,
		     char out[FD_DISC_DNS_NAME_MAX + 1])
{
	size_t pos = *offset, written = 0, consumed = 0, steps = 0;
	int jumped = 0;
	for (;;) {
		unsigned int n;
		size_t i;
		if (pos >= p->len || ++steps > 128)
			return 0;
		n = p->bytes[pos];
		if ((n & 0xc0) == 0xc0) {
			size_t dest;
			if (pos + 1 >= p->len || (!jumped && pos + 2 > end))
				return 0;
			dest = ((size_t)(n & 0x3f) << 8) | p->bytes[pos + 1];
			if (dest >= pos)
				return 0;
			if (!jumped)
				consumed += 2;
			jumped = 1;
			pos = dest;
			continue;
		}
		if (n & 0xc0 || n > 63 || pos + 1 + n > p->len ||
		    (!jumped && pos + 1 + n > end))
			return 0;
		if (!jumped)
			consumed += 1 + n;
		pos++;
		if (!n)
			break;
		if (written && written + 1 > 253)
			return 0;
		if (written)
			out[written++] = '.';
		if (written + n > 253)
			return 0;
		for (i = 0; i < n; i++) {
			unsigned char c = p->bytes[pos + i];
			if (!label_char(c))
				return 0;
			out[written++] = (char)c;
		}
		pos += n;
	}
	out[written] = '\0'; /* root is the empty string */
	*offset += consumed;
	return 1;
}

static enum fd_disc_dns_status system_query(void *v, const char *name,
	uint16_t type, unsigned char *out, size_t cap, size_t *length)
{
#ifdef FD_DISC_HAVE_RES_NQUERY
	struct context *ctx = v;
	res_state state;
	int n, error;
	if (cancelled(ctx))
		return FD_DISC_DNS_CANCELLED;
	state = calloc(1, sizeof(*state));
	if (!state)
		return FD_DISC_DNS_NOMEM;
	if (res_ninit(state) != 0) {
		free(state);
		return FD_DISC_DNS_TEMPORARY;
	}
	/* Limit each resolver round; cancellation is checked when it returns. */
	state->retrans = 1;
	state->retry = 1;
	if (cancelled(ctx)) {
		res_nclose(state);
		free(state);
		return FD_DISC_DNS_CANCELLED;
	}
	n = res_nquery(state, name, 1, type, out, (int)cap);
	error = state->res_h_errno;
	res_nclose(state);
	free(state);
	if (cancelled(ctx))
		return FD_DISC_DNS_CANCELLED;
	if (n < 0) {
		if (error == HOST_NOT_FOUND || error == NO_DATA)
			return FD_DISC_DNS_ABSENT;
		return FD_DISC_DNS_TEMPORARY;
	}
	if ((size_t)n > cap)
		return FD_DISC_DNS_MALFORMED;
	*length = (size_t)n;
	return FD_DISC_DNS_OK;
#else
	(void)v;
	(void)name; (void)type; (void)out; (void)cap; (void)length;
	return FD_DISC_DNS_UNSUPPORTED;
#endif
}

static enum fd_disc_dns_status parse_packet(struct packet *p, const char *name,
	uint16_t type)
{
	size_t off = 12, i, total;
	char qname[FD_DISC_DNS_NAME_MAX + 1];
	unsigned int flags, an, ns, ar, rcode;
	if (p->len < 12)
		return FD_DISC_DNS_MALFORMED;
	flags = u16(p->bytes + 2);
	if (!(flags & 0x8000) || (flags & 0x7800) || (flags & 0x0200) ||
	    u16(p->bytes + 4) != 1)
		return FD_DISC_DNS_MALFORMED;
	rcode = flags & 15;
	if (rcode != 0 && rcode != 3)
		return FD_DISC_DNS_TEMPORARY;
	if (!read_name(p, &off, p->len, qname) || off + 4 > p->len ||
	    !name_equal(qname, name) || u16(p->bytes + off) != type ||
	    u16(p->bytes + off + 2) != 1)
		return FD_DISC_DNS_MALFORMED;
	off += 4;
	an = u16(p->bytes + 6);
	ns = u16(p->bytes + 8);
	ar = u16(p->bytes + 10);
	total = (size_t)an + ns + ar;
	if (total > DNS_RR_MAX)
		return FD_DISC_DNS_LIMIT;
	p->count = an;
	for (i = 0; i < total; i++) {
		struct rr rr;
		if (!read_name(p, &off, p->len, rr.owner) || off + 10 > p->len)
			return FD_DISC_DNS_MALFORMED;
		rr.type = u16(p->bytes + off);
		rr.cls = u16(p->bytes + off + 2);
		rr.ttl = u32(p->bytes + off + 4);
		rr.len = u16(p->bytes + off + 8);
		off += 10;
		rr.data = off;
		if (rr.len > p->len - off)
			return FD_DISC_DNS_MALFORMED;
		off += rr.len;
		if (i < an)
			p->rr[i] = rr;
	}
	if (off != p->len)
		return FD_DISC_DNS_MALFORMED;
	return rcode == 3 ? FD_DISC_DNS_ABSENT : FD_DISC_DNS_OK;
}

static enum fd_disc_dns_status get_packet(struct context *ctx, const char *name,
	uint16_t type, struct packet **out)
{
	enum fd_disc_dns_status status;
	struct packet *p;
	if (cancelled(ctx))
		return FD_DISC_DNS_CANCELLED;
	if (++ctx->queries > DNS_QUERY_MAX)
		return FD_DISC_DNS_LIMIT;
	p = malloc(sizeof(*p));
	if (!p)
		return FD_DISC_DNS_NOMEM;
	p->len = 0;
	status = ctx->opt.query(ctx->opt.query_context, name, type, p->bytes,
			       sizeof(p->bytes), &p->len);
	if (cancelled(ctx))
		status = FD_DISC_DNS_CANCELLED;
	if (status == FD_DISC_DNS_OK) {
		if (p->len > sizeof(p->bytes))
			status = FD_DISC_DNS_MALFORMED;
		else
			status = parse_packet(p, name, type);
	}
	if (status != FD_DISC_DNS_OK) {
		free(p);
		return status;
	}
	*out = p;
	return FD_DISC_DNS_OK;
}

static enum fd_disc_dns_status add_name(struct path *p, const char *name)
{
	if (p->count && name_equal(p->names[p->count - 1], name))
		return FD_DISC_DNS_OK;
	if (p->count == FD_DISC_DNS_CHAIN_MAX)
		return FD_DISC_DNS_LIMIT;
	strcpy(p->names[p->count++], name);
	return FD_DISC_DNS_OK;
}

static void min_ttl(struct path *p, uint32_t ttl)
{
	if (ttl < p->ttl)
		p->ttl = ttl;
}

/* Follow CNAMEs, including chains contained in a single answer. */
static enum fd_disc_dns_status lookup(struct context *ctx, const char *name,
	uint16_t type, struct path *path, int include_names,
	struct packet **result, char *owner)
{
	char seen[DNS_DEPTH_MAX][FD_DISC_DNS_NAME_MAX + 1];
	size_t depth = 0;
	enum fd_disc_dns_status status;
	strcpy(owner, name);
	for (;;) {
		struct packet *p;
		int changed = 0, found = 0;
		size_t i, j;
		if (depth == DNS_DEPTH_MAX)
			return FD_DISC_DNS_LIMIT;
		for (i = 0; i < depth; i++)
			if (name_equal(owner, seen[i]))
				return FD_DISC_DNS_MALFORMED;
		strcpy(seen[depth++], owner);
		if (include_names) {
			status = add_name(path, owner);
			if (status != FD_DISC_DNS_OK)
				return status;
		}
		status = get_packet(ctx, owner, type, &p);
		if (status != FD_DISC_DNS_OK)
			return status;
		for (;;) {
			struct rr *alias = NULL;
			found = 0;
			for (i = 0; i < p->count; i++) {
				struct rr *r = &p->rr[i];
				if (r->cls != 1 || !name_equal(r->owner, owner))
					continue;
				if (r->type == type)
					found = 1;
				if (r->type == DNS_CNAME) {
					if (alias) { free(p); return FD_DISC_DNS_MALFORMED; }
					alias = r;
				}
			}
			if (found && alias) { free(p); return FD_DISC_DNS_MALFORMED; }
			if (!alias)
				break;
			{
				size_t off = alias->data;
				char next[FD_DISC_DNS_NAME_MAX + 1];
				if (!read_name(p, &off, alias->data + alias->len, next) ||
				    off != alias->data + alias->len || !*next) {
					free(p); return FD_DISC_DNS_MALFORMED;
				}
				for (j = 0; j < depth; j++)
					if (name_equal(next, seen[j])) {
						free(p); return FD_DISC_DNS_MALFORMED;
					}
				if (depth == DNS_DEPTH_MAX) {
					free(p); return FD_DISC_DNS_LIMIT;
				}
				strcpy(seen[depth++], next);
				min_ttl(path, alias->ttl);
				if (include_names) {
					status = add_name(path, next);
					if (status != FD_DISC_DNS_OK) { free(p); return status; }
				}
				strcpy(owner, next);
				changed = 1;
			}
		}
		if (found) {
			*result = p;
			return FD_DISC_DNS_OK;
		}
		free(p);
		if (!changed)
			return FD_DISC_DNS_ABSENT;
		/* The last alias is already in seen; the next packet will check it. */
		depth--;
	}
}

static unsigned int protocol_tag(const char *s)
{
	if (name_equal(s, "diameter.tcp")) return FD_DISC_DNS_TCP;
	if (name_equal(s, "diameter.sctp")) return FD_DISC_DNS_SCTP;
	if (name_equal(s, "diameter.tls.tcp")) return FD_DISC_DNS_TLS_TCP;
	if (name_equal(s, "diameter.dtls.sctp")) return FD_DISC_DNS_DTLS_SCTP;
	return 0;
}

static int tag_valid(const char *s)
{
	size_t i, n = strlen(s);
	if (!n || n > 32 || !((s[0] >= 'a' && s[0] <= 'z') ||
				(s[0] >= 'A' && s[0] <= 'Z')))
		return 0;
	for (i = 1; i < n; i++)
		if (!label_char((unsigned char)s[i]) && s[i] != '+' && s[i] != '.')
			return 0;
	/* Underscore is valid in DNS labels but not in S-NAPTR tags. */
	return strchr(s, '_') == NULL;
}

static void parse_service(char *s, struct naptr *n)
{
	char *tag = strchr(s, ':'), *next, *p;
	unsigned int mask = 0;
	uint32_t id = 0;
	n->service = 0;
	n->protocols = 0;
	n->application_id = 0;
	if (tag)
		*tag++ = '\0';
	if (name_equal(s, "aaa")) {
		n->service = 1;
	} else if (strlen(s) >= 7 &&
		   tolower((unsigned char)s[0]) == 'a' &&
		   tolower((unsigned char)s[1]) == 'a' &&
		   tolower((unsigned char)s[2]) == 'a' && s[3] == '+' &&
		   tolower((unsigned char)s[4]) == 'a' &&
		   tolower((unsigned char)s[5]) == 'p') {
		p = s + 6;
		if ((p[0] == '0' && p[1]) || strlen(p) > 10)
			return;
		for (; *p; p++) {
			unsigned int digit;
			if (*p < '0' || *p > '9')
				return;
			digit = (unsigned int)(*p - '0');
			if (id > (UINT32_MAX - digit) / 10)
				return;
			id = id * 10 + digit;
		}
		n->application_id = id;
		n->service = 2;
	} else {
		return;
	}
	if (!tag) {
		n->protocols = FD_DISC_DNS_TCP | FD_DISC_DNS_SCTP |
			FD_DISC_DNS_TLS_TCP | FD_DISC_DNS_DTLS_SCTP;
		return;
	}
	while (tag) {
		next = strchr(tag, ':');
		if (next)
			*next++ = '\0';
		if (!tag_valid(tag)) {
			n->service = 0;
			return;
		}
		mask |= protocol_tag(tag);
		tag = next;
	}
	n->protocols = mask;
}

static int char_string(const struct packet *p, size_t *off, size_t end,
		       char *out, size_t cap)
{
	size_t n, i;
	if (*off >= end)
		return 0;
	n = p->bytes[(*off)++];
	if (n > end - *off || n >= cap)
		return 0;
	for (i = 0; i < n; i++)
		if (!p->bytes[*off + i])
			return 0;
	memcpy(out, p->bytes + *off, n);
	out[n] = '\0';
	*off += n;
	return 1;
}

static enum fd_disc_dns_status parse_naptr(const struct packet *p,
	const struct rr *r, struct naptr *n)
{
	size_t off = r->data, end = off + r->len;
	char flags[256], service[256], regexp[256];
	if (end - off < 4)
		return FD_DISC_DNS_MALFORMED;
	n->order = u16(p->bytes + off);
	n->preference = u16(p->bytes + off + 2);
	off += 4;
	if (!char_string(p, &off, end, flags, sizeof(flags)) ||
	    !char_string(p, &off, end, service, sizeof(service)) ||
	    !char_string(p, &off, end, regexp, sizeof(regexp)) ||
	    !read_name(p, &off, end, n->target) || off != end)
		return FD_DISC_DNS_MALFORMED;
	parse_service(service, n);
	n->flag = (char)tolower((unsigned char)flags[0]);
	if (flags[0] && flags[1])
		n->protocols = 0;
	if (n->flag != '\0' && n->flag != 's' && n->flag != 'a')
		n->protocols = 0;
	if (*regexp || !*n->target)
		n->protocols = 0;
	n->ttl = r->ttl;
	return FD_DISC_DNS_OK;
}

static uint32_t random_bounded(struct context *ctx, uint32_t bound)
{
	uint32_t x, threshold;
	if (ctx->opt.random)
		return ctx->opt.random(ctx->opt.random_context, bound) % bound;
	threshold = (uint32_t)(-bound) % bound;
	do {
		ctx->seed = ctx->seed * UINT32_C(1664525) + UINT32_C(1013904223);
		x = ctx->seed;
	} while (x < threshold);
	return x % bound;
}

static enum fd_disc_dns_status addresses(struct context *ctx, const char *name,
	struct path *path, struct fd_disc_dns_candidate *candidate)
{
	static const uint16_t types[] = { DNS_A, DNS_AAAA };
	size_t t, i;
	for (t = 0; t < 2; t++) {
		struct path branch = *path;
		struct packet *p;
		char owner[FD_DISC_DNS_NAME_MAX + 1];
		enum fd_disc_dns_status st = lookup(ctx, name, types[t], &branch, 1,
						    &p, owner);
		if (st == FD_DISC_DNS_ABSENT)
			continue;
		if (st != FD_DISC_DNS_OK)
			return st;
		for (i = 0; i < p->count; i++) {
			struct rr *r = &p->rr[i];
			struct fd_disc_dns_address *a;
			if (r->cls != 1 || r->type != types[t] || !name_equal(r->owner, owner))
				continue;
			if (r->len != (t ? 16 : 4)) { free(p); return FD_DISC_DNS_MALFORMED; }
			if (candidate->address_count == FD_DISC_DNS_ADDRESS_MAX) {
				free(p); return FD_DISC_DNS_LIMIT;
			}
			a = &candidate->addresses[candidate->address_count++];
			a->length = r->len;
			memcpy(a->bytes, p->bytes + r->data, r->len);
			a->ttl = r->ttl;
			min_ttl(&branch, r->ttl);
		}
		free(p);
		/* Preserve both address lookup paths, including distinct CNAMEs. */
		for (i = path->count; i < branch.count; i++) {
			size_t j;
			for (j = 0; j < path->count; j++)
				if (name_equal(path->names[j], branch.names[i]))
					break;
			if (j < path->count)
				continue;
			st = add_name(path, branch.names[i]);
			if (st != FD_DISC_DNS_OK)
				return st;
		}
		min_ttl(path, branch.ttl);
	}
	return candidate->address_count ? FD_DISC_DNS_OK : FD_DISC_DNS_ABSENT;
}

static enum fd_disc_dns_status add_candidate(struct context *ctx, const char *target,
	uint16_t port, unsigned int protocol, struct path path)
{
	struct fd_disc_dns_candidate *c;
	enum fd_disc_dns_status st;
	if (ctx->result->count == FD_DISC_DNS_CANDIDATE_MAX)
		return FD_DISC_DNS_LIMIT;
	c = &ctx->result->candidates[ctx->result->count];
	memset(c, 0, sizeof(*c));
	strcpy(c->target, target);
	c->port = port;
	c->protocol = (enum fd_disc_dns_protocol)protocol;
	st = addresses(ctx, target, &path, c);
	if (st != FD_DISC_DNS_OK)
		return st;
	c->ttl = path.ttl;
	c->chain_count = path.count;
	memcpy(c->chain, path.names, sizeof(c->chain));
	ctx->result->count++;
	return FD_DISC_DNS_OK;
}

static enum fd_disc_dns_status resolve_srv(struct context *ctx, const char *name,
	unsigned int protocol, struct path path)
{
	struct packet *p;
	struct srv records[DNS_RR_MAX];
	char owner[FD_DISC_DNS_NAME_MAX + 1];
	size_t count = 0, i, left;
	uint32_t rrset_ttl = UINT32_MAX;
	enum fd_disc_dns_status st = lookup(ctx, name, DNS_SRV, &path, 0, &p, owner);
	if (st != FD_DISC_DNS_OK)
		return st;
	for (i = 0; i < p->count; i++) {
		struct rr *r = &p->rr[i];
		size_t off = r->data;
		struct srv *s;
		if (r->cls != 1 || r->type != DNS_SRV || !name_equal(r->owner, owner))
			continue;
		if (r->len < 7) { free(p); return FD_DISC_DNS_MALFORMED; }
		s = &records[count++];
		s->priority = u16(p->bytes + off);
		s->weight = u16(p->bytes + off + 2);
		s->port = u16(p->bytes + off + 4);
		off += 6;
		if (!read_name(p, &off, r->data + r->len, s->target) ||
		    off != r->data + r->len) { free(p); return FD_DISC_DNS_MALFORMED; }
		s->ttl = r->ttl;
		if (r->ttl < rrset_ttl)
			rrset_ttl = r->ttl;
		s->used = 0;
	}
	free(p);
	min_ttl(&path, rrset_ttl);
	/* A root target explicitly says the service is unavailable. */
	for (i = 0; i < count; i++)
		if (!*records[i].target)
			return count == 1 ? FD_DISC_DNS_ABSENT : FD_DISC_DNS_MALFORMED;
	left = count;
	while (left) {
		uint16_t priority = UINT16_MAX;
		uint32_t total = 0, chosen, sum = 0;
		size_t selected = count, pass;
		int have_priority = 0;
		struct path branch;
		for (i = 0; i < count; i++)
			if (!records[i].used &&
			    (!have_priority || records[i].priority < priority)) {
				priority = records[i].priority;
				have_priority = 1;
			}
		if (!have_priority)
			return FD_DISC_DNS_MALFORMED;
		for (i = 0; i < count; i++)
			if (!records[i].used && records[i].priority == priority)
				total += records[i].weight;
		chosen = random_bounded(ctx, total + 1);
		/* RFC 2782 places zero-weight records before nonzero weights. */
		for (pass = 0; pass < 2 && selected == count; pass++)
			for (i = 0; i < count; i++) {
				if (records[i].used || records[i].priority != priority ||
				    (!!records[i].weight) != (int)pass)
					continue;
				sum += records[i].weight;
				if (sum >= chosen) { selected = i; break; }
			}
		if (selected == count)
			return FD_DISC_DNS_MALFORMED;
		records[selected].used = 1;
		left--;
		branch = path;
		min_ttl(&branch, records[selected].ttl);
		st = add_candidate(ctx, records[selected].target, records[selected].port,
				   protocol, branch);
		if (st != FD_DISC_DNS_OK && st != FD_DISC_DNS_ABSENT)
			return st;
	}
	return FD_DISC_DNS_OK;
}

static enum fd_disc_dns_status resolve_naptr(struct context *ctx, const char *name,
	unsigned int protocol, struct path path, unsigned int depth,
	char ancestors[DNS_DEPTH_MAX][FD_DISC_DNS_NAME_MAX + 1])
{
	struct packet *p;
	struct naptr records[DNS_RR_MAX];
	char owner[FD_DISC_DNS_NAME_MAX + 1];
	size_t count = 0, i, j;
	uint32_t rrset_ttl = UINT32_MAX;
	enum fd_disc_dns_status st;
	if (depth == DNS_DEPTH_MAX)
		return FD_DISC_DNS_LIMIT;
	for (i = 0; i < depth; i++)
		if (name_equal(ancestors[i], name))
			return FD_DISC_DNS_MALFORMED;
	strcpy(ancestors[depth], name);
	st = lookup(ctx, name, DNS_NAPTR, &path, 1, &p, owner);
	if (st != FD_DISC_DNS_OK)
		return st;
	for (i = 0; i < p->count; i++) {
		struct rr *r = &p->rr[i];
		if (r->cls != 1 || r->type != DNS_NAPTR || !name_equal(r->owner, owner))
			continue;
		st = parse_naptr(p, r, &records[count]);
		if (st != FD_DISC_DNS_OK) { free(p); return st; }
		if (r->ttl < rrset_ttl)
			rrset_ttl = r->ttl;
		count++;
	}
	free(p);
	min_ttl(&path, rrset_ttl);
	/* Stable insertion sort by order, then preference. */
	for (i = 1; i < count; i++) {
		struct naptr v = records[i];
		j = i;
		while (j && (records[j-1].order > v.order ||
		       (records[j-1].order == v.order &&
			records[j-1].preference > v.preference))) {
			records[j] = records[j-1];
			j--;
		}
		records[j] = v;
	}
	{
		size_t order_start_count = ctx->result->count;
		for (i = 0; i < count; i++) {
			struct path branch = path;
			struct naptr *n = &records[i];
			if (n->service == (ctx->extended ? 2 : 1) &&
			    (!ctx->extended ||
			     n->application_id == ctx->opt.application_id) &&
			    (n->protocols & protocol)) {
				min_ttl(&branch, n->ttl);
				if (n->flag == 's')
					st = resolve_srv(ctx, n->target, protocol, branch);
				else if (n->flag == 'a')
					st = add_candidate(ctx, n->target,
						protocol == FD_DISC_DNS_TLS_TCP ||
						protocol == FD_DISC_DNS_DTLS_SCTP
						? 5658 : 3868, protocol, branch);
				else
					st = resolve_naptr(ctx, n->target, protocol,
							   branch, depth + 1, ancestors);
				if (st != FD_DISC_DNS_OK &&
				    st != FD_DISC_DNS_ABSENT)
					return st;
			}
			if (i + 1 == count ||
			    records[i + 1].order != records[i].order) {
				if (ctx->result->count > order_start_count)
					break;
				order_start_count = ctx->result->count;
			}
		}
	}
	return FD_DISC_DNS_OK;
}

enum fd_disc_dns_status fd_disc_dns_resolve(const char *realm,
	const struct fd_disc_dns_options *options, struct fd_disc_dns_result **result)
{
	static const struct { unsigned int bit; const char *prefix; } protocols[] = {
		{ FD_DISC_DNS_TLS_TCP, "_diameters._tcp." },
		{ FD_DISC_DNS_DTLS_SCTP, "_diameters._sctp." },
		{ FD_DISC_DNS_TCP, "_diameter._tcp." },
		{ FD_DISC_DNS_SCTP, "_diameter._sctp." }
	};
	struct context ctx;
	struct path path;
	struct packet *p;
	char owner[FD_DISC_DNS_NAME_MAX + 1], domain[FD_DISC_DNS_NAME_MAX + 1];
	char ancestors[DNS_DEPTH_MAX][FD_DISC_DNS_NAME_MAX + 1];
	enum fd_disc_dns_status st;
	size_t len, i;
	int naptr_present = 0;
	if (!result)
		return FD_DISC_DNS_INVALID;
	*result = NULL;
	if (!realm || !options || !options->protocols || (options->protocols & ~15u))
		return FD_DISC_DNS_INVALID;
	len = strlen(realm);
	if (len && realm[len - 1] == '.')
		len--;
	if (!len || len >= sizeof(domain))
		return FD_DISC_DNS_INVALID;
	memcpy(domain, realm, len);
	domain[len] = '\0';
	if (!valid_name(domain))
		return FD_DISC_DNS_INVALID;
	memset(&ctx, 0, sizeof(ctx));
	ctx.opt = *options;
	if (!ctx.opt.query) {
		ctx.opt.query = system_query;
		ctx.opt.query_context = &ctx;
	}
	ctx.seed = (uint32_t)time(NULL) ^ (uint32_t)getpid() ^ (uint32_t)(uintptr_t)&ctx;
	if (!ctx.seed)
		ctx.seed = 1;
	ctx.result = calloc(1, sizeof(*ctx.result));
	if (!ctx.result)
		return FD_DISC_DNS_NOMEM;
	memset(&path, 0, sizeof(path));
	path.ttl = UINT32_MAX;
	st = lookup(&ctx, domain, DNS_NAPTR, &path, 1, &p, owner);
	if (st == FD_DISC_DNS_OK) {
		for (i = 0; i < p->count; i++) {
			struct rr *r = &p->rr[i];
			struct naptr n;
			if (r->cls != 1 || r->type != DNS_NAPTR ||
			    !name_equal(r->owner, owner))
				continue;
			st = parse_naptr(p, r, &n);
			if (st != FD_DISC_DNS_OK) {
				free(p);
				goto fail;
			}
			if (n.service == 2)
				ctx.extended = 1;
		}
		naptr_present = 1;
		free(p);
	} else if (st != FD_DISC_DNS_ABSENT)
		goto fail;
	/* Pursue each protocol independently so no NAPTR branch can switch it. */
	for (i = 0; i < sizeof(protocols)/sizeof(protocols[0]); i++) {
		struct path branch;
		if (!(options->protocols & protocols[i].bit))
			continue;
		memset(&branch, 0, sizeof(branch));
		branch.ttl = UINT32_MAX;
		if (naptr_present) {
			st = resolve_naptr(&ctx, domain, protocols[i].bit, branch, 0, ancestors);
		} else {
			char srvname[FD_DISC_DNS_NAME_MAX + 1];
			size_t prefix = strlen(protocols[i].prefix);
			if (prefix + len > 253) { st = FD_DISC_DNS_LIMIT; goto fail; }
			memcpy(srvname, protocols[i].prefix, prefix);
			memcpy(srvname + prefix, domain, len + 1);
			st = add_name(&branch, domain);
			if (st == FD_DISC_DNS_OK)
				st = resolve_srv(&ctx, srvname, protocols[i].bit, branch);
		}
		if (st != FD_DISC_DNS_OK && st != FD_DISC_DNS_ABSENT)
			goto fail;
	}
	if (cancelled(&ctx)) { st = FD_DISC_DNS_CANCELLED; goto fail; }
	if (!ctx.result->count) { st = FD_DISC_DNS_ABSENT; goto fail; }
	*result = ctx.result;
	return FD_DISC_DNS_OK;
fail:
	free(ctx.result);
	return st;
}
