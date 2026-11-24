/* SPDX-License-Identifier: BSD-3-Clause */

#include "fdcore-internal.h"
#include "cnxctx.h"

#include <netinet/sctp.h>
#include <linux/sock_diag.h>
#include <gnutls/dtls.h>

#define DTLS_RECORD_HEADER 13
#define DTLS_BUFFERED_BYTES_MAX (2 * 1024 * 1024)

struct dtls_record {
	struct fd_list chain;
	uint8_t *data;
	size_t length;
};

struct sctp_dtls_ctx {
	struct fd_list pending;
	struct fd_list early;
	size_t buffered_bytes;
	int handshaking;
	int ccs_delivered;
	int key_installed;
	int key_active;
	int endpoints_changed;
	struct timespec handshake_time;
	struct timespec bye_time;
};

static int transport_timeout(struct cnxctx *conn, unsigned int *timeout)
{
	struct sctp_dtls_ctx *ctx = conn->cc_sctp_dtls;
	struct timespec now, *start;
	unsigned int limit;
	long elapsed;
	int ret;

	if (ctx->handshaking) {
		start = &ctx->handshake_time;
		limit = 30000;
	} else if (fd_cnx_teststate(conn, CC_STATUS_DTLS_BYE)) {
		start = &ctx->bye_time;
		limit = 5000;
	} else {
		return 0;
	}
	if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) {
		ret = errno;
		LOG_E("Reading DTLS/SCTP deadline clock failed: %s.", strerror(ret));
		fd_cnx_markerror(conn);
		return ret;
	}
	elapsed = (now.tv_sec - start->tv_sec) * 1000 + (now.tv_nsec - start->tv_nsec) / 1000000;
	if (elapsed >= limit) {
		LOG_E("DTLS/SCTP %s timed out on %s.", ctx->handshaking ? "handshake" : "close-notify wait",
		      conn->cc_id);
		fd_cnx_markerror(conn);
		return ETIMEDOUT;
	}
	if (*timeout > limit - elapsed)
		*timeout = limit - elapsed;
	return 0;
}

int fd_sctp_auth_supported(int sock, int *supported)
{
	struct sctp_assoc_value auth;
	socklen_t length = sizeof(auth);

	CHECK_PARAMS(supported);
	memset(&auth, 0, sizeof(auth));
	if (getsockopt(sock, IPPROTO_SCTP, SCTP_AUTH_SUPPORTED, &auth, &length) < 0) {
		int error = errno;
		if (error == ENOPROTOOPT || error == EOPNOTSUPP) {
			*supported = 0;
			return 0;
		}
		LOG_E("Reading SCTP-AUTH status failed: %s.", strerror(error));
		return error;
	}
	if (length != sizeof(auth)) {
		LOG_E("Invalid SCTP-AUTH status length.");
		return EPROTO;
	}
	*supported = auth.assoc_value != 0;
	return 0;
}

int fd_sctp_dtls_init_socket(int sock)
{
	struct sctp_authchunk chunk;
	struct sctp_authkeyid active;
	unsigned int partial_delivery = 18446;
	int supported;

	CHECK_FCT(fd_sctp_auth_supported(sock, &supported));
	if (!supported) {
		LOG_E("DTLS/SCTP requires kernel SCTP-AUTH support (net.sctp.auth_enable=1).");
		return ENOTSUP;
	}
	memset(&chunk, 0, sizeof(chunk));
	chunk.sauth_chunk = 0; /* SCTP DATA */
	CHECK_SYS(setsockopt(sock, IPPROTO_SCTP, SCTP_AUTH_CHUNK, &chunk, sizeof(chunk)));
	chunk.sauth_chunk = 192; /* SCTP FORWARD-TSN */
	CHECK_SYS(setsockopt(sock, IPPROTO_SCTP, SCTP_AUTH_CHUNK, &chunk, sizeof(chunk)));
	/* A fresh Linux SCTP endpoint already has the required empty key 0. */
	memset(&active, 0, sizeof(active));
	CHECK_SYS(setsockopt(sock, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY, &active, sizeof(active)));
	CHECK_SYS(setsockopt(sock, IPPROTO_SCTP, SCTP_PARTIAL_DELIVERY_POINT,
			     &partial_delivery, sizeof(partial_delivery)));
	return 0;
}

static int install_key(struct cnxctx *conn)
{
	struct sctp_authkey *key;
	int ret;

	if (conn->cc_sctp_dtls->key_installed)
		return 0;
	CHECK_MALLOC(key = calloc(1, sizeof(*key) + 64));
	key->sca_keynumber = 1;
	key->sca_keylength = 64;
	ret = gnutls_prf_rfc5705(conn->cc_tls_para.session, 23, "EXPORTER_DTLS_OVER_SCTP",
				0, NULL, 64, (char *)key->sca_key);
	if (ret < 0) {
		LOG_E("DTLS/SCTP exporter failed: %s", gnutls_strerror(ret));
		ret = EINVAL;
	} else {
		ret = setsockopt(conn->cc_socket, IPPROTO_SCTP, SCTP_AUTH_KEY, key, sizeof(*key) + 64);
		if (ret < 0) {
			ret = errno;
			LOG_E("Installing DTLS/SCTP AUTH key failed: %s", strerror(ret));
		}
	}
	gnutls_memset(key->sca_key, 0, 64);
	free(key);
	if (!ret)
		conn->cc_sctp_dtls->key_installed = 1;
	return ret;
}

static int handshake_hook(gnutls_session_t session, unsigned int type, unsigned int when,
			  unsigned int incoming, const gnutls_datum_t *message)
{
	struct cnxctx *conn = gnutls_transport_get_ptr(session);
	(void)message;
	if (type == GNUTLS_HANDSHAKE_FINISHED && when == GNUTLS_HOOK_PRE && !incoming) {
		/* The exporter is ready here, but queued old-epoch records still need key 0. */
		if (install_key(conn))
			return GNUTLS_E_INTERNAL_ERROR;
	}
	return 0;
}

static int wait_drained(struct cnxctx *conn, unsigned int timeout)
{
	struct timespec start, now;
	struct timespec pause = { 0, 10000000 };
	uint32_t memory[SK_MEMINFO_VARS];

	CHECK_FCT(transport_timeout(conn, &timeout));
	CHECK_SYS(clock_gettime(CLOCK_MONOTONIC, &start));
	for (;;) {
		socklen_t length = sizeof(memory);
		CHECK_SYS(getsockopt(conn->cc_socket, SOL_SOCKET, SO_MEMINFO, memory, &length));
		if (length < (SK_MEMINFO_WMEM_QUEUED + 1) * sizeof(memory[0])) {
			LOG_E("DTLS/SCTP send-buffer accounting is unavailable.");
			return ENOTSUP;
		}
		/* Linux retains reliable DATA's queued memory until cumulative ACK, not gap ACK. */
		if (memory[SK_MEMINFO_WMEM_QUEUED] == 0)
			return 0;
		CHECK_SYS(clock_gettime(CLOCK_MONOTONIC, &now));
		if ((now.tv_sec - start.tv_sec) * 1000 +
		    (now.tv_nsec - start.tv_nsec) / 1000000 >= timeout) {
			LOG_E("DTLS/SCTP acknowledgment barrier timed out on %s.", conn->cc_id);
			return ETIMEDOUT;
		}
		if (nanosleep(&pause, NULL) < 0 && errno != EINTR) {
			LOG_E("DTLS/SCTP acknowledgment wait failed: %s", strerror(errno));
			return errno;
		}
	}
}

static ssize_t dtls_push(gnutls_transport_ptr_t transport, const void *data, size_t length)
{
	struct cnxctx *conn = transport;
	const uint8_t *record = data;
	uint16_t stream = 0;
	struct iovec iov;
	int ret;

	if (length < DTLS_RECORD_HEADER ||
	    (((size_t)record[11] << 8) | record[12]) != length - DTLS_RECORD_HEADER) {
		LOG_E("GnuTLS supplied an invalid DTLS/SCTP record boundary.");
		errno = EPROTO;
		return -1;
	}
	if (record[0] == 20) {
		ret = wait_drained(conn, 30000);
		if (ret) {
			errno = ret;
			return -1;
		}
	}
	if (record[0] == 22 && (record[3] || record[4]) &&
	    !conn->cc_sctp_dtls->key_active) {
		struct sctp_authkeyid active;
		if (!conn->cc_sctp_dtls->key_installed) {
			LOG_E("DTLS/SCTP Finished arrived before AUTH key installation.");
			errno = EPROTO;
			return -1;
		}
		/* CCS must remain retransmittable with key 0 until cumulatively acknowledged. */
		ret = wait_drained(conn, 30000);
		if (ret) {
			errno = ret;
			return -1;
		}
		memset(&active, 0, sizeof(active));
		active.scact_keynumber = 1;
		if (setsockopt(conn->cc_socket, IPPROTO_SCTP, SCTP_AUTH_ACTIVE_KEY,
			       &active, sizeof(active)) < 0) {
			LOG_E("Activating DTLS/SCTP AUTH key failed: %s", strerror(errno));
			return -1;
		}
		conn->cc_sctp_dtls->key_active = 1;
	}
	if (record[0] == 23 && conn->cc_sctp_para.unordered && conn->cc_sctp_para.str_out > 1) {
		conn->cc_sctp_para.next = (conn->cc_sctp_para.next + 1) % conn->cc_sctp_para.str_out;
		stream = conn->cc_sctp_para.next;
	}
	iov.iov_base = (void *)data;
	iov.iov_len = length;
	return fd_sctp_sendstrv(conn, stream, &iov, 1);
}

static void free_record(struct dtls_record *record)
{
	free(record->data);
	free(record);
}

static int read_record(struct cnxctx *conn, struct dtls_record **out, int flags)
{
	for (;;) {
		struct dtls_record *record;
		uint8_t *data = NULL;
		size_t length = 0;
		uint16_t stream = 0;
		unsigned int timeout = 0;
		int event, ret;

		CHECK_FCT(transport_timeout(conn, &timeout));
		ret = fd_sctp_recvmeta_flags(conn, &stream, &data, &length, &event, flags);
		if (ret)
			return ret;
		if (event == FDEVP_CNX_EP_CHANGE) {
			/* No queue consumer runs during the handshake; coalesce notifications. */
			if (conn->cc_sctp_dtls->handshaking)
				conn->cc_sctp_dtls->endpoints_changed = 1;
			else
				CHECK_FCT(fd_event_send(fd_cnx_target_queue(conn), event, 0, NULL));
			continue;
		}
		if (event == FDEVP_CNX_SHUTDOWN)
			continue;
		if (event != FDEVP_CNX_MSG_RECV)
			return ECONNRESET;
		if (length < DTLS_RECORD_HEADER ||
		    (((size_t)data[11] << 8) | data[12]) != length - DTLS_RECORD_HEADER ||
		    (data[0] != 23 && stream != 0)) {
			LOG_E("Invalid DTLS/SCTP record from %s.", conn->cc_remid);
			free(data);
			return EPROTO;
		}
		record = malloc(sizeof(*record));
		if (!record) {
			free(data);
			LOG_E("Allocating DTLS/SCTP receive record failed.");
			return ENOMEM;
		}
		record->data = data;
		record->length = length;
		fd_list_init(&record->chain, record);
		*out = record;
		return 0;
	}
}

static int enqueue(struct cnxctx *conn, struct fd_list *queue, struct dtls_record *record)
{
	if (record->length > DTLS_BUFFERED_BYTES_MAX - conn->cc_sctp_dtls->buffered_bytes) {
		LOG_E("DTLS/SCTP receive buffering limit exceeded on %s.", conn->cc_id);
		free_record(record);
		return ENOBUFS;
	}
	fd_list_insert_before(queue, &record->chain);
	conn->cc_sctp_dtls->buffered_bytes += record->length;
	return 0;
}

static struct dtls_record *dequeue(struct cnxctx *conn, struct fd_list *queue)
{
	struct dtls_record *record = queue->next->o;
	fd_list_unlink(&record->chain);
	conn->cc_sctp_dtls->buffered_bytes -= record->length;
	return record;
}

static int next_record(struct cnxctx *conn, struct dtls_record **out)
{
	struct sctp_dtls_ctx *ctx = conn->cc_sctp_dtls;
	struct dtls_record *record;
	int ret;

	if (!FD_IS_LIST_EMPTY(&ctx->pending)) {
		*out = dequeue(conn, &ctx->pending);
		return 0;
	}
	ret = read_record(conn, &record, 0);
	if (ret)
		return ret;
	if (record->data[0] == 20 || record->data[0] == 21) {
		struct fd_list after = FD_LIST_INITIALIZER(after);
		unsigned int epoch = ((unsigned int)record->data[3] << 8) | record->data[4];
		struct dtls_record *next;

		/* Drain buffered old-epoch data before CCS, and all buffered data before alerts. */
		while (!(ret = read_record(conn, &next, MSG_DONTWAIT))) {
			unsigned int next_epoch = ((unsigned int)next->data[3] << 8) | next->data[4];
			ret = enqueue(conn, record->data[0] == 20 && next_epoch > epoch ?
				      &after : &ctx->pending, next);
			if (ret)
				break;
		}
		if (ret != EAGAIN && !(ret == ECONNRESET && record->data[0] == 21)) {
			free_record(record);
			while (!FD_IS_LIST_EMPTY(&after))
				free_record(dequeue(conn, &after));
			return ret;
		}
		ret = enqueue(conn, &ctx->pending, record);
		while (!FD_IS_LIST_EMPTY(&after)) {
			next = dequeue(conn, &after);
			if (!ret)
				ret = enqueue(conn, &ctx->pending, next);
			else
				free_record(next);
		}
		if (ret)
			return ret;
		record = dequeue(conn, &ctx->pending);
	}
	*out = record;
	return 0;
}

static int prepare_incoming_key(struct cnxctx *conn)
{
	/* The server master secret is initialized only after GnuTLS processes incoming CCS. */
	/* SCTP retransmits a peer's Finished if it arrives before the new key is installed. */
	if (conn->cc_sctp_dtls->ccs_delivered && !conn->cc_sctp_dtls->key_installed)
		return install_key(conn);
	return 0;
}

static int dtls_pull_timeout(gnutls_transport_ptr_t transport, unsigned int ms)
{
	struct cnxctx *conn = transport;
	fd_set fds;
	struct timeval timeout;
	int ret;

	ret = transport_timeout(conn, &ms);
	if (ret) {
		errno = ret;
		return -1;
	}
	if (prepare_incoming_key(conn)) {
		fd_cnx_markerror(conn);
		errno = EIO;
		return -1;
	}
	if (!FD_IS_LIST_EMPTY(&conn->cc_sctp_dtls->pending))
		return 1;
	FD_ZERO(&fds);
	FD_SET(conn->cc_socket, &fds);
	timeout.tv_sec = ms / 1000;
	timeout.tv_usec = (ms % 1000) * 1000;
	return select(conn->cc_socket + 1, &fds, NULL, NULL, &timeout);
}

static ssize_t dtls_pull(gnutls_transport_ptr_t transport, void *buffer, size_t length)
{
	struct cnxctx *conn = transport;
	struct dtls_record *record;
	int ret;
	unsigned int timeout = 0;

	ret = transport_timeout(conn, &timeout);
	if (ret) {
		errno = ret;
		return -1;
	}
	if (prepare_incoming_key(conn)) {
		fd_cnx_markerror(conn);
		errno = EIO;
		return -1;
	}
	for (;;) {
		ret = next_record(conn, &record);
		if (ret) {
			if (ret != EAGAIN && ret != EINTR) {
				LOG_E("DTLS/SCTP transport receive failed on %s: %s.", conn->cc_id, strerror(ret));
				fd_cnx_markerror(conn);
			}
			errno = ret;
			return -1;
		}
		if (conn->cc_sctp_dtls->handshaking && record->data[0] == 23) {
			ret = enqueue(conn, &conn->cc_sctp_dtls->early, record);
			if (ret) {
				fd_cnx_markerror(conn);
				errno = ret;
				return -1;
			}
			continue;
		}
		break;
	}
	if (record->length > length) {
		LOG_E("DTLS/SCTP receive buffer cannot hold a complete record.");
		free_record(record);
		fd_cnx_markerror(conn);
		errno = EMSGSIZE;
		return -1;
	}
	length = record->length;
	memcpy(buffer, record->data, length);
	if (record->data[0] == 20)
		conn->cc_sctp_dtls->ccs_delivered = 1;
	free_record(record);
	return length;
}

int fd_sctp_dtls_prepare(struct cnxctx *conn)
{
	struct sctp_authchunks *chunks;
	socklen_t length;
	unsigned int i;
	int data_authenticated = 0, ret;

	CHECK_PARAMS(conn->cc_sctp_para.dtls);
	CHECK_MALLOC(chunks = calloc(1, sizeof(*chunks) + 256));
	length = sizeof(*chunks) + 256;
	ret = getsockopt(conn->cc_socket, IPPROTO_SCTP, SCTP_PEER_AUTH_CHUNKS, chunks, &length);
	if (ret < 0) {
		ret = errno;
		LOG_E("Reading peer SCTP-AUTH requirements failed: %s", strerror(ret));
		free(chunks);
		return ret;
	}
	if (length >= sizeof(*chunks) && chunks->gauth_number_of_chunks <= length - sizeof(*chunks)) {
		for (i = 0; i < chunks->gauth_number_of_chunks; i++)
			if (chunks->gauth_chunks[i] == 0)
				data_authenticated = 1;
	}
	free(chunks);
	if (!data_authenticated) {
		LOG_E("DTLS/SCTP peer does not require authenticated DATA; connection rejected.");
		return ENOTSUP;
	}
	CHECK_MALLOC(conn->cc_sctp_dtls = calloc(1, sizeof(*conn->cc_sctp_dtls)));
	fd_list_init(&conn->cc_sctp_dtls->pending, NULL);
	fd_list_init(&conn->cc_sctp_dtls->early, NULL);
	conn->cc_sctp_dtls->handshaking = 1;
	CHECK_SYS(clock_gettime(CLOCK_MONOTONIC, &conn->cc_sctp_dtls->handshake_time));
	gnutls_transport_set_ptr(conn->cc_tls_para.session, conn);
	gnutls_transport_set_push_function(conn->cc_tls_para.session, dtls_push);
	gnutls_transport_set_pull_function(conn->cc_tls_para.session, dtls_pull);
	gnutls_transport_set_pull_timeout_function(conn->cc_tls_para.session, dtls_pull_timeout);
	gnutls_handshake_set_hook_function(conn->cc_tls_para.session, GNUTLS_HANDSHAKE_FINISHED,
					 GNUTLS_HOOK_PRE, handshake_hook);
	return 0;
}

int fd_sctp_dtls_handshake_done(struct cnxctx *conn)
{
	struct sctp_authkeyid old;
	struct sctp_dtls_ctx *ctx = conn->cc_sctp_dtls;
	CHECK_PARAMS(ctx && ctx->key_installed && ctx->key_active);
	CHECK_FCT(wait_drained(conn, 30000));
	memset(&old, 0, sizeof(old));
	CHECK_SYS(setsockopt(conn->cc_socket, IPPROTO_SCTP, SCTP_AUTH_DELETE_KEY, &old, sizeof(old)));
	ctx->handshaking = 0;
	if (ctx->endpoints_changed)
		CHECK_FCT(fd_event_send(fd_cnx_target_queue(conn), FDEVP_CNX_EP_CHANGE, 0, NULL));
	while (!FD_IS_LIST_EMPTY(&ctx->early)) {
		struct dtls_record *record = dequeue(conn, &ctx->early);
		CHECK_FCT(enqueue(conn, &ctx->pending, record));
	}
	return 0;
}

int fd_sctp_dtls_send(struct cnxctx *conn, const void *data, size_t length)
{
	ssize_t ret;
	size_t limit = gnutls_dtls_get_data_mtu(conn->cc_tls_para.session);
	if (length > limit) {
		LOG_E("Diameter message of %zu bytes exceeds DTLS/SCTP limit %zu on %s.",
		      length, limit, conn->cc_id);
		return EMSGSIZE;
	}
	do {
		ret = gnutls_record_send(conn->cc_tls_para.session, data, length);
	} while (ret == GNUTLS_E_INTERRUPTED);
	if (ret < 0 || (size_t)ret != length) {
		LOG_E("DTLS/SCTP send failed on %s: %s.", conn->cc_id,
		      ret < 0 ? gnutls_strerror(ret) : "incomplete record");
		fd_cnx_markerror(conn);
		return ENOTCONN;
	}
	return 0;
}

int fd_sctp_dtls_bye(struct cnxctx *conn)
{
	CHECK_FCT(wait_drained(conn, 5000));
	CHECK_SYS(clock_gettime(CLOCK_MONOTONIC, &conn->cc_sctp_dtls->bye_time));
	CHECK_GNUTLS_DO(gnutls_bye(conn->cc_tls_para.session, GNUTLS_SHUT_WR), return ENOTCONN);
	fd_cnx_addstate(conn, CC_STATUS_DTLS_BYE);
	return 0;
}

void fd_sctp_dtls_destroy(struct cnxctx *conn)
{
	struct sctp_dtls_ctx *ctx = conn->cc_sctp_dtls;
	if (!ctx)
		return;
	while (!FD_IS_LIST_EMPTY(&ctx->pending))
		free_record(dequeue(conn, &ctx->pending));
	while (!FD_IS_LIST_EMPTY(&ctx->early))
		free_record(dequeue(conn, &ctx->early));
	free(ctx);
	conn->cc_sctp_dtls = NULL;
}
