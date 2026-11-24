import errno
import os
import re
import shutil
import signal
import socket
import subprocess
import struct
import sys
import tempfile
import time
from pathlib import Path

SCTP_AUTH_SUPPORTED = 129


def quote(value):
    return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'


def free_ports(count, sctp):
    sockets = []
    try:
        protocol = socket.IPPROTO_TCP
        if sctp:
            protocol = getattr(socket, "IPPROTO_SCTP", None)
            if protocol is None:
                protocol = socket.getprotobyname("sctp")
        for _ in range(count):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
            sockets.append(sock)
            sock.bind(("127.0.0.1", 0))
        return [sock.getsockname()[1] for sock in sockets]
    finally:
        for sock in sockets:
            sock.close()


def create_tls_credentials(directory, openssl, identities):
    ca_config = directory / "ca.cnf"
    ca_config.write_text(
        "[req]\nprompt = no\ndistinguished_name = dn\nx509_extensions = ca\n"
        "[dn]\nCN = multi-peer test CA\n[ca]\n"
        "basicConstraints = critical,CA:TRUE\n"
        "keyUsage = critical,keyCertSign,cRLSign\nsubjectKeyIdentifier = hash\n"
    )
    ca_key, ca_cert = directory / "ca.key", directory / "ca.pem"

    def run(*arguments):
        try:
            result = subprocess.run([openssl] + list(arguments), stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE, timeout=20)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError("openssl certificate generation timed out") from error
        if result.returncode:
            raise RuntimeError("openssl failed: " + result.stderr.decode(errors="replace"))

    run("req", "-new", "-x509", "-newkey", "rsa:2048", "-nodes", "-sha256",
        "-days", "2", "-config", str(ca_config), "-keyout", str(ca_key),
        "-out", str(ca_cert))
    credentials = []
    for index, identity in enumerate(identities):
        key = directory / ("peer%d.key" % (index + 1))
        csr = directory / ("peer%d.csr" % (index + 1))
        cert = directory / ("peer%d.pem" % (index + 1))
        extensions = directory / ("peer%d.cnf" % (index + 1))
        extensions.write_text(
            "[peer]\nbasicConstraints = critical,CA:FALSE\n"
            "keyUsage = critical,digitalSignature,keyEncipherment\n"
            "extendedKeyUsage = serverAuth,clientAuth\nsubjectAltName = DNS:%s\n" % identity
        )
        run("req", "-new", "-newkey", "rsa:2048", "-nodes", "-sha256",
            "-subj", "/CN=" + identity, "-keyout", str(key), "-out", str(csr))
        run("x509", "-req", "-in", str(csr), "-CA", str(ca_cert), "-CAkey",
            str(ca_key), "-set_serial", str(index + 1), "-days", "2", "-sha256",
            "-extfile", str(extensions), "-extensions", "peer", "-out", str(cert))
        credentials.append((cert, key))
    return ca_cert, credentials


class Peers:
    def __init__(self, daemon, directory, count, links, tls=False, sctp=False, openssl=None,
                 dtls=False):
        if count < 1:
            raise ValueError("peer count must be positive")
        self.identities = tuple("peer%d.localdomain" % (index + 1) for index in range(count))
        self.links = []
        self.neighbors = [[] for _ in range(count)]
        for first, second in links:
            if (not 0 <= first < count or not 0 <= second < count or first == second
                    or second in self.neighbors[first]):
                raise ValueError("invalid or duplicate peer link: %s" % ((first, second),))
            self.links.append((first, second))
            self.neighbors[first].append(second)
            self.neighbors[second].append(first)
        self.daemon = daemon
        self.directory = directory
        self.tls = tls
        self.sctp = sctp
        self.dtls = dtls
        self.ports = free_ports(count, sctp)
        if tls:
            if not openssl:
                raise ValueError("TLS scenarios require an openssl executable")
            self.ca_cert, self.credentials = create_tls_credentials(directory, openssl,
                                                                     self.identities)
        self.processes = {}
        self.start_order = []
        self.logs = {}
        self.error_files = []
        self.stop_callbacks = []
        self.stopped_callbacks = []

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, traceback):
        try:
            failures = self.stop()
            if exc_type is not None or failures:
                for index in sorted(self.logs):
                    print("\n--- peer %d log ---\n%s" % (index + 1, self.text(index)),
                          file=sys.stderr)
                if failures:
                    if exc_type is None:
                        raise RuntimeError("; ".join(failures))
                    print("Cleanup errors: " + "; ".join(failures), file=sys.stderr)
        finally:
            for log in self.logs.values():
                log.close()
        return False

    def start(self, index, extensions, extra_config="", env=None, connect_neighbors=True):
        if not 0 <= index < len(self.identities) or index in self.processes:
            raise ValueError("peer index must be valid and not already started")
        path = self.directory / ("peer%d.conf" % (index + 1))
        loads = "".join(
            "LoadExtension = %s%s;\n" %
            (quote(extension), " : " + quote(config) if config is not None else "")
            for extension, config in extensions
        )
        port_config = ("Port = 0;\nSecPort = %d;\n" if self.tls
                       else "Port = %d;\nSecPort = 0;\n") % self.ports[index]
        transport = "No_TCP;\n" if self.sctp else "No_SCTP;\n"
        security = ""
        if self.tls:
            cert, key = self.credentials[index]
            security = "TLS_Cred = %s, %s;\nTLS_CA = %s;\n" % (
                quote(cert), quote(key), quote(self.ca_cert))
        peer_security = "" if self.tls else "No_TLS; "
        connections = "".join(
            "ConnectPeer = %s { ConnectTo = \"127.0.0.1\"; %sPort = %d; };\n" %
            (quote(self.identities[remote]), peer_security, self.ports[remote])
            for remote in self.neighbors[index] if connect_neighbors
        )
        path.write_text(
            "Identity = %s;\nRealm = \"localdomain\";\n"
            "%s%sNo_IPv6;\nListenOn = \"127.0.0.1\";\n%s%s%s%s\n"
            % (quote(self.identities[index]), port_config, transport, security, loads,
               connections, extra_config)
        )
        log = (self.directory / ("peer%d.log" % (index + 1))).open("w+b")
        self.logs[index] = log
        environment = os.environ.copy()
        if env:
            environment.update(env)
        self.processes[index] = subprocess.Popen(
            [self.daemon, "-c", str(path)], stdin=subprocess.DEVNULL,
            stdout=log, stderr=subprocess.STDOUT, env=environment,
        )
        self.start_order.append(index)

    def text(self, index):
        self.logs[index].flush()
        return Path(self.logs[index].name).read_text(errors="replace")

    def watch_error_file(self, path):
        self.error_files.append(path)

    def on_stop(self, callback):
        self.stop_callbacks.append(callback)

    def after_stop(self, callback):
        self.stopped_callbacks.append(callback)

    def wait(self, condition, description, timeout=15):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            for process in self.processes.values():
                if process.poll() is not None:
                    raise RuntimeError("daemon exited before " + description)
            for path in self.error_files:
                if path.exists():
                    raise RuntimeError(path.read_text())
            result = condition()
            if result:
                return result
            time.sleep(0.05)
        raise RuntimeError("timed out waiting for " + description)

    def wait_open(self, index, remote):
        identity = re.escape(self.identities[remote])
        self.wait(lambda: re.search(r"'STATE_OPEN'\s+'%s'" % identity, self.text(index)),
                  "%s connection to %s" % (self.identities[index], self.identities[remote]))

    def wait_all_open(self):
        self.wait(
            lambda: all(
                re.search(r"'STATE_OPEN'\s+'%s'" % re.escape(self.identities[remote]),
                          self.text(index))
                for first, second in self.links
                for index, remote in ((first, second), (second, first))
            ),
            "all peer links to reach STATE_OPEN",
        )

    def wait_transport(self):
        protocol = "SCTP" if self.sctp else "TCP"
        security = (",DTLS" if self.dtls and self.sctp else ",TLS") if self.tls else ""
        self.wait(
            lambda: all(
                re.search(r"(?:Connected to|CONNECTED TO) '%s' \(%s%s,soc#\d+\)" %
                          (re.escape(self.identities[remote]), protocol, security),
                          self.text(index))
                for first, second in self.links
                for index, remote in ((first, second), (second, first))
            ),
            "all peer links to report %s%s" % (protocol, security),
        )

    def dump_state(self, index, directory):
        process = self.processes[index]
        with (directory / ("peer%d-backtrace.txt" % (index + 1))).open("w") as output:
            subprocess.run(
                ["gdb", "-nx", "-batch", "-ex", "set auto-load off",
                 "-ex", "set pagination off", "-ex", "attach %d" % process.pid,
                 "-ex", "thread apply all bt", "-ex", "detach"],
                stdout=output, stderr=subprocess.STDOUT, timeout=15, check=True,
            )

    def stop(self):
        failures = []
        diagnostics_root = os.environ.get("FD_MULTI_PEER_DIAGNOSTICS")
        diagnostics = None
        for callback in reversed(self.stop_callbacks):
            try:
                callback()
            except Exception as error:
                failures.append("stop callback failed: %s" % error)
        for index in reversed(self.start_order):
            process = self.processes[index]
            if process.poll() is None:
                try:
                    process.send_signal(signal.SIGTERM)
                except ProcessLookupError:
                    pass
                try:
                    process.wait(timeout=25)
                except subprocess.TimeoutExpired:
                    if diagnostics_root:
                        try:
                            if diagnostics is None:
                                Path(diagnostics_root).mkdir(parents=True, exist_ok=True)
                                diagnostics = Path(tempfile.mkdtemp(
                                    prefix=Path(sys.argv[0]).stem + "-", dir=diagnostics_root))
                            self.dump_state(index, diagnostics)
                        except (OSError, subprocess.SubprocessError) as error:
                            failures.append("peer %d diagnostic capture failed: %s" %
                                            (index + 1, error))
                    try:
                        process.kill()
                    except ProcessLookupError:
                        pass
                    process.wait(timeout=5)
                    failures.append("peer %d did not stop gracefully" % (index + 1))
            if process.returncode:
                failures.append("peer %d exited with status %s" %
                                (index + 1, process.returncode))
        for callback in self.stopped_callbacks:
            try:
                callback()
            except Exception as error:
                failures.append("post-stop check failed: %s" % error)
        if diagnostics is not None:
            for index, log in self.logs.items():
                try:
                    log.flush()
                    shutil.copyfile(log.name, diagnostics / ("peer%d.log" % (index + 1)))
                except OSError as error:
                    failures.append("peer %d log capture failed: %s" % (index + 1, error))
            print("Peer shutdown diagnostics saved to %s" % diagnostics, file=sys.stderr)
        return failures


def require_sctp_auth():
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP)
    except OSError as error:
        if error.errno not in (errno.EPROTONOSUPPORT, errno.EAFNOSUPPORT,
                               errno.ESOCKTNOSUPPORT):
            raise
        print("SKIP: SCTP is unavailable.", file=sys.stderr)
        raise SystemExit(77)
    with sock:
        try:
            result = sock.getsockopt(socket.IPPROTO_SCTP, SCTP_AUTH_SUPPORTED, 8)
        except OSError as error:
            if error.errno not in (errno.ENOPROTOOPT, errno.EOPNOTSUPP):
                raise
            result = None
        if result is None or struct.unpack("=iI", result)[1] == 0:
            print("SKIP: SCTP-AUTH is unavailable (net.sctp.auth_enable=1 required).",
                  file=sys.stderr)
            raise SystemExit(77)


def run_scenario(daemon, scenario, count, links, requires_sctp_auth=True):
    def terminate(signum, frame):
        raise SystemExit("test interrupted by signal %d" % signum)

    signal.signal(signal.SIGTERM, terminate)

    def enabled(name):
        value = os.environ.get(name, "0")
        if value not in ("0", "1"):
            raise ValueError("%s must be 0 or 1" % name)
        return value == "1"

    tls = enabled("FD_MULTI_PEER_TLS")
    sctp = enabled("FD_MULTI_PEER_SCTP")
    dtls = enabled("FD_MULTI_PEER_DTLS")
    if requires_sctp_auth and tls and sctp and dtls:
        require_sctp_auth()
    build_dir = Path(daemon).resolve().parent.parent / "tests"
    with tempfile.TemporaryDirectory(dir=build_dir) as temporary:
        with Peers(daemon, Path(temporary), count, links, tls=tls, sctp=sctp,
                   openssl=os.environ.get("FD_MULTI_PEER_OPENSSL"),
                   dtls=dtls) as peers:
            scenario(peers)
