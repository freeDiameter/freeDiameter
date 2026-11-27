#include "tests.h"
#include "../extensions/rt_discover/rt_discover.h"
#include <freeDiameter/discovery.h>

static pthread_mutex_t test_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t test_wake = PTHREAD_COND_INITIALIZER;
static void (*completion[8])(void *, int);
static void *completion_data[8];
static int starts, stops, resumed, retries, immediate, fail_start, peer_available = 1, checks;
static int starts_for[8];
static int hold_stop, stop_entered, stop_failures;
static int fail_create, failed_creates, fail_join, failed_joins;
static time_t fake_now = 1000;

static int mock_pthread_create(pthread_t *thread, const pthread_attr_t *attr,
			       void *(*fn)(void *), void *data)
{
	int fail;
	CHECK(0, pthread_mutex_lock(&test_lock));
	fail = fail_create;
	if (fail) {
		fail_create = 0;
		failed_creates++;
		CHECK(0, pthread_cond_broadcast(&test_wake));
	}
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return fail ? EAGAIN : pthread_create(thread, attr, fn, data);
}

static int mock_pthread_join(pthread_t thread, void **result)
{
	int fail;
	CHECK(0, pthread_mutex_lock(&test_lock));
	fail = fail_join;
	if (fail) {
		fail_join = 0;
		failed_joins++;
		CHECK(0, pthread_cond_broadcast(&test_wake));
	}
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return fail ? EINVAL : pthread_join(thread, result);
}

static int mock_clock_gettime(clockid_t clock, struct timespec *value)
{
	if (clock != CLOCK_MONOTONIC)
		return clock_gettime(clock, value);
	CHECK(0, pthread_mutex_lock(&test_lock));
	value->tv_sec = fake_now;
	value->tv_nsec = 0;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return 0;
}

static int mock_start(const struct fd_discovery_config *config,
		      void (*cb)(void *, int), void *data, struct fd_discovery **op)
{
	int sync, fail;
	CHECK(0, strcmp(config->realm, "example.org"));
	CHECK(1, config->authorized_peer_count);
	CHECK(0, strcmp(config->authorized_peers[0], "peer.example.net"));
	CHECK(FD_DISCOVERY_TLS_TCP, config->transports);
	CHECK(FD_DISCOVERY_IPV4, config->peer_options.ip_family);
	CHECK(60, config->peer_options.tc_timer);
	CHECK(45, config->peer_options.tw_timer);
	CHECK(0, strcmp("NORMAL", config->peer_options.tls_priority));
	CHECK(0, strcmp("origin.example.org", config->peer_options.peer_realm));
	CHECK(1, config->application_id < 8);
	CHECK(0, pthread_mutex_lock(&test_lock));
	starts++;
	starts_for[config->application_id]++;
	completion[config->application_id] = cb;
	completion_data[config->application_id] = data;
	sync = immediate;
	fail = fail_start;
	if (!fail)
		*op = (struct fd_discovery *)data;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	if (fail)
		return EIO;
	if (sync)
		cb(data, sync == 1 ? 0 : sync);
	return 0;
}

static int mock_stop(struct fd_discovery **op)
{
	/* A callback racing with stop must not revive the failed operation. */
	void *data = *op;
	completion[1](data, EIO);
	CHECK(0, pthread_mutex_lock(&test_lock));
	stop_entered++;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	while (hold_stop)
		CHECK(0, pthread_cond_wait(&test_wake, &test_lock));
	if (stop_failures) {
		stop_failures--;
		CHECK(0, pthread_cond_broadcast(&test_wake));
		CHECK(0, pthread_mutex_unlock(&test_lock));
		return EIO;
	}
	*op = NULL;
	stops++;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return 0;
}

static int mock_has_peer(struct fd_discovery *op, const char *identity,
			 size_t len, int *matched)
{
	(void)op;
	CHECK(0, strncmp(identity, "peer.example.net", len));
	CHECK(0, pthread_mutex_lock(&test_lock));
	checks++;
	*matched = peer_available;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return 0;
}

static int mock_resume(struct msg **msg, int retry)
{
	CHECK(0, fd_msg_free(*msg));
	*msg = NULL;
	CHECK(0, pthread_mutex_lock(&test_lock));
	resumed++;
	retries += retry;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	return 0;
}

#define clock_gettime mock_clock_gettime
#define fd_discovery_start mock_start
#define fd_discovery_stop mock_stop
#define fd_discovery_has_peer mock_has_peer
#define fd_rt_out_recovery_resume mock_resume
#define pthread_create mock_pthread_create
#define pthread_join mock_pthread_join
#include "../extensions/rt_discover/rt_discover.c"
#undef clock_gettime
#undef fd_discovery_start
#undef fd_discovery_stop
#undef fd_discovery_has_peer
#undef fd_rt_out_recovery_resume
#undef pthread_create
#undef pthread_join

int rd_config_load(const char *path, struct fd_list *list)
{
	(void)path;
	(void)list;
	return 0;
}

void rd_config_free(struct fd_list *list)
{
	(void)list;
}

static struct msg *request(const char *realm, uint32_t application)
{
	struct msg *msg;
	struct msg_hdr *hdr;
	struct avp *avp;
	union avp_value value;
	CHECK(0, fd_msg_new(NULL, 0, &msg));
	CHECK(0, fd_msg_hdr(msg, &hdr));
	hdr->msg_flags = CMD_FLAG_REQUEST;
	hdr->msg_appl = application;
	CHECK(0, fd_msg_avp_new(destination_realm, 0, &avp));
	value.os.data = (uint8_t *)realm;
	value.os.len = strlen(realm);
	CHECK(0, fd_msg_avp_setvalue(avp, &value));
	CHECK(0, fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp));
	return msg;
}

static void enqueue(uint32_t application)
{
	struct msg *msg = request("EXAMPLE.ORG", application);
	CHECK(0, recover(NULL, &msg));
	CHECK(1, msg == NULL);
}

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

static void advance_to(time_t now)
{
	CHECK(0, pthread_mutex_lock(&test_lock));
	fake_now = now;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	CHECK(0, pthread_mutex_lock(&lock));
	CHECK(0, pthread_cond_signal(&wake));
	CHECK(0, pthread_mutex_unlock(&lock));
}

static time_t wait_backoff(uint32_t application, unsigned int base)
{
	int i;
	for (i = 0; i < 5000; i++) {
		struct fd_list *li;
		struct timespec pause = { 0, 1000000 };
		time_t due = 0, now;
		CHECK(0, pthread_mutex_lock(&lock));
		for (li = operations.next; li != &operations; li = li->next) {
			struct rd_operation *op = li->o;
			if (op->application == application && op->state == RD_BACKOFF) {
				CHECK(base, op->retry_delay);
				due = op->retry_at.tv_sec;
				break;
			}
		}
		CHECK(0, pthread_mutex_unlock(&lock));
		if (due) {
			CHECK(0, pthread_mutex_lock(&test_lock));
			now = fake_now;
			CHECK(0, pthread_mutex_unlock(&test_lock));
			CHECK(1, due >= now + base);
			CHECK(1, due <= now + (base + base / 4 > RD_RETRY_MAX ?
					      RD_RETRY_MAX : base + base / 4));
			return due;
		}
		nanosleep(&pause, NULL);
	}
	CHECK(1, 0);
	return 0;
}

static void complete(unsigned int app, int error)
{
	void (*cb)(void *, int);
	void *data;
	CHECK(0, pthread_mutex_lock(&test_lock));
	cb = completion[app];
	data = completion_data[app];
	CHECK(0, pthread_mutex_unlock(&test_lock));
	CHECK(1, cb != NULL);
	cb(data, error);
}

static time_t wait_stop_retry(unsigned int app)
{
	int i;
	for (i = 0; i < 5000; i++) {
		struct fd_list *li;
		struct timespec pause = { 0, 1000000 };
		time_t due = 0;
		CHECK(0, pthread_mutex_lock(&lock));
		for (li = operations.next; li != &operations; li = li->next) {
			struct rd_operation *op = li->o;
			if (op->application == app && op->state == RD_STOPPING &&
			    op->stop_result && !op->stop_started && !op->stop_done) {
				CHECK(1, op->discovery != NULL);
				due = op->stop_retry_at.tv_sec;
				break;
			}
		}
		CHECK(0, pthread_mutex_unlock(&lock));
		if (due)
			return due;
		nanosleep(&pause, NULL);
	}
	CHECK(1, 0);
	return 0;
}

static time_t wait_join_retry(unsigned int app)
{
	int i;
	for (i = 0; i < 5000; i++) {
		struct fd_list *li;
		struct timespec pause = { 0, 1000000 };
		time_t due = 0;
		CHECK(0, pthread_mutex_lock(&lock));
		for (li = operations.next; li != &operations; li = li->next) {
			struct rd_operation *op = li->o;
			if (op->application == app && op->state == RD_STOPPING &&
			    op->stop_done && op->stop_started &&
			    op->stop_retry_at.tv_sec > 1015) {
				CHECK(1, op->discovery == NULL);
				due = op->stop_retry_at.tv_sec;
				break;
			}
		}
		CHECK(0, pthread_mutex_unlock(&lock));
		if (due)
			return due;
		nanosleep(&pause, NULL);
	}
	CHECK(1, 0);
	return 0;
}

static void wait_suppressed(unsigned int app)
{
	int i;
	for (i = 0; i < 5000; i++) {
		struct fd_list *li;
		struct timespec pause = { 0, 1000000 };
		int found = 0;
		CHECK(0, pthread_mutex_lock(&lock));
		for (li = operations.next; li != &operations; li = li->next) {
			struct rd_operation *op = li->o;
			if (op->application == app && op->state == RD_SUPPRESSED) {
				CHECK(1, op->discovery == NULL);
				found = 1;
				break;
			}
		}
		CHECK(0, pthread_mutex_unlock(&lock));
		if (found)
			return;
		nanosleep(&pause, NULL);
	}
	CHECK(1, 0);
}

int main(int argc, char **argv)
{
	struct rd_realm realm;
	struct rd_allowed_peer peer;
	struct msg *other;
	time_t due;
	unsigned int delays[] = { 240, 300, 300, 300 };
	int i, check_target, stop_target;

	INIT_FD();
	CHECK(0, fd_msg_init());
	CHECK(0, fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME,
			       "Destination-Realm", &destination_realm, ENOENT));
	memset(&realm, 0, sizeof(realm));
	memset(&peer, 0, sizeof(peer));
	realm.name = "example.org";
	{
		struct rd_operation op;
		memset(&op, 0, sizeof(op));
		op.realm = &realm;
		schedule_retry(&op);
		CHECK(30, op.retry_delay);
		realm.options.tc_timer = 400;
		op.retry_delay = 0;
		schedule_retry(&op);
		CHECK(400, op.retry_delay);
		CHECK(fake_now + 400, op.retry_at.tv_sec);
		schedule_retry(&op);
		CHECK(400, op.retry_delay);
		realm.options.tc_timer = 1;
		op.retry_delay = 0;
		for (i = 0; i < 6; i++) {
			schedule_retry(&op);
			CHECK(1u << i, op.retry_delay);
		}
	}
	realm.transport = FD_DISCOVERY_TLS_TCP;
	realm.options.ip_family = FD_DISCOVERY_IPV4;
	realm.options.tc_timer = 60;
	realm.options.tw_timer = 45;
	realm.options.tls_priority = "NORMAL";
	realm.options.peer_realm = "origin.example.org";
	peer.identity = "peer.example.net";
	fd_list_init(&realm.chain, &realm);
	fd_list_init(&realm.peers, NULL);
	fd_list_init(&peer.chain, &peer);
	fd_list_insert_before(&realms, &realm.chain);
	fd_list_insert_before(&realm.peers, &peer.chain);
	CHECK(0, pthread_create(&worker, NULL, worker_main, NULL));

	other = request("badexample.org", 1);
	CHECK(0, recover(NULL, &other));
	CHECK(1, other != NULL);
	CHECK(0, fd_msg_free(other));
	CHECK(0, starts);

	enqueue(1);
	wait_for(&starts, 1);
	advance_to(1005);
	enqueue(1);
	CHECK(0, pthread_mutex_lock(&test_lock));
	hold_stop = 1;
	fail_create = 1;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	complete(1, EIO);
	wait_for(&failed_creates, 1);
	advance_to(wait_stop_retry(1));
	wait_for(&stop_entered, 1);
	advance_to(1010);
	wait_for(&resumed, 1);
	CHECK(0, pthread_mutex_lock(&test_lock));
	CHECK(1, resumed);
	CHECK(0, pthread_mutex_unlock(&test_lock));
	advance_to(1015);
	wait_for(&resumed, 2);
	CHECK(0, pthread_mutex_lock(&test_lock));
	hold_stop = 0;
	fail_join = 1;
	CHECK(0, pthread_cond_broadcast(&test_wake));
	CHECK(0, pthread_mutex_unlock(&test_lock));
	wait_for(&failed_joins, 1);
	advance_to(wait_join_retry(1));
	due = wait_backoff(1, 60);
	wait_for(&stops, 1);
	CHECK(0, retries);
	enqueue(1);
	advance_to(due - 1);
	wait_for(&resumed, 3);
	CHECK(1, starts);
	advance_to(due);
	wait_for(&starts, 2);
	complete(1, EIO);
	due = wait_backoff(1, 120);
	wait_for(&stops, 2);
	for (i = 0; i < 4; i++) {
		advance_to(due);
		wait_for(&starts, 3 + i);
		complete(1, EIO);
		due = wait_backoff(1, delays[i]);
		wait_for(&stops, 3 + i);
	}

	/* An incoming request near the scheduled retry can use that attempt. */
	advance_to(due - 1);
	enqueue(1);
	CHECK(0, pthread_mutex_lock(&test_lock));
	immediate = 1;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	advance_to(due);
	wait_for(&starts, 7);
	wait_for(&resumed, 4);
	CHECK(1, retries);

	/* A post-success failure starts again at the initial delay. */
	complete(1, EIO);
	due = wait_backoff(1, 60);
	wait_for(&stops, 7);
	advance_to(due - 1);
	enqueue(1);
	advance_to(due);
	wait_for(&starts, 8);
	wait_for(&resumed, 5);
	CHECK(2, retries);

	/* An unavailable successful operation neither bypasses the deadline nor
	 * needs another discovery flight when its peer becomes usable again. */
	CHECK(0, pthread_mutex_lock(&test_lock));
	peer_available = 0;
	check_target = checks + 1;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	enqueue(1);
	wait_for(&checks, check_target);
	advance_to(due + 10);
	wait_for(&resumed, 6);
	CHECK(0, pthread_mutex_lock(&test_lock));
	peer_available = 1;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	enqueue(1);
	wait_for(&resumed, 7);
	CHECK(3, retries);

	/* Both remote DPR causes suppress automatic retries for the pair. */
	CHECK(0, pthread_mutex_lock(&test_lock));
	immediate = FD_DISCOVERY_PEER_BUSY;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	enqueue(3);
	wait_for(&resumed, 8);
	wait_suppressed(3);
	CHECK(0, pthread_mutex_lock(&test_lock));
	immediate = 1;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	enqueue(4);
	wait_for(&resumed, 9);
	complete(4, FD_DISCOVERY_PEER_NOT_FRIEND);
	wait_suppressed(4);
	for (i = 3; i <= 4; i++) {
		other = request("example.org", i);
		CHECK(0, recover(NULL, &other));
		CHECK(1, other != NULL);
		CHECK(0, fd_msg_free(other));
	}
	advance_to(due + 10010);
	enqueue(1);
	wait_for(&resumed, 10);
	CHECK(5, retries);

	/* Start errors without a handle also back off independently. */
	CHECK(0, pthread_mutex_lock(&test_lock));
	fail_start = 1;
	immediate = 0;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	enqueue(2);
	wait_for(&starts, 11);
	due = wait_backoff(2, 60);
	advance_to(due - 1);
	wait_for(&resumed, 11);
	CHECK(11, starts);
	CHECK(1, starts_for[3]);
	CHECK(1, starts_for[4]);

	/* Failed stops retain the handle, allow deadlines, and block restarts. */
	CHECK(0, pthread_mutex_lock(&test_lock));
	stop_failures = 2;
	stop_target = stop_entered + 2;
	CHECK(0, pthread_mutex_unlock(&test_lock));
	complete(1, EIO);
	due = wait_stop_retry(1);
	enqueue(1);
	advance_to(due + 9);
	wait_for(&resumed, 12);
	wait_for(&stop_entered, stop_target);
	wait_for(&starts, 12);
	due = wait_stop_retry(1);
	complete(1, EIO);
	CHECK(12, starts);

	/* Saturation preserves routing ownership; shutdown drains without retry. */
	for (i = 0; i < RD_MAX_PENDING; i++)
		enqueue(2);
	other = request("example.org", 2);
	CHECK(0, recover(NULL, &other));
	CHECK(1, other != NULL);
	CHECK(0, fd_msg_free(other));
	CHECK(0, pthread_mutex_lock(&lock));
	stopping = 1;
	CHECK(0, pthread_cond_signal(&wake));
	CHECK(0, pthread_mutex_unlock(&lock));
	wait_for(&resumed, 12 + RD_MAX_PENDING);
	/* Shutdown cannot release callback data until the stop eventually succeeds. */
	CHECK(1, wait_stop_retry(1) == due);
	advance_to(due);
	CHECK(0, pthread_join(worker, NULL));
	CHECK(12, starts);
	CHECK(10, stops);
	CHECK(12 + RD_MAX_PENDING, resumed);
	CHECK(5, retries);
	CHECK(1, FD_IS_LIST_EMPTY(&operations));
	CHECK(0, pending_count);
	fd_list_unlink(&peer.chain);
	fd_list_unlink(&realm.chain);
	PASSTEST();
}
