import os
import re
import subprocess
import sys
from pathlib import Path

from multi_peer_harness import run_scenario
from multi_peer_test_app import configure


daemon, test_app, role = sys.argv[1:4]
openssl = os.environ.get("FD_DTLS_SCTP_OPENSSL") or os.environ["FD_MULTI_PEER_OPENSSL"]


def scenario(peers):
    help_text = subprocess.run([openssl, "s_" + role, "-help"], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, check=True).stdout.decode(errors="replace")
    if not re.search(r"^\s+-sctp\s", help_text, re.MULTILINE):
        print("SKIP: OpenSSL was built without SCTP support.", file=sys.stderr)
        raise SystemExit(77)

    cert, key = peers.credentials[1]
    arguments = [openssl, "s_" + role, "-sctp", "-dtls1_2", "-brief",
                 "-cert", str(cert), "-key", str(key),
                 "-CAfile", str(peers.ca_cert), "-verify_return_error"]
    output = peers.directory / "openssl.out"
    errors = peers.directory / "openssl.err"
    if role == "client":
        peers.start(0, [(test_app, configure(peers, 0, 1, mode="server"))],
                    connect_neighbors=False)
        peers.wait(lambda: "Local server address(es)" in peers.text(0), "server startup")
        arguments += ["-connect", "127.0.0.1:%d" % peers.ports[0],
                      "-verify_hostname", peers.identities[0], "-quiet"]
    elif role == "server":
        arguments += ["-accept", str(peers.ports[1]), "-Verify", "1", "-quiet"]
    else:
        raise ValueError("unknown OpenSSL role: " + role)

    with output.open("w+b") as out, errors.open("w+b") as err:
        process = subprocess.Popen(arguments, stdin=subprocess.PIPE, stdout=out, stderr=err)
        try:
            def running():
                if process.poll() is not None:
                    raise RuntimeError("OpenSSL exited with status %d" % process.returncode)
                return True

            if role == "server":
                def listening():
                    running()
                    sockets = set()
                    fd_directory = "/proc/%d/fd" % process.pid
                    for name in os.listdir(fd_directory):
                        try:
                            sockets.add(os.readlink(os.path.join(fd_directory, name)))
                        except FileNotFoundError:
                            pass
                    for line in Path("/proc/net/sctp/eps").read_text().splitlines():
                        fields = line.split()
                        if (len(fields) >= 8 and fields[5] == str(peers.ports[1])
                                and "socket:[%s]" % fields[7] in sockets):
                            return True
                    return False

                peers.wait(listening,
                           "OpenSSL server startup")
                config = ('Port = 0; SecPort = 0;\n'
                          'ConnectPeer = "%s" { ConnectTo = "127.0.0.1"; SctpSecDTLS; Port = %d; };\n'
                          % (peers.identities[1], peers.ports[1]))
                peers.start(0, [(test_app, configure(peers, 0, 1, mode="client"))],
                            extra_config=config, connect_neighbors=False)

            marker = "CONNECTION ESTABLISHED" if role == "client" else "Verification: OK"
            peers.wait(lambda: running() and marker in errors.read_text(errors="replace"),
                       "OpenSSL DTLS handshake", timeout=40)
            if "DTLSv1.2" not in errors.read_text(errors="replace"):
                raise AssertionError("OpenSSL did not negotiate DTLS 1.2")
            if role == "client":
                process.stdin.write(b"\x00" * 20)
                process.stdin.flush()
                peers.wait(lambda: "Invalid Diameter message in DTLS/SCTP record" in peers.text(0),
                           "freeDiameter receipt of OpenSSL application data")
            else:
                def received_cer():
                    running()
                    data = output.read_bytes()
                    return len(data) >= 20 and data[0] == 1 and int.from_bytes(data[5:8], "big") == 257

                peers.wait(received_cer, "OpenSSL receipt of Diameter CER")
        except BaseException:
            print("\n--- OpenSSL log ---\n" + errors.read_text(errors="replace"), file=sys.stderr)
            raise
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
            process.stdin.close()


if __name__ == "__main__":
    run_scenario(daemon, scenario, 2, [])
