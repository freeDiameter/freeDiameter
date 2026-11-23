import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, extension, observer, name = sys.argv[1:6]


def scenario(peers):
    config = peers.directory / "extension.conf"
    client_config = peers.directory / "client.conf"
    server_extensions = [(test_app, configure(peers, 2, 0, mode="server"))]
    relay_extensions = []
    if name == "rt_rewrite":
        config.write_text('MAP = "User-Name" > "Class";\n')
        client_config.write_text('mode = client;\ndest-host = "%s";\n'
                                 'user-name = "rewrite-marker";\n' % peers.identities[2])
        server_extensions.append((observer, None))
        relay_extensions.extend([
            (test_app, configure(peers, 1, 0, mode="server")),
            (extension, config),
        ])
    else:
        client_config.write_text('mode = client;\ndest-host = "virtual.localdomain";\n')
        relay_extensions.append((extension, None))

    peers.start(2, server_extensions, extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, relay_extensions)
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, client_config)], extra_config="NoRelay;")
    peers.wait_open(0, 1)

    if name == "rt_rewrite":
        offset = len(peers.text(2))
        exchange(peers, 0, 2, True)
        peers.wait(lambda: re.search(r"AVP: 'Class'.*<72 65 77 72 69 74 65 2D 6D 61 72 6B 65 72>",
                                     peers.text(2)[offset:]), "rewritten Class AVP")
        if "AVP: 'User-Name'" in peers.text(2)[offset:]:
            raise AssertionError("source AVP was not removed")
    else:
        client_offset = len(peers.text(0))
        peers.processes[0].send_signal(signal.SIGUSR1)
        answer = peers.wait(lambda: re.search(
            r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From 'virtual\.localdomain'",
            peers.text(0)[client_offset:]), "answer with restored origin host")
        sent = re.search(r"SEND ([0-9a-f]+) to 'localdomain' \(virtual\.localdomain\)",
                         peers.text(0)[client_offset:])
        if not sent or sent.group(1) != answer.group(1):
            raise AssertionError("answer did not echo the sent value")
        peers.wait(lambda: "ECHO Test-Request received from '%s'" % peers.identities[0]
                   in peers.text(2), "server callback")
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
