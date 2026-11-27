/* Standalone: mkdir -p test.discovery
 * cc -D_DEFAULT_SOURCE -std=c99 -Wall -Wextra -Werror
 * tests/test_discovery_dns.c libfdcore/discovery_dns.c -lresolv -o test.discovery/test_dns
 * ./test.discovery/test_dns
 */
#include "../libfdcore/discovery_dns.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test checks and their side effects must run with or without NDEBUG. */
#define assert(expr) do { \
	if (!(expr)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

struct fixture {
	int scenario;
	int fallback_queries;
	unsigned int random;
	int cancelled;
	int cancel_on_query;
	unsigned int query_count;
};

struct wire {
	unsigned char *b;
	size_t n, cap;
	unsigned int answers;
};

static void byte(struct wire *w, unsigned int v)
{
	assert(w->n < w->cap);
	w->b[w->n++] = (unsigned char)v;
}

static void word(struct wire *w, unsigned int v)
{
	byte(w, v >> 8);
	byte(w, v);
}

static void dword(struct wire *w, uint32_t v)
{
	word(w, v >> 16);
	word(w, v);
}

static void name(struct wire *w, const char *s)
{
	const char *end;
	while (*s) {
		end = strchr(s, '.');
		if (!end)
			end = s + strlen(s);
		assert(end - s > 0 && end - s <= 63);
		byte(w, (unsigned int)(end - s));
		while (s != end)
			byte(w, (unsigned char)*s++);
		if (*s)
			s++;
	}
	byte(w, 0);
}

static void string(struct wire *w, const char *s)
{
	size_t n = strlen(s), i;
	assert(n < 256);
	byte(w, (unsigned int)n);
	for (i = 0; i < n; i++)
		byte(w, (unsigned char)s[i]);
}

static size_t rr_start(struct wire *w, const char *owner, unsigned int type,
		       uint32_t ttl)
{
	size_t pos;
	name(w, owner);
	word(w, type); word(w, 1); dword(w, ttl);
	pos = w->n;
	word(w, 0);
	w->answers++;
	return pos;
}

static void rr_end(struct wire *w, size_t pos)
{
	size_t len = w->n - pos - 2;
	assert(len <= 65535);
	w->b[pos] = (unsigned char)(len >> 8);
	w->b[pos + 1] = (unsigned char)len;
}

static void naptr(struct wire *w, const char *owner, unsigned int order,
	unsigned int pref, const char *flags, const char *service, const char *target,
	uint32_t ttl)
{
	size_t p = rr_start(w, owner, 35, ttl);
	word(w, order); word(w, pref);
	string(w, flags); string(w, service); string(w, "");
	name(w, target);
	rr_end(w, p);
}

static void srv(struct wire *w, const char *owner, unsigned int priority,
	unsigned int weight, unsigned int port, const char *target, uint32_t ttl)
{
	size_t p = rr_start(w, owner, 33, ttl);
	word(w, priority); word(w, weight); word(w, port); name(w, target);
	rr_end(w, p);
}

static void address(struct wire *w, const char *owner, unsigned int type, uint32_t ttl)
{
	size_t p = rr_start(w, owner, type, ttl);
	unsigned int i;
	for (i = 0; i < (type == 1 ? 4u : 16u); i++)
		byte(w, i + 1);
	rr_end(w, p);
}

static void cname(struct wire *w, const char *owner, const char *target, uint32_t ttl)
{
	size_t p = rr_start(w, owner, 5, ttl);
	name(w, target);
	rr_end(w, p);
}

static int is_cancelled(void *v)
{
	return ((struct fixture *)v)->cancelled;
}

static enum fd_disc_dns_status query(void *v, const char *q, uint16_t type,
	unsigned char *buf, size_t cap, size_t *length)
{
	struct fixture *f = v;
	struct wire w;
	f->query_count++;
	if (f->cancel_on_query)
		f->cancelled = 1;
	memset(&w, 0, sizeof(w));
	w.b = buf; w.cap = cap;
	word(&w, 123); word(&w, 0x8180); word(&w, 1);
	word(&w, 0); word(&w, 0); word(&w, 0);
	name(&w, q); word(&w, type); word(&w, 1);
	if (!strncmp(q, "_diameter", 9))
		f->fallback_queries++;

	if (f->scenario == 1) {
		if (type == 35 && !strcmp(q, "example.test")) {
			/* Lower preference fails; a higher one must be tried. */
			naptr(&w, q, 10, 10, "", "aaa:diameter.tcp", "dead.test", 100);
			naptr(&w, q, 10, 20, "", "AAA:diameter.tcp:diameter.sctp", "next.test", 90);
			naptr(&w, q, 20, 1, "a", "aaa:diameter.sctp", "other.test", 50);
		} else if (type == 35 && !strcmp(q, "next.test")) {
			naptr(&w, q, 1, 1, "S", "aaa:diameter.tcp", "_svc.test", 80);
			/* The ancestor must not switch protocol to SCTP. */
			naptr(&w, q, 1, 2, "a", "aaa:diameter.sctp", "bad.test", 80);
		} else if (type == 33 && !strcmp(q, "_svc.test")) {
			srv(&w, q, 20, 0, 3868, "backup.test", 70);
			srv(&w, q, 10, 5, 3868, "alias.test", 60);
		} else if (type == 1 && !strcmp(q, "alias.test")) {
			cname(&w, q, "host.test", 45);
			address(&w, "host.test", 1, 40);
		} else if (type == 28 && !strcmp(q, "alias.test")) {
			cname(&w, q, "host.test", 45);
			address(&w, "host.test", 28, 30);
		} else if ((type == 1 || type == 28) &&
			   (!strcmp(q, "backup.test") || !strcmp(q, "bad.test"))) {
			address(&w, q, type, 55);
		}
	} else if (f->scenario == 2) {
		if (type == 33 && !strcmp(q, "_diameter._tcp.example.test")) {
			srv(&w, q, 1, 10, 3868, "weighted.test", 60);
			srv(&w, q, 1, 0, 3868, "zero.test", 60);
		} else if (type == 1 && (!strcmp(q, "weighted.test") || !strcmp(q, "zero.test")))
			address(&w, q, type, 50);
	} else if (f->scenario == 3) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "s", "aaa:unknown", "_svc.test", 30);
	} else if (f->scenario == 4) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "", "aaa:diameter.tcp", "example.test", 30);
	} else if (f->scenario == 5) {
		if (type == 33 && !strcmp(q, "_diameter._tcp.example.test"))
			srv(&w, q, 0, 0, 0, "", 10);
	} else if (f->scenario == 6) {
		return FD_DISC_DNS_TEMPORARY;
	} else if (f->scenario == 7) {
		/* Compression pointer points to itself in an answer owner. */
		word(&w, 0xc000 | (unsigned int)w.n);
		word(&w, 35); word(&w, 1); dword(&w, 10); word(&w, 0);
		w.answers++;
	} else if (f->scenario == 8) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "A", "aaa:diameter.tls.tcp", "secure.test", 50);
		else if (type == 1 && !strcmp(q, "secure.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 9) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "s", "aaa:diameter.tcp", "_good.test", 50);
			naptr(&w, q, 1, 2, "s", "aaa:diameter.tcp", "_error.test", 50);
		} else if (type == 33 && !strcmp(q, "_good.test")) {
			srv(&w, q, 1, 1, 3868, "good.test", 30);
		} else if (type == 1 && !strcmp(q, "good.test")) {
			address(&w, q, type, 20);
		} else if (type == 33 && !strcmp(q, "_error.test")) {
			return FD_DISC_DNS_TEMPORARY;
		}
	} else if (f->scenario == 10) {
		if (type == 35 && !strcmp(q, "example.test"))
			cname(&w, q, "alias.test", 15);
		else if (type == 35 && !strcmp(q, "alias.test"))
			cname(&w, q, "example.test", 10);
	} else if (f->scenario == 11) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "a", "aaa:diameter.tcp", "short.test", 50);
		else if (type == 1 && !strcmp(q, "short.test")) {
			size_t p = rr_start(&w, q, type, 20);
			byte(&w, 1);
			rr_end(&w, p);
		}
	} else if (f->scenario == 12) {
		if (type == 35 && !strcmp(q, "example.test")) {
			/* The answer owner is a compression pointer to the question. */
			size_t p;
			word(&w, 0xc00c); word(&w, 35); word(&w, 1); dword(&w, 60);
			p = w.n; word(&w, 0); w.answers++;
			word(&w, 1); word(&w, 1);
			string(&w, "a"); string(&w, "aaa:diameter.tcp"); string(&w, "");
			name(&w, "compressed.test");
			rr_end(&w, p);
		} else if (type == 1 && !strcmp(q, "compressed.test"))
			address(&w, q, type, 25);
	} else if (f->scenario == 13) {
		buf[3] = 0x82; /* SERVFAIL */
		*length = w.n;
		return FD_DISC_DNS_OK;
	} else if (f->scenario == 14) {
		if (type == 35 && !strcmp(q, "example.test")) {
			/* Legacy is lower-order but must not defeat extended records. */
			naptr(&w, q, 1, 1, "a", "aaa:diameter.tcp", "legacy.test", 50);
			naptr(&w, q, 10, 1, "a", "aaa+ap1:diameter.tcp", "app1.test", 50);
			naptr(&w, q, 10, 2, "a", "AAA+AP4:unknown:DIAMETER.TCP", "app4.test", 50);
		} else if (type == 1 && (!strcmp(q, "legacy.test") ||
			   !strcmp(q, "app1.test") || !strcmp(q, "app4.test")))
			address(&w, q, type, 20);
	} else if (f->scenario == 15) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "a", "aaa", "legacy.test", 50);
			naptr(&w, q, 2, 1, "a", "aaa+ap1:diameter.sctp", "app1.test", 50);
		} else if (type == 1)
			address(&w, q, type, 20);
	} else if (f->scenario == 16 || f->scenario == 17) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "a", f->scenario == 16 ? "aaa+ap4" : "aaa",
			      "all.test", 50);
		else if (type == 1 && !strcmp(q, "all.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 18) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "a", "aaa+ap04:diameter.tcp", "invalid.test", 50);
			naptr(&w, q, 1, 2, "a", "aaa+ap4294967296", "invalid.test", 50);
			naptr(&w, q, 1, 3, "a", "aaa+ap4:", "invalid.test", 50);
			naptr(&w, q, 1, 4, "a", "aaa:diameter.tcp", "legacy.test", 50);
		} else if (type == 1 && !strcmp(q, "legacy.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 19) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "", "aaa+ap4:diameter.tcp", "next.test", 50);
		else if (type == 35 && !strcmp(q, "next.test")) {
			naptr(&w, q, 1, 1, "a", "aaa+ap1:diameter.tcp", "app1.test", 50);
			naptr(&w, q, 1, 2, "a", "aaa:diameter.tcp", "legacy.test", 50);
			naptr(&w, q, 1, 3, "a", "aaa+ap4:diameter.sctp", "sctp.test", 50);
			naptr(&w, q, 1, 4, "a", "aaa+ap4:diameter.tcp", "app4.test", 50);
		} else if (type == 1)
			address(&w, q, type, 20);
	} else if (f->scenario == 20) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "a", "aaa+ap4294967295:diameter.tcp", "max.test", 50);
			naptr(&w, q, 1, 2, "a", "aaa+ap0:diameter.tcp", "zero.test", 50);
		} else if (type == 1)
			address(&w, q, type, 20);
	} else if (f->scenario == 21) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "a", "aaa:diameter.tcp", "legacy.test", 50);
			naptr(&w, q, 2, 1, "a", "aaa+ap4:diameter.sctp", "app4.test", 50);
		} else if (type == 1)
			address(&w, q, type, 20);
	} else if (f->scenario == 22) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "a", "aaa+ap4:diameter.tcp:diameter.sctp",
			      "both.test", 50);
		else if (type == 1 && !strcmp(q, "both.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 23 || f->scenario == 24) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "a", "aaa:diameter.tcp", "legacy.test", 50);
			naptr(&w, q, 2, 1, "a",
			      f->scenario == 23 ? "aaa+ap4:diameter.tcp" : "aaa+ap4:unknown",
			      "missing.test", 50);
		} else if (type == 1 && !strcmp(q, "legacy.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 25) {
		if (type == 33 && !strcmp(q, "_diameter._tcp.example.test"))
			srv(&w, q, 65535, 0, 3868, "max-priority.test", 30);
		else if (type == 1 && !strcmp(q, "max-priority.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 26) {
		if (type == 35 && !strcmp(q, "example.test"))
			naptr(&w, q, 1, 1, "a", "aaa+ap4:diameter.dtls.sctp",
			      "dtls.test", 50);
		else if (type == 1 && !strcmp(q, "dtls.test"))
			address(&w, q, type, 20);
	} else if (f->scenario == 27) {
		if (type == 35 && !strcmp(q, "example.test")) {
			naptr(&w, q, 1, 1, "a", "aaa:diameter.tcp", "missing.test", 50);
			naptr(&w, q, 2, 1, "a", "aaa:diameter.tcp", "chosen.test", 40);
			naptr(&w, q, 3, 1, "a", "aaa:diameter.tcp", "later.test", 30);
		} else if (type == 1 && (!strcmp(q, "chosen.test") ||
			   !strcmp(q, "later.test")))
			address(&w, q, type, 20);
	}
	if (!w.answers)
		return FD_DISC_DNS_ABSENT;
	buf[6] = (unsigned char)(w.answers >> 8);
	buf[7] = (unsigned char)w.answers;
	*length = w.n;
	return FD_DISC_DNS_OK;
}

static uint32_t random_value(void *v, uint32_t bound)
{
	struct fixture *f = v;
	return f->random % bound;
}

static enum fd_disc_dns_status run_app(struct fixture *f, unsigned int protocols,
	uint32_t application_id, struct fd_disc_dns_result **result)
{
	struct fd_disc_dns_options options;
	memset(&options, 0, sizeof(options));
	options.protocols = protocols;
	options.application_id = application_id;
	options.query = query;
	options.query_context = f;
	options.random = random_value;
	options.random_context = f;
	options.cancelled = is_cancelled;
	options.cancel_context = f;
	return fd_disc_dns_resolve("example.test.", &options, result);
}

static enum fd_disc_dns_status run(struct fixture *f, unsigned int protocols,
	struct fd_disc_dns_result **result)
{
	return run_app(f, protocols, 0, result);
}

int main(void)
{
	struct fixture f;
	struct fd_disc_dns_result *r;
#ifndef FD_DISC_HAVE_RES_NQUERY
	struct fd_disc_dns_options options;
	memset(&options, 0, sizeof(options));
	options.protocols = FD_DISC_DNS_TCP;
	assert(fd_disc_dns_resolve("example.test", &options, &r) ==
	       FD_DISC_DNS_UNSUPPORTED && !r);
#endif
	memset(&f, 0, sizeof(f));
	f.scenario = 1;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_OK);
	assert(r->count == 2);
	assert(!strcmp(r->candidates[0].target, "alias.test"));
	assert(r->candidates[0].port == 3868 && r->candidates[0].ttl == 30);
	assert(r->candidates[0].address_count == 2);
	assert(r->candidates[0].chain_count == 4);
	assert(!strcmp(r->candidates[0].chain[0], "example.test"));
	assert(!strcmp(r->candidates[0].chain[1], "next.test"));
	assert(!strcmp(r->candidates[0].chain[2], "alias.test"));
	assert(!strcmp(r->candidates[0].chain[3], "host.test"));
	assert(!strcmp(r->candidates[1].target, "backup.test"));
	assert(f.fallback_queries == 0);
	free(r);
	f.scenario = 2;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_OK);
	assert(r->count == 2 && !strcmp(r->candidates[0].target, "zero.test"));
	assert(r->candidates[0].chain_count == 2);
	assert(!strcmp(r->candidates[0].chain[0], "example.test"));
	assert(!strcmp(r->candidates[0].chain[1], "zero.test"));
	free(r);
	f.random = 10;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_OK);
	assert(!strcmp(r->candidates[0].target, "weighted.test"));
	free(r);
	f.fallback_queries = 0; f.scenario = 3;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_ABSENT && !r);
	assert(f.fallback_queries == 0);
	f.scenario = 4;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_MALFORMED && !r);
	f.scenario = 5;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_ABSENT && !r);
	f.scenario = 6;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_TEMPORARY && !r);
	f.scenario = 7;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_MALFORMED && !r);
	f.scenario = 8;
	assert(run(&f, FD_DISC_DNS_TLS_TCP, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && r->candidates[0].port == 5868 &&
	       r->candidates[0].protocol == FD_DISC_DNS_TLS_TCP &&
	       r->candidates[0].ttl == 20);
	free(r);
	f.scenario = 9;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_TEMPORARY && !r);
	f.scenario = 10;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_MALFORMED && !r);
	f.scenario = 11;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_MALFORMED && !r);
	f.scenario = 12;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && r->candidates[0].ttl == 25);
	free(r);
	f.scenario = 13;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_TEMPORARY && !r);
	f.scenario = 14;
	assert(run_app(&f, FD_DISC_DNS_TCP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "app4.test"));
	free(r);
	assert(run_app(&f, FD_DISC_DNS_TCP, 1, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "app1.test"));
	free(r);
	f.fallback_queries = 0;
	assert(run_app(&f, FD_DISC_DNS_TCP, 3, &r) == FD_DISC_DNS_ABSENT && !r);
	assert(!f.fallback_queries);
	f.scenario = 15;
	assert(run_app(&f, FD_DISC_DNS_TCP, 4, &r) == FD_DISC_DNS_ABSENT && !r);
	f.scenario = 16;
	assert(run_app(&f, FD_DISC_DNS_TCP | FD_DISC_DNS_SCTP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 2 && r->candidates[0].protocol == FD_DISC_DNS_TCP &&
	       r->candidates[1].protocol == FD_DISC_DNS_SCTP);
	free(r);
	assert(run_app(&f, FD_DISC_DNS_TCP, 1, &r) == FD_DISC_DNS_ABSENT && !r);
	f.scenario = 17;
	assert(run_app(&f, FD_DISC_DNS_TCP | FD_DISC_DNS_SCTP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 2 && r->candidates[0].protocol == FD_DISC_DNS_TCP &&
	       r->candidates[1].protocol == FD_DISC_DNS_SCTP);
	free(r);
	f.scenario = 18;
	assert(run_app(&f, FD_DISC_DNS_TCP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "legacy.test"));
	free(r);
	f.scenario = 19;
	assert(run_app(&f, FD_DISC_DNS_TCP | FD_DISC_DNS_SCTP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "app4.test") &&
	       r->candidates[0].protocol == FD_DISC_DNS_TCP);
	free(r);
	f.scenario = 20;
	assert(run_app(&f, FD_DISC_DNS_TCP, UINT32_MAX, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "max.test"));
	free(r);
	assert(run_app(&f, FD_DISC_DNS_TCP, 0, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "zero.test"));
	free(r);
	f.scenario = 21;
	f.fallback_queries = 0;
	assert(run_app(&f, FD_DISC_DNS_TCP, 4, &r) == FD_DISC_DNS_ABSENT && !r);
	assert(!f.fallback_queries);
	f.scenario = 22;
	assert(run_app(&f, FD_DISC_DNS_TCP | FD_DISC_DNS_SCTP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 2 && r->candidates[0].protocol == FD_DISC_DNS_TCP &&
	       r->candidates[1].protocol == FD_DISC_DNS_SCTP);
	free(r);
	f.scenario = 23;
	assert(run_app(&f, FD_DISC_DNS_TCP, 4, &r) == FD_DISC_DNS_ABSENT && !r);
	f.scenario = 24;
	assert(run_app(&f, FD_DISC_DNS_TCP, 4, &r) == FD_DISC_DNS_ABSENT && !r);
	f.scenario = 25;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "max-priority.test"));
	free(r);
	f.scenario = 26;
	assert(run_app(&f, FD_DISC_DNS_DTLS_SCTP, 4, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && r->candidates[0].protocol == FD_DISC_DNS_DTLS_SCTP &&
	       r->candidates[0].port == 5868);
	free(r);
	f.scenario = 27;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_OK);
	assert(r->count == 1 && !strcmp(r->candidates[0].target, "chosen.test"));
	free(r);
	f.cancelled = 1;
	f.query_count = 0;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_CANCELLED && !r);
	assert(f.query_count == 0);
	f.cancelled = 0;
	f.cancel_on_query = 1;
	f.query_count = 0;
	assert(run(&f, FD_DISC_DNS_TCP, &r) == FD_DISC_DNS_CANCELLED && !r);
	assert(f.query_count == 1);
	puts("discovery DNS tests passed");
	return 0;
}
