import re
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app = sys.argv[1:3]


def scenario(peers):
    def connection(remote, port, security):
        return ('ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; %s; Port = %d; };\n'
                % (peers.identities[remote], security, port))

    hub_config = (
        connection(1, peers.ports[1], "SctpSecDTLS")
        + connection(2, peers.ports[2], "SctpSec3436")
    )
    dtls_config = connection(0, peers.ports[0], "SctpSecDTLS")
    legacy_config = connection(0, peers.ports[0], "SctpSec3436")

    peers.start(0, [(test_app, configure(peers, 0, 1, mode="server"))],
                extra_config=hub_config, connect_neighbors=False)
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "hub startup")
    peers.start(1, [(test_app, configure(peers, 1, 0, mode="client"))],
                extra_config=dtls_config, connect_neighbors=False)
    peers.start(2, [(test_app, configure(peers, 2, 0, mode="client"))],
                extra_config=legacy_config, connect_neighbors=False)
    peers.wait_all_open()

    def transport(index, remote, security):
        return re.search(r"(?:Connected to|CONNECTED TO) '%s' \(SCTP,%s,soc#\d+\)" %
                         (re.escape(peers.identities[remote]), security), peers.text(index))

    peers.wait(lambda: transport(0, 1, "DTLS") and transport(1, 0, "DTLS")
               and transport(0, 2, "TLS") and transport(2, 0, "TLS"),
               "simultaneous DTLS/SCTP and legacy TLS/SCTP connections")

    def open_offsets():
        logs = [peers.text(index) for index in range(3)]
        for index, remote in ((0, 1), (1, 0), (0, 2), (2, 0)):
            states = re.findall(r"'STATE_[A-Z_]+'\s*->\s*'(STATE_[A-Z_]+)'\s*'%s'" %
                                re.escape(peers.identities[remote]), logs[index])
            if not states or states[-1] != "STATE_OPEN":
                return None
        return [len(log) for log in logs]

    offsets = peers.wait(open_offsets, "both links to be open simultaneously")
    exchange(peers, 1, 0, True)
    exchange(peers, 2, 0, True)
    for index, remote in ((0, 1), (1, 0), (0, 2), (2, 0)):
        if re.search(r"'STATE_OPEN'\s*->\s*'STATE_[A-Z_]+'\s*'%s'" %
                     re.escape(peers.identities[remote]), peers.text(index)[offsets[index]:]):
            raise AssertionError("peer link closed during mixed-transport exchanges")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 3, [(0, 1), (0, 2)])
