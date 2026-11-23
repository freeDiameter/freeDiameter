import re
import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, extension, name = sys.argv[1:5]


def size_scenario(peers):
    config = peers.directory / "size.conf"
    config.write_text("MaximumSize=1;\n")
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2), "server startup")
    peers.start(1, [(extension, config)])
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, configure(peers, 0, 2, mode="client"))],
                extra_config="NoRelay;")
    peers.wait_open(0, 1)

    client_offset, server_offset = len(peers.text(0)), len(peers.text(2))
    peers.processes[0].send_signal(signal.SIGUSR1)
    peers.wait(lambda: re.search(r"RECV .*Status: 5012 From '%s' \('localdomain'\)" %
                                 re.escape(peers.identities[1]),
                                 peers.text(0)[client_offset:]),
               "size rejection from relay")
    if "[rt_deny_by_size] Message is too big" not in peers.text(0)[client_offset:]:
        raise AssertionError("size rejection did not include the expected error message")
    if "ECHO Test-Request" in peers.text(2)[server_offset:]:
        raise AssertionError("oversized request reached the server")

    config.write_text("MaximumSize=4096;\n")
    peers.processes[1].send_signal(signal.SIGUSR1)
    peers.wait(lambda: "rt_deny_by_size: reloaded configuration, maximum size now 4096"
               in peers.text(1), "size limit reload")
    exchange(peers, 0, 2, True)
    peers.wait_all_open()
    peers.wait_transport()


def routing_scenario(peers):
    config = peers.directory / "routing.conf"

    def write_rule(index):
        if name == "rt_default":
            config.write_text('un="route-me" : "%s" += 100;\n' % peers.identities[index])
        else:
            config.write_text('AVP = "User-Name";\n"^route-me$" : "%s" += 100;\n' %
                              peers.identities[index])

    write_rule(3)
    for index in (2, 3):
        peers.start(index, [(test_app, configure(peers, index, 0, mode="server"))],
                    extra_config="NoRelay;")
        peers.wait(lambda: "Local server address(es)" in peers.text(index), "server startup")
    peers.start(1, [(extension, config)])
    peers.wait_open(1, 2)
    peers.wait_open(1, 3)
    client_config = peers.directory / "client.conf"
    client_config.write_text('mode = client;\ndest-realm = "localdomain";\n'
                             'user-name = "route-me";\n')
    peers.start(0, [(test_app, client_config)], extra_config="NoRelay;")
    peers.wait_open(0, 1)
    def check_route(selected, other):
        client_offset = len(peers.text(0))
        selected_offset, other_offset = len(peers.text(selected)), len(peers.text(other))
        peers.processes[0].send_signal(signal.SIGUSR1)
        answer = peers.wait(
            lambda: re.search(r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From '%s'" %
                              re.escape(peers.identities[selected]), peers.text(0)[client_offset:]),
            "answer from selected server")
        sent = re.search(r"SEND ([0-9a-f]+) to 'localdomain' \(-\)",
                         peers.text(0)[client_offset:])
        if not sent or sent.group(1) != answer.group(1):
            raise AssertionError("selected server did not echo the sent value")
        peers.wait(lambda: "ECHO Test-Request received from '%s'" % peers.identities[0]
                   in peers.text(selected)[selected_offset:], "selected server callback")
        if "ECHO Test-Request" in peers.text(other)[other_offset:]:
            raise AssertionError("request reached the unselected server")

    check_route(3, 2)
    write_rule(2)
    offset = len(peers.text(1))
    peers.processes[1].send_signal(signal.SIGUSR1)
    peers.wait(lambda: "reloaded configuration" in peers.text(1)[offset:], "routing rule reload")
    check_route(2, 3)
    peers.wait_all_open()
    peers.wait_transport()


if __name__ == "__main__":
    if name == "rt_deny_by_size":
        run_scenario(daemon, size_scenario, 3, [(0, 1), (1, 2)])
    else:
        run_scenario(daemon, routing_scenario, 4, [(0, 1), (1, 2), (1, 3)])
