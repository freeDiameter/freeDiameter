/* SPDX-License-Identifier: BSD-3-Clause */
#include "fdcore-internal.h"
#include "discovery_config.h"
#include <freeDiameter/discovery.h>
#include <limits.h>

#define DISC_RETRY_MAX 300

struct discovery_entry {
	struct fd_list chain;
	char *realm;
	char **peers;
	size_t peer_count;
	uint32_t application;
	int has_application;
	struct fd_discovery_peer_options options;
	unsigned int transport;
	struct fd_discovery *operation;
	int event;
	int error;
	int suppressed;
	unsigned int failures;
	struct timespec retry_at;
};

static struct fd_list entries = FD_LIST_INITIALIZER(entries);
static struct discovery_entry *current;
static pthread_mutex_t retry_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t retry_wake = PTHREAD_COND_INITIALIZER;
static pthread_t retry_thread;
static int retry_running, retry_stopping;

int fd_disc_conf_begin(const char *realm)
{
	struct discovery_entry *entry;
	CHECK_PARAMS(!current && realm &&
		     fd_os_is_valid_DiameterIdentity((os0_t)realm, strlen(realm)));
	CHECK_MALLOC(entry = calloc(1, sizeof(*entry)));
	entry->realm = strdup(realm);
	if (!entry->realm) {
		free(entry);
		return ENOMEM;
	}
	fd_list_init(&entry->chain, entry);
	fd_list_insert_before(&entries, &entry->chain);
	current = entry;
	return 0;
}

int fd_disc_conf_application(const char *application)
{
	uint32_t value = 0;
	const unsigned char *p = (const unsigned char *)application;
	CHECK_PARAMS(current && !current->has_application && p && *p);
	for (; *p; p++) {
		unsigned int digit;
		if (*p < '0' || *p > '9')
			return EINVAL;
		digit = *p - '0';
		if (value > (UINT32_MAX - digit) / 10)
			return EINVAL;
		value = value * 10 + digit;
	}
	current->application = value;
	current->has_application = 1;
	return 0;
}

int fd_disc_conf_peer(const char *identity)
{
	char **peers, *copy;
	size_t i;
	CHECK_PARAMS(current && identity &&
		     fd_os_is_valid_DiameterIdentity((os0_t)identity, strlen(identity)));
	for (i = 0; i < current->peer_count; i++)
		if (!strcasecmp(current->peers[i], identity))
			return EEXIST;
	CHECK_MALLOC(copy = strdup(identity));
	peers = realloc(current->peers, (current->peer_count + 1) * sizeof(*peers));
	if (!peers) {
		free(copy);
		return ENOMEM;
	}
	current->peers = peers;
	current->peers[current->peer_count++] = copy;
	return 0;
}

int fd_disc_conf_family(unsigned int family)
{
	CHECK_PARAMS(current && (family == FD_DISCOVERY_IPV4 || family == FD_DISCOVERY_IPV6));
	CHECK_PARAMS(!current->options.ip_family || current->options.ip_family == family);
	current->options.ip_family = family;
	return 0;
}

int fd_disc_conf_transport(unsigned int transport)
{
	CHECK_PARAMS(current && (transport == FD_DISCOVERY_TLS_TCP || transport == FD_DISCOVERY_DTLS_SCTP));
	CHECK_PARAMS(!current->transport || current->transport == transport);
	current->transport = transport;
	return 0;
}

int fd_disc_conf_timer(int tc, int value)
{
	CHECK_PARAMS(current && value >= 0 && value <= (tc ? INT_MAX / 200 : INT_MAX / 2));
	if (tc)
		current->options.tc_timer = value;
	else
		current->options.tw_timer = value;
	return 0;
}

int fd_disc_conf_priority(const char *priority)
{
	CHECK_PARAMS(current && priority && !current->options.tls_priority);
	CHECK_MALLOC(current->options.tls_priority = strdup(priority));
	return 0;
}

int fd_disc_conf_peer_realm(const char *realm)
{
	CHECK_PARAMS(current && realm && !current->options.peer_realm &&
		     fd_os_is_valid_DiameterIdentity((os0_t)realm, strlen(realm)));
	CHECK_MALLOC(current->options.peer_realm = strdup(realm));
	return 0;
}

int fd_disc_conf_end(void)
{
	struct fd_list *item;
	CHECK_PARAMS(current && current->has_application && current->peer_count);
	for (item = entries.next; item != &current->chain; item = item->next) {
		struct discovery_entry *other = item->o;
		if (other->application == current->application &&
		    !strcasecmp(other->realm, current->realm))
			return EEXIST;
	}
	current = NULL;
	return 0;
}

static void discovery_complete(void *data, int error)
{
	struct discovery_entry *entry = data;
	pthread_mutex_lock(&retry_lock);
	entry->event = 1;
	entry->error = error;
	if (!error)
		entry->failures = 0;
	pthread_cond_signal(&retry_wake);
	pthread_mutex_unlock(&retry_lock);
}

static int start_entry(struct discovery_entry *entry)
{
	struct fd_discovery_config config;
	memset(&config, 0, sizeof(config));
	config.realm = entry->realm;
	config.application_id = entry->application;
	config.authorized_peers = (const char *const *)entry->peers;
	config.authorized_peer_count = entry->peer_count;
	config.peer_options = entry->options;
	if (!fd_g_config->cnf_flags.no_tcp)
		config.transports |= FD_DISCOVERY_TLS_TCP;
#ifdef ENABLE_DTLS_SCTP
	if (!fd_g_config->cnf_flags.no_sctp)
		config.transports |= FD_DISCOVERY_DTLS_SCTP;
#endif
	if (entry->transport)
		config.transports &= entry->transport;
	config.timeout_seconds = 30;
	return fd_discovery_start(&config, discovery_complete, entry, &entry->operation);
}

static void *retry_main(void *unused)
{
	unsigned int seed;
	struct timespec now;
	(void)unused;
	clock_gettime(CLOCK_MONOTONIC, &now);
	seed = (unsigned int)now.tv_nsec ^ (unsigned int)now.tv_sec ^ (unsigned int)getpid();
	pthread_mutex_lock(&retry_lock);
	while (!retry_stopping) {
		struct fd_list *item;
		struct discovery_entry *action = NULL;
		int failed = 0;
		clock_gettime(CLOCK_MONOTONIC, &now);
		for (item = entries.next; item != &entries; item = item->next) {
			struct discovery_entry *entry = item->o;
			if (entry->event) {
				entry->event = 0;
				if (entry->error) {
					action = entry;
					failed = 1;
					break;
				}
				LOG_N("Startup discovery for %s application %u succeeded",
				      entry->realm, entry->application);
			}
			if (!entry->operation && !entry->suppressed &&
			    (now.tv_sec > entry->retry_at.tv_sec ||
			     (now.tv_sec == entry->retry_at.tv_sec &&
			      now.tv_nsec >= entry->retry_at.tv_nsec))) {
				action = entry;
				break;
			}
		}
		if (action && failed) {
			struct fd_discovery *op = action->operation;
			unsigned int delay = action->options.tc_timer ?
				action->options.tc_timer : fd_g_config->cnf_timer_tc;
			unsigned int maximum = delay > DISC_RETRY_MAX ? delay : DISC_RETRY_MAX;
			unsigned int i;
			int error = action->error, ret = 0;
			int suppress = error == FD_DISCOVERY_PEER_BUSY ||
				error == FD_DISCOVERY_PEER_NOT_FRIEND;
			action->operation = NULL;
			action->suppressed = suppress;
			if (!suppress) {
				for (i = 0; i < action->failures && delay < maximum; i++)
					delay = delay > maximum / 2 ? maximum : delay * 2;
				if (action->failures < 32)
					action->failures++;
				delay += rand_r(&seed) % (delay / 10 + 1);
				if (delay > maximum)
					delay = maximum;
				clock_gettime(CLOCK_MONOTONIC, &action->retry_at);
				action->retry_at.tv_sec += delay;
			}
			pthread_mutex_unlock(&retry_lock);
			if (suppress)
				LOG_E("Startup discovery for %s application %u: peer sent %s; "
				      "automatic retry suppressed",
				      action->realm, action->application,
				      error == FD_DISCOVERY_PEER_BUSY ? "BUSY" : "DO_NOT_WANT_TO_TALK_TO_YOU");
			else
				LOG_E("Startup discovery for %s application %u failed: %s; retrying in %u seconds",
				      action->realm, action->application, strerror(error), delay);
			if (op)
				ret = fd_discovery_stop(&op);
			if (ret)
				LOG_E("Cannot stop discovery for %s: %s", action->realm, strerror(ret));
			pthread_mutex_lock(&retry_lock);
			if (ret) {
				action->operation = op;
				retry_stopping = 1;
			}
		} else if (action) {
			int ret;
			pthread_mutex_unlock(&retry_lock);
			ret = start_entry(action);
			pthread_mutex_lock(&retry_lock);
			if (ret) {
				action->event = 1;
				action->error = ret;
			}
		} else {
			struct timespec until;
			clock_gettime(CLOCK_REALTIME, &until);
			until.tv_sec++;
			pthread_cond_timedwait(&retry_wake, &retry_lock, &until);
		}
	}
	pthread_mutex_unlock(&retry_lock);
	return NULL;
}

int fd_disc_conf_start(void)
{
	int ret;
	CHECK_PARAMS(!current && !retry_running);
	if (FD_IS_LIST_EMPTY(&entries))
		return 0;
	retry_stopping = 0;
	ret = pthread_create(&retry_thread, NULL, retry_main, NULL);
	if (ret)
		return ret;
	retry_running = 1;
	return 0;
}

int fd_disc_conf_fini(void)
{
	int result = 0;
	if (retry_running) {
		pthread_mutex_lock(&retry_lock);
		retry_stopping = 1;
		pthread_cond_signal(&retry_wake);
		pthread_mutex_unlock(&retry_lock);
		result = pthread_join(retry_thread, NULL);
		if (result)
			return result;
		retry_running = 0;
	}
	while (!FD_IS_LIST_EMPTY(&entries)) {
		struct discovery_entry *entry = entries.next->o;
		size_t i;
		int ret;
		if (entry->operation) {
			ret = fd_discovery_stop(&entry->operation);
			if (ret) {
				LOG_E("Cannot stop discovery for %s: %s", entry->realm, strerror(ret));
				if (!result)
					result = ret;
				break;
			}
		}
		fd_list_unlink(&entry->chain);
		for (i = 0; i < entry->peer_count; i++)
			free(entry->peers[i]);
		free(entry->peers);
		free(entry->realm);
		free((char *)entry->options.tls_priority);
		free((char *)entry->options.peer_realm);
		free(entry);
	}
	if (FD_IS_LIST_EMPTY(&entries))
		current = NULL;
	return result;
}
