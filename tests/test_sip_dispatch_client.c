#include <freeDiameter/extension.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SIP_APP_ID 6
#define ALICE_AOR "sip:alice.smith@example.test"
#define SERVER_URI "sip:scscf1.example.test:5060"

static struct dict_object *uar, *lir, *sar, *mar, *result_code;
static struct dict_object *auth_app, *auth_state, *dest_host, *dest_realm;
static struct dict_object *sip_aor, *user_name, *visited_network, *auth_type;
static struct dict_object *server_uri, *capabilities, *mandatory_capability;
static struct dict_object *assignment_type, *data_available, *sip_method;

struct scenario {
	const char *name;
	uint32_t command;
	const char *aor;
	const char *network;
	uint32_t expected_result;
	int has_server;
};

static const struct scenario scenarios[] = {
	{ "authorization", 283, ALICE_AOR, "access.example.test", 2007, 1 },
	{ "identities-mismatch", 283, "sip:bob@example.test", NULL, 5033, 0 },
	{ "roaming-denied", 283, ALICE_AOR, "roaming.example.test", 5035, 0 },
	{ "location", 285, ALICE_AOR, NULL, 2001, 1 },
	{ "unknown-location", 285, "sip:missing@example.test", NULL, 5032, 0 },
	{ "multimedia-identities-mismatch", 286, "sip:bob@example.test", NULL, 5033, 0 },
	{ "server-assignment", 284, ALICE_AOR, NULL, 2001, 0 },
};

static int add_octets(struct msg *msg, struct dict_object *model, const char *text)
{
	struct avp *avp;
	union avp_value value;

	memset(&value, 0, sizeof(value));
	value.os.data = (unsigned char *)text;
	value.os.len = strlen(text);
	CHECK_FCT(fd_msg_avp_new(model, 0, &avp));
	CHECK_FCT(fd_msg_avp_setvalue(avp, &value));
	CHECK_FCT(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp));
	return 0;
}

static int add_u32(struct msg *msg, struct dict_object *model, uint32_t number)
{
	struct avp *avp;
	union avp_value value;

	memset(&value, 0, sizeof(value));
	value.u32 = number;
	CHECK_FCT(fd_msg_avp_new(model, 0, &avp));
	CHECK_FCT(fd_msg_avp_setvalue(avp, &value));
	CHECK_FCT(fd_msg_avp_add(msg, MSG_BRW_LAST_CHILD, avp));
	return 0;
}

static int send_scenario(size_t index);

static void check_answer(void *data, struct msg **message)
{
	size_t index = *(size_t *)data;
	const struct scenario *test = &scenarios[index];
	struct msg_hdr *msg_header;
	struct avp *avp = NULL, *child = NULL;
	struct avp_hdr *header;
	int valid = 1;

	free(data);
	if (fd_msg_hdr(*message, &msg_header) ||
	    msg_header->msg_code != test->command ||
	    (msg_header->msg_flags & CMD_FLAG_REQUEST) ||
	    msg_header->msg_appl != SIP_APP_ID ||
	    fd_msg_search_avp(*message, result_code, &avp) || !avp ||
	    fd_msg_avp_hdr(avp, &header) ||
	    header->avp_value->u32 != test->expected_result)
		valid = 0;

	avp = NULL;
	if (fd_msg_search_avp(*message, auth_app, &avp) || !avp ||
	    fd_msg_avp_hdr(avp, &header) || header->avp_value->u32 != SIP_APP_ID)
		valid = 0;
	avp = NULL;
	if (fd_msg_search_avp(*message, auth_state, &avp) || !avp ||
	    fd_msg_avp_hdr(avp, &header) || header->avp_value->u32 != 1)
		valid = 0;

	avp = NULL;
	if (fd_msg_search_avp(*message, server_uri, &avp)) {
		valid = 0;
	} else if (test->has_server) {
		if (!avp || fd_msg_avp_hdr(avp, &header) ||
		    header->avp_value->os.len != strlen(SERVER_URI) ||
		    memcmp(header->avp_value->os.data, SERVER_URI, strlen(SERVER_URI)))
			valid = 0;
	} else if (avp) {
		valid = 0;
	}

	avp = NULL;
	if (fd_msg_search_avp(*message, capabilities, &avp)) {
		valid = 0;
	} else if (test->has_server) {
		if (!avp || fd_msg_search_avp(avp, mandatory_capability, &child) ||
		    !child || fd_msg_avp_hdr(child, &header) || header->avp_value->u32 != 11)
			valid = 0;
	} else if (avp) {
		valid = 0;
	}

	fd_msg_free(*message);
	*message = NULL;
	if (!valid) {
		LOG_E("SIP_DISPATCH_FAIL %s", test->name);
		return;
	}
	LOG_N("SIP_DISPATCH_PASS %s", test->name);
	if (++index < sizeof(scenarios) / sizeof(scenarios[0]) && send_scenario(index))
		LOG_E("SIP_DISPATCH_FAIL unable to send %s", scenarios[index].name);
}

static int send_scenario(size_t index)
{
	const struct scenario *test = &scenarios[index];
	struct msg *request = NULL;
	struct dict_object *command;
	size_t *context = malloc(sizeof(*context));
	int ret;

	if (!context)
		return ENOMEM;
	*context = index;
	switch (test->command) {
	case 283: command = uar; break;
	case 284: command = sar; break;
	case 285: command = lir; break;
	case 286: command = mar; break;
	default:
		free(context);
		return EINVAL;
	}
	ret = fd_msg_new(command, MSGFL_ALLOC_ETEID, &request);
	if (!ret)
		ret = fd_msg_new_session(request, (os0_t)test->name, strlen(test->name));
	if (!ret)
		ret = fd_msg_add_origin(request, 0);
	if (!ret)
		ret = add_u32(request, auth_app, SIP_APP_ID);
	if (!ret)
		ret = add_u32(request, auth_state, 1);
	if (!ret)
		ret = add_octets(request, dest_host, "peer2.localdomain");
	if (!ret)
		ret = add_octets(request, dest_realm, "localdomain");
	if (!ret)
		ret = add_octets(request, sip_aor, test->aor);
	if (!ret && test->command != 285)
		ret = add_octets(request, user_name, "alice.smith");
	if (!ret && test->network)
		ret = add_octets(request, visited_network, test->network);
	if (!ret && test->command == 283)
		ret = add_u32(request, auth_type, 0);
	if (!ret && test->command == 286)
		ret = add_octets(request, sip_method, "REGISTER");
	if (!ret && test->command == 284)
		ret = add_u32(request, assignment_type, 1);
	if (!ret && test->command == 284)
		ret = add_u32(request, data_available, 1);
	if (!ret && test->command == 284)
		ret = add_octets(request, server_uri, SERVER_URI);
	if (!ret)
		ret = fd_msg_send(&request, check_answer, context);
	if (ret) {
		if (request)
			fd_msg_free(request);
		free(context);
	}
	return ret;
}

static void start_tests(void)
{
	if (send_scenario(0))
		LOG_E("SIP_DISPATCH_FAIL unable to send first request");
}

static int client_init(char *config)
{
	(void)config;
#define FIND(type, name, target) \
	CHECK_FCT(fd_dict_search(fd_g_config->cnf_dict, type, \
		type == DICT_COMMAND ? CMD_BY_NAME : AVP_BY_NAME, name, &target, ENOENT))
	FIND(DICT_COMMAND, "User-Authorization-Request", uar);
	FIND(DICT_COMMAND, "Location-Info-Request", lir);
	FIND(DICT_COMMAND, "Server-Assignment-Request", sar);
	FIND(DICT_COMMAND, "Multimedia-Auth-Request", mar);
	FIND(DICT_AVP, "Result-Code", result_code);
	FIND(DICT_AVP, "Auth-Application-Id", auth_app);
	FIND(DICT_AVP, "Auth-Session-State", auth_state);
	FIND(DICT_AVP, "Destination-Host", dest_host);
	FIND(DICT_AVP, "Destination-Realm", dest_realm);
	FIND(DICT_AVP, "SIP-AOR", sip_aor);
	FIND(DICT_AVP, "User-Name", user_name);
	FIND(DICT_AVP, "SIP-Visited-Network-Id", visited_network);
	FIND(DICT_AVP, "SIP-User-Authorization-Type", auth_type);
	FIND(DICT_AVP, "SIP-Server-URI", server_uri);
	FIND(DICT_AVP, "SIP-Server-Capabilities", capabilities);
	FIND(DICT_AVP, "SIP-Mandatory-Capability", mandatory_capability);
	FIND(DICT_AVP, "SIP-Server-Assignment-Type", assignment_type);
	FIND(DICT_AVP, "SIP-User-Data-Already-Available", data_available);
	FIND(DICT_AVP, "SIP-Method", sip_method);
#undef FIND
	CHECK_FCT(fd_event_trig_regcb(SIGUSR1, "test_sip_dispatch.start", start_tests));
	LOG_N("SIP_DISPATCH_READY");
	return 0;
}

EXTENSION_ENTRY("test_sip_dispatch_client", client_init, "dict_sip");
