import re
import sys

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange


daemon, test_app = sys.argv[1:3]


def scenario(peers):
    mode = sys.argv[3] if len(sys.argv) > 3 else "default"
    configs = ["", ""]
    if mode == "legacy":
        for index in range(2):
            remote = 1 - index
            configs[index] = (
                'ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; SctpSec3436; Port = %d; };\n'
                % (peers.identities[remote], peers.ports[remote])
            )
        peers.dtls = False
    elif mode in ("explicit-dtls", "no-tls-legacy", "old-method-legacy"):
        for index in range(2):
            remote = 1 - index
            security = {
                "explicit-dtls": "SctpSecDTLS;",
                "no-tls-legacy": ("No_TLS; SctpSec3436;" if index == 0
                                  else "SctpSec3436; No_TLS;"),
                "old-method-legacy": "TLS_old_method; SctpSec3436;",
            }[mode]
            plain_port = ("Port = %d; SecPort = 0;\n" % peers.ports[index]
                          if mode in ("no-tls-legacy", "old-method-legacy") else "")
            old_method = "TLS_old_method;\n" if mode == "old-method-legacy" else ""
            peer_option = (
                'ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; %s Port = %d; };\n'
                % (peers.identities[remote], security, peers.ports[remote])
            )
            configs[index] = plain_port + old_method + peer_option
    elif mode in ("tcp-legacy", "tcp-dtls"):
        security = "SctpSec3436" if mode == "tcp-legacy" else "SctpSecDTLS"
        for index in range(2):
            remote = 1 - index
            configs[index] = (
                'ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; %s; Port = %d; };\n'
                % (peers.identities[remote], security, peers.ports[remote])
            )
    elif mode != "default":
        raise ValueError("unknown scenario mode: " + mode)
    peers.start(0, [(test_app, configure(peers, 0, 1))],
                extra_config=configs[0], connect_neighbors=mode == "default")
    peers.wait(lambda: "Local server address(es)" in peers.text(0),
               "first daemon startup")
    peers.start(1, [(test_app, configure(peers, 1, 0))],
                extra_config=configs[1], connect_neighbors=mode == "default")
    if mode == "no-tls-legacy":
        peers.tls = False
        peers.dtls = False
    elif mode == "old-method-legacy":
        peers.dtls = False
    peers.wait_open(1, 0)
    exchange(peers, 1, 0, True)
    peers.wait_all_open()
    if mode == "old-method-legacy":
        peers.wait(
            lambda: all(
                re.search(r"'STATE_OPEN_HANDSHAKE'\s*->\s*'STATE_OPEN'\s*'%s'" %
                          re.escape(peers.identities[remote]), peers.text(index))
                for index, remote in ((0, 1), (1, 0))
            ),
            "in-band TLS handshakes on both peers",
        )
    else:
        peers.wait_transport()
    exchange(peers, 0, 1, True)


if __name__ == "__main__":
    mode = sys.argv[3] if len(sys.argv) > 3 else "default"
    run_scenario(daemon, scenario, 2, [(0, 1)],
                 requires_sctp_auth=mode not in ("no-tls-legacy", "old-method-legacy"))
