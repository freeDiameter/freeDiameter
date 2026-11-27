import ctypes
import ctypes.util
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path


daemon, build_dir, sctp_disabled = sys.argv[1:4]


def rejected(config, message):
    with tempfile.TemporaryDirectory(dir=Path(build_dir) / "tests") as temporary:
        path = Path(temporary) / "invalid.conf"
        path.write_text(config)
        result = subprocess.run([daemon, "-c", str(path)], stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, universal_newlines=True,
                                timeout=30)
        if result.returncode == 0 or message not in result.stdout:
            raise AssertionError("Expected configuration rejection %r:\n%s" %
                                 (message, result.stdout))


def base_config(extra):
    return ('Identity = "bind-test.localdomain";\n'
            'Realm = "localdomain";\n'
            'Port = 0;\nSecPort = 0;\nNo_IPv6;\n%s\n' % extra)


def terminate(process, log):
    if process.poll() is None:
        process.send_signal(signal.SIGTERM)
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            raise AssertionError("freeDiameterd did not stop after SIGTERM")
    log.close()


def start_daemon(config, directory):
    path = directory / "daemon.conf"
    path.write_text(config)
    log_path = directory / "daemon.log"
    log = log_path.open("w+b")
    process = subprocess.Popen([daemon, "-c", str(path)], stdin=subprocess.DEVNULL,
                               stdout=log, stderr=subprocess.STDOUT)
    return process, log, log_path


def wait_for_tcp_connection(listener, process, log_path):
    listener.settimeout(0.25)
    deadline = time.monotonic() + 15
    last_error = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError("freeDiameterd exited:\n%s" %
                                 log_path.read_text(errors="replace"))
        try:
            connection, address = listener.accept()
            connection.close()
            return address[0]
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise AssertionError("TCP connection was not accepted: %s\n%s" %
                         (last_error, log_path.read_text(errors="replace")))


def test_tcp_source_address(directory):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    listener.settimeout(15)
    config = base_config(
        'ConnectPeer = "bind-server.localdomain" { No_TLS; No_SCTP; '
        'ConnectTo = "127.0.0.1"; BindAddress = "127.0.0.2"; '
        'Port = %d; TcTimer = 1; };' % listener.getsockname()[1])
    process, log, log_path = start_daemon(config, directory)
    try:
        source = wait_for_tcp_connection(listener, process, log_path)
        if source != "127.0.0.2":
            raise AssertionError("BindAddress selected %s, expected 127.0.0.2" % source)
    finally:
        terminate(process, log)
        listener.close()


def local_ipv4_address():
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("198.51.100.1", 9))
        address = probe.getsockname()[0]
    finally:
        probe.close()
    if address.startswith("127.") or address == "0.0.0.0":
        raise RuntimeError("no non-loopback local IPv4 address is available")
    return address


def free_listener_port(address, protocol, other_address):
    for _ in range(10):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
        try:
            sock.bind((address, 0))
            port = sock.getsockname()[1]
        finally:
            sock.close()
        other = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
        try:
            other.bind((other_address, port))
            return port
        except OSError:
            pass
        finally:
            other.close()
    raise RuntimeError("unable to find a free listener port")


def wait_for_connection(address, port, process, log_path, protocol):
    deadline = time.monotonic() + 15
    last_error = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError("freeDiameterd exited:\n%s" %
                                 log_path.read_text(errors="replace"))
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
        client.settimeout(1)
        try:
            client.connect((address, port))
            return client
        except OSError as error:
            last_error = error
            client.close()
        time.sleep(0.05)
    raise AssertionError("connection to %s:%d failed: %s\n%s" %
                         (address, port, last_error, log_path.read_text(errors="replace")))


def test_tcp_private_listener(directory):
    public = local_ipv4_address()
    port = free_listener_port(public, socket.IPPROTO_TCP, "127.0.0.2")
    config = ('Identity = "listener-test.localdomain";\n'
              'Realm = "localdomain";\n'
              'Port = %d;\nSecPort = 0;\nNo_IPv6;\nNo_SCTP;\n'
              'ListenOn = "%s";\nPrivateListenOn = "127.0.0.2";\n'
              % (port, public))
    process, log, log_path = start_daemon(config, directory)
    try:
        for target in (public, "127.0.0.2"):
            client = wait_for_connection(target, port, process, log_path,
                                         socket.IPPROTO_TCP)
            client.close()
    finally:
        terminate(process, log)


def wait_for_sctp_accept(listener, process, log_path):
    listener.settimeout(0.25)
    deadline = time.monotonic() + 15
    last_error = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError("freeDiameterd exited:\n%s" %
                                 log_path.read_text(errors="replace"))
        try:
            connection, address = listener.accept()
            connection.close()
            return address[0]
        except OSError as error:
            last_error = error
            time.sleep(0.05)
    raise AssertionError("SCTP connection was not accepted: %s\n%s" %
                         (last_error, log_path.read_text(errors="replace")))


def test_sctp_source_address(directory, protocol):
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    config = base_config(
        'ConnectPeer = "bind-server.localdomain" { No_TLS; No_TCP; '
        'ConnectTo = "127.0.0.1"; BindAddress = "127.0.0.2"; '
        'Port = %d; TcTimer = 1; };' % listener.getsockname()[1])
    process, log, log_path = start_daemon(config, directory)
    try:
        source = wait_for_sctp_accept(listener, process, log_path)
        if source != "127.0.0.2":
            raise AssertionError("SCTP BindAddress selected %s, expected 127.0.0.2" %
                                 source)
    finally:
        terminate(process, log)
        listener.close()


def wait_for_sctp_connection(address, port, process, log_path, protocol):
    deadline = time.monotonic() + 15
    last_error = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise AssertionError("freeDiameterd exited:\n%s" %
                                 log_path.read_text(errors="replace"))
        client = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
        client.settimeout(1)
        try:
            client.connect((address, port))
            return client
        except OSError as error:
            last_error = error
            client.close()
        time.sleep(0.05)
    raise AssertionError("SCTP connection was not accepted: %s\n%s" %
                         (last_error, log_path.read_text(errors="replace")))


def sctp_peer_addresses(sock):
    library = ctypes.util.find_library("sctp")
    if not library:
        raise RuntimeError("libsctp is unavailable")
    sctp = ctypes.CDLL(library)
    getpaddrs = sctp.sctp_getpaddrs
    getpaddrs.argtypes = [ctypes.c_int, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p)]
    getpaddrs.restype = ctypes.c_int
    freepaddrs = sctp.sctp_freepaddrs
    freepaddrs.argtypes = [ctypes.c_void_p]
    freepaddrs.restype = None

    class SockaddrIn(ctypes.Structure):
        _fields_ = [("family", ctypes.c_ushort), ("port", ctypes.c_ushort),
                    ("address", ctypes.c_ubyte * 4), ("padding", ctypes.c_ubyte * 8)]

    addresses = ctypes.c_void_p()
    count = getpaddrs(sock.fileno(), 0, ctypes.byref(addresses))
    if count < 0:
        raise OSError(ctypes.get_errno(), "sctp_getpaddrs failed")
    try:
        values = ctypes.cast(addresses, ctypes.POINTER(SockaddrIn))
        return [socket.inet_ntoa(bytes(values[index].address)) for index in range(count)]
    finally:
        freepaddrs(addresses)


def test_sctp_private_listener_addresses(directory, protocol):
    if not ctypes.util.find_library("sctp"):
        print("Skipping SCTP address-list check: libsctp is unavailable", file=sys.stderr)
        return

    public = local_ipv4_address()
    port = free_listener_port(public, protocol, "127.0.0.2")
    config = ('Identity = "listener-test.localdomain";\n'
              'Realm = "localdomain";\n'
              'Port = %d;\nSecPort = 0;\nNo_IPv6;\nNo_TCP;\n'
              'ListenOn = "%s";\nPrivateListenOn = "127.0.0.2";\n'
              % (port, public))
    process, log, log_path = start_daemon(config, directory)
    try:
        for target, expected in ((public, public), ("127.0.0.2", "127.0.0.2")):
            client = wait_for_sctp_connection(target, port, process, log_path, protocol)
            try:
                advertised = sctp_peer_addresses(client)
                if expected not in advertised:
                    raise AssertionError("SCTP listener %s omitted its address: %s" %
                                         (target, advertised))
                unexpected = "127.0.0.2" if target == public else public
                if unexpected in advertised:
                    raise AssertionError("SCTP listener %s exposed separate endpoint %s: %s" %
                                         (target, unexpected, advertised))
            finally:
                client.close()
    finally:
        terminate(process, log)


rejected(base_config('PrivateListenOn = "127.0.0.2";'),
         "PrivateListenOn requires at least one explicit ListenOn endpoint.")
rejected(base_config('ListenOn = "192.0.2.1"; '
                     'PrivateListenOn = "192.0.2.1";'),
         "An endpoint cannot be listed in both ListenOn and PrivateListenOn.")
rejected(base_config('ListenOn = "192.0.2.1"; PrivateListenOn = "0.0.0.0";'),
         "PrivateListenOn requires a unicast address.")
rejected(base_config('ConnectPeer = "peer.localdomain" { '
                     'BindAddress = "0.0.0.0"; };'),
         "BindAddress requires a unicast address.")
rejected(base_config('ConnectPeer = "peer.localdomain" { No_IPv6; '
                     'BindAddress = "::1"; };'),
         "BindAddress conflicts with the ConnectPeer address family.")
rejected(base_config('ConnectPeer = "peer.localdomain" { BindAddress = "127.0.0.1"; '
                     'BindAddress = "127.0.0.2"; };'),
         "BindAddress may only be specified once per ConnectPeer.")

with tempfile.TemporaryDirectory(dir=Path(build_dir) / "tests") as temporary:
    directory = Path(temporary)
    test_tcp_source_address(directory)
    test_tcp_private_listener(directory)
    if sctp_disabled.lower() not in ("1", "on", "true"):
        try:
            protocol = getattr(socket, "IPPROTO_SCTP", None)
            if protocol is None:
                protocol = socket.getprotobyname("sctp")
            probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM, protocol)
            probe.close()
        except OSError as error:
            print("Skipping SCTP tests: %s" % error, file=sys.stderr)
        else:
            test_sctp_source_address(directory, protocol)
            test_sctp_private_listener_addresses(directory, protocol)
