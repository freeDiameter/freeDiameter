/* Linux-only resolver interposition for test_multi_peer_discovery.py. */
#define _GNU_SOURCE
#include <arpa/nameser.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct wire {
	unsigned char *data;
	size_t size, cap;
	int overflow;
};

static void byte(struct wire *w, unsigned int v)
{
	if (w->size < w->cap)
		w->data[w->size] = v;
	else
		w->overflow = 1;
	w->size++;
}

static void word(struct wire *w, unsigned int v)
{
	byte(w, v >> 8);
	byte(w, v);
}

static void dword(struct wire *w, unsigned int v)
{
	word(w, v >> 16);
	word(w, v);
}

static void name(struct wire *w, const char *s)
{
	while (*s) {
		const char *end = strchr(s, '.');
		if (!end)
			end = s + strlen(s);
		byte(w, end - s);
		while (s != end)
			byte(w, (unsigned char)*s++);
		if (*s)
			s++;
	}
	byte(w, 0);
}

static void text(struct wire *w, const char *s)
{
	byte(w, strlen(s));
	while (*s)
		byte(w, (unsigned char)*s++);
}

int res_ninit(res_state state)
{
	memset(state, 0, sizeof(*state));
	return 0;
}

void res_nclose(res_state state)
{
	(void)state;
}

int res_nquery(res_state state, const char *query, int cls, int type,
	       unsigned char *answer, int capacity)
{
	const char *scenario = getenv("FD_DISCOVERY_FIXTURE_SCENARIO");
	const char *logfile = getenv("FD_DISCOVERY_FIXTURE_LOG");
	const char *port_string = getenv("FD_DISCOVERY_FIXTURE_PORT");
	const char *realm = getenv("FD_DISCOVERY_FIXTURE_REALM");
	const char *ready = getenv("FD_DISCOVERY_FIXTURE_READY");
	int cross_realm = realm && !strcmp(realm, "route.example");
	const char *domain = cross_realm ? "route.example" : "localdomain";
	const char *naptr_srv = cross_realm ? "_naptr._tcp.route.example" :
		"_naptr._tcp.localdomain";
	const char *fallback_srv = cross_realm ? "_diameters._tcp.route.example" :
		"_diameters._tcp.localdomain";
	const char *target = cross_realm ? "srv.route.example" : "srv.localdomain";
	int is_naptr = type == ns_t_naptr && !strcmp(query, domain);
	int is_srv = type == ns_t_srv && (!strcmp(query, fallback_srv) ||
					 !strcmp(query, naptr_srv));
	int is_a = type == ns_t_a && !strcmp(query, target);
	int has_answer = (is_naptr && scenario && !strcmp(scenario, "naptr")) || is_srv || is_a;
	struct wire w = { answer, 0, capacity > 0 ? (size_t)capacity : 0, 0 };
	size_t length_pos, start;
	if (logfile) {
		int fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, 0600);
		if (fd >= 0) {
			char line[512];
			int n = snprintf(line, sizeof(line), "%d %s\n", type, query);
			if (n > 0 && (size_t)n < sizeof(line))
				(void)write(fd, line, (size_t)n);
			close(fd);
		}
	}
	if (ready && access(ready, F_OK) != 0) {
		state->res_h_errno = TRY_AGAIN;
		return -1;
	}
	word(&w, 0); word(&w, 0x8180); word(&w, 1);
	word(&w, has_answer ? 1 : 0); word(&w, 0); word(&w, 0);
	name(&w, query); word(&w, type); word(&w, cls);
	if (has_answer) {
		name(&w, query); word(&w, type); word(&w, 1); dword(&w, 60);
		length_pos = w.size; word(&w, 0); start = w.size;
		if (is_naptr) {
			word(&w, 1); word(&w, 1); text(&w, "s");
			text(&w, "aaa:diameter.tls.tcp"); text(&w, "");
			name(&w, naptr_srv);
		} else if (is_srv) {
			word(&w, 1); word(&w, 0);
			word(&w, port_string ? (unsigned int)atoi(port_string) : 0);
			name(&w, target);
		} else {
			byte(&w, 127); byte(&w, 0); byte(&w, 0); byte(&w, 1);
		}
		if (!w.overflow) {
			answer[length_pos] = (unsigned char)((w.size - start) >> 8);
			answer[length_pos + 1] = (unsigned char)(w.size - start);
		}
	}
	return w.overflow ? -1 : (int)w.size;
}

/* Numeric-only getaddrinfo guarantees the daemons cannot use host DNS via NSS. */
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints,
		struct addrinfo **result)
{
	int (*real_getaddrinfo)(const char *, const char *, const struct addrinfo *,
				struct addrinfo **) = dlsym(RTLD_NEXT, "getaddrinfo");
	struct addrinfo numeric;
	if (!real_getaddrinfo)
		return EAI_SYSTEM;
	memset(&numeric, 0, sizeof(numeric));
	if (hints)
		numeric = *hints;
	if (node)
		numeric.ai_flags |= AI_NUMERICHOST;
	return real_getaddrinfo(node, service, &numeric, result);
}

/* ListenOn loopback can be filtered by the daemon; never expose test listeners. */
int bind(int fd, const struct sockaddr *address, socklen_t length)
{
	int (*real_bind)(int, const struct sockaddr *, socklen_t) = dlsym(RTLD_NEXT, "bind");
	struct sockaddr_in local;
	if (!real_bind) {
		errno = ENOSYS;
		return -1;
	}
	if (address && address->sa_family == AF_INET && length >= sizeof(local)) {
		memcpy(&local, address, sizeof(local));
		local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		return real_bind(fd, (const struct sockaddr *)&local, sizeof(local));
	}
	if (address && address->sa_family == AF_INET6) {
		errno = EAFNOSUPPORT;
		return -1;
	}
	return real_bind(fd, address, length);
}
