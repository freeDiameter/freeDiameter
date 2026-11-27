/* SPDX-License-Identifier: BSD-3-Clause */
/* Explicit DNS-based Diameter peer discovery. */
#ifndef FREE_DIAMETER_DISCOVERY_H
#define FREE_DIAMETER_DISCOVERY_H

#include <stddef.h>
#include <stdint.h>

#define FD_DISCOVERY_TLS_TCP   0x01u
#define FD_DISCOVERY_DTLS_SCTP 0x02u

#define FD_DISCOVERY_IP_ANY  0u
#define FD_DISCOVERY_IPV4    1u
#define FD_DISCOVERY_IPV6    2u

/* Remote DPR causes; all other callback failures are positive errno values. */
#define FD_DISCOVERY_PEER_BUSY       (-1)
#define FD_DISCOVERY_PEER_NOT_FRIEND (-2)

struct fd_discovery;

struct fd_discovery_peer_options {
	unsigned int ip_family; /* FD_DISCOVERY_IP_ANY, IPV4 or IPV6 */
	int tc_timer;           /* 0 uses the core default; at most INT_MAX / 200 */
	int tw_timer;           /* 0 uses the core default; at most INT_MAX / 2 */
	const char *tls_priority;
	const char *peer_realm; /* optional expected Origin-Realm */
};

struct fd_discovery_config {
	const char *realm;
	uint32_t application_id;
	const char *const *authorized_peers;
	size_t authorized_peer_count;
	unsigned int transports;
	unsigned int timeout_seconds;
	struct fd_discovery_peer_options peer_options;
};

/*
 * Start discovery and maintain the resulting peer until stopped. Inputs are
 * copied. All peers must be explicitly authorized by Diameter identity, and
 * TLS uses the core's configured credentials and trust anchors. The peer
 * certificate must cover the authorized identity, queried realm, SRV target,
 * and certificate-eligible names encountered during DNS resolution.
 * transports is a bitwise OR of FD_DISCOVERY_* values. timeout_seconds bounds
 * each DNS resolution and connection attempt cycle; it must be nonzero.
 * peer_options apply to each created peer and are copied. The optional
 * peer_realm restricts Origin-Realm independently of the queried realm.
 * tc_timer influences peer state-machine timers but does not enable retries;
 * API callers remain responsible for their own retry policy. Port, endpoint
 * and security mode come from DNS; discovery supports immediate TLS/DTLS only.
 *
 * callback is invoked with 0 when an authorized peer supporting the
 * application first opens, or with a nonzero error if the initial attempt
 * fails. After success, DNS data is refreshed when it expires; healthy
 * refreshes do not invoke the callback. An unexpected disconnect or failed
 * refresh invalidates the route, invokes the callback once more with a
 * nonzero error, and ends the operation. Failures are not retried. The owner
 * must stop the retained handle before starting a replacement operation.
 * FD_DISCOVERY_PEER_BUSY and FD_DISCOVERY_PEER_NOT_FRIEND indicate the
 * corresponding DPR causes received after the peer is authenticated and
 * authorized. RFC 6733 section 5.4.3 says the receiver SHOULD NOT
 * automatically reconnect in either case. Other failures use positive errno
 * values; a normal TTL refresh is not reported as a failure. If a peer opens
 * and immediately disconnects before success is reported, its DPR status may
 * instead be the initial failure callback.
 * The callback runs outside manager locks, may run before this function
 * returns, and must not stop its own operation or shut down the core.
 * Unsupported system resolvers report ENOTSUP via the callback.
 * A peer identity already owned by a static peer or another discovery
 * operation is not reused.
 *
 * On success, *operation owns the operation and must be stopped before the
 * caller's callback data or code is released. On error no callback is invoked
 * and *operation is NULL.
 */
int fd_discovery_start(const struct fd_discovery_config *config,
			void (*callback)(void *data, int error), void *data,
			struct fd_discovery **operation);

/* Report whether identity is the operation's currently open, authorized and
 * unexpired peer. identity has len bytes and need not be NUL-terminated.
 * The caller must keep operation alive until this call returns. */
int fd_discovery_has_peer(struct fd_discovery *operation, const char *identity,
			  size_t len, int *matched);

/* Stop and release an operation. Waits for its worker and callback to finish;
 * no callback occurs after it returns. Core shutdown similarly waits for
 * callbacks, but the owner must still stop the retained handle. Calls using
 * the same operation must be externally synchronized with stop. The
 * operation pointer is set to NULL on success. */
int fd_discovery_stop(struct fd_discovery **operation);

#endif
