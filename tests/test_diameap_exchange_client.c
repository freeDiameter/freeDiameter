#include <freeDiameter/extension.h>

#include <errno.h>
#include <gcrypt.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define EAP_APPLICATION_ID 5
#define TEST_USERNAME "test'user"
#define TEST_PASSWORD "test-password"
#define TEST_SERVER "peer2.localdomain"
#define TEST_REALM "localdomain"

static struct dict_object *diameap_request;
static struct dict_object *session_id_avp;
static struct dict_object *destination_host_avp;
static struct dict_object *destination_realm_avp;
static struct dict_object *auth_application_id_avp;
static struct dict_object *auth_request_type_avp;
static struct dict_object *user_name_avp;
static struct dict_object *eap_payload_avp;
static struct dict_object *result_code_avp;

struct exchange {
	unsigned char *session_id;
	size_t session_id_len;
	int expect_success;
	int challenge_received;
};

static int add_octets(struct msg *msg, struct dict_object *object,
		const unsigned char *data, size_t length)
{
	struct avp *avp = NULL;
	union avp_value value;

	memset(&value, 0, sizeof(value));
	value.os.data = (unsigned char *)data;
	value.os.len = length;
	CHECK_FCT(fd_msg_avp_new(object, 0, &avp));
	CHECK_FCT(fd_msg_avp_setvalue(avp, &value));
	CHECK_FCT(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp));
	return 0;
}

static int add_u32(struct msg *msg, struct dict_object *object, uint32_t number)
{
	struct avp *avp = NULL;
	union avp_value value;

	memset(&value, 0, sizeof(value));
	value.u32 = number;
	CHECK_FCT(fd_msg_avp_new(object, 0, &avp));
	CHECK_FCT(fd_msg_avp_setvalue(avp, &value));
	CHECK_FCT(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp));
	return 0;
}

static int add_common_request_avps(struct msg *msg)
{
	CHECK_FCT(fd_msg_add_origin(msg, 0));
	CHECK_FCT(add_octets(msg, destination_host_avp,
			(const unsigned char *)TEST_SERVER, strlen(TEST_SERVER)));
	CHECK_FCT(add_octets(msg, destination_realm_avp,
			(const unsigned char *)TEST_REALM, strlen(TEST_REALM)));
	CHECK_FCT(add_u32(msg, auth_application_id_avp, EAP_APPLICATION_ID));
	CHECK_FCT(add_u32(msg, auth_request_type_avp, 1));
	return 0;
}

static void exchange_answer(void *data, struct msg **message);
static int start_exchange(int expect_success, const char *suffix);

static int send_response(struct exchange *exchange, const unsigned char *payload,
		size_t payload_len)
{
	struct msg *request = NULL;
	struct avp *sid = NULL;
	union avp_value value;
	int ret;

	ret = fd_msg_new(diameap_request, MSGFL_ALLOC_ETEID, &request);
	if (ret)
		return ret;
	memset(&value, 0, sizeof(value));
	value.os.data = exchange->session_id;
	value.os.len = exchange->session_id_len;
	ret = fd_msg_avp_new(session_id_avp, 0, &sid);
	if (!ret)
		ret = fd_msg_avp_setvalue(sid, &value);
	if (!ret)
		ret = fd_msg_avp_add(request, MSG_BRW_FIRST_CHILD, sid);
	if (!ret)
		ret = add_common_request_avps(request);
	if (!ret)
		ret = add_octets(request, eap_payload_avp, payload, payload_len);
	if (!ret)
		ret = fd_msg_send(&request, exchange_answer, exchange);
	if (ret && request)
		fd_msg_free(request);
	return ret;
}

static int answer_values(struct msg *message, uint32_t *result,
		const unsigned char **payload, size_t *payload_len)
{
	struct avp *avp = NULL;
	struct avp_hdr *header = NULL;

	CHECK_FCT(fd_msg_search_avp(message, result_code_avp, &avp));
	if (!avp)
		return EINVAL;
	CHECK_FCT(fd_msg_avp_hdr(avp, &header));
	*result = header->avp_value->u32;

	avp = NULL;
	CHECK_FCT(fd_msg_search_avp(message, eap_payload_avp, &avp));
	if (!avp)
		return EINVAL;
	CHECK_FCT(fd_msg_avp_hdr(avp, &header));
	*payload = header->avp_value->os.data;
	*payload_len = header->avp_value->os.len;
	return 0;
}

static void finish_exchange(struct exchange *exchange, int passed)
{
	if (passed)
		LOG_N("DIAMEAP_EXCHANGE_PASS rejected-bad-password");
	else
		LOG_E("DIAMEAP_EXCHANGE_FAIL expected %s authentication",
				exchange->expect_success ? "successful" : "rejected");
	free(exchange->session_id);
	free(exchange);
}

static void exchange_answer(void *data, struct msg **message)
{
	struct exchange *exchange = data;
	struct avp *session = NULL;
	struct avp_hdr *session_header = NULL;
	const unsigned char *payload = NULL;
	size_t payload_len = 0;
	uint32_t result = 0;
	int passed = 0;

	if (fd_msg_search_avp(*message, session_id_avp, &session) || !session
			|| fd_msg_avp_hdr(session, &session_header)
			|| session_header->avp_value->os.len != exchange->session_id_len
			|| memcmp(session_header->avp_value->os.data, exchange->session_id,
					exchange->session_id_len)
			|| answer_values(*message, &result, &payload, &payload_len))
		goto done;

	if (result == 1001) {
		unsigned char response[22];
		unsigned char digest_input[1 + sizeof("incorrect-password") - 1 + 16];
		unsigned char digest[16];
		const char *password = exchange->expect_success
				? TEST_PASSWORD : "incorrect-password";
		size_t password_len = strlen(password);

		if (exchange->challenge_received || payload_len != 22
				|| payload[0] != 1 || payload[2] != 0 || payload[3] != 22
				|| payload[4] != 4
				|| payload[5] != 16)
			goto done;
		exchange->challenge_received = 1;
		response[0] = 2;
		response[1] = payload[1];
		response[2] = 0;
		response[3] = sizeof(response);
		response[4] = 4;
		response[5] = 16;
		digest_input[0] = payload[1];
		memcpy(digest_input + 1, password, password_len);
		memcpy(digest_input + 1 + password_len, payload + 6, 16);
		gcry_md_hash_buffer(GCRY_MD_MD5, digest, digest_input,
				1 + password_len + 16);
		memcpy(response + 6, digest, sizeof(digest));
		fd_msg_free(*message);
		*message = NULL;
		if (send_response(exchange, response, sizeof(response)))
			finish_exchange(exchange, 0);
		return;
	}

	if (!exchange->challenge_received
			|| result != (exchange->expect_success ? 2001 : 4001)
			|| payload_len != 4
			|| payload[0] != (exchange->expect_success ? 3 : 4)
			|| payload[2] != 0 || payload[3] != 4)
		goto done;
	passed = 1;

done:
	fd_msg_free(*message);
	*message = NULL;
	if (passed && exchange->expect_success) {
		LOG_N("DIAMEAP_EXCHANGE_PASS success");
		free(exchange->session_id);
		free(exchange);
		if (start_exchange(0, "diameap-exchange-reject"))
			LOG_E("DIAMEAP_EXCHANGE_FAIL could not start rejection exchange");
		return;
	}
	finish_exchange(exchange, passed);
}

static int start_exchange(int expect_success, const char *suffix)
{
	struct exchange *exchange = calloc(1, sizeof(*exchange));
	struct msg *request = NULL;
	struct avp *session = NULL;
	struct avp_hdr *header = NULL;
	unsigned char identity[5 + sizeof(TEST_USERNAME) - 1];

	if (!exchange)
		return ENOMEM;
	exchange->expect_success = expect_success;
	if (fd_msg_new(diameap_request, MSGFL_ALLOC_ETEID, &request)
			|| fd_msg_new_session(request, (os0_t)suffix, strlen(suffix))
			|| fd_msg_search_avp(request, session_id_avp, &session)
			|| fd_msg_avp_hdr(session, &header))
		goto error;
	exchange->session_id = malloc(header->avp_value->os.len);
	if (!exchange->session_id)
		goto error;
	exchange->session_id_len = header->avp_value->os.len;
	memcpy(exchange->session_id, header->avp_value->os.data,
			exchange->session_id_len);
	if (add_common_request_avps(request)
			|| add_octets(request, user_name_avp,
					(const unsigned char *)TEST_USERNAME,
					sizeof(TEST_USERNAME) - 1))
		goto error;
	identity[0] = 2;
	identity[1] = 1;
	identity[2] = 0;
	identity[3] = sizeof(identity);
	identity[4] = 1;
	memcpy(identity + 5, TEST_USERNAME, sizeof(TEST_USERNAME) - 1);
	if (add_octets(request, eap_payload_avp, identity, sizeof(identity))
			|| fd_msg_send(&request, exchange_answer, exchange))
		goto error;
	return 0;

error:
	if (request)
		fd_msg_free(request);
	free(exchange->session_id);
	free(exchange);
	return EINVAL;
}

static void begin_exchanges(void)
{
	if (start_exchange(1, "diameap-exchange-success"))
		LOG_E("DIAMEAP_EXCHANGE_FAIL could not start success exchange");
}

static int exchange_init(char *config)
{
	(void)config;
	CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_COMMAND, CMD_BY_NAME,
			"Diameter-EAP-Request", &diameap_request, ENOENT));
#define FIND_AVP(name, target) \
	CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, DICT_AVP, AVP_BY_NAME, \
			name, &target, ENOENT))
	FIND_AVP("Session-Id", session_id_avp);
	FIND_AVP("Destination-Host", destination_host_avp);
	FIND_AVP("Destination-Realm", destination_realm_avp);
	FIND_AVP("Auth-Application-Id", auth_application_id_avp);
	FIND_AVP("Auth-Request-Type", auth_request_type_avp);
	FIND_AVP("User-Name", user_name_avp);
	FIND_AVP("EAP-Payload", eap_payload_avp);
	FIND_AVP("Result-Code", result_code_avp);
#undef FIND_AVP
	CHECK_FCT(gcry_check_version(NULL) ? 0 : EINVAL);
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, "test_diameap_exchange.start",
			begin_exchanges));
	LOG_N("DIAMEAP_EXCHANGE_READY");
	return 0;
}

EXTENSION_ENTRY("test_diameap_exchange_client", exchange_init, "dict_eap");
