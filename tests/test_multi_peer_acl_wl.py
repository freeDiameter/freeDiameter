import signal
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app, acl_wl = sys.argv[1:4]


def scenario(peers, allowed, cleartext_flag=True, reload_config=False, wildcard=False):
    config = peers.directory / "whitelist.conf"
    identity = ("*.localdomain" if wildcard else peers.identities[1]) if allowed else "other.localdomain"
    flag = "ALLOW_IPSEC " if cleartext_flag else ""
    config.write_text("%s%s\n" % (flag, "other.localdomain" if reload_config else identity))

    peers.start(0, [(acl_wl, config),
                    (test_app, configure(peers, 0, 1, mode="server"))],
                extra_config="NoRelay;", connect_neighbors=False)
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "server startup")
    if reload_config:
        config.write_text("%s%s\n" % (flag, identity))
        peers.processes[0].send_signal(signal.SIGUSR1)
        peers.wait(lambda: "acl_wl: reloaded configuration" in peers.text(0),
                   "whitelist reload")
    peers.start(1, [(test_app, configure(peers, 1, 0, mode="client"))],
                extra_config="NoRelay;")

    if allowed and (peers.tls or cleartext_flag):
        peers.wait_open(1, 0)
        exchange(peers, 1, 0, True)
        peers.wait_all_open()
        peers.wait_transport()
    else:
        peers.wait(lambda: "Connection to '%s' failed: 'DIAMETER_UNKNOWN_PEER'" %
                   peers.identities[1] in peers.text(0), "peer validation rejection")


if __name__ == "__main__":
    run_scenario(daemon, lambda peers: scenario(peers, True), 2, [(0, 1)])
    run_scenario(daemon, lambda peers: scenario(peers, False), 2, [(0, 1)])
    run_scenario(daemon, lambda peers: scenario(peers, True, False), 2, [(0, 1)])
    run_scenario(daemon, lambda peers: scenario(peers, True, wildcard=True), 2, [(0, 1)])
    run_scenario(daemon, lambda peers: scenario(peers, True, reload_config=True), 2, [(0, 1)])
