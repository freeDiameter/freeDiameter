#include "tests.h"
#include "../extensions/app_diameap/diameap_mysql.h"

MYSQL *db_conn;

int main(int argc, char **argv)
{
	struct eap_user user;
	const char *socket = getenv("FD_TEST_MYSQL_SOCKET");

	INIT_FD();
	CHECK(1, socket != NULL && *socket != '\0');
	db_conn = mysql_init(NULL);
	CHECK(1, db_conn != NULL);
	CHECK(1, mysql_real_connect(db_conn, "localhost", "fd_test", "",
				    "fd_diameap", 0, socket, 0) != NULL);
	memset(&user, 0, sizeof(user));
	CHECK(0, diameap_get_eap_user(&user, "test'user"));
	CHECK(0, strcmp((char *)user.userid, "test'user"));
	CHECK(0, strcmp((char *)user.password, "test-password"));
	CHECK(strlen("test'user"), user.useridLength);
	CHECK(strlen("test-password"), user.passwordLength);
	CHECK(4, user.proposed_eap_method);
	CHECK(0, user.proposed_eap_method_vendor);
	free(user.userid);
	free(user.password);
	memset(&user, 0, sizeof(user));
	CHECK(EINVAL, diameap_get_eap_user(&user, "disabled"));
	CHECK(EINVAL, diameap_get_eap_user(&user, "missing"));
	CHECK(EINVAL, diameap_get_eap_user(&user, "' OR 1=1 -- "));
	diameap_mysql_disconnect();
	PASSTEST();
}
