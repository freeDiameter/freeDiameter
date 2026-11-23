import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, rt_busypeers = sys.argv[1:4]


def scenario(peers):
    config = peers.directory / "busy.conf"
    config.write_text("RelayTimeout=500;\nRetryMaxPeers=1;\n")
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, [(rt_busypeers, config)])
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, configure(peers, 0, 2, mode="client"))],
                extra_config="NoRelay;")
    peers.wait_open(0, 1)

    server = peers.processes[2]
    peers.on_stop(lambda: server.send_signal(signal.SIGCONT) if server.poll() is None else None)
    server.send_signal(signal.SIGSTOP)
    client_offset, relay_offset, server_offset = (len(peers.text(i)) for i in range(3))
    peers.processes[0].send_signal(signal.SIGUSR1)
    peers.wait(lambda: re.search(r"RECV .*Status: 3004 ", peers.text(0)[client_offset:]),
               "TOO_BUSY answer after relay timeout")
    peers.wait(lambda: "giving up and sending error reply" in peers.text(1)[relay_offset:],
               "relay timeout")
    if "ECHO Test-Request" in peers.text(2)[server_offset:]:
        raise AssertionError("suspended server processed the request")
    server.send_signal(signal.SIGCONT)
    peers.wait(lambda: "ECHO Test-Request" in peers.text(2)[server_offset:],
               "server processing the expired request")
    exchange(peers, 0, 2, True)
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
