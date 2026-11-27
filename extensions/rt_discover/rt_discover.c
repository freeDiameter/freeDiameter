#include "rt_discover.h"
#include <freeDiameter/discovery.h>

#define RD_MAX_PENDING 256
#define RD_MAX_OPERATIONS 64
#define RD_TIMEOUT 10
#define RD_RETRY_INITIAL 30
#define RD_RETRY_MAX 300
#define RD_STOP_RETRY 1

struct rd_pending {
	struct fd_list chain;
	struct msg *msg;
	struct timespec deadline;
};

enum rd_state { RD_NEW, RD_STARTING, RD_WAITING, RD_ACTIVE, RD_STOPPING, RD_BACKOFF, RD_SUPPRESSED };

struct rd_operation {
	struct fd_list chain;
	struct fd_list pending;
	struct rd_realm *realm;
	uint32_t application;
	enum rd_state state;
	int complete;
	int error;
	int suppressed;
	unsigned int retry_delay;
	struct timespec retry_at;
	struct fd_discovery *discovery;
	pthread_t stop_thread;
	int stop_started;
	int stop_done;
	int stop_result;
	struct timespec stop_retry_at;
};

static struct fd_list realms = FD_LIST_INITIALIZER(realms);
static struct fd_list operations = FD_LIST_INITIALIZER(operations);
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t wake = PTHREAD_COND_INITIALIZER;
static pthread_t worker;
static int worker_started, stopping;
static unsigned int operation_count, pending_count;
static struct dict_object *destination_realm;
static struct fd_rt_out_recovery_hdl *recovery_handler;

static int before(const struct timespec *a, const struct timespec *b)
{
	return a->tv_sec < b->tv_sec ||
		(a->tv_sec == b->tv_sec && a->tv_nsec < b->tv_nsec);
}

static struct rd_realm *find_realm(const uint8_t *name, size_t len)
{
	struct fd_list *li;
	for (li = realms.next; li != &realms; li = li->next) {
		struct rd_realm *realm = li->o;
		if (!fd_os_almostcasesrch(name, len, realm->name, strlen(realm->name), NULL))
			return realm;
	}
	return NULL;
}

static const char *discovery_error(int error)
{
	if (error == FD_DISCOVERY_PEER_BUSY)
		return "peer disconnected with BUSY";
	if (error == FD_DISCOVERY_PEER_NOT_FRIEND)
		return "peer disconnected with DO_NOT_WANT_TO_TALK_TO_YOU";
	return strerror(error);
}

static void discovery_complete(void *data, int error)
{
	struct rd_operation *op = data;
	pthread_mutex_lock(&lock);
	if (error)
		LOG_E("rt_discover: discovery failed for %s application %u: %s",
		      op->realm->name, (unsigned int)op->application, discovery_error(error));
	if (op->state != RD_STOPPING && !stopping) {
		op->error = error;
		op->complete = 1;
		if (error == FD_DISCOVERY_PEER_BUSY || error == FD_DISCOVERY_PEER_NOT_FRIEND)
			op->suppressed = 1;
		if (!error)
			op->retry_delay = 0;
		pthread_cond_signal(&wake);
	}
	pthread_mutex_unlock(&lock);
}

/* Worker-owned PRNG; jitter is for spreading retries, not security. */
static unsigned int retry_random(void)
{
	static unsigned int seed;
	if (!seed) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		seed = (unsigned int)now.tv_nsec ^ (unsigned int)now.tv_sec ^
			(unsigned int)(uintptr_t)&seed;
		if (!seed)
			seed = 1;
	}
	seed ^= seed << 13;
	seed ^= seed >> 17;
	seed ^= seed << 5;
	return seed;
}

static void schedule_retry(struct rd_operation *op)
{
	unsigned int delay, initial = op->realm->options.tc_timer ?
		op->realm->options.tc_timer : RD_RETRY_INITIAL;
	unsigned int maximum = initial > RD_RETRY_MAX ? initial : RD_RETRY_MAX;
	if (!op->retry_delay)
		op->retry_delay = initial;
	else if (op->retry_delay > maximum / 2)
		op->retry_delay = maximum;
	else
		op->retry_delay *= 2;
	delay = op->retry_delay + retry_random() % (op->retry_delay / 4 + 1);
	if (delay > maximum)
		delay = maximum;
	clock_gettime(CLOCK_MONOTONIC, &op->retry_at);
	op->retry_at.tv_sec += delay;
	op->complete = 0;
	op->error = 0;
	op->state = RD_BACKOFF;
}

static void *stop_operation(void *data)
{
	struct rd_operation *op = data;
	struct fd_discovery *handle = op->discovery;
	int ret = fd_discovery_stop(&handle);
	pthread_mutex_lock(&lock);
	if (ret)
		LOG_E("rt_discover: stop failed for %s application %u: %s",
		      op->realm->name, (unsigned int)op->application, strerror(ret));
	else
		op->discovery = NULL;
	op->stop_result = ret;
	op->stop_done = 1;
	pthread_cond_signal(&wake);
	pthread_mutex_unlock(&lock);
	return NULL;
}

static void defer_stop(struct rd_operation *op)
{
	clock_gettime(CLOCK_MONOTONIC, &op->stop_retry_at);
	op->stop_retry_at.tv_sec += RD_STOP_RETRY;
}

static int start_operation(struct rd_operation *op)
{
	struct fd_discovery_config config;
	struct fd_list *li;
	const char **peers;
	size_t count = 0, i = 0;
	int ret;

	for (li = op->realm->peers.next; li != &op->realm->peers; li = li->next)
		count++;
	peers = calloc(count, sizeof(*peers));
	if (!peers)
		return ENOMEM;
	for (li = op->realm->peers.next; li != &op->realm->peers; li = li->next)
		peers[i++] = ((struct rd_allowed_peer *)li->o)->identity;
	memset(&config, 0, sizeof(config));
	config.realm = op->realm->name;
	config.application_id = op->application;
	config.authorized_peers = peers;
	config.authorized_peer_count = count;
	config.peer_options = op->realm->options;
	config.transports = FD_DISCOVERY_TLS_TCP;
#if defined(ENABLE_DTLS_SCTP) && !defined(DISABLE_SCTP)
	config.transports |= FD_DISCOVERY_DTLS_SCTP;
#endif
	if (op->realm->transport)
		config.transports &= op->realm->transport;
	config.timeout_seconds = RD_TIMEOUT;
	ret = fd_discovery_start(&config, discovery_complete, op, &op->discovery);
	free(peers);
	return ret;
}

static void finish_message(struct rd_pending *pending, int retry)
{
	int ret = fd_rt_out_recovery_resume(&pending->msg, retry);
	if (ret) {
		LOG_D("rt_discover: unable to resume request: %s", strerror(ret));
		if (pending->msg)
			fd_msg_free(pending->msg);
	}
	free(pending);
}

static int operation_available(struct rd_operation *op)
{
	struct fd_list *li;
	for (li = op->realm->peers.next; li != &op->realm->peers; li = li->next) {
		struct rd_allowed_peer *peer = li->o;
		int matched = 0;
		if (!fd_discovery_has_peer(op->discovery, peer->identity,
					   strlen(peer->identity), &matched) && matched)
			return 1;
	}
	return 0;
}

static void *worker_main(void *unused)
{
	(void)unused;
	pthread_mutex_lock(&lock);
	for (;;) {
		struct fd_list *li, *pi;
		struct rd_operation *start = NULL, *remove = NULL, *failed = NULL;
		struct rd_operation *stopped = NULL, *stop_start = NULL;
		struct rd_pending *ready = NULL;
		struct timespec now;
		int timed_wait = 0, poll_pending = 0, retry = 0;

		clock_gettime(CLOCK_MONOTONIC, &now);
		for (li = operations.next; li != &operations; li = li->next) {
			struct rd_operation *op = li->o;
			int available = 0;
			if ((op->state == RD_WAITING || op->state == RD_ACTIVE) && op->complete) {
				op->complete = 0;
				if (op->error)
					failed = op;
				else
					op->state = RD_ACTIVE;
			}
			if (failed)
				break;
			if (!stopping && op->state == RD_ACTIVE &&
			    !FD_IS_LIST_EMPTY(&op->pending)) {
				/* Only this worker removes operations; do not hold the
				 * recovery lock while querying the manager. */
				pthread_mutex_unlock(&lock);
				available = operation_available(op);
				pthread_mutex_lock(&lock);
				clock_gettime(CLOCK_MONOTONIC, &now);
			}
			for (pi = op->pending.next; pi != &op->pending; pi = pi->next) {
				struct rd_pending *p = pi->o;
				if (stopping || op->state == RD_SUPPRESSED ||
				    !before(&now, &p->deadline) ||
				    (op->state == RD_ACTIVE && !op->complete && available)) {
					ready = p;
					retry = !stopping && before(&now, &p->deadline) &&
						op->state == RD_ACTIVE && !op->complete && available;
					break;
				}
				timed_wait = 1;
				poll_pending = 1;
			}
			if (ready)
				break;
			if (op->state == RD_STOPPING) {
				if ((op->stop_done || !op->stop_started) &&
				    !before(&now, &op->stop_retry_at)) {
					if (!op->stop_done) {
						stop_start = op;
						break;
					}
					stopped = op;
					break;
				}
				if (op->stop_done || !op->stop_started)
					timed_wait = 1;
				continue;
			}
			if (stopping) {
				remove = op;
				break;
			}
			if ((op->state == RD_NEW ||
			     (op->state == RD_BACKOFF && !before(&now, &op->retry_at))) && !start)
				start = op;
			if (op->state == RD_BACKOFF)
				timed_wait = 1;
		}

		if (ready) {
			fd_list_unlink(&ready->chain);
			pending_count--;
			pthread_mutex_unlock(&lock);
			finish_message(ready, retry);
			pthread_mutex_lock(&lock);
		} else if (stopped) {
			int ret = 0;
			if (stopped->stop_started) {
				pthread_mutex_unlock(&lock);
				ret = pthread_join(stopped->stop_thread, NULL);
				pthread_mutex_lock(&lock);
			}
			if (ret) {
				LOG_E("rt_discover: stop thread join failed for %s application %u: %s",
				      stopped->realm->name, (unsigned int)stopped->application, strerror(ret));
				defer_stop(stopped);
			} else if (stopped->stop_result) {
				stopped->stop_started = 0;
				stopped->stop_done = 0;
				defer_stop(stopped);
			} else if (stopping) {
				stopped->stop_started = 0;
				stopped->stop_done = 0;
				if (FD_IS_LIST_EMPTY(&stopped->pending)) {
					fd_list_unlink(&stopped->chain);
					operation_count--;
					free(stopped);
				} else
					stopped->state = RD_BACKOFF;
			} else if (stopped->suppressed) {
				stopped->stop_started = 0;
				stopped->stop_done = 0;
				stopped->state = RD_SUPPRESSED;
			} else {
				stopped->stop_started = 0;
				stopped->stop_done = 0;
				schedule_retry(stopped);
			}
		} else if (remove || failed) {
			struct rd_operation *op = remove ? remove : failed;
			op->state = RD_STOPPING;
			op->stop_retry_at.tv_sec = 0;
			op->stop_retry_at.tv_nsec = 0;
			op->stop_result = 0;
			if (!op->discovery)
				op->stop_done = 1;
		} else if (stop_start) {
			int ret = pthread_create(&stop_start->stop_thread, NULL, stop_operation, stop_start);
			if (ret) {
				LOG_E("rt_discover: stop thread creation failed for %s application %u: %s",
				      stop_start->realm->name, (unsigned int)stop_start->application, strerror(ret));
				stop_start->stop_result = ret;
				defer_stop(stop_start);
			} else
				stop_start->stop_started = 1;
		} else if (start && !stopping) {
			int ret;
			start->state = RD_STARTING;
			pthread_mutex_unlock(&lock);
			ret = start_operation(start);
			pthread_mutex_lock(&lock);
			if (ret) {
				LOG_E("rt_discover: discovery start failed for %s application %u: %s",
				      start->realm->name, (unsigned int)start->application, discovery_error(ret));
				start->error = ret;
				start->complete = 1;
			}
			start->state = RD_WAITING;
		} else {
			if (stopping && FD_IS_LIST_EMPTY(&operations))
				break;
			if (timed_wait) {
				struct timespec until;
				/* Recheck monotonic deadlines and peer availability periodically. */
				clock_gettime(CLOCK_REALTIME, &until);
				if (poll_pending)
					until.tv_nsec += 100000000;
				else
					until.tv_sec++;
				if (until.tv_nsec >= 1000000000) {
					until.tv_sec++;
					until.tv_nsec -= 1000000000;
				}
				pthread_cond_timedwait(&wake, &lock, &until);
			} else
				pthread_cond_wait(&wake, &lock);
		}
	}
	pthread_mutex_unlock(&lock);
	return NULL;
}

static int message_key(struct msg *msg, struct rd_realm **realm, uint32_t *application)
{
	struct msg_hdr *hdr;
	struct avp *avp;
	struct avp_hdr *ahdr;

	if (fd_msg_hdr(msg, &hdr) ||
	    fd_msg_search_avp(msg, destination_realm, &avp) || !avp ||
	    fd_msg_parse_dict(avp, fd_g_config->cnf_dict, NULL) ||
	    fd_msg_avp_hdr(avp, &ahdr) || !ahdr->avp_value)
		return EINVAL;
	*realm = find_realm(ahdr->avp_value->os.data, ahdr->avp_value->os.len);
	*application = hdr->msg_appl;
	return *realm ? 0 : ENOENT;
}

static int recover(void *data, struct msg **pmsg)
{
	struct rd_realm *realm;
	uint32_t application;
	struct rd_operation *op = NULL;
	struct rd_pending *pending;
	struct fd_list *li;
	struct timespec deadline;
	(void)data;

	if (message_key(*pmsg, &realm, &application) ||
	    clock_gettime(CLOCK_MONOTONIC, &deadline))
		return 0;
	deadline.tv_sec += RD_TIMEOUT;
	pending = calloc(1, sizeof(*pending));
	if (!pending)
		return 0;
	fd_list_init(&pending->chain, pending);
	pending->deadline = deadline;

	pthread_mutex_lock(&lock);
	if (stopping || pending_count >= RD_MAX_PENDING)
		goto reject;
	for (li = operations.next; li != &operations; li = li->next) {
		struct rd_operation *candidate = li->o;
		if (candidate->realm == realm && candidate->application == application) {
			op = candidate;
			break;
		}
	}
	if (op && op->suppressed)
		goto reject;
	if (!op) {
		if (operation_count >= RD_MAX_OPERATIONS)
			goto reject;
		op = calloc(1, sizeof(*op));
		if (!op)
			goto reject;
		fd_list_init(&op->chain, op);
		fd_list_init(&op->pending, NULL);
		op->realm = realm;
		op->application = application;
		fd_list_insert_before(&operations, &op->chain);
		operation_count++;
	}
	pending->msg = *pmsg;
	*pmsg = NULL;
	fd_list_insert_before(&op->pending, &pending->chain);
	pending_count++;
	pthread_cond_signal(&wake);
	pthread_mutex_unlock(&lock);
	return 0;

reject:
	pthread_mutex_unlock(&lock);
	free(pending);
	return 0;
}

static void cleanup(void)
{
	int ret;
	if (recovery_handler) {
		fd_rt_out_recovery_unregister(recovery_handler, NULL);
		recovery_handler = NULL;
	}
	if (worker_started) {
		pthread_mutex_lock(&lock);
		stopping = 1;
		pthread_cond_signal(&wake);
		pthread_mutex_unlock(&lock);
		while ((ret = pthread_join(worker, NULL)) != 0) {
			struct timespec pause = { 1, 0 };
			LOG_E("rt_discover: worker thread join failed: %s", strerror(ret));
			nanosleep(&pause, NULL);
		}
		worker_started = 0;
	}
	rd_config_free(&realms);
}

static int rd_entry(char *conffile)
{
	int ret;
	if (!conffile)
		return EINVAL;
	ret = rd_config_load(conffile, &realms);
	if (ret)
		return ret;
	ret = fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME,
			     "Destination-Realm", &destination_realm, ENOENT);
	if (!ret) {
		ret = pthread_create(&worker, NULL, worker_main, NULL);
		if (ret)
			LOG_E("rt_discover: worker thread creation failed: %s", strerror(ret));
	}
	if (!ret) {
		worker_started = 1;
		ret = fd_rt_out_recovery_register(recover, NULL, &recovery_handler);
	}
	if (ret)
		cleanup();
	return ret;
}

void fd_ext_fini(void)
{
	cleanup();
}

EXTENSION_ENTRY("rt_discover", rd_entry);
