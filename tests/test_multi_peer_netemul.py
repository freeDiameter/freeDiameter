import re
import signal
import sys
import time

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, test_netemul = sys.argv[1:4]


def scenario(peers, duplicate=False):
    config = peers.directory / "netemul.conf"
    config.write_text("latency_average = %d ms;\n"
                      "latency_deviation = 0 %%;\n"
                      "dupl_proba = %d;\n" %
                      (0 if duplicate else 1000, 1 if duplicate else 0))
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay; AppServThreads = 1;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, [(test_netemul, config)])
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, configure(peers, 0, 2, mode="client"))],
                extra_config="NoRelay;")
    peers.wait_open(0, 1)

    server_offset = len(peers.text(2))
    if duplicate:
        client_offset = len(peers.text(0))
        peers.processes[0].send_signal(signal.SIGUSR1)
        peers.wait(
            lambda: peers.text(2)[server_offset:].count(
                "ECHO Test-Request received from '%s'" % peers.identities[0]) >= 2,
            "duplicate request at server",
        )
        peers.wait(
            lambda: "RECV " in peers.text(0)[client_offset:] and
                    re.search(r"\bin \d+\.\d+ sec", peers.text(0)[client_offset:]),
            "answer callback at client",
        )
    else:
        start = time.monotonic()
        exchange(peers, 0, 2, True)
        elapsed = time.monotonic() - start
        if elapsed < 0.9:
            raise AssertionError("request was not delayed: %.3f seconds" % elapsed)
    peers.wait_all_open()
    peers.wait_transport()
    for endpoint, other in ((0, 2), (2, 0)):
        if "Connected to '%s'" % peers.identities[other] in peers.text(endpoint):
            raise AssertionError("endpoints connected directly instead of through the relay")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
    run_scenario(daemon, lambda peers: scenario(peers, duplicate=True),
                 3, [(0, 1), (1, 2)])
