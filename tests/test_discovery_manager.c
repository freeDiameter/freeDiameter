/* Standalone manager tests, linked with discovery.c and these fake backends. */
#include <freeDiameter/freeDiameter-host.h>
#include <freeDiameter/discovery.h>
#include "../libfdcore/discovery_dns.h"
#include "../libfdcore/discovery_peer.h"
#include "../libfdcore/discovery_internal.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); \
} } while (0)

struct fd_rt_out_hdl;
int fd_rt_out_unregister(struct fd_rt_out_hdl *handler, void **data)
{
	(void)handler;
	(void)data;
	return 0;
}

enum { INITIAL_FAIL, INITIAL_TIMEOUT, REFRESH_FAIL, REFRESH_TIMEOUT, HEALTHY, LONG_TTL, FALLBACK, DPR_EARLY, DPR_NOT_FRIEND, DPR_EXPIRED, OPTIONS };
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static int scenario, queries, adds, cancels, callbacks, errors[2];
static int block_callback, release_callback, callback_finished, stop_finished;
static int retiring;
static int options_ready;
static uint64_t open_token;
static uint64_t cb_token;
static void (*peer_cb)(int, void *);
static void *peer_cb_data;

static void wait_for(int *value, int target, int seconds)
{
	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += seconds;
	pthread_mutex_lock(&lock);
	while (*value < target)
		CHECK(pthread_cond_timedwait(&changed, &lock, &deadline) == 0);
	pthread_mutex_unlock(&lock);
}

static void pause_ms(long ms)
{
	struct timespec delay = { ms / 1000, (ms % 1000) * 1000000L };
	while (nanosleep(&delay, &delay) && errno == EINTR)
		;
}

static void reset(int mode)
{
	pthread_mutex_lock(&lock);
	scenario = mode;
	queries = adds = cancels = callbacks = 0;
	errors[0] = errors[1] = 0;
	open_token = 0;
	cb_token = 0;
	peer_cb = NULL;
	peer_cb_data = NULL;
	retiring = 0;
	options_ready = 0;
	block_callback = release_callback = callback_finished = stop_finished = 0;
	pthread_mutex_unlock(&lock);
}

enum fd_disc_dns_status fd_disc_dns_resolve(const char *realm,
	const struct fd_disc_dns_options *options, struct fd_disc_dns_result **result)
{
	int n, mode;
	CHECK(!strcmp(realm, "example.net"));
	CHECK(options->protocols == FD_DISC_DNS_TLS_TCP);
	pthread_mutex_lock(&lock);
	n = ++queries;
	mode = scenario;
	pthread_cond_broadcast(&changed);
	while (mode == OPTIONS && !options_ready)
		pthread_cond_wait(&changed, &lock);
	pthread_mutex_unlock(&lock);
	*result = NULL;
	if (mode == INITIAL_TIMEOUT || (mode == REFRESH_TIMEOUT && n > 1)) {
		while (!options->cancelled(options->cancel_context))
			pause_ms(20);
		return FD_DISC_DNS_CANCELLED;
	}
	if (mode == INITIAL_FAIL || (mode == REFRESH_FAIL && n > 1))
		return FD_DISC_DNS_TEMPORARY;
	*result = calloc(1, sizeof(**result));
	CHECK(*result != NULL);
	(*result)->count = mode == FALLBACK || mode == DPR_EARLY || mode == DPR_NOT_FRIEND ? 2 : 1;
	strcpy((*result)->candidates[0].target, "first.example.net");
	(*result)->candidates[0].protocol = FD_DISC_DNS_TLS_TCP;
	(*result)->candidates[0].ttl = mode == LONG_TTL || mode == FALLBACK ? 8 : 1;
	(*result)->candidates[1] = (*result)->candidates[0];
	strcpy((*result)->candidates[1].target, "second.example.net");
	return FD_DISC_DNS_OK;
}

int fd_peer_add_discovered(const struct fd_discovery_peer *peer,
	void (*cb)(int, void *), void *data)
{
	int fail, early;
	CHECK(!strcmp(peer->identity, "peer.example.net") ||
	      !strcmp(peer->identity, "other.example.net"));
	CHECK(!strcmp(peer->realm, "example.net"));
	CHECK(peer->token != 0);
	if (scenario == OPTIONS) {
		CHECK(peer->options.ip_family == FD_DISCOVERY_IPV4);
		CHECK(peer->options.tc_timer == 60 && peer->options.tw_timer == 45);
		CHECK(!strcmp(peer->options.tls_priority, "NORMAL"));
		CHECK(!strcmp(peer->options.peer_realm, "origin.example.net"));
	}
	pthread_mutex_lock(&lock);
	if (retiring) {
		retiring--;
		pthread_mutex_unlock(&lock);
		return EEXIST;
	}
	adds++;
	fail = scenario == FALLBACK && adds == 1;
	early = scenario == DPR_EARLY || scenario == DPR_NOT_FRIEND || scenario == DPR_EXPIRED;
	if (!fail) {
		open_token = peer->token;
		cb_token = peer->token;
		peer_cb = cb;
		peer_cb_data = data;
	}
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	cb(fail ? ECONNRESET : 0, data);
	if (early) {
		if (scenario == DPR_EXPIRED)
			pause_ms(1100);
		pthread_mutex_lock(&lock);
		peer_cb = NULL;
		peer_cb_data = NULL;
		cb_token = 0;
		open_token = 0;
		pthread_mutex_unlock(&lock);
		cb(scenario == DPR_NOT_FRIEND ? FD_DISCOVERY_PEER_NOT_FRIEND :
		   FD_DISCOVERY_PEER_BUSY, data);
	}
	return 0;
}

int fd_peer_discovery_cancel(const char *identity, uint64_t token)
{
	void (*cb)(int, void *) = NULL;
	void *data = NULL;
	CHECK(!strcmp(identity, "peer.example.net") || !strcmp(identity, "other.example.net"));
	CHECK(token != 0);
	pthread_mutex_lock(&lock);
	cancels++;
	if (cb_token == token) {
		open_token = 0;
		cb = peer_cb;
		data = peer_cb_data;
		peer_cb = NULL;
		peer_cb_data = NULL;
		cb_token = 0;
		if (scenario == HEALTHY)
			retiring = 2;
	}
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	if (cb)
		cb(ECONNRESET, data);
	return 0;
}

static void remote_disconnect(int status)
{
	void (*cb)(int, void *);
	void *data;
	pthread_mutex_lock(&lock);
	cb = peer_cb;
	data = peer_cb_data;
	CHECK(cb != NULL);
	peer_cb = NULL;
	peer_cb_data = NULL;
	cb_token = 0;
	pthread_mutex_unlock(&lock);
	cb(status, data);
	pthread_mutex_lock(&lock);
	open_token = 0;
	pthread_mutex_unlock(&lock);
}

int fd_peer_discovery_is_open(const char *identity, uint64_t token)
{
	int open;
	CHECK(!strcmp(identity, "peer.example.net"));
	pthread_mutex_lock(&lock);
	open = token == open_token;
	pthread_mutex_unlock(&lock);
	return open;
}

static void callback(void *data, int error)
{
	CHECK(data == &callbacks);
	pthread_mutex_lock(&lock);
	CHECK(callbacks < 2);
	errors[callbacks++] = error;
	pthread_cond_broadcast(&changed);
	while (block_callback && !release_callback)
		pthread_cond_wait(&changed, &lock);
	callback_finished++;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
}

static void *stop_thread(void *arg)
{
	struct fd_discovery **op = arg;
	CHECK(fd_discovery_stop(op) == 0);
	pthread_mutex_lock(&lock);
	stop_finished = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	return NULL;
}

static void *shutdown_thread(void *arg)
{
	(void)arg;
	fd_discovery_shutdown();
	pthread_mutex_lock(&lock);
	stop_finished = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	return NULL;
}

int main(void)
{
	char realm[] = "example.net", identity[] = "peer.example.net";
	const char *identities[] = { identity, "other.example.net" };
	struct fd_discovery_config config = { realm, 0, identities, 1, FD_DISCOVERY_TLS_TCP, 2, { 0 } };
	struct fd_discovery *op = NULL;
	pthread_t thread;
	int matched;

	{
		char priority[] = "NORMAL", peer_realm[] = "origin.example.net";
		config.peer_options.ip_family = FD_DISCOVERY_IPV4;
		config.peer_options.tc_timer = 60;
		config.peer_options.tw_timer = 45;
		config.peer_options.tls_priority = priority;
		config.peer_options.peer_realm = peer_realm;
		reset(OPTIONS);
		CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
		wait_for(&queries, 1, 3);
		memset(priority, 'x', sizeof(priority) - 1);
		memset(peer_realm, 'x', sizeof(peer_realm) - 1);
		pthread_mutex_lock(&lock);
		options_ready = 1;
		pthread_cond_broadcast(&changed);
		pthread_mutex_unlock(&lock);
		wait_for(&callbacks, 1, 3);
		CHECK(errors[0] == 0);
		CHECK(fd_discovery_stop(&op) == 0 && !op);
		memset(&config.peer_options, 0, sizeof(config.peer_options));
		config.peer_options.ip_family = 3;
		CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == EINVAL && !op);
		config.peer_options.ip_family = 0;
		config.peer_options.tc_timer = -1;
		CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == EINVAL && !op);
		memset(&config.peer_options, 0, sizeof(config.peer_options));
	}

	reset(INITIAL_FAIL);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 1, 3);
	pause_ms(1300);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == EAGAIN && queries == 1 && adds == 0 && callbacks == 1);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);

	reset(INITIAL_TIMEOUT);
	config.timeout_seconds = 1;
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 1, 3);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == ETIMEDOUT && queries == 1);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);

	reset(INITIAL_TIMEOUT);
	config.timeout_seconds = 2;
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&queries, 1, 3);
	CHECK(fd_discovery_stop(&op) == 0 && !op);
	pthread_mutex_lock(&lock);
	CHECK(callbacks == 0 && queries == 1);
	pthread_mutex_unlock(&lock);

	reset(FALLBACK);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	memset(realm, 'x', sizeof(realm) - 1);
	memset(identity, 'y', sizeof(identity) - 1);
	wait_for(&callbacks, 1, 3);
	CHECK(fd_discovery_has_peer(op, "PEER.EXAMPLE.NET", 16, &matched) == 0 && matched);
	CHECK(fd_discovery_has_peer(op, "other.example.net", 17, &matched) == 0 && !matched);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == 0 && adds == 2);
	open_token = 0; /* Unexpected disconnect, well before the eight-second TTL. */
	pthread_mutex_unlock(&lock);
	wait_for(&callbacks, 2, 2);
	CHECK(fd_discovery_has_peer(op, "peer.example.net", 16, &matched) == 0 && !matched);
	pause_ms(1300);
	pthread_mutex_lock(&lock);
	CHECK(errors[1] != 0 && queries == 1 && adds == 2 && callbacks == 2);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);
	strcpy(realm, "example.net");
	strcpy(identity, "peer.example.net");

	{
		int statuses[] = { FD_DISCOVERY_PEER_BUSY, FD_DISCOVERY_PEER_NOT_FRIEND, ECONNRESET };
		size_t i;
		for (i = 0; i < sizeof(statuses) / sizeof(statuses[0]); i++) {
			reset(LONG_TTL);
			CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
			wait_for(&callbacks, 1, 3);
			remote_disconnect(statuses[i]);
			wait_for(&callbacks, 2, 2);
			CHECK(fd_discovery_has_peer(op, identity, strlen(identity), &matched) == 0 && !matched);
			pthread_mutex_lock(&lock);
			CHECK(errors[0] == 0 && errors[1] == statuses[i] && queries == 1 && callbacks == 2);
			pthread_mutex_unlock(&lock);
			CHECK(fd_discovery_stop(&op) == 0 && !op);
		}
	}

	{
		int modes[] = { DPR_EARLY, DPR_NOT_FRIEND };
		int statuses[] = { FD_DISCOVERY_PEER_BUSY, FD_DISCOVERY_PEER_NOT_FRIEND };
		size_t i;
		config.authorized_peer_count = 2;
		for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
			reset(modes[i]);
			CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
			wait_for(&callbacks, 1, 3);
			pthread_mutex_lock(&lock);
			CHECK(errors[0] == statuses[i] && callbacks == 1 && adds == 1 && queries == 1);
			pthread_mutex_unlock(&lock);
			CHECK(fd_discovery_stop(&op) == 0 && !op);
		}
		config.authorized_peer_count = 1;
	}

	reset(DPR_EXPIRED);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 1, 3);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == ETIMEDOUT && callbacks == 1 && adds == 1);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);

	reset(REFRESH_FAIL);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 2, 4);
	CHECK(fd_discovery_has_peer(op, identity, strlen(identity), &matched) == 0 && !matched);
	pause_ms(1300);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == 0 && errors[1] == EAGAIN && queries == 2 && callbacks == 2);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);

	reset(REFRESH_TIMEOUT);
	config.timeout_seconds = 1;
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 2, 4);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == 0 && errors[1] == ETIMEDOUT && queries == 2);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);

	reset(HEALTHY);
	config.timeout_seconds = 2;
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&adds, 2, 4);
	wait_for(&callback_finished, 1, 2);
	CHECK(fd_discovery_has_peer(op, identity, strlen(identity), &matched) == 0 && matched);
	pthread_mutex_lock(&lock);
	CHECK(errors[0] == 0 && callbacks == 1 && queries >= 2);
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_stop(&op) == 0 && !op);
	pause_ms(1200);
	pthread_mutex_lock(&lock);
	CHECK(callbacks == 1);
	pthread_mutex_unlock(&lock);

	reset(LONG_TTL);
	pthread_mutex_lock(&lock);
	block_callback = 1;
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 1, 3);
	CHECK(pthread_create(&thread, NULL, stop_thread, &op) == 0);
	pause_ms(200);
	pthread_mutex_lock(&lock);
	CHECK(!stop_finished);
	release_callback = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	CHECK(pthread_join(thread, NULL) == 0 && !op);
	pause_ms(200);
	pthread_mutex_lock(&lock);
	CHECK(callbacks == 1 && callback_finished == 1);
	pthread_mutex_unlock(&lock);

	reset(LONG_TTL);
	pthread_mutex_lock(&lock);
	block_callback = 1;
	pthread_mutex_unlock(&lock);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == 0);
	wait_for(&callbacks, 1, 3);
	CHECK(pthread_create(&thread, NULL, shutdown_thread, NULL) == 0);
	pause_ms(200);
	pthread_mutex_lock(&lock);
	CHECK(!stop_finished);
	release_callback = 1;
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&lock);
	CHECK(pthread_join(thread, NULL) == 0);
	CHECK(fd_discovery_stop(&op) == 0 && !op);
	CHECK(fd_discovery_start(&config, callback, &callbacks, &op) == ESHUTDOWN);
	return 0;
}
