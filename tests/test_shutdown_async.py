import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure


daemon, test_app, test_netemul, dumps = sys.argv[1:5]


def scenario(peers):
    config = peers.directory / "shutdown-netemul.conf"
    config.write_text("latency_average = 5000 ms;\nlatency_deviation = 0 %;\n"
                      "dupl_proba = 0;\n")
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, [(test_netemul, config), (dumps, "0x22")])
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, configure(peers, 0, 2, mode="client"))],
                extra_config="NoRelay;")
    peers.wait_all_open()

    offsets = [len(peers.text(index)) for index in range(3)]
    peers.processes[0].send_signal(signal.SIGUSR1)
    sent = peers.wait(lambda: re.search(r"SEND ([0-9a-f]+) to", peers.text(0)[offsets[0]:]),
                      "client request")
    peers.wait(lambda: re.search(r"RCV from .*16777215/16777214 f:RP",
                                 peers.text(1)[offsets[1]:]),
               "request arriving at the relay")
    if "ECHO Test-Request received" in peers.text(2)[offsets[2]:]:
        raise AssertionError("request was not delayed by the relay")
    peers.processes[1].send_signal(signal.SIGTERM)
    peers.wait(lambda: "Initiating freeDiameter shutdown sequence" in peers.text(1)[offsets[1]:],
               "relay shutdown")
    peers.wait(lambda: "RECV %s (Ok) Status: 2001" % sent.group(1)
               in peers.text(0)[offsets[0]:], "answer callback", timeout=15)
    if "ECHO Test-Request received" not in peers.text(2)[offsets[2]:]:
        raise AssertionError("server did not process the retained request")
    peers.processes[1].wait(timeout=20)
    if peers.processes[1].returncode:
        raise AssertionError("relay failed during shutdown")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
