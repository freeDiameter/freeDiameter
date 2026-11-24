import os
import re
import signal
import socket
import sys
import time

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange
from sctp_netem_helpers import AckProbe, command, run_isolated, setup_namespace, statistics


daemon, test_app, probe, dumps, mode = sys.argv[1:6]


def scenario(peers):
    address = "fd-shutdown-probe-%d" % os.getpid()
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind("\0" + address)
    listener.listen(1)
    listener.settimeout(15)
    peers.after_stop(listener.close)

    def config(index):
        remote = 1 - index
        port = 9 if index == 0 else peers.ports[0]
        return ('ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; SctpSecDTLS; Port = %d; };\n'
                % (peers.identities[remote], port))

    peers.start(0, [(test_app, configure(peers, 0, 1)), (dumps, "0x22")],
                extra_config=config(0), connect_neighbors=False)
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "server startup")
    peers.start(1, [(test_app, configure(peers, 1, 0)), (dumps, "0x22")],
                extra_config=config(1), connect_neighbors=False,
                env={"LD_PRELOAD": probe, "FD_ACK_TEST_SOCKET": "@" + address,
                     "FD_ACK_TEST_MODE": "shutdown-peer"})
    control, _ = listener.accept()
    observer = AckProbe(control)
    peers.on_stop(lambda: observer.release() if observer.gated else None)
    peers.after_stop(control.close)
    observer.wait("CCS", timeout=30)
    peers.wait_all_open()
    peers.wait_transport()
    exchange(peers, 1, 0, True)

    offsets = [len(peers.text(index)) for index in (0, 1)]
    control.sendall(b"A")
    peers.processes[1].send_signal(signal.SIGUSR1)
    sent = peers.wait(lambda: re.search(r"SEND ([0-9a-f]+) to", peers.text(1)[offsets[1]:]),
                      "pending request")
    peers.wait(lambda: re.search(r"SND to .*'Test-Request'16777215/16777214",
                                 peers.text(1)[offsets[1]:]),
               "request to enter the SCTP sender")
    observer.wait("DATA", timeout=15)
    observer.gated = True
    command("tc", "qdisc", "add", "dev", "lo", "root", "netem", "loss", "100%")
    observer.release()
    observer.wait("DATA_SENT")
    if observer.wait("QUEUED") != 1:
        raise AssertionError("request DATA was not queued while traffic was blocked")
    peers.wait(lambda: statistics()["drops"] > 0, "dropped pending request")
    if ("ECHO Test-Request received" in peers.text(0)[offsets[0]:] or
            "RECV %s " % sent.group(1) in peers.text(1)[offsets[1]:]):
        raise AssertionError("the request completed before shutdown")

    stopping = (0, 1) if mode == "simultaneous" else (1,)
    for index in stopping:
        peers.processes[index].send_signal(signal.SIGTERM)
    peers.wait(lambda: all("Initiating freeDiameter shutdown sequence" in
                           peers.text(index)[offsets[index]:] for index in stopping),
               "daemons to initiate shutdown")
    command("tc", "qdisc", "change", "dev", "lo", "root", "netem", "delay", "100ms")
    delayed_packets = statistics()["packets"]

    deadline = time.monotonic() + 30
    for index in stopping:
        peers.processes[index].wait(timeout=max(0.1, deadline - time.monotonic()))
        if peers.processes[index].returncode:
            raise AssertionError("peer %d exited with status %d" %
                                 (index + 1, peers.processes[index].returncode))
    if statistics()["packets"] <= delayed_packets:
        raise AssertionError("netem did not process traffic after delay was applied")

    server, client = (peers.text(index)[offsets[index]:] for index in (0, 1))
    if not re.search(r"SND to .*'Test-Request'16777215/16777214", client):
        raise AssertionError("the outstanding Test-Request was not recorded as sent")
    answered = "RECV %s (Ok) Status: 2001" % sent.group(1) in client
    failover = bool(re.search(r"FAILOVER from .*'Test-Request'16777215/16777214", client))
    answer_dropped = bool(re.search(
        r"DROPPED 'Message lost because framework is terminating\.' "
        r".*16777215/16777214 f:-P--", client))
    server_callback = "ECHO Test-Request received" in server
    server_received = bool(re.search(r"RCV from .*16777215/16777214 f:RP--", server))
    request_drop = (r"DROPPED 'Message lost because framework is terminating\.' "
                    r".*16777215/16777214 f:RP")
    sender_request_dropped = bool(re.search(request_drop, client))
    receiver_request_dropped = bool(re.search(request_drop, server))
    if not answered and not failover and not answer_dropped:
        raise AssertionError("pending request had no observed answer, failover, or dropped answer")
    if (answered or answer_dropped) and not server_callback:
        raise AssertionError("answered request was not observed by the server")
    if answer_dropped and not re.search(
            r"RCV from .*16777215/16777214 f:-P--", client):
        raise AssertionError("dropped answer was not observed at the connection")
    print("shutdown=%s: answered=%s, failover=%s, server_received=%s, "
          "server_callback=%s, answer_dropped=%s, sender_request_dropped=%s, "
          "receiver_request_dropped=%s" %
          (mode, answered, failover, server_received, server_callback, answer_dropped,
           sender_request_dropped, receiver_request_dropped))


if __name__ == "__main__":
    if len(sys.argv) > 6 and sys.argv[6] == "--isolated":
        setup_namespace()
        run_scenario(daemon, scenario, 2, [(0, 1)])
    else:
        run_isolated([__file__, daemon, test_app, probe, dumps, mode])
