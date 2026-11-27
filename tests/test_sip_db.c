#include "tests.h"
#include "../extensions/app_sip/app_sip.h"
#include <mysql.h>

struct as_conf *as_conf;
struct app_sip_dict sip_dict;
struct session_handler *ds_sess_hdl;

#define TEST_USERNAME "alice.smith"
#define TEST_AOR "sip:alice.smith@example.test"
#define TEST_NETWORK "access.example.test"

static void expect_sql_value(const char *query, const char *expected)
{
	MYSQL_RES *result;
	MYSQL_ROW row;

	CHECK(0, mysql_query(conn, query));
	result = mysql_store_result(conn);
	CHECK(1, result != NULL);
	row = mysql_fetch_row(result);
	CHECK(1, row != NULL && row[0] != NULL);
	CHECK(0, strcmp(row[0], expected));
	mysql_free_result(result);
}

int main(int argc, char **argv)
{
	const char *socket = getenv("FD_TEST_MYSQL_SOCKET");
	const char *database = getenv("FD_TEST_MYSQL_DATABASE");
	const char *user = getenv("FD_TEST_MYSQL_USER");
	const char *password = getenv("FD_TEST_MYSQL_PASSWORD");
	unsigned char *server_uri = NULL;
	size_t server_uri_len = 0;
	char *diameter_uri = NULL;
	size_t diameter_uri_len = 0;
	char retrieved_password[128];

	INIT_FD();
	CHECK(1, socket != NULL && *socket != '\0');
	CHECK(1, database != NULL && *database != '\0');
	CHECK(1, user != NULL && *user != '\0');

	as_conf = calloc(1, sizeof(*as_conf));
	CHECK(1, as_conf != NULL);
	as_conf->datasource = ASMYSQL;

	conn = mysql_init(NULL);
	CHECK(1, conn != NULL);
	CHECK(1, mysql_real_connect(conn, "localhost", user,
				    password ? password : "", database,
				    0, socket, 0) != NULL);

	/* These writes make the test repeatable; use only the disposable fixture database. */
	CHECK(0, mysql_query(conn,
		"UPDATE ds_users SET sip_server_uri='sip:scscf1.example.test:5060', "
		"temp_sip_server_uri='', authentication_pending=0, registrated=0 "
		"WHERE username='" TEST_USERNAME "'"));

	CHECK(0, get_password((const unsigned char *)TEST_USERNAME,
			      strlen(TEST_USERNAME), retrieved_password,
			      sizeof(retrieved_password)));
	CHECK(0, strcmp(retrieved_password, "correct-horse-battery-staple"));
	CHECK(0, get_password((const unsigned char *)TEST_USERNAME,
			      strlen(TEST_USERNAME), NULL, 0));
	CHECK(2, get_password((const unsigned char *)"missing.user",
			      strlen("missing.user"), retrieved_password,
			      sizeof(retrieved_password)));

	CHECK(0, exist_username((const unsigned char *)TEST_AOR, strlen(TEST_AOR)));
	CHECK(1, exist_username((const unsigned char *)"sip:missing@example.test",
				strlen("sip:missing@example.test")));
	CHECK(2, exist_username((const unsigned char *)"sip:", strlen("sip:")));
	CHECK(0, check_sipaor((const unsigned char *)TEST_USERNAME,
			      strlen(TEST_USERNAME), TEST_AOR, strlen(TEST_AOR)));
	CHECK(1, check_sipaor((const unsigned char *)TEST_USERNAME,
			      strlen(TEST_USERNAME), "sip:bob@example.test",
			      strlen("sip:bob@example.test")));

	CHECK(0, get_sipserver_uri((const unsigned char *)TEST_AOR, strlen(TEST_AOR),
				   &server_uri, &server_uri_len));
	CHECK(strlen("sip:scscf1.example.test:5060"), server_uri_len);
	CHECK(0, strcmp((const char *)server_uri, "sip:scscf1.example.test:5060"));
	free(server_uri);

	CHECK(0, get_diameter_uri((const unsigned char *)TEST_AOR, strlen(TEST_AOR),
				 &diameter_uri, &diameter_uri_len));
	CHECK(strlen("aaa://hss.example.test"), diameter_uri_len);
	CHECK(0, strcmp(diameter_uri, "aaa://hss.example.test"));
	free(diameter_uri);

	CHECK(0, allow_roaming((const unsigned char *)TEST_USERNAME,
			       strlen(TEST_USERNAME), TEST_NETWORK, strlen(TEST_NETWORK)));
	CHECK(1, allow_roaming((const unsigned char *)TEST_USERNAME,
			       strlen(TEST_USERNAME), "roaming.example.test",
			       strlen("roaming.example.test")));

	CHECK(0, set_pending_flag((const unsigned char *)TEST_USERNAME,
				  strlen(TEST_USERNAME)));
	expect_sql_value("SELECT authentication_pending FROM ds_users WHERE username='"
			 TEST_USERNAME "'", "1");
	CHECK(0, clear_pending_flag((const unsigned char *)TEST_USERNAME,
				   strlen(TEST_USERNAME)));
	expect_sql_value("SELECT authentication_pending FROM ds_users WHERE username='"
			 TEST_USERNAME "'", "0");
	expect_sql_value("SELECT registrated FROM ds_users WHERE username='"
			 TEST_USERNAME "'", "1");

	CHECK(0, set_sipserver_uri((const unsigned char *)TEST_USERNAME,
				   strlen(TEST_USERNAME),
				   (const unsigned char *)"sip:edge.example.test:5060",
				   strlen("sip:edge.example.test:5060")));
	expect_sql_value("SELECT temp_sip_server_uri FROM ds_users WHERE username='"
			 TEST_USERNAME "'", "sip:edge.example.test:5060");
	CHECK(0, set_real_sipserver_uri((const unsigned char *)TEST_USERNAME,
					strlen(TEST_USERNAME),
					(const unsigned char *)"sip:edge.example.test:5060",
					strlen("sip:edge.example.test:5060")));
	expect_sql_value("SELECT sip_server_uri FROM ds_users WHERE username='"
			 TEST_USERNAME "'", "sip:edge.example.test:5060");

	close_mysql_connection();
	free(as_conf);
	PASSTEST();
}
