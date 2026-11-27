/* Internal-only secure peer creation for the core DNS discovery manager. */
#ifndef FD_DISCOVERY_PEER_H
#define FD_DISCOVERY_PEER_H

#include "discovery_dns.h"
#include <freeDiameter/discovery.h>
#include <time.h>

struct peer_info;
struct fd_peer;
struct cnxctx;

struct fd_discovery_peer {
	/* Explicitly authorized Diameter identity, never inferred from DNS. */
	const char *identity;
	/* Queried routing realm to authenticate in the certificate, not Origin-Realm. */
	const char *realm;
	const struct fd_disc_dns_candidate *candidate;
	struct fd_discovery_peer_options options;
	uint32_t application_id;
	/* Nonzero manager-unique token, preventing cancellation of another add. */
	uint64_t token;
	/* Absolute CLOCK_REALTIME DNS deadline, measured before resolution. */
	struct timespec expires;
};

/* Copies all inputs; EEXIST never modifies the existing peer.
 * Only TLS/TCP and DTLS/SCTP are accepted; unsupported transports return
 * ENOTSUP. On return 0, callback receives 0 on OPEN and exactly one nonzero
 * terminal status, or just a nonzero status if OPEN was never reached.
 * A remote DPR status is delivered before leaving OPEN; termination is
 * reported before garbage collection. On error no callback runs.
 * Certificate names include identity, realm, target and every DNS chain
 * domain (the two underscore-prefixed SRV service labels are not hostnames).
 * No refresh/re-authorization of an existing peer is performed. */
int fd_peer_add_discovered(const struct fd_discovery_peer *discovery,
	void (*cb)(int, void *), void *cb_data);
/* Called by the PSM without peer locks; a nonzero status is terminal. */
void fd_peer_discovery_notify(struct fd_peer *peer, int status);
/* Asynchronous termination; ENOENT if identity/token no longer matches.
 * Never terminates a static peer or another discovery attempt.
 * Non-OPEN attempts leave identity lookup synchronously, allowing the next
 * candidate to be added; callbacks still require caller synchronization. */
int fd_peer_discovery_cancel(const char *identity, uint64_t token);
/* Boolean snapshot; lock order peers read lock -> peer state mutex.
 * No caller-owned lock needed and no peer pointer escapes. */
int fd_peer_discovery_is_open(const char *identity, uint64_t token);

/* Core implementation helpers; check() is called only by the owning PSM. */
int fd_peer_discovery_check(struct fd_peer *peer, struct cnxctx *cnx);
int fd_peer_discovery_expired(struct fd_peer *peer);
void fd_peer_discovery_retire(struct fd_peer *peer);
/* Reap completed retired peers when no core work can hold their old pointers.
 * Called by the existing peer GC thread; returns the number freed. */
size_t fd_peer_discovery_reap(void);
int fd_cnx_verify_discovery(struct cnxctx *cnx, const char *identity,
	const char *realm, const struct fd_disc_dns_candidate *candidate,
	time_t *certificate_expiry);

#endif
