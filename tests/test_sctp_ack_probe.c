/* SPDX-License-Identifier: BSD-3-Clause */
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <linux/sock_diag.h>
#include <netinet/sctp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static int control = -1;
static int watched = -1;
static int observing;
static int shutdown_mode;
static int timeout_mode;
static int gate_finished;
static int peer_shutdown_mode;

static void fail(const char *message)
{
	perror(message);
	_exit(1);
}

static void event(const char *name, int value)
{
	char text[80];
	struct timespec now;
	int length;
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		fail("ACK test probe clock");
	length = snprintf(text, sizeof(text), "%s %d %lld.%09ld\n",
			  name, value, (long long)now.tv_sec, now.tv_nsec);
	if (length < 0 || (size_t)length >= sizeof(text) ||
	    send(control, text, length, MSG_NOSIGNAL) != length)
		fail("ACK test probe event");
}

static void wait_go(void)
{
	char command;
	ssize_t ret;
	do {
		ret = read(control, &command, 1);
	} while (ret < 0 && errno == EINTR);
	if (ret != 1 || command != 'G') {
		errno = EPROTO;
		fail("ACK test probe control");
	}
}

__attribute__((constructor))
static void init(void)
{
	const char *path = getenv("FD_ACK_TEST_SOCKET");
	const char *mode = getenv("FD_ACK_TEST_MODE");
	struct sockaddr_un address;
	size_t address_length;

	if (!path || path[0] != '@' || !path[1] || !mode ||
	    strlen(path) > sizeof(address.sun_path)) {
		errno = EINVAL;
		fail("ACK test probe configuration");
	}
	shutdown_mode = !strncmp(mode, "shutdown", 8);
	timeout_mode = !strcmp(mode, "handshake-timeout") || !strcmp(mode, "shutdown-timeout");
	gate_finished = !strcmp(mode, "handshake-server");
	peer_shutdown_mode = !strcmp(mode, "shutdown-peer");
	memset(&address, 0, sizeof(address));
	address.sun_family = AF_UNIX;
	memcpy(address.sun_path + 1, path + 1, strlen(path + 1));
	address_length = offsetof(struct sockaddr_un, sun_path) + strlen(path);
	control = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (control < 0 || connect(control, (struct sockaddr *)&address, address_length) < 0)
		fail("ACK test probe connect");
}

ssize_t sendmsg(int fd, const struct msghdr *message, int flags)
{
	ssize_t (*real_sendmsg)(int, const struct msghdr *, int) = dlsym(RTLD_NEXT, "sendmsg");
	const unsigned char *data;
	ssize_t ret;
	int gate = 0;

	if (!real_sendmsg) {
		errno = ENOSYS;
		fail("ACK test probe sendmsg");
	}
	if (!message->msg_iovlen || message->msg_iov[0].iov_len < 13)
		return real_sendmsg(fd, message, flags);
	data = message->msg_iov[0].iov_base;
	if (data[1] != 0xfe)
		return real_sendmsg(fd, message, flags);

	if (data[0] == 20 && __atomic_load_n(&watched, __ATOMIC_SEQ_CST) == -1) {
		struct sockaddr_in local;
		socklen_t length = sizeof(local);
		if (getsockname(fd, (struct sockaddr *)&local, &length) < 0 || local.sin_family != AF_INET)
			fail("ACK test probe local address");
		if (timeout_mode) {
			struct sctp_paddrparams path;
			struct sctp_assocparams association;
			struct sockaddr_storage peer;
			memset(&peer, 0, sizeof(peer));
			length = sizeof(peer);
			if (getpeername(fd, (struct sockaddr *)&peer, &length) < 0)
				fail("ACK test probe peer address");
			memset(&path, 0, sizeof(path));
			memcpy(&path.spp_address, &peer, length);
			length = sizeof(path);
			if (getsockopt(fd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &path, &length) < 0)
				fail("ACK test probe path parameters");
			path.spp_pathmaxrxt = 100;
			if (setsockopt(fd, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &path, sizeof(path)) < 0)
				fail("ACK test probe path retransmissions");
			memset(&association, 0, sizeof(association));
			length = sizeof(association);
			if (getsockopt(fd, IPPROTO_SCTP, SCTP_ASSOCINFO, &association, &length) < 0)
				fail("ACK test probe association parameters");
			association.sasoc_asocmaxrxt = 100;
			if (setsockopt(fd, IPPROTO_SCTP, SCTP_ASSOCINFO, &association, sizeof(association)) < 0)
				fail("ACK test probe association retransmissions");
		}
		__atomic_store_n(&watched, fd, __ATOMIC_SEQ_CST);
		event("CCS", ntohs(local.sin_port));
		if (!shutdown_mode) {
			wait_go();
			gate = 1;
		}
	} else if (shutdown_mode && data[0] == 23 && fd == __atomic_load_n(&watched, __ATOMIC_SEQ_CST)) {
		char command;
		ssize_t n = recv(control, &command, 1, MSG_DONTWAIT);
		if (n == 1) {
			if (command != 'A') {
				errno = EPROTO;
				fail("ACK test probe arm");
			}
			event("DATA", 0);
			wait_go();
			gate = 2;
		} else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
			fail("ACK test probe arm read");
		}
	}
	if (fd == __atomic_load_n(&watched, __ATOMIC_SEQ_CST)) {
		if (data[0] == 22 && (data[3] || data[4])) {
			event("FINISHED", 0);
			if (gate_finished) {
				gate_finished = 0;
				wait_go();
				gate = 3;
			}
		}
		if (data[0] == 21)
			event("ALERT", 0);
	}
	ret = real_sendmsg(fd, message, flags);
	if (gate && ret >= 0) {
		event(gate == 1 ? "CCS_SENT" : gate == 2 ? "DATA_SENT" : "FINISHED_SENT",
		      (int)ret);
		if (gate == 2 && peer_shutdown_mode) {
			unsigned int memory[SK_MEMINFO_VARS];
			socklen_t length = sizeof(memory);
			if (getsockopt(fd, SOL_SOCKET, SO_MEMINFO, memory, &length) < 0)
				fail("ACK test probe queued data");
			if (length < (SK_MEMINFO_WMEM_QUEUED + 1) * sizeof(memory[0])) {
				errno = EPROTO;
				fail("ACK test probe queued data length");
			}
			event("QUEUED", memory[SK_MEMINFO_WMEM_QUEUED] != 0);
		}
		__atomic_store_n(&observing, 1, __ATOMIC_SEQ_CST);
	}
	return ret;
}

int getsockopt(int fd, int level, int option, void *value, socklen_t *length)
{
	int (*real_getsockopt)(int, int, int, void *, socklen_t *) = dlsym(RTLD_NEXT, "getsockopt");
	int ret;
	static int last_queued = -1;

	if (!real_getsockopt) {
		errno = ENOSYS;
		fail("ACK test probe getsockopt");
	}
	ret = real_getsockopt(fd, level, option, value, length);
	if (!ret && fd == __atomic_load_n(&watched, __ATOMIC_SEQ_CST) &&
	    __atomic_load_n(&observing, __ATOMIC_SEQ_CST) &&
	    level == SOL_SOCKET && option == SO_MEMINFO &&
	    *length >= (SK_MEMINFO_WMEM_QUEUED + 1) * sizeof(unsigned int)) {
		unsigned int queued = ((unsigned int *)value)[SK_MEMINFO_WMEM_QUEUED];
		int pending = queued != 0;
		if (__atomic_exchange_n(&last_queued, pending, __ATOMIC_SEQ_CST) != pending)
			event(pending ? "PENDING" : "DRAINED", pending);
	}
	return ret;
}

int setsockopt(int fd, int level, int option, const void *value, socklen_t length)
{
	int (*real_setsockopt)(int, int, int, const void *, socklen_t) = dlsym(RTLD_NEXT, "setsockopt");
	int ret;

	if (!real_setsockopt) {
		errno = ENOSYS;
		fail("ACK test probe setsockopt");
	}
	if (fd == __atomic_load_n(&watched, __ATOMIC_SEQ_CST) &&
	    level == IPPROTO_SCTP && option == SCTP_AUTH_ACTIVE_KEY &&
	    length >= sizeof(struct sctp_authkeyid) &&
	    ((const struct sctp_authkeyid *)value)->scact_keynumber == 1) {
		const struct sctp_authkeyid *key = value;
		event("KEY", key->scact_keynumber);
		ret = real_setsockopt(fd, level, option, value, length);
		if (!ret)
			event("KEY_OK", key->scact_keynumber);
		return ret;
	}
	if (fd == __atomic_load_n(&watched, __ATOMIC_SEQ_CST) &&
	    level == IPPROTO_SCTP && option == SCTP_AUTH_DELETE_KEY &&
	    length >= sizeof(struct sctp_authkeyid)) {
		const struct sctp_authkeyid *key = value;
		event("DELETE", key->scact_keynumber);
		ret = real_setsockopt(fd, level, option, value, length);
		if (!ret)
			event("DELETE_OK", key->scact_keynumber);
		return ret;
	}
	return real_setsockopt(fd, level, option, value, length);
}
