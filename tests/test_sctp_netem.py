import re
import signal
import socket
import struct
import sys
import time

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange
from sctp_netem_helpers import command, run_isolated, setup_namespace, statistics


daemon, test_app = sys.argv[1:3]
SCTP_NODELAY = 3
SCTP_STATUS = 14
SCTP_AUTH_CHUNK = 21
SO_MEMINFO = 55
SK_MEMINFO_WMEM_QUEUED = 5


def authenticated(sock):
    for chunk in (0, 192):
        sock.setsockopt(socket.IPPROTO_SCTP, SCTP_AUTH_CHUNK, bytes([chunk]))
    sock.settimeout(10)


def queued(sock):
    memory = sock.getsockopt(socket.SOL_SOCKET, SO_MEMINFO, 9 * 4)
    if len(memory) < (SK_MEMINFO_WMEM_QUEUED + 1) * 4:
        raise AssertionError("SO_MEMINFO did not return send-queue accounting")
    return struct.unpack_from("=I", memory, SK_MEMINFO_WMEM_QUEUED * 4)[0]


def check_ack_queue():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as listener:
        authenticated(listener)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as sender:
            authenticated(sender)
            sender.connect(listener.getsockname())
            receiver, _ = listener.accept()
            with receiver:
                receiver.settimeout(10)
                command("tc", "qdisc", "add", "dev", "lo", "root", "netem", "delay", "500ms")
                sender.sendall(b"x" * 4096)
                if receiver.recv(4096) != b"x" * 4096:
                    raise AssertionError("SCTP test message was not received intact")
                if queued(sender) == 0:
                    raise AssertionError("send queue drained before the delayed ACK")
                deadline = time.monotonic() + 10
                while queued(sender) and time.monotonic() < deadline:
                    time.sleep(0.01)
                if queued(sender):
                    raise AssertionError("send queue did not drain after acknowledgment")
                if statistics()["packets"] < 2:
                    raise AssertionError("netem did not process both DATA and acknowledgment")
                command("tc", "qdisc", "del", "dev", "lo", "root")


def check_gap_ack_queue():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as listener:
        authenticated(listener)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as sender:
            authenticated(sender)
            sender.setsockopt(socket.IPPROTO_SCTP, SCTP_NODELAY, 1)
            sender.connect(listener.getsockname())
            receiver, _ = listener.accept()
            try:
                capture = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.htons(0x0800))
            except PermissionError as error:
                print("SKIP: raw socket capture is unavailable: %s" % error, file=sys.stderr)
                raise SystemExit(77)
            with receiver, capture:
                receiver.settimeout(10)
                capture.bind(("lo", 0))
                capture.settimeout(1)
                # Delay only the sender's first DATA packet; let the gap SACK return immediately.
                command("tc", "qdisc", "add", "dev", "lo", "root", "handle", "1:", "prio")
                command("tc", "qdisc", "add", "dev", "lo", "parent", "1:1", "handle", "10:",
                        "netem", "delay", "2000ms", "reorder", "100%", "gap", "2")
                for port, band in ((listener.getsockname()[1], "1:1"),
                                   (sender.getsockname()[1], "1:2")):
                    command("tc", "filter", "add", "dev", "lo", "protocol", "ip", "parent",
                            "1:", "prio", "1", "u32", "match", "ip", "protocol", "132", "0xff",
                            "match", "ip", "dport", str(port), "0xffff", "flowid", band)
                sender.sendall(b"a" * 1000)
                time.sleep(0.05)
                sender.sendall(b"b" * 1000)
                deadline = time.monotonic() + 1
                gap_ack = False
                while time.monotonic() < deadline:
                    try:
                        frame = capture.recv(65535)
                    except socket.timeout:
                        break
                    if len(frame) < 46 or frame[12:14] != b"\x08\x00" or frame[23] != 132:
                        continue
                    sctp = 14 + (frame[14] & 15) * 4
                    if sctp + 12 > len(frame) or struct.unpack_from("!HH", frame, sctp) != (
                            listener.getsockname()[1], sender.getsockname()[1]):
                        continue
                    offset = sctp + 12
                    while offset + 4 <= len(frame):
                        kind, _, length = struct.unpack_from("!BBH", frame, offset)
                        if length < 4 or offset + length > len(frame):
                            break
                        if kind == 3 and length >= 20 and struct.unpack_from("!H", frame, offset + 12)[0]:
                            gap_ack = True
                            break
                        offset += (length + 3) & ~3
                    if gap_ack:
                        break
                if not gap_ack:
                    raise AssertionError("netem did not produce an SCTP gap acknowledgment")
                deadline = time.monotonic() + 0.5
                while time.monotonic() < deadline:
                    status = sender.getsockopt(socket.IPPROTO_SCTP, SCTP_STATUS, 256)
                    if struct.unpack_from("=H", status, 12)[0] == 1:
                        break
                    time.sleep(0.01)
                else:
                    raise AssertionError("sender did not process the gap acknowledgment")
                if queued(sender) == 0:
                    raise AssertionError("send queue drained on a gap acknowledgment")
                if receiver.recv(1000) != b"a" * 1000 or receiver.recv(1000) != b"b" * 1000:
                    raise AssertionError("reordered SCTP messages were not delivered in order")
                deadline = time.monotonic() + 10
                while queued(sender) and time.monotonic() < deadline:
                    time.sleep(0.01)
                if queued(sender):
                    raise AssertionError("send queue did not drain after cumulative acknowledgment")
                command("tc", "qdisc", "del", "dev", "lo", "root")


def scenario(peers):
    peers.start(0, [(test_app, configure(peers, 0, 1))])
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "first daemon startup")
    command("tc", "qdisc", "add", "dev", "lo", "root", "netem", "loss", "100%")
    peers.start(1, [(test_app, configure(peers, 1, 0))])
    peers.wait(lambda: statistics()["drops"] > 0, "netem to drop SCTP connection traffic")
    command("tc", "qdisc", "change", "dev", "lo", "root", "netem",
            "delay", "20ms", "reorder", "25%", "50%", "gap", "3")
    peers.wait_all_open()
    peers.wait_transport()
    exchange(peers, 0, 1, True)

    client_offset = len(peers.text(0))
    server_offset = len(peers.text(1))
    command("tc", "qdisc", "change", "dev", "lo", "root", "netem", "loss", "100%")
    peers.processes[0].send_signal(signal.SIGUSR1)
    sent = peers.wait(lambda: re.search(r"SEND ([0-9a-f]+) to", peers.text(0)[client_offset:]),
                      "request during packet loss")
    peers.wait(lambda: statistics()["drops"] > 0, "netem to drop established traffic")
    command("tc", "qdisc", "change", "dev", "lo", "root", "netem",
            "delay", "20ms", "reorder", "25%", "50%", "gap", "3")
    peers.wait(lambda: "RECV %s (Ok) Status: 2001" % sent.group(1)
               in peers.text(0)[client_offset:], "request recovery after packet loss", timeout=30)
    peers.wait(lambda: "ECHO Test-Request received from '%s'" % peers.identities[0]
               in peers.text(1)[server_offset:], "server receipt after packet loss")

    for _ in range(3):
        exchange(peers, 0, 1, True)
        exchange(peers, 1, 0, True)
    stats = statistics()
    if stats["packets"] == 0:
        raise AssertionError("netem did not process any packets")


def main():
    if len(sys.argv) > 3 and sys.argv[3] == "--isolated":
        setup_namespace()
        check_ack_queue()
        check_gap_ack_queue()
        run_scenario(daemon, scenario, 2, [(0, 1)])
        return

    run_isolated([__file__, daemon, test_app])


if __name__ == "__main__":
    main()
