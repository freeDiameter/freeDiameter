"""End-to-end EAP-MD5 exchange against a disposable DiamEAP MariaDB fixture.

The database must contain the schema from extensions/app_diameap/diameap.sql and
an active users row for username ``test'user`` with password ``test-password``,
EAP method 4, and vendor 0. The fixture credentials are fd_test (empty
password) in database fd_diameap. Set FD_TEST_MYSQL_SOCKET to its Unix socket;
the daemon also receives MYSQL_UNIX_PORT because app_diameap connects through
the MariaDB client library's default localhost socket.
"""

import os
import signal
import sys

from multi_peer_harness import quote, run_scenario


(daemon, client_extension, diameap_extension, identity_plugin, md5_plugin,
 dict_nasreq, dict_eap) = sys.argv[1:]


def scenario(peers):
    database_socket = os.environ.get("FD_TEST_MYSQL_SOCKET")
    if not database_socket:
        raise RuntimeError("FD_TEST_MYSQL_SOCKET must name the disposable MariaDB socket")

    extension_config = peers.directory / "diameap.conf"
    extension_config.write_text(
        'DiamEAP_MySQL = "fd_test", "", "localhost", "fd_diameap";\n'
        'Load_plugin = "EAP Identity":1:0:%s:"";\n'
        'Load_plugin = "EAP MD5":4:0:%s:"";\n'
        % (quote(identity_plugin), quote(md5_plugin))
    )

    peers.start(
        1,
        [(dict_nasreq, None), (dict_eap, None), (diameap_extension, extension_config)],
        env={"MYSQL_UNIX_PORT": database_socket},
    )
    peers.wait(
        lambda: "Local server address(es)" in peers.text(1),
        "DiamEAP startup and MariaDB connection",
    )
    peers.start(0, [(dict_nasreq, None), (dict_eap, None), (client_extension, None)])
    peers.wait(lambda: "DIAMEAP_EXCHANGE_READY" in peers.text(0),
               "EAP exchange client startup")
    peers.wait_open(0, 1)

    offset = len(peers.text(0))
    peers.processes[0].send_signal(signal.SIGUSR1)
    outcome = peers.wait(
        lambda: (
            "DIAMEAP_EXCHANGE_FAIL"
            if "DIAMEAP_EXCHANGE_FAIL" in peers.text(0)[offset:]
            else "DIAMEAP_EXCHANGE_PASS rejected-bad-password"
            if "DIAMEAP_EXCHANGE_PASS rejected-bad-password"
            in peers.text(0)[offset:]
            else None
        ),
        "successful and rejected EAP-MD5 exchanges",
        timeout=30,
    )
    if outcome != "DIAMEAP_EXCHANGE_PASS rejected-bad-password":
        raise AssertionError(peers.text(0)[offset:])
    if "DIAMEAP_EXCHANGE_PASS success" not in peers.text(0)[offset:]:
        raise AssertionError("the valid EAP-MD5 exchange did not complete")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [(0, 1)])
