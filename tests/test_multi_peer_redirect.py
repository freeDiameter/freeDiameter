import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure


daemon, test_app, rt_redirect, rt_default, app_redirect, dbg_msg_dumps = sys.argv[1:7]


def scenario(peers):
    redirect_config = peers.directory / "redirect.conf"
    redirect_config.write_text('DONT_CACHE : to "aaa://%s";\n' % peers.identities[3])
    route_config = peers.directory / "route.conf"
    route_config.write_text('* : "%s" += 100;\n' % peers.identities[2])
    peers.start(3, [(test_app, configure(peers, 3, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.start(2, [(app_redirect, redirect_config)])
    for index in (2, 3):
        peers.wait(lambda: "Local server address(es)" in peers.text(index), "server startup")
    peers.start(1, [(rt_redirect, None), (rt_default, route_config), (dbg_msg_dumps, None)])
    peers.wait_open(1, 2)
    peers.wait_open(1, 3)
    config = peers.directory / "client.conf"
    config.write_text('mode = client;\ndest-realm = "localdomain";\n')
    peers.start(0, [(test_app, config)], extra_config="NoRelay;")
    peers.wait_open(0, 1)

    client_offset, relay_offset = len(peers.text(0)), len(peers.text(1))
    server_offset = len(peers.text(3))
    peers.processes[0].send_signal(signal.SIGUSR1)
    answer = peers.wait(lambda: re.search(
        r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From '%s'" % re.escape(peers.identities[3]),
        peers.text(0)[client_offset:]), "answer from redirect target")
    sent = re.search(r"SEND ([0-9a-f]+) to 'localdomain' \(-\)", peers.text(0)[client_offset:])
    if not sent or sent.group(1) != answer.group(1):
        raise AssertionError("redirect target did not echo the sent value")
    peers.wait(lambda: re.search(
        r"RCV from '%s':.*?Flags: 0x20 \(--E-\).*?"
        r"AVP: 261\(not searched in dictionary\).*?"
        r"AVP: 292\(not searched in dictionary\).*?SND to '%s':" %
        (re.escape(peers.identities[2]), re.escape(peers.identities[3])),
        peers.text(1)[relay_offset:], re.DOTALL), "redirect and retry at relay")
    peers.wait(lambda: "ECHO Test-Request received from '%s'" % peers.identities[0]
               in peers.text(3)[server_offset:], "redirect target callback")
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 4, [(0, 1), (1, 2), (1, 3)])
