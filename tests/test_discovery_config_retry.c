#include "tests.h"
#include <freeDiameter/discovery.h>
#include "../libfdcore/fdcore-internal.h"

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_wake = PTHREAD_COND_INITIALIZER;
static int starts, stops, callbacks;
static int expected_tc = 60;
static void (*completion)(void *, int);
static void *completion_data;

static int mock_start(const struct fd_discovery_config *config,
		      void (*callback)(void *, int), void *data,
		      struct fd_discovery **operation)
{
	int attempt;
	CHECK(0, strcmp(config->realm, "example.org"));
	CHECK(4, config->application_id);
	CHECK(FD_DISCOVERY_TLS_TCP, config->transports);
	CHECK(FD_DISCOVERY_IPV4, config->peer_options.ip_family);
	CHECK(expected_tc, config->peer_options.tc_timer);
	CHECK(45, config->peer_options.tw_timer);
	CHECK(0, strcmp("NORMAL", config->peer_options.tls_priority));
	CHECK(0, strcmp("origin.example.org", config->peer_options.peer_realm));
	CHECK(0, pthread_mutex_lock(&test_lock));
	attempt = ++starts;
	completion = callback;
	completion_data = data;
	*operation = (struct fd_discovery *)data;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	callback(data, attempt <= 2 ? EAGAIN : 0);
	CHECK(0, pthread_mutex_lock(&test_lock));
	callbacks++;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return 0;
}

static int mock_stop(struct fd_discovery **operation)
{
	CHECK(0, pthread_mutex_lock(&test_lock));
	stops++;
	*operation = NULL;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return 0;
}

#define fd_discovery_start mock_start
#define fd_discovery_stop mock_stop
#include "../libfdcore/discovery_config.c"
#undef fd_discovery_start
#undef fd_discovery_stop

static void wait_for(int *value, int expected)
{
	struct timespec deadline;
	CHECK(0, clock_gettime(CLOCK_REALTIME, &deadline));
	deadline.tv_sec += 5;
	CHECK(0, pthread_mutex_lock(&test_lock));
	while (*value < expected)
		CHECK(0, pthread_cond_timedwait(&test_wake, &test_lock, &deadline));
	CHECK(0, pthread_mutex_unlock(&test_lock));
}

int main(int argc, char **argv)
{
	struct discovery_entry *entry;
	struct timespec now;
	struct timespec pause = { 0, 100000000 };
	void (*callback)(void *, int);
	void *data;
	int i;
	const int suppress_errors[] = {
		FD_DISCOVERY_PEER_BUSY, FD_DISCOVERY_PEER_NOT_FRIEND
	};

	INIT_FD();
	CHECK(0, fd_disc_conf_begin("example.org"));
	CHECK(0, fd_disc_conf_application("4"));
	CHECK(0, fd_disc_conf_peer("peer.example.org"));
	CHECK(0, fd_disc_conf_family(FD_DISCOVERY_IPV4));
	CHECK(0, fd_disc_conf_transport(FD_DISCOVERY_TLS_TCP));
	CHECK(0, fd_disc_conf_timer(1, 60));
	CHECK(0, fd_disc_conf_timer(0, 45));
	CHECK(0, fd_disc_conf_priority("NORMAL"));
	CHECK(0, fd_disc_conf_peer_realm("origin.example.org"));
	CHECK(0, fd_disc_conf_end());
	CHECK(0, fd_disc_conf_start());
	wait_for(&starts, 1);
	wait_for(&stops, 1);
	CHECK(0, pthread_mutex_lock(&retry_lock));
	entry = entries.next->o;
	CHECK(0, clock_gettime(CLOCK_MONOTONIC, &now));
	CHECK(1, entry->retry_at.tv_sec - now.tv_sec >= 59);
	CHECK(1, entry->retry_at.tv_sec - now.tv_sec <= 66);
	entry->retry_at.tv_sec = 0;
	CHECK(0, pthread_cond_signal(&retry_wake));
	CHECK(0, pthread_mutex_unlock(&retry_lock));
	wait_for(&starts, 2);
	wait_for(&stops, 2);
	CHECK(0, pthread_mutex_lock(&retry_lock));
	CHECK(0, clock_gettime(CLOCK_MONOTONIC, &now));
	CHECK(1, entry->retry_at.tv_sec - now.tv_sec >= 119);
	CHECK(1, entry->retry_at.tv_sec - now.tv_sec <= 132);
	entry->retry_at.tv_sec = 0;
	CHECK(0, pthread_cond_signal(&retry_wake));
	CHECK(0, pthread_mutex_unlock(&retry_lock));
	wait_for(&starts, 3);
	wait_for(&callbacks, 3);

	CHECK(0, pthread_mutex_lock(&test_lock));
	callback = completion;
	data = completion_data;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	callback(data, ECONNRESET);
	wait_for(&stops, 3);
	CHECK(0, pthread_mutex_lock(&retry_lock));
	CHECK(0, clock_gettime(CLOCK_MONOTONIC, &now));
	CHECK(1, entry->retry_at.tv_sec - now.tv_sec >= 59);
	CHECK(1, entry->retry_at.tv_sec - now.tv_sec <= 66);
	entry->retry_at.tv_sec = 0;
	CHECK(0, pthread_cond_signal(&retry_wake));
	CHECK(0, pthread_mutex_unlock(&retry_lock));
	wait_for(&starts, 4);
	wait_for(&callbacks, 4);

	for (i = 0; i < 2; i++) {
		CHECK(0, pthread_mutex_lock(&test_lock));
		callback = completion;
		data = completion_data;
		CHECK(0, pthread_mutex_unlock(&test_lock));
		callback(data, suppress_errors[i]);
		wait_for(&stops, 4 + i);
		CHECK(0, pthread_mutex_lock(&retry_lock));
		entry = entries.next->o;
		CHECK(1, entry->suppressed);
		entry->retry_at.tv_sec = 0;
		CHECK(0, pthread_cond_signal(&retry_wake));
		CHECK(0, pthread_mutex_unlock(&retry_lock));
		nanosleep(&pause, NULL);
		CHECK(0, pthread_mutex_lock(&test_lock));
		CHECK(4 + i, starts);
		CHECK(0, pthread_mutex_unlock(&test_lock));
		CHECK(0, fd_disc_conf_fini());
		if (!i) {
			CHECK(0, fd_disc_conf_begin("example.org"));
			CHECK(0, fd_disc_conf_application("4"));
			CHECK(0, fd_disc_conf_peer("peer.example.org"));
			CHECK(0, fd_disc_conf_family(FD_DISCOVERY_IPV4));
			CHECK(0, fd_disc_conf_transport(FD_DISCOVERY_TLS_TCP));
			CHECK(0, fd_disc_conf_timer(1, 60));
			CHECK(0, fd_disc_conf_timer(0, 45));
			CHECK(0, fd_disc_conf_priority("NORMAL"));
			CHECK(0, fd_disc_conf_peer_realm("origin.example.org"));
			CHECK(0, fd_disc_conf_end());
			CHECK(0, fd_disc_conf_start());
			wait_for(&starts, 5);
			wait_for(&callbacks, 5);
		}
	}
	CHECK(5, starts);
	CHECK(5, stops);
	expected_tc = 1;
	CHECK(0, fd_disc_conf_begin("example.org"));
	CHECK(0, fd_disc_conf_application("4"));
	CHECK(0, fd_disc_conf_peer("peer.example.org"));
	CHECK(0, fd_disc_conf_family(FD_DISCOVERY_IPV4));
	CHECK(0, fd_disc_conf_transport(FD_DISCOVERY_TLS_TCP));
	CHECK(0, fd_disc_conf_timer(1, expected_tc));
	CHECK(0, fd_disc_conf_timer(0, 45));
	CHECK(0, fd_disc_conf_priority("NORMAL"));
	CHECK(0, fd_disc_conf_peer_realm("origin.example.org"));
	CHECK(0, fd_disc_conf_end());
	CHECK(0, fd_disc_conf_start());
	wait_for(&starts, 6);
	wait_for(&callbacks, 6);
	for (i = 0; i < 6; i++) {
		unsigned int delay = 1u << i;
		CHECK(0, pthread_mutex_lock(&test_lock));
		callback = completion;
		data = completion_data;
		CHECK(0, pthread_mutex_unlock(&test_lock));
		callback(data, ECONNRESET);
		wait_for(&stops, 6 + i);
		CHECK(0, pthread_mutex_lock(&retry_lock));
		entry = entries.next->o;
		CHECK(0, clock_gettime(CLOCK_MONOTONIC, &now));
		CHECK(1, entry->retry_at.tv_sec - now.tv_sec >= delay - 1);
		CHECK(1, entry->retry_at.tv_sec - now.tv_sec <= delay + delay / 10 + 1);
		entry->retry_at.tv_sec = 0;
		CHECK(0, pthread_cond_signal(&retry_wake));
		CHECK(0, pthread_mutex_unlock(&retry_lock));
		wait_for(&starts, 7 + i);
		wait_for(&callbacks, 7 + i);
		/* Keep the failure streak for this synthetic backoff test. */
		CHECK(0, pthread_mutex_lock(&retry_lock));
		entry->failures = i + 1;
		CHECK(0, pthread_mutex_unlock(&retry_lock));
	}
	CHECK(0, fd_disc_conf_fini());
	PASSTEST();
}
