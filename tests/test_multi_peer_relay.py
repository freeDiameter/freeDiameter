import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app = sys.argv[1:3]


def scenario(peers):
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="server"))],
                extra_config="NoRelay;")
    peers.wait(lambda: "Local server address(es)" in peers.text(2),
               "server startup")
    peers.start(1, [])
    peers.wait_open(1, 2)
    peers.start(0, [(test_app, configure(peers, 0, 2, mode="client"))],
                extra_config="NoRelay;")
    peers.wait_open(0, 1)
    exchange(peers, 0, 2, True)
    peers.wait_all_open()
    peers.wait_transport()
    for endpoint, other in ((0, 2), (2, 0)):
        if "Connected to '%s'" % peers.identities[other] in peers.text(endpoint):
            raise AssertionError("endpoints connected directly instead of through the relay")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])
