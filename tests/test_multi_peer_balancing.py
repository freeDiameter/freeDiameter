import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure


daemon, test_app, extension, dbg_rt, dbg_loglevel, name = sys.argv[1:7]


def scenario(peers):
    log_config = peers.directory / "loglevel.conf"
    log_config.write_text("LogLevel=1;\n")
    for index in (2, 3):
        peers.start(index, [(test_app, configure(peers, index, 0, mode="server"))],
                    extra_config="NoRelay;")
        peers.wait(lambda: "Local server address(es)" in peers.text(index), "server startup")
    peers.start(1, [(dbg_loglevel, log_config), (extension, None), (dbg_rt, None)])
    peers.wait_open(1, 2)
    peers.wait_open(1, 3)
    config = peers.directory / "client.conf"
    config.write_text('mode = client;\ndest-realm = "localdomain";\n')
    peers.start(0, [(test_app, config)], extra_config="NoRelay;")
    peers.wait_open(0, 1)

    client_offset, relay_offset = len(peers.text(0)), len(peers.text(1))
    peers.processes[0].send_signal(signal.SIGUSR1)
    answer = peers.wait(lambda: re.search(
        r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From '(peer[34]\.localdomain)'",
        peers.text(0)[client_offset:]), "answer from a selected server")
    sent = re.search(r"SEND ([0-9a-f]+) to 'localdomain' \(-\)",
                     peers.text(0)[client_offset:])
    if not sent or sent.group(1) != answer.group(1):
        raise AssertionError("answer did not echo the sent value")
    scores = {}
    for index in (2, 3):
        match = re.search(r"\[dbg_rt\]\s+(-?\d+) -\s+%s" %
                          re.escape(peers.identities[index]), peers.text(1)[relay_offset:])
        if not match:
            raise AssertionError("missing candidate score for " + peers.identities[index])
        scores[peers.identities[index]] = int(match.group(1))
    expected = [13, 14] if name == "rt_randomize" else [14, 15]
    if sorted(scores.values()) != expected or scores[answer.group(2)] != expected[1]:
        raise AssertionError("unexpected candidate scores or selected server: %s" % scores)
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 4, [(0, 1), (1, 2), (1, 3)])
