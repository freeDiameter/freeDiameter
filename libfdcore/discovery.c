/* Explicit, asynchronous DNS-based discovery. */
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/libfdcore.h>
#include <freeDiameter/discovery.h>
#include "discovery_dns.h"
#include "discovery_peer.h"
#include "discovery_internal.h"
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

struct fd_discovery {
	pthread_mutex_t lock;
	pthread_cond_t changed;
	struct fd_discovery *next;
	unsigned int refs;
	int stopping, finished, shutdown_seen;
	char *realm;
	char **identities;
	size_t identity_count;
	uint32_t application;
	unsigned int transports, timeout;
	struct fd_discovery_peer_options peer_options;
	void (*callback)(void *, int);
	void *data;
	int reported;
	int dns_pending, dns_ready;
	int dns_cancel;
	pthread_t dns_thread;
	enum fd_disc_dns_status dns_status;
	struct fd_disc_dns_result *dns_result;
	struct timespec dns_started;
	uint64_t token;
	size_t active_identity;
	size_t previous_identity;
	int replacing_peer;
	int peer_event, peer_ok, peer_failure;
	struct timespec active_expiry;
	struct timespec initial_deadline;
};

static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
static struct fd_discovery *operations;
static int shutting_down;
static uint64_t next_token;
static struct fd_rt_out_hdl *route_handler;

static void release(struct fd_discovery *op)
{
	size_t i;
	int destroy;
	pthread_mutex_lock(&registry_lock);
	destroy = --op->refs == 0;
	pthread_mutex_unlock(&registry_lock);
	if (!destroy)
		return;
	for (i = 0; i < op->identity_count; i++)
		free(op->identities[i]);
	free(op->identities);
	free(op->realm);
	free((char *)op->peer_options.tls_priority);
	free((char *)op->peer_options.peer_realm);
	free(op->dns_result);
	pthread_cond_destroy(&op->changed);
	pthread_mutex_destroy(&op->lock);
	free(op);
}

static void retain(struct fd_discovery *op)
{
	pthread_mutex_lock(&registry_lock);
	op->refs++;
	pthread_mutex_unlock(&registry_lock);
}

static int before(const struct timespec *a, const struct timespec *b)
{
	return a->tv_sec < b->tv_sec ||
		(a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

/* Timed waits use short realtime intervals, while all manager deadlines use
 * monotonic time.  Peer expiry is separately enforced using realtime. */
static void wait_until(struct fd_discovery *op, const struct timespec *deadline)
{
	struct timespec now, until;
	long ns;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!before(&now, deadline))
		return;
	clock_gettime(CLOCK_REALTIME, &until);
	if (deadline->tv_sec - now.tv_sec > 1)
		ns = 1000000000L;
	else {
		ns = (deadline->tv_sec - now.tv_sec) * 1000000000L +
			deadline->tv_nsec - now.tv_nsec;
		if (ns > 1000000000L)
			ns = 1000000000L;
	}
	until.tv_sec += ns / 1000000000L;
	until.tv_nsec += ns % 1000000000L;
	if (until.tv_nsec >= 1000000000L) {
		until.tv_sec++;
		until.tv_nsec -= 1000000000L;
	}
	pthread_cond_timedwait(&op->changed, &op->lock, &until);
}

static void wait_for_peer_change(struct fd_discovery *op)
{
	struct timespec until;
	clock_gettime(CLOCK_REALTIME, &until);
	until.tv_nsec += 100000000L;
	if (until.tv_nsec >= 1000000000L) {
		until.tv_sec++;
		until.tv_nsec -= 1000000000L;
	}
	pthread_cond_timedwait(&op->changed, &op->lock, &until);
}

static int dns_error(enum fd_disc_dns_status status)
{
	switch (status) {
	case FD_DISC_DNS_ABSENT: return ENOENT;
	case FD_DISC_DNS_TEMPORARY: return EAGAIN;
	case FD_DISC_DNS_NOMEM: return ENOMEM;
	case FD_DISC_DNS_LIMIT: return EOVERFLOW;
	case FD_DISC_DNS_UNSUPPORTED: return ENOTSUP;
	default: return EPROTO;
	}
}

static int resolution_cancelled(void *arg)
{
	struct fd_discovery *op = arg;
	int cancelled;
	pthread_mutex_lock(&op->lock);
	cancelled = op->stopping || op->dns_cancel;
	pthread_mutex_unlock(&op->lock);
	return cancelled;
}

static void *resolve_thread(void *arg)
{
	struct fd_discovery *op = arg;
	struct fd_disc_dns_options options;
	struct fd_disc_dns_result *result = NULL;
	enum fd_disc_dns_status status;

	memset(&options, 0, sizeof(options));
	options.application_id = op->application;
	options.cancelled = resolution_cancelled;
	options.cancel_context = op;
	if (op->transports & FD_DISCOVERY_TLS_TCP)
		options.protocols |= FD_DISC_DNS_TLS_TCP;
	if (op->transports & FD_DISCOVERY_DTLS_SCTP)
		options.protocols |= FD_DISC_DNS_DTLS_SCTP;
	status = fd_disc_dns_resolve(op->realm, &options, &result);
	pthread_mutex_lock(&op->lock);
	op->dns_pending = 0;
	if (!op->stopping) {
		op->dns_result = result;
		op->dns_status = status;
		op->dns_ready = 1;
		result = NULL;
	}
	pthread_cond_broadcast(&op->changed);
	pthread_mutex_unlock(&op->lock);
	free(result);
	return NULL;
}

static int begin_resolution(struct fd_discovery *op)
{
	int ret;
	clock_gettime(CLOCK_MONOTONIC, &op->dns_started);
	op->dns_ready = 0;
	op->dns_cancel = 0;
	op->dns_pending = 1;
	ret = pthread_create(&op->dns_thread, NULL, resolve_thread, op);
	if (ret) {
		op->dns_pending = 0;
		return ret;
	}
	return 0;
}

static void finish_resolution(struct fd_discovery *op)
{
	/* Never leave resolver code running across shutdown or library unload. */
	pthread_mutex_unlock(&op->lock);
	pthread_join(op->dns_thread, NULL);
	pthread_mutex_lock(&op->lock);
}

struct peer_event {
	struct fd_discovery *op;
	uint64_t token;
};

static void peer_callback(int status, void *data)
{
	struct peer_event *event = data;
	struct fd_discovery *op = event->op;
	struct timespec now;
	pthread_mutex_lock(&op->lock);
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!op->stopping && op->token == event->token &&
	    before(&now, &op->active_expiry)) {
		op->peer_event = 1;
		op->peer_ok = status == 0;
		op->peer_failure = status;
		pthread_cond_broadcast(&op->changed);
	}
	pthread_mutex_unlock(&op->lock);
	if (status) {
		free(event);
		release(op);
	}
}

static uint64_t allocate_token(void)
{
	uint64_t token;
	pthread_mutex_lock(&registry_lock);
	token = ++next_token;
	if (!token)
		token = ++next_token;
	pthread_mutex_unlock(&registry_lock);
	return token;
}

static void cancel_peer(struct fd_discovery *op)
{
	uint64_t token = op->token;
	size_t identity = op->active_identity;
	op->token = 0;
	if (token) {
		pthread_mutex_unlock(&op->lock);
		fd_peer_discovery_cancel(op->identities[identity], token);
		pthread_mutex_lock(&op->lock);
	}
}

static void report(struct fd_discovery *op, int error)
{
	if (op->stopping || op->reported == 2 ||
	    (op->reported == 1 && !error))
		return;
	op->reported = error ? 2 : 1;
	pthread_mutex_unlock(&op->lock);
	op->callback(op->data, error);
	pthread_mutex_lock(&op->lock);
}

static int try_candidate(struct fd_discovery *op,
	const struct fd_disc_dns_candidate *candidate, size_t identity,
	const struct timespec *expiry, const struct timespec *deadline)
{
	struct fd_discovery_peer peer;
	struct peer_event *event;
	struct timespec now, real;
	int ret;

retry_add:
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!before(&now, expiry) || !before(&now, deadline))
		return ETIMEDOUT;
	event = malloc(sizeof(*event));
	if (!event)
		return ENOMEM;
	memset(&peer, 0, sizeof(peer));
	peer.identity = op->identities[identity];
	peer.realm = op->realm;
	peer.candidate = candidate;
	peer.options = op->peer_options;
	peer.application_id = op->application;
	peer.token = allocate_token();
	clock_gettime(CLOCK_REALTIME, &real);
	peer.expires.tv_sec = real.tv_sec + expiry->tv_sec - now.tv_sec;
	peer.expires.tv_nsec = real.tv_nsec + expiry->tv_nsec - now.tv_nsec;
	if (peer.expires.tv_nsec < 0) {
		peer.expires.tv_sec--;
		peer.expires.tv_nsec += 1000000000L;
	} else if (peer.expires.tv_nsec >= 1000000000L) {
		peer.expires.tv_sec++;
		peer.expires.tv_nsec -= 1000000000L;
	}
	event->op = op;
	event->token = peer.token;
	op->token = peer.token;
	op->active_identity = identity;
	op->peer_event = 0;
	op->peer_ok = 0;
	op->peer_failure = 0;
	op->active_expiry = *expiry;
	retain(op);
	/* Peer callbacks may arrive immediately, so never hold the lock here. */
	pthread_mutex_unlock(&op->lock);
	ret = fd_peer_add_discovered(&peer, peer_callback, event);
	pthread_mutex_lock(&op->lock);
	if (ret) {
		free(event);
		release(op);
		op->token = 0;
		/* An expired OPEN peer is retired asynchronously. Wait for that
		 * identity to leave the peer registry within this refresh budget. */
		if (ret == EEXIST && op->replacing_peer &&
		    identity == op->previous_identity && !op->stopping) {
			wait_for_peer_change(op);
			goto retry_add;
		}
		return ret;
	}
	while (!op->stopping && !op->peer_event) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (!before(&now, deadline) || !before(&now, expiry))
			break;
		wait_until(op, before(deadline, expiry) ? deadline : expiry);
	}
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!op->stopping && op->peer_event && op->peer_ok &&
	    before(&now, expiry) && before(&now, deadline))
		return 0;
	ret = op->peer_failure < 0 ? op->peer_failure :
		(op->peer_event && !op->peer_ok ? ECONNREFUSED : ETIMEDOUT);
	cancel_peer(op);
	return ret;
}

static struct timespec attempt_deadline(const struct timespec *deadline,
	size_t remaining)
{
	struct timespec now, result;
	uint64_t ns, slice;
	clock_gettime(CLOCK_MONOTONIC, &now);
	if (!before(&now, deadline))
		return now;
	ns = (uint64_t)(deadline->tv_sec - now.tv_sec) * 1000000000ULL;
	if (deadline->tv_nsec >= now.tv_nsec)
		ns += deadline->tv_nsec - now.tv_nsec;
	else
		ns -= now.tv_nsec - deadline->tv_nsec;
	slice = ns / remaining;
	if (slice < 500000000ULL)
		slice = ns < 500000000ULL ? ns : 500000000ULL;
	result = now;
	result.tv_sec += slice / 1000000000ULL;
	result.tv_nsec += slice % 1000000000ULL;
	if (result.tv_nsec >= 1000000000L) {
		result.tv_sec++;
		result.tv_nsec -= 1000000000L;
	}
	return result;
}

static void *discovery_worker(void *arg)
{
	struct fd_discovery *op = arg;
	struct timespec deadline, now, expiry;
	struct fd_disc_dns_result *result;
	size_t i, j, remaining;
	int ret, connected;

	deadline = op->initial_deadline;
	pthread_mutex_lock(&op->lock);
	while (!op->stopping) {
		ret = begin_resolution(op);
		if (ret) {
			report(op, ret);
			break;
		}
		while (!op->stopping && !op->dns_ready) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (!before(&now, &deadline))
				break;
			wait_until(op, &deadline);
		}
		if (op->stopping)
		{
			finish_resolution(op);
			break;
		}
		if (!op->dns_ready) {
			op->dns_cancel = 1;
			report(op, ETIMEDOUT);
			finish_resolution(op);
			break;
		}
		finish_resolution(op);
		if (op->stopping)
			break;
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (!before(&now, &deadline)) {
			report(op, ETIMEDOUT);
			break;
		}
		result = op->dns_result;
		op->dns_result = NULL;
		op->dns_ready = 0;
		if (op->dns_status != FD_DISC_DNS_OK || !result || !result->count) {
			ret = op->dns_status == FD_DISC_DNS_OK ? ENOENT : dns_error(op->dns_status);
			free(result);
			report(op, ret);
			break;
		}
		connected = 0;
		ret = ENOENT;
		for (i = 0; i < result->count && !op->stopping && !connected; i++) {
			if ((result->candidates[i].protocol == FD_DISC_DNS_TLS_TCP &&
			     !(op->transports & FD_DISCOVERY_TLS_TCP)) ||
			    (result->candidates[i].protocol == FD_DISC_DNS_DTLS_SCTP &&
			     !(op->transports & FD_DISCOVERY_DTLS_SCTP)) ||
			    (result->candidates[i].protocol != FD_DISC_DNS_TLS_TCP &&
			     result->candidates[i].protocol != FD_DISC_DNS_DTLS_SCTP)) {
				ret = EPROTO;
				continue;
			}
			expiry = op->dns_started;
			expiry.tv_sec += result->candidates[i].ttl;
			for (j = 0; j < op->identity_count && !op->stopping; j++) {
				struct timespec attempt;
				if (op->identity_count > (SIZE_MAX - 1) / (result->count - i))
					remaining = SIZE_MAX;
				else
					remaining = (result->count - i) * op->identity_count - j;
				attempt = attempt_deadline(&deadline, remaining);
				ret = try_candidate(op, &result->candidates[i], j, &expiry, &attempt);
				if (ret < 0)
					break;
				if (!ret) {
					connected = 1;
					break;
				}
			}
			if (ret < 0)
				break;
		}
		free(result);
		if (op->stopping)
			break;
		if (!connected) {
			report(op, ret);
			break;
		}
		report(op, 0);
		while (!op->stopping) {
			if (op->peer_failure) {
				ret = op->peer_failure;
				cancel_peer(op);
				report(op, ret);
				goto done;
			}
			clock_gettime(CLOCK_MONOTONIC, &now);
			if (!before(&now, &expiry))
				break;
			if (!fd_peer_discovery_is_open(op->identities[op->active_identity],
						       op->token)) {
				cancel_peer(op);
				report(op, ECONNRESET);
				goto done;
			}
			wait_for_peer_change(op);
		}
		op->previous_identity = op->active_identity;
		op->replacing_peer = 1;
		cancel_peer(op);
		clock_gettime(CLOCK_MONOTONIC, &deadline);
		deadline.tv_sec += op->timeout;
	}
done:
	cancel_peer(op);
	op->finished = 1;
	pthread_cond_broadcast(&op->changed);
	pthread_mutex_unlock(&op->lock);
	release(op);
	return NULL;
}

int fd_discovery_start(const struct fd_discovery_config *config,
	void (*callback)(void *, int), void *data, struct fd_discovery **operation)
{
	struct fd_discovery *op;
	pthread_t thread;
	size_t i;
	int ret;
	unsigned int supported = FD_DISCOVERY_TLS_TCP;
#if defined(ENABLE_DTLS_SCTP) && !defined(DISABLE_SCTP)
	supported |= FD_DISCOVERY_DTLS_SCTP;
#endif
	if (!operation)
		return EINVAL;
	*operation = NULL;
	if (!config || !callback || !config->realm || !*config->realm ||
	    !config->authorized_peers || !config->authorized_peer_count ||
	    !config->timeout_seconds || !config->transports ||
	    (config->transports & ~(FD_DISCOVERY_TLS_TCP | FD_DISCOVERY_DTLS_SCTP)))
		return EINVAL;
	if (config->transports & ~supported)
		return ENOTSUP;
	if (config->peer_options.ip_family > FD_DISCOVERY_IPV6 ||
	    config->peer_options.tc_timer < 0 || config->peer_options.tc_timer > INT_MAX / 200 ||
	    config->peer_options.tw_timer < 0 || config->peer_options.tw_timer > INT_MAX / 2 ||
	    (config->peer_options.peer_realm &&
	     !fd_os_is_valid_DiameterIdentity((os0_t)config->peer_options.peer_realm,
					      strlen(config->peer_options.peer_realm))))
		return EINVAL;
	op = calloc(1, sizeof(*op));
	if (!op)
		return ENOMEM;
	ret = pthread_mutex_init(&op->lock, NULL);
	if (ret) { free(op); return ret; }
	ret = pthread_cond_init(&op->changed, NULL);
	if (ret) { pthread_mutex_destroy(&op->lock); free(op); return ret; }
	op->refs = 1;
	op->realm = strdup(config->realm);
	op->identities = calloc(config->authorized_peer_count, sizeof(*op->identities));
	if (!op->realm || !op->identities) { ret = ENOMEM; goto fail; }
	op->peer_options = config->peer_options;
	op->peer_options.tls_priority = NULL;
	op->peer_options.peer_realm = NULL;
	if (config->peer_options.tls_priority) {
		op->peer_options.tls_priority = strdup(config->peer_options.tls_priority);
		if (!op->peer_options.tls_priority) { ret = ENOMEM; goto fail; }
	}
	if (config->peer_options.peer_realm) {
		op->peer_options.peer_realm = strdup(config->peer_options.peer_realm);
		if (!op->peer_options.peer_realm) { ret = ENOMEM; goto fail; }
	}
	for (i = 0; i < config->authorized_peer_count; i++) {
		if (!config->authorized_peers[i] || !*config->authorized_peers[i]) {
			ret = EINVAL;
			goto fail;
		}
		op->identities[i] = strdup(config->authorized_peers[i]);
		if (!op->identities[i]) { ret = ENOMEM; goto fail; }
		op->identity_count++;
	}
	op->application = config->application_id;
	op->transports = config->transports;
	op->timeout = config->timeout_seconds;
	op->callback = callback;
	op->data = data;
	clock_gettime(CLOCK_MONOTONIC, &op->initial_deadline);
	op->initial_deadline.tv_sec += op->timeout;
	pthread_mutex_lock(&registry_lock);
	if (shutting_down) {
		pthread_mutex_unlock(&registry_lock);
		ret = ESHUTDOWN;
		goto fail;
	}
	op->refs++;
	ret = pthread_create(&thread, NULL, discovery_worker, op);
	if (ret) {
		op->refs--;
		pthread_mutex_unlock(&registry_lock);
		goto fail;
	}
	op->next = operations;
	operations = op;
	*operation = op;
	pthread_mutex_unlock(&registry_lock);
	pthread_detach(thread);
	return 0;
fail:
	release(op);
	return ret;
}

int fd_discovery_stop(struct fd_discovery **operation)
{
	struct fd_discovery *op, **link;
	if (!operation || !*operation)
		return EINVAL;
	op = *operation;
	pthread_mutex_lock(&registry_lock);
	for (link = &operations; *link && *link != op; link = &(*link)->next)
		;
	if (!*link) {
		pthread_mutex_unlock(&registry_lock);
		return EINVAL;
	}
	pthread_mutex_unlock(&registry_lock);
	pthread_mutex_lock(&op->lock);
	op->stopping = 1;
	pthread_cond_broadcast(&op->changed);
	while (!op->finished)
		pthread_cond_wait(&op->changed, &op->lock);
	pthread_mutex_unlock(&op->lock);
	pthread_mutex_lock(&registry_lock);
	for (link = &operations; *link && *link != op; link = &(*link)->next)
		;
	if (*link)
		*link = op->next;
	pthread_mutex_unlock(&registry_lock);
	*operation = NULL;
	release(op);
	return 0;
}

int fd_discovery_has_peer(struct fd_discovery *op, const char *identity,
	size_t len, int *matched)
{
	struct timespec now;
	if (!op || !identity || !matched)
		return EINVAL;
	pthread_mutex_lock(&op->lock);
	clock_gettime(CLOCK_MONOTONIC, &now);
	*matched = !op->stopping && op->token && op->peer_event && op->peer_ok &&
		before(&now, &op->active_expiry) &&
		strlen(op->identities[op->active_identity]) == len &&
		!strncasecmp(op->identities[op->active_identity], identity, len) &&
		fd_peer_discovery_is_open(op->identities[op->active_identity], op->token);
	pthread_mutex_unlock(&op->lock);
	return 0;
}

static int score_discovered(void *data, struct msg **message, struct fd_list *candidates)
{
	struct fd_discovery **snapshot, *op;
	struct fd_list *li;
	struct msg_hdr *hdr;
	struct avp *avp;
	struct avp_hdr *ahdr;
	size_t count = 0, i = 0;
	(void)data;
	if (fd_msg_hdr(*message, &hdr) ||
	    fd_msg_browse(*message, MSG_BRW_FIRST_CHILD, &avp, NULL))
		return 0;
	while (avp) {
		if (fd_msg_avp_hdr(avp, &ahdr))
			return 0;
		if (!(ahdr->avp_flags & AVP_FLAG_VENDOR) && ahdr->avp_code == 283)
			break;
		if (fd_msg_browse(avp, MSG_BRW_NEXT, &avp, NULL))
			return 0;
	}
	if (!avp || fd_msg_parse_dict(avp, fd_g_config->cnf_dict, NULL) ||
	    fd_msg_avp_hdr(avp, &ahdr) || !ahdr->avp_value)
		return 0;
	/* Discovery is a fallback, never a replacement for an existing route. */
	for (li = candidates->next; li != candidates; li = li->next)
		if (((struct rtd_candidate *)li)->score >= 0)
			return 0;
	pthread_mutex_lock(&registry_lock);
	for (op = operations; op; op = op->next)
		if (op->application == hdr->msg_appl &&
		    strlen(op->realm) == ahdr->avp_value->os.len &&
		    !strncasecmp(op->realm, (const char *)ahdr->avp_value->os.data,
				ahdr->avp_value->os.len))
			count++;
	snapshot = count ? calloc(count, sizeof(*snapshot)) : NULL;
	if (snapshot)
		for (op = operations; op; op = op->next)
			if (op->application == hdr->msg_appl &&
			    strlen(op->realm) == ahdr->avp_value->os.len &&
			    !strncasecmp(op->realm, (const char *)ahdr->avp_value->os.data,
					ahdr->avp_value->os.len)) {
				op->refs++;
				snapshot[i++] = op;
			}
	pthread_mutex_unlock(&registry_lock);
	for (i = 0; snapshot && i < count; i++) {
		for (li = candidates->next; li != candidates; li = li->next) {
			struct rtd_candidate *candidate = (struct rtd_candidate *)li;
			int matched;
			if (candidate->score == FD_SCORE_INI &&
			    !fd_discovery_has_peer(snapshot[i], (char *)candidate->diamid,
				candidate->diamidlen, &matched) && matched)
				candidate->score += FD_SCORE_DEFAULT_REALM;
		}
		release(snapshot[i]);
	}
	free(snapshot);
	return 0;
}

int fd_discovery_routing_init(void)
{
	return fd_rt_out_register(score_discovered, NULL, 0, &route_handler);
}

void fd_discovery_shutdown(void)
{
	struct fd_discovery *op;
	pthread_mutex_lock(&registry_lock);
	shutting_down = 1;
	pthread_mutex_unlock(&registry_lock);
	if (route_handler) {
		fd_rt_out_unregister(route_handler, NULL);
		route_handler = NULL;
	}
	pthread_mutex_lock(&registry_lock);
	for (;;) {
		for (op = operations; op && op->shutdown_seen; op = op->next)
			;
		if (!op)
			break;
		op->shutdown_seen = 1;
		op->refs++;
		pthread_mutex_unlock(&registry_lock);
		pthread_mutex_lock(&op->lock);
		op->stopping = 1;
		pthread_cond_broadcast(&op->changed);
		while (!op->finished)
			pthread_cond_wait(&op->changed, &op->lock);
		pthread_mutex_unlock(&op->lock);
		release(op);
		pthread_mutex_lock(&registry_lock);
	}
	pthread_mutex_unlock(&registry_lock);
}
