/* Internal, synchronous Diameter DNS discovery.  Not a public library API. */
#ifndef FD_DISCOVERY_DNS_H
#define FD_DISCOVERY_DNS_H

#include <stddef.h>
#include <stdint.h>

#define FD_DISC_DNS_NAME_MAX 255
#define FD_DISC_DNS_CHAIN_MAX 24
#define FD_DISC_DNS_ADDRESS_MAX 16
#define FD_DISC_DNS_CANDIDATE_MAX 128

enum fd_disc_dns_status {
	FD_DISC_DNS_OK = 0,
	FD_DISC_DNS_ABSENT,
	FD_DISC_DNS_TEMPORARY,
	FD_DISC_DNS_MALFORMED,
	FD_DISC_DNS_LIMIT,
	FD_DISC_DNS_NOMEM,
	FD_DISC_DNS_INVALID,
	FD_DISC_DNS_CANCELLED,
	FD_DISC_DNS_UNSUPPORTED
};

enum fd_disc_dns_protocol {
	FD_DISC_DNS_TCP = 1,
	FD_DISC_DNS_SCTP = 2,
	FD_DISC_DNS_TLS_TCP = 4,
	FD_DISC_DNS_DTLS_SCTP = 8
};

struct fd_disc_dns_address {
	/* 4 or 16 bytes, in network order. */
	unsigned int length;
	unsigned char bytes[16];
	uint32_t ttl;
};

struct fd_disc_dns_candidate {
	char target[FD_DISC_DNS_NAME_MAX + 1];
	uint16_t port;
	enum fd_disc_dns_protocol protocol;
	uint32_t ttl;
	/* Certificate-eligible DNS names on the path (excluding SRV owner names),
	 * including both address-lookup CNAME paths. */
	size_t chain_count;
	char chain[FD_DISC_DNS_CHAIN_MAX][FD_DISC_DNS_NAME_MAX + 1];
	size_t address_count;
	struct fd_disc_dns_address addresses[FD_DISC_DNS_ADDRESS_MAX];
};

struct fd_disc_dns_result {
	size_t count;
	struct fd_disc_dns_candidate candidates[FD_DISC_DNS_CANDIDATE_MAX];
};

/* Query returns an unmodified complete DNS response in the supplied buffer.
 * ABSENT means NXDOMAIN or NODATA; all other errors must remain distinct.
 * The callback must not return OK for a truncated response. */
typedef enum fd_disc_dns_status (*fd_disc_dns_query_fn)(
	void *context, const char *name, uint16_t type,
	unsigned char *response, size_t capacity, size_t *length);
/* Nonzero means the current discovery operation has been cancelled. */
typedef int (*fd_disc_dns_cancel_fn)(void *context);
/* Uniform integer in [0, upper_bound); upper_bound is always nonzero. */
typedef uint32_t (*fd_disc_dns_random_fn)(void *context, uint32_t upper_bound);

struct fd_disc_dns_options {
	unsigned int protocols; /* OR of enum fd_disc_dns_protocol values */
	uint32_t application_id; /* Required Diameter Application-Id, including 0. */
	/* NULL selects the per-query system resolver on supported platforms.
	 * Injected callbacks run synchronously and must have their own timeout;
	 * fd_disc_dns_resolve cannot cancel a callback that blocks. */
	fd_disc_dns_query_fn query;
	void *query_context;
	/* NULL selects an internal non-cryptographic per-call PRNG. */
	fd_disc_dns_random_fn random;
	void *random_context;
	/* Polled before and after each query; callback must be thread-safe. */
	fd_disc_dns_cancel_fn cancelled;
	void *cancel_context;
};

/* On OK, *result is owned by the caller and released with free().
 * Otherwise *result is NULL.  No connection, authorization, certificate
 * validation is performed. Cancellation is polled before/after each DNS
 * query and returns FD_DISC_DNS_CANCELLED with *result set to NULL. The
 * system resolver cannot be interrupted mid-query; its per-server timeout
 * and retry settings bound the wait before cancellation is observed.
 * A worker must run this call.
 * The target is a DNS name, not necessarily the Diameter Origin-Host.
 * Candidates are grouped by protocol in local preference order; within each
 * protocol they follow NAPTR order/preference and RFC 2782 SRV selection.
 * If the initial RRset contains RFC 6408 extended services, only the requested
 * application is used; legacy services are not a fallback for a mismatch.
 * Candidate TTL is the minimum over its discovery and returned address RRs,
 * without deducting time spent resolving.  Calculate expiry conservatively
 * from the time this call started, not the time it returned.
 * Resolver and cancellation callbacks must be thread-safe when used
 * concurrently. If no compatible system resolver is available and no query
 * callback is injected, resolution returns FD_DISC_DNS_UNSUPPORTED (mapped to
 * ENOTSUP by libfdcore). The CMake build detects the compatible resolver API;
 * standalone builds may define FD_DISC_HAVE_RES_NQUERY when they provide it.
 * The built-in resolver uses a private resolver state with bounded retry
 * settings; the overall walk is capped at 512 DNS queries. Unsupported
 * platforms can still use an injected query callback. */
enum fd_disc_dns_status fd_disc_dns_resolve(
	const char *realm, const struct fd_disc_dns_options *options,
	struct fd_disc_dns_result **result);

#endif
