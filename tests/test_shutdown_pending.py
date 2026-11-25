import re
import signal
import sys
import time

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange
from sctp_netem_helpers import command, run_isolated, setup_namespace, statistics


daemon, test_app, dumps, mode = sys.argv[1:5]


def scenario(peers):
    peers.start(0, [(test_app, configure(peers, 0, 1)), (dumps, "0x22")])
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "server startup")
    peers.start(1, [(test_app, configure(peers, 1, 0)), (dumps, "0x22")])
    peers.wait_all_open()
    peers.wait_transport()
    exchange(peers, 1, 0, True)

    offsets = [len(peers.text(index)) for index in (0, 1)]
    command("tc", "qdisc", "add", "dev", "lo", "root", "netem", "loss", "100%")
    peers.processes[1].send_signal(signal.SIGUSR1)
    sent = peers.wait(lambda: re.search(r"SEND ([0-9a-f]+) to", peers.text(1)[offsets[1]:]),
                      "pending request")
    peers.wait(lambda: re.search(r"SND to .*'Test-Request'16777215/16777214",
                                 peers.text(1)[offsets[1]:]), "request to enter transport")
    peers.wait(lambda: statistics()["drops"] > 0, "dropped pending request")
    if "ECHO Test-Request received" in peers.text(0)[offsets[0]:]:
        raise AssertionError("request completed before shutdown")

    stopping = (0, 1) if mode == "simultaneous" else (1,)
    for index in stopping:
        peers.processes[index].send_signal(signal.SIGTERM)
    peers.wait(lambda: all("Initiating freeDiameter shutdown sequence" in
                           peers.text(index)[offsets[index]:] for index in stopping),
               "daemons to initiate shutdown")
    if mode != "deadline":
        command("tc", "qdisc", "change", "dev", "lo", "root", "netem", "delay", "100ms")

    deadline = time.monotonic() + 30
    for index in stopping:
        peers.processes[index].wait(timeout=max(0.1, deadline - time.monotonic()))
        if peers.processes[index].returncode:
            raise AssertionError("peer %d exited with status %d" %
                                 (index + 1, peers.processes[index].returncode))

    server, client = (peers.text(index)[offsets[index]:] for index in (0, 1))
    if mode == "deadline":
        if "Shutdown drain did not complete: Connection timed out" not in client:
            raise AssertionError("shutdown did not report the drain deadline")
        if not re.search(r"RECV no_Test-AVP Status: 3002 ", client):
            raise AssertionError("the unresolved request did not reach its error callback")
    elif ("ECHO Test-Request received" not in server or
          "RECV %s (Ok) Status: 2001" % sent.group(1) not in client):
        raise AssertionError("pending request did not complete during shutdown")


if __name__ == "__main__":
    if "--isolated" in sys.argv:
        setup_namespace(enable_auth=False)
        run_scenario(daemon, scenario, 2, [(0, 1)], requires_sctp_auth=False)
    else:
        run_isolated([__file__, daemon, test_app, dumps, mode], require_auth=False)
