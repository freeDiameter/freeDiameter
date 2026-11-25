import errno
import json
import select
import shutil
import subprocess
import sys
import time
from pathlib import Path

from multi_peer_harness import require_sctp_auth


class AckProbe:
    def __init__(self, sock):
        self.sock = sock
        self.buffer = b""
        self.gated = False
        self.closed = False
        self.history = []

    def receive(self, timeout):
        deadline = time.monotonic() + timeout
        while b"\n" not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.sock], [], [], remaining)[0]:
                return None
            data = self.sock.recv(4096)
            if not data:
                self.closed = True
                return None
            self.buffer += data
        line, self.buffer = self.buffer.split(b"\n", 1)
        name, value, timestamp = line.decode().split()
        item = name, int(value)
        if not self.history or self.history[-1][1] != item:
            self.history.append((float(timestamp), item))
        return item

    def wait(self, name, timeout=15, forbidden=()):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            item = self.receive(deadline - time.monotonic())
            if item is None:
                break
            if item[0] in forbidden:
                raise AssertionError("unexpected ACK barrier event: %s" % (item,))
            if item[0] == name:
                return item[1]
        raise AssertionError("timed out waiting for ACK barrier event " + name)

    def guard(self, duration, forbidden, allow_closed=False):
        deadline = time.monotonic() + duration
        while time.monotonic() < deadline:
            item = self.receive(deadline - time.monotonic())
            if item is None:
                if self.closed and not allow_closed:
                    raise AssertionError("ACK probe disconnected before barrier check completed")
                return
            if item[0] in forbidden:
                raise AssertionError("ACK barrier advanced before acknowledgment: %s" % (item,))

    def release(self):
        self.sock.sendall(b"G")
        self.gated = False


def command(*args):
    subprocess.run(args, check=True)


def statistics():
    result = subprocess.run(["tc", "-s", "-j", "qdisc", "show", "dev", "lo"],
                            check=True, stdout=subprocess.PIPE)
    queues = [q for q in json.loads(result.stdout) if q["kind"] == "netem"]
    if len(queues) != 1:
        raise AssertionError("isolated loopback has no netem queue")
    return queues[0]


def setup_namespace(enable_auth=True):
    command("ip", "link", "set", "lo", "up")
    command("ip", "link", "add", "dev", "fdtest0", "type", "dummy")
    command("ip", "addr", "add", "192.0.2.1/32", "dev", "fdtest0")
    command("ip", "link", "set", "fdtest0", "up")
    if not enable_auth:
        return
    try:
        Path("/proc/sys/net/sctp/auth_enable").write_text("1")
    except OSError as error:
        if error.errno not in (errno.EACCES, errno.EPERM, errno.EROFS):
            raise
        print("SKIP: cannot enable SCTP-AUTH in the private namespace: %s" % error,
              file=sys.stderr)
        raise SystemExit(77)


def run_isolated(arguments, require_auth=True):
    if require_auth:
        require_sctp_auth()
    for tool in ("unshare", "ip", "tc"):
        if not shutil.which(tool):
            print("SKIP: %s is required for isolated netem tests." % tool, file=sys.stderr)
            raise SystemExit(77)
    prefix = ["unshare", "--user", "--map-root-user", "--net"]
    probe = subprocess.run(prefix + ["true"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if probe.returncode:
        error = probe.stderr.decode(errors="replace")
        if "Operation not permitted" in error or "Function not implemented" in error:
            print("SKIP: isolated network namespaces are unavailable: " + error, file=sys.stderr)
            raise SystemExit(77)
        raise RuntimeError("network namespace probe failed: " + error)
    for args in (["ip", "link", "set", "lo", "up"],
                 ["ip", "link", "add", "dev", "fdtest0", "type", "dummy"],
                 ["tc", "qdisc", "add", "dev", "lo", "root", "netem", "loss", "100%"]):
        probe = subprocess.run(prefix + args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        if probe.returncode:
            error = probe.stderr.decode(errors="replace")
            if any(reason in error for reason in ("Operation not permitted", "Permission denied",
                                                   "Operation not supported",
                                                   "Specified qdisc kind is unknown")) or (
                    args[0] == "tc" and "No such file or directory" in error):
                print("SKIP: isolated netem is unavailable: " + error, file=sys.stderr)
                raise SystemExit(77)
            raise RuntimeError("network capability probe failed: " + error)
    result = subprocess.run(prefix + [sys.executable, "-B"] + arguments + ["--isolated"])
    raise SystemExit(result.returncode)
