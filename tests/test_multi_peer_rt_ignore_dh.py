import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure


daemon, test_app, rt_ignore_dh, dbg_loglevel = sys.argv[1:5]


def scenario(peers):
    log_config = peers.directory / "loglevel.conf"
    log_config.write_text("LogLevel=1;\n")
    client_config = peers.directory / "client.conf"
    client_config.write_text('mode = client;\ndest-host = "virtual.localdomain";\n')
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, [(dbg_loglevel, log_config), (rt_ignore_dh, None)])
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, client_config)], extra_config="NoRelay;")
    peers.wait_open(0, 1)

    client_offset, relay_offset, server_offset = (len(peers.text(i)) for i in range(3))
    peers.processes[0].send_signal(signal.SIGUSR1)
    answer = peers.wait(lambda: re.search(
        r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From 'virtual\.localdomain' \('localdomain'\)",
        peers.text(0)[client_offset:]), "answer with restored origin host")
    sent = re.search(r"SEND ([0-9a-f]+) to 'localdomain' \(virtual\.localdomain\)",
                     peers.text(0)[client_offset:])
    if not sent or sent.group(1) != answer.group(1):
        raise AssertionError("answer did not echo the sent value")
    peers.wait(lambda: "ECHO Test-Request received from '%s'" % peers.identities[0]
               in peers.text(2)[server_offset:], "server callback")
    peers.wait(lambda: all(message in peers.text(1)[relay_offset:] for message in (
        "Stowed Destination-Host 'virtual.localdomain' into Proxy-Info",
        "Restored Origin-Host 'virtual.localdomain' from Proxy-Info")),
        "stowed and restored host debug logs")
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
