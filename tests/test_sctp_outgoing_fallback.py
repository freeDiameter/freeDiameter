import socket
import ssl
import struct
import sys

from multi_peer_harness import SCTP_AUTH_SUPPORTED, require_sctp_auth, run_scenario
from multi_peer_test_app import configure


daemon, test_app, mode = sys.argv[1:4]
SCTP_INITMSG = 2
SCTP_AUTH_CHUNK = 21
fallback_warning = "WARNING: SCTP-AUTH unavailable"


def scenario(peers):
    auth = mode in ("handshake-failure", "no-data-auth")
    if mode not in ("fallback", "strict-local"):
        require_sctp_auth()

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as listener:
        listener.settimeout(15)
        if mode == "strict-local":
            _, local_auth = struct.unpack("=iI", listener.getsockopt(
                socket.IPPROTO_SCTP, SCTP_AUTH_SUPPORTED, 8))
            if local_auth:
                print("SKIP: local SCTP-AUTH is enabled.", file=sys.stderr)
                raise SystemExit(77)
        listener.setsockopt(socket.IPPROTO_SCTP, SCTP_AUTH_SUPPORTED, struct.pack("=iI", 0, auth))
        if mode == "handshake-failure":
            listener.setsockopt(socket.IPPROTO_SCTP, SCTP_AUTH_CHUNK, b"\x00")
        listener.setsockopt(socket.IPPROTO_SCTP, SCTP_INITMSG, struct.pack("=HHHH", 1, 1, 0, 0))
        listener.bind(("127.0.0.1", peers.ports[1]))
        listener.listen(1)

        selection = "SctpSecDTLS; " if mode in ("strict", "strict-local") else ""
        config = ('Port = 0; SecPort = 0;\n'
                  'ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; %sPort = %d; };\n'
                  % (peers.identities[1], selection, peers.ports[1]))
        peers.start(0, [(test_app, configure(peers, 0, 1, mode="client"))],
                    extra_config=config, connect_neighbors=False)
        if mode == "strict-local":
            peers.wait(lambda: "DTLS/SCTP requires kernel SCTP-AUTH support" in peers.text(0),
                       "strict DTLS rejection without local SCTP-AUTH")
            listener.settimeout(1)
            try:
                connection, _ = listener.accept()
            except socket.timeout:
                connection = None
            if connection is not None:
                connection.close()
                raise AssertionError("strict DTLS connected without local SCTP-AUTH")
            if fallback_warning in peers.text(0):
                raise AssertionError("unexpected TLS fallback")
            return
        connection, _ = listener.accept()
        with connection:
            connection.settimeout(15)
            _, negotiated = struct.unpack("=iI", connection.getsockopt(
                socket.IPPROTO_SCTP, SCTP_AUTH_SUPPORTED, 8))
            if bool(negotiated) != auth:
                raise AssertionError("unexpected negotiated SCTP-AUTH capability")

            if mode == "fallback":
                context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
                context.load_verify_locations(cafile=str(peers.ca_cert))
                context.verify_mode = ssl.CERT_REQUIRED
                cert, key = peers.credentials[1]
                context.load_cert_chain(str(cert), str(key))
                with context.wrap_socket(connection, server_side=True) as secure:
                    header = b""
                    while len(header) < 20:
                        chunk = secure.recv(20 - len(header))
                        if not chunk:
                            raise AssertionError("connection closed before Diameter CER")
                        header += chunk
                    if header[0] != 1 or int.from_bytes(header[5:8], "big") != 257:
                        raise AssertionError("TLS connection did not receive a Diameter CER")
                    peers.wait(lambda: fallback_warning in peers.text(0), "fallback warning")
            elif mode == "strict":
                peers.wait(lambda: "DTLS/SCTP requires SCTP-AUTH on the association" in peers.text(0),
                           "strict DTLS rejection")
                if connection.recv(1):
                    raise AssertionError("strict DTLS sent handshake data without SCTP-AUTH")
            elif mode == "no-data-auth":
                peers.wait(lambda: "peer does not require authenticated DATA" in peers.text(0),
                           "DTLS authenticated DATA rejection")
                if connection.recv(1):
                    raise AssertionError("handshake started without authenticated DATA")
            elif mode == "handshake-failure":
                header = connection.recv(13)
                if len(header) < 3 or header[:2] != b"\x16\xfe":
                    raise AssertionError("expected a DTLS ClientHello")
                connection.shutdown(socket.SHUT_RDWR)
                peers.wait(lambda: "TLS ERROR:" in peers.text(0), "failed DTLS handshake")
            else:
                raise ValueError("unknown scenario mode: " + mode)

        if mode != "fallback" and fallback_warning in peers.text(0):
            raise AssertionError("unexpected TLS fallback")


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [], requires_sctp_auth=False)
