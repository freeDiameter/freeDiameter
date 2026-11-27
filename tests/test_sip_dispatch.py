"""Exercise app_sip request dispatch with the disposable SIP database fixture."""

import os
import signal
import subprocess
import sys

from multi_peer_harness import run_scenario

daemon, client_extension, sip_extension, dict_sip = sys.argv[1:]
cases = ("authorization", "identities-mismatch", "roaming-denied",
         "location", "unknown-location", "multimedia-identities-mismatch",
         "server-assignment")


def scenario(peers):
    database_socket = os.environ.get("FD_TEST_MYSQL_SOCKET")
    if not database_socket:
        raise RuntimeError("FD_TEST_MYSQL_SOCKET must name the disposable MariaDB socket")

    def sql(statement):
        return subprocess.run(
            ["mariadb", "--no-defaults", "--socket=" + database_socket,
             "--user=fd_test", "--batch", "--skip-column-names", "fd_sip", "-e", statement],
            check=True, text=True, stdout=subprocess.PIPE, timeout=10,
        ).stdout.strip()

    sql("UPDATE ds_users SET authentication_pending=1, registrated=0 "
        "WHERE username='alice.smith'")

    config = peers.directory / "sip.conf"
    config.write_text(
        'mode = DSSERVER;\n'
        'datasource = MYSQL;\n'
        'mysql_login = "fd_test";\n'
        'mysql_database = "fd_sip";\n'
        'mysql_server = "localhost";\n'
        'mysql_port = 0;\n'
        'rtr_port = 0;\n'
        'ppr_port = 0;\n'
    )
    peers.start(1, [(dict_sip, None), (sip_extension, config)],
                env={"MYSQL_UNIX_PORT": database_socket})
    peers.wait(lambda: "Local server address(es)" in peers.text(1), "SIP server startup")
    peers.start(0, [(dict_sip, None), (client_extension, None)])
    peers.wait(lambda: "SIP_DISPATCH_READY" in peers.text(0), "SIP client startup")
    peers.wait_open(0, 1)

    offset = len(peers.text(0))
    peers.processes[0].send_signal(signal.SIGUSR1)
    outcome = peers.wait(
        lambda: ("failure" if "SIP_DISPATCH_FAIL" in peers.text(0)[offset:]
                 else "success" if "SIP_DISPATCH_PASS " + cases[-1] in peers.text(0)[offset:]
                 else None),
        "SIP request answers", timeout=30,
    )
    output = peers.text(0)[offset:]
    if outcome != "success" or any("SIP_DISPATCH_PASS " + name not in output for name in cases):
        raise AssertionError(output)
    if sql("SELECT CONCAT(authentication_pending, ':', registrated) FROM ds_users "
           "WHERE username='alice.smith'") != "0:1":
        raise AssertionError("server assignment did not update registration state")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [(0, 1)])
