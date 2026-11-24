import socket
import ssl
import struct
import sys

from multi_peer_harness import SCTP_AUTH_SUPPORTED, run_scenario
from multi_peer_test_app import configure


daemon, test_app, mode = sys.argv[1:4]
SCTP_INITMSG = 2
SCTP_SNDINFO = 2
SCTP_AUTH_CHUNK = 21
SCTP_PEER_AUTH_CHUNKS = 26


def scenario(peers):
    peers.start(0, [(test_app, configure(peers, 0, 1, mode="server"))],
                connect_neighbors=False)
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "server startup")
    context = ssl.create_default_context(cafile=str(peers.ca_cert))
    cert, key = peers.credentials[1]
    context.load_cert_chain(str(cert), str(key))
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as sock:
        sock.settimeout(10)
        auth = mode in ("auth", "fragmented", "dtls-no-data-auth", "wrong-ppid", "idle")
        sock.setsockopt(socket.IPPROTO_SCTP, SCTP_AUTH_SUPPORTED, struct.pack("=iI", 0, auth))
        if auth and mode != "dtls-no-data-auth":
            sock.setsockopt(socket.IPPROTO_SCTP, SCTP_AUTH_CHUNK, b"\x00")
        sock.setsockopt(socket.IPPROTO_SCTP, SCTP_INITMSG, struct.pack("=HHHH", 1, 1, 0, 0))
        sock.connect(("127.0.0.1", peers.ports[0]))
        _, negotiated = struct.unpack("=iI", sock.getsockopt(socket.IPPROTO_SCTP,
                                                             SCTP_AUTH_SUPPORTED, 8))
        if negotiated != auth:
            raise AssertionError("unexpected negotiated SCTP-AUTH capability")
        if mode == "idle":
            peers.wait(lambda: "Timed out inspecting SCTP handshake" in peers.text(0),
                       "idle handshake timeout")
            return
        if mode in ("invalid-dtls", "dtls-no-data-auth", "wrong-ppid"):
            record = b"\x16\xfe\xfd" + b"\x00" * 10
            if mode == "wrong-ppid":
                info = struct.pack("=HH4sIi", 0, 0, struct.pack("!I", 46), 0, 0)
                sock.sendmsg([record], [(socket.IPPROTO_SCTP, SCTP_SNDINFO, info)])
                diagnostic = "Invalid SCTP metadata for DTLS record"
            else:
                sock.sendall(record)
                diagnostic = ("TLS ERROR:" if mode == "invalid-dtls"
                              else "peer does not require authenticated DATA")
            peers.wait(lambda: diagnostic in peers.text(0),
                       "rejection of invalid DTLS input")
            return
        if auth:
            chunks = sock.getsockopt(socket.IPPROTO_SCTP, SCTP_PEER_AUTH_CHUNKS, 264)
            _, count = struct.unpack_from("=iI", chunks)
            if 0 not in chunks[8:8 + count]:
                raise AssertionError("server did not require authenticated SCTP DATA")
            if 192 not in chunks[8:8 + count]:
                raise AssertionError("server did not require authenticated FORWARD-TSN")
        if mode == "fragmented":
            incoming, outgoing = ssl.MemoryBIO(), ssl.MemoryBIO()
            secure = context.wrap_bio(incoming, outgoing, server_side=False,
                                      server_hostname=peers.identities[0])
            first = True
            while True:
                try:
                    secure.do_handshake()
                    complete = True
                except ssl.SSLWantReadError:
                    complete = False
                data = outgoing.read()
                if data:
                    if first:
                        sock.sendall(data[:1])
                        sock.sendall(data[1:2])
                        sock.sendall(data[2:])
                        first = False
                    else:
                        sock.sendall(data)
                if complete:
                    break
                data = sock.recv(65536)
                if not data:
                    raise RuntimeError("TLS peer closed during handshake")
                incoming.write(data)
            secure.write(b"\x00" * 4)
            sock.sendall(outgoing.read())
            peers.wait(lambda: "Received suspect header" in peers.text(0),
                       "server receipt of TLS application data")
            while sock.recv(65536):
                pass
        else:
            with context.wrap_socket(sock, server_hostname=peers.identities[0]) as secure:
                if not secure.cipher():
                    raise AssertionError("TLS handshake did not negotiate a cipher")
                secure.sendall(b"\x00" * 4)
                peers.wait(lambda: "Received suspect header" in peers.text(0),
                           "server receipt of TLS application data")
                while secure.recv(65536):
                    pass


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [])
