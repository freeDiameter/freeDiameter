import os
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure, exchange
from sctp_netem_helpers import AckProbe, command, run_isolated, setup_namespace, statistics


daemon, test_app, probe, testcnx, mode = sys.argv[1:6]


def block_acknowledgments(port):
    command("tc", "qdisc", "add", "dev", "lo", "root", "handle", "1:", "prio")
    command("tc", "qdisc", "add", "dev", "lo", "parent", "1:1", "handle", "10:",
            "netem", "loss", "100%")
    command("tc", "filter", "add", "dev", "lo", "protocol", "ip", "parent", "1:",
            "prio", "1", "u32", "match", "ip", "protocol", "132", "0xff",
            "match", "ip", "dport", str(port), "0xffff", "flowid", "1:1")
    # SCTP can retransmit over IPv6 even when the initial connection uses IPv4.
    command("tc", "filter", "add", "dev", "lo", "protocol", "ipv6", "parent", "1:",
            "prio", "2", "u32", "match", "u8", "132", "0xff", "at", "6",
            "match", "u16", str(port), "0xffff", "at", "42", "flowid", "1:1")


def restore_network():
    command("tc", "qdisc", "del", "dev", "lo", "root")


def scenario(peers):
    target = 0 if mode in ("handshake-server", "handshake-timeout") else 1
    address = "fd-ack-probe-%d" % os.getpid()
    listener = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    listener.bind("\0" + address)
    listener.listen(1)
    listener.settimeout(15)
    peers.after_stop(listener.close)

    def config(index):
        remote = 1 - index
        port = 9 if index == 0 else peers.ports[0]
        return ('ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; SctpSecDTLS; Port = %d; };\n'
                % (peers.identities[remote], port))

    def start(index):
        environment = None
        if index == target:
            environment = {"LD_PRELOAD": probe, "FD_ACK_TEST_SOCKET": "@" + address,
                           "FD_ACK_TEST_MODE": mode}
        peers.start(index, [(test_app, configure(peers, index, 1 - index))],
                    extra_config=config(index), env=environment, connect_neighbors=False)

    start(0)
    if target == 0:
        control, _ = listener.accept()
    peers.wait(lambda: "Local server address(es)" in peers.text(0), "server startup")
    start(1)
    if target == 1:
        control, _ = listener.accept()
    observer = AckProbe(control)
    peers.on_stop(lambda: observer.release() if observer.gated else None)
    peers.after_stop(control.close)

    port = observer.wait("CCS", timeout=30)
    observer.gated = True
    block_acknowledgments(port)
    observer.release()
    observer.wait("CCS_SENT", forbidden=("KEY", "FINISHED"))
    observer.wait("PENDING", forbidden=("DRAINED", "KEY", "FINISHED"))
    peers.wait(lambda: statistics()["drops"] > 0, "dropped handshake acknowledgments")
    observer.guard(1, ("DRAINED", "KEY", "FINISHED"))
    if mode == "handshake-timeout":
        deadline = time.monotonic() + 35
        while time.monotonic() < deadline:
            text = peers.text(target)
            if "acknowledgment barrier timed out" in text and "TLS ERROR:" in text:
                break
            observer.guard(0.1, ("DRAINED", "KEY", "FINISHED"))
        else:
            raise AssertionError("handshake did not report its ACK barrier timeout")
        return

    restore_network()
    observer.wait("DRAINED", timeout=15, forbidden=("KEY", "FINISHED"))
    if observer.wait("KEY", timeout=15, forbidden=("FINISHED",)) != 1:
        raise AssertionError("incorrect SCTP-AUTH key activated")
    if observer.wait("KEY_OK", timeout=15, forbidden=("FINISHED",)) != 1:
        raise AssertionError("SCTP-AUTH key activation did not succeed")
    observer.wait("FINISHED", timeout=15)
    if mode == "handshake-server":
        observer.gated = True
        block_acknowledgments(port)
        observer.release()
        observer.wait("FINISHED_SENT", forbidden=("DELETE",))
        observer.wait("PENDING", forbidden=("DRAINED", "DELETE"))
        peers.wait(lambda: statistics()["drops"] > 0, "dropped Finished acknowledgment")
        observer.guard(1, ("DRAINED", "DELETE"))
        restore_network()
        observer.wait("DRAINED", timeout=15, forbidden=("DELETE",))
    if observer.wait("DELETE", timeout=15) != 0:
        raise AssertionError("incorrect SCTP-AUTH key deleted")
    if observer.wait("DELETE_OK", timeout=15) != 0:
        raise AssertionError("SCTP-AUTH old-key deletion did not succeed")
    peers.wait_all_open()
    peers.wait_transport()
    exchange(peers, 1, 0, True)


def shutdown_scenario():
    with tempfile.TemporaryDirectory(dir=Path(testcnx).resolve().parent) as temporary:
        directory = Path(temporary)
        address = "fd-ack-probe-%d" % os.getpid()
        log = directory / "testcnx.log"
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as listener, log.open("w+b") as output:
            listener.bind("\0" + address)
            listener.listen(1)
            listener.settimeout(15)
            environment = dict(os.environ, LD_PRELOAD=probe, FD_ACK_TEST_SOCKET="@" + address,
                               FD_ACK_TEST_MODE=mode)
            process = subprocess.Popen([testcnx, "-p", "3"], env=environment,
                                       stdin=subprocess.DEVNULL, stdout=output,
                                       stderr=subprocess.STDOUT)
            observer = None
            try:
                control, _ = listener.accept()
                with control:
                    observer = AckProbe(control)
                    port = observer.wait("CCS", timeout=30)
                    control.sendall(b"A")
                    observer.wait("DATA", timeout=30)
                    observer.gated = True
                    block_acknowledgments(port)
                    observer.release()
                    observer.wait("DATA_SENT")
                    observer.wait("PENDING", timeout=15, forbidden=("ALERT",))
                    pending_at = observer.history[-1][0]
                    deadline = time.monotonic() + 5
                    while statistics()["drops"] == 0 and time.monotonic() < deadline:
                        time.sleep(0.05)
                    if statistics()["drops"] == 0:
                        raise AssertionError("netem did not drop application acknowledgments")
                    observer.guard(1, ("DRAINED", "ALERT"))
                    if mode == "shutdown-timeout":
                        deadline = time.monotonic() + 10
                        while time.monotonic() < deadline:
                            if "acknowledgment barrier timed out" in log.read_text(errors="replace"):
                                break
                            observer.guard(0.1, ("DRAINED", "ALERT"), allow_closed=True)
                        else:
                            raise AssertionError("shutdown did not report its ACK barrier timeout")
                        elapsed = time.monotonic() - pending_at
                        if not 4.5 <= elapsed <= 10:
                            raise AssertionError("shutdown barrier timed out after %.2f seconds" % elapsed)
                        observer.guard(0.01, ("DRAINED", "ALERT"), allow_closed=True)
                        process.wait(timeout=20)
                    else:
                        restore_network()
                        observer.wait("DRAINED", timeout=15, forbidden=("ALERT",))
                        observer.wait("ALERT", timeout=15)
                        process.wait(timeout=20)
                    if process.returncode:
                        raise AssertionError("connection test exited with status %d" % process.returncode)
            except BaseException:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                if observer:
                    print("ACK probe events: %s" % observer.history, file=sys.stderr)
                print("\n--- connection test log ---\n" + log.read_text(errors="replace"),
                      file=sys.stderr)
                raise
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)


if __name__ == "__main__":
    if len(sys.argv) > 6 and sys.argv[6] == "--isolated":
        setup_namespace()
        if mode.startswith("shutdown"):
            shutdown_scenario()
        else:
            run_scenario(daemon, scenario, 2, [(0, 1)])
    else:
        run_isolated([__file__, daemon, test_app, probe, testcnx, mode])
