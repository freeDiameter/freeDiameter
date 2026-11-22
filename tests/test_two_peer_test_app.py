import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app = sys.argv[1:3]


def scenario(peers):
    peers.start(0, [(test_app, configure(peers, 0, 1))])
    peers.wait(lambda: "Local server address(es)" in peers.text(0),
               "first daemon startup")
    peers.start(1, [(test_app, configure(peers, 1, 0))])
    peers.wait_open(1, 0)
    exchange(peers, 1, 0, True)
    peers.wait_all_open()
    peers.wait_transport()
    exchange(peers, 0, 1, True)


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [(0, 1)])
