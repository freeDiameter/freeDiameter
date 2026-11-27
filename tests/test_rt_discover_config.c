#include "tests.h"
#include "../extensions/rt_discover/rt_discover.h"

static void check_config(const char *text, int expected)
{
	char path[] = BUILD_DIR "/rd-config-XXXXXX";
	struct fd_list realms;
	int fd;
	FILE *file;

	fd_list_init(&realms, NULL);
	fd = mkstemp(path);
	CHECK(1, fd >= 0);
	file = fdopen(fd, "w");
	CHECK(1, file != NULL);
	CHECK(1, fputs(text, file) >= 0);
	CHECK(0, fclose(file));
	CHECK(expected, rd_config_load(path, &realms));
	if (expected) {
		CHECK(1, FD_IS_LIST_EMPTY(&realms));
	} else if (*text) {
		struct rd_realm *realm;
		struct rd_allowed_peer *first, *second;

		CHECK(0, FD_IS_LIST_EMPTY(&realms));
		realm = realms.next->o;
		CHECK(0, strcmp("example.org", realm->name));
		CHECK(1, realm->chain.next == &realms);
		CHECK(0, FD_IS_LIST_EMPTY(&realm->peers));
		first = realm->peers.next->o;
		CHECK(0, strcmp("p1.example.org", first->identity));
		CHECK(1, first->chain.next != &realm->peers);
		second = first->chain.next->o;
		CHECK(0, strcmp("p2.example.org", second->identity));
		CHECK(1, second->chain.next == &realm->peers);
		if (strstr(text, "TcTimer")) {
			CHECK(FD_DISCOVERY_IPV4, realm->options.ip_family);
			CHECK(FD_DISCOVERY_TLS_TCP, realm->transport);
			CHECK(60, realm->options.tc_timer);
			CHECK(45, realm->options.tw_timer);
			CHECK(0, strcmp("NORMAL", realm->options.tls_priority));
			CHECK(0, strcmp("origin.example.org", realm->options.peer_realm));
		}
		if (strstr(text, "No_IP;"))
			CHECK(FD_DISCOVERY_IPV6, realm->options.ip_family);
		if (strstr(text, "No_TCP;"))
			CHECK(FD_DISCOVERY_DTLS_SCTP, realm->transport);
	}
	rd_config_free(&realms);
	CHECK(0, unlink(path));
}

int main(int argc, char *argv[])
{
	INIT_FD();
	check_config("", 0);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; "
		     "Peer = \"p2.example.org\"; };", 0);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; "
		     "Peer = \"p2.example.org\"; No_IPv6; No_SCTP; Prefer_TCP; "
		     "TcTimer = 60; TwTimer = 45; TLS_Prio = \"NORMAL\"; "
		     "Realm = \"origin.example.org\"; };", 0);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; "
		     "Peer = \"p2.example.org\"; No_IP; };", 0);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; No_IP; No_IPv6; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; No_TCP; No_SCTP; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; TcTimer = 999999999999999999999; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; TcTimer = 10737419; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; TwTimer = 2147483647; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; Realm = \"bad realm\"; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; Realm = \"a.test\"; Realm = \"b.test\"; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; TLS_Prio = \"NORMAL\"; TLS_Prio = \"NORMAL\"; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; TLS_old_method; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; Port = 3868; };", EINVAL);
#if defined(ENABLE_DTLS_SCTP) && !defined(DISABLE_SCTP)
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; "
		     "Peer = \"p2.example.org\"; No_TCP; };", 0);
#else
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; No_TCP; };", EINVAL);
#endif
	fd_g_config->cnf_flags.no_tcp = 1;
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; No_SCTP; };", EINVAL);
	fd_g_config->cnf_flags.no_tcp = 0;
	fd_g_config->cnf_flags.no_ip6 = 1;
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; No_IP; };", EINVAL);
	fd_g_config->cnf_flags.no_ip6 = 0;
	check_config("Realm = \"example.org\" {};", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; }; "
		     "Realm = \"EXAMPLE.ORG\" { Peer = \"p2.example.org\"; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; "
		     "Peer = \"P1.example.org\"; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"bad name\"; };", EINVAL);
	check_config("Realm = \"example.org\" { Peer = \"p1.example.org\"; ", EINVAL);
	PASSTEST();
}
