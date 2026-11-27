"""Linux integration test; run with daemon, test_app, acl_wl and rt_discover paths.

Example:
  python3 tests/test_multi_peer_discovery.py test.discovery/freeDiameterd/freeDiameterd \
    test.discovery/extensions/test_app.fdx test.discovery/extensions/acl_wl.fdx \
    test.discovery/extensions/rt_discover.fdx

The fixture intercepts the resolver only in the two child processes, answers
from an in-process DNS wire fixture and refuses nonnumeric NSS lookups. No
system resolver configuration or external DNS service is used.
"""
import os
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from multi_peer_harness import Peers

APP = 0xffffff
REALM = "localdomain"
CROSS_REALM = "route.example"


def server_certificate(peers, openssl, realm, target=True, extra_identity=False,
                       include_realm=True):
    cert, _ = peers.credentials[1]
    ext = peers.directory / "server-san.cnf"
    sans = [peers.identities[1], REALM]
    if include_realm and realm != REALM:
        sans.append(realm)
    if target:
        sans.append("srv." + realm)
    if extra_identity:
        sans.append("other.localdomain")
    ext.write_text("[peer]\nbasicConstraints = critical,CA:FALSE\n"
                   "keyUsage = critical,digitalSignature,keyEncipherment\n"
                   "extendedKeyUsage = serverAuth,clientAuth\nsubjectAltName = %s\n" %
                   ",".join("DNS:" + name for name in sans))
    subprocess.run([openssl, "x509", "-req", "-in", str(peers.directory / "peer2.csr"),
                    "-CA", str(peers.ca_cert), "-CAkey", str(peers.directory / "ca.key"),
                    "-set_serial", "2", "-days", "2", "-sha256", "-extfile", str(ext),
                    "-extensions", "peer", "-out", str(cert)],
                   check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)


def scenario(daemon, test_app, acl, routing, fixture, openssl, directory, mode, dns,
             rejection=None, realm=REALM):
    with Peers(daemon, directory, 2, [], tls=True, openssl=openssl) as peers:
        server_certificate(peers, openssl, realm, target=rejection != "certificate",
                           extra_identity=rejection == "identity",
                           include_realm=rejection != "realm-certificate")
        query_log = directory / "queries.log"
        environment = {"LD_PRELOAD": str(fixture), "FD_DISCOVERY_FIXTURE_PORT": str(peers.ports[1]),
                       "FD_DISCOVERY_FIXTURE_SCENARIO": dns,
                       "FD_DISCOVERY_FIXTURE_REALM": realm,
                       "FD_DISCOVERY_FIXTURE_LOG": str(query_log)}
        ready = directory / "dns-ready"
        if mode == "startup-retry":
            environment["FD_DISCOVERY_FIXTURE_READY"] = str(ready)
        whitelist = directory / "acl.conf"
        whitelist.write_text(peers.identities[0] + "\n")
        server = directory / "server.conf"
        server.write_text("mode = server;\n")
        peers.start(1, [(acl, whitelist), (test_app, server)], extra_config="NoRelay;",
                    env=environment)
        peers.wait(lambda: "Local server address(es)" in peers.text(1), "server startup")

        client = directory / "client.conf"
        # A non-relay server accepts a different destination realm only when
        # the request names it explicitly as Destination-Host.
        client.write_text('mode = client;\ndest-realm = "%s";\n%s' %
                          (realm, ('dest-host = "%s";\n' % peers.identities[1])
                           if realm != REALM else ""))
        extensions = [(test_app, client)]
        allowed = "other.localdomain" if rejection == "identity" else peers.identities[1]
        extra = "NoRelay;\n"
        if mode in ("startup", "startup-retry"):
            options = ""
            if mode == "startup":
                origin_realm = "wrong.example" if rejection == "origin-realm" else REALM
                priority = "INVALID" if rejection == "priority" else "NORMAL"
                options = ('TcTimer = 60; TwTimer = 45; No_IPv6; No_SCTP; '
                           'TLS_Prio = "%s"; Realm = "%s"; ' % (priority, origin_realm))
            extra += ('DiscoverPeer = "%s" { ApplicationId = "%d"; '
                      'AuthorizedPeer = "%s"; %s};\n' % (realm, APP, allowed, options))
        elif mode == "routing":
            conf = directory / "discovery.conf"
            origin_realm = "wrong.example" if rejection == "origin-realm" else REALM
            priority = "INVALID" if rejection == "priority" else "NORMAL"
            conf.write_text('Realm = "%s" { Peer = "%s"; TcTimer = 60; TwTimer = 45; '
                            'No_IPv6; No_SCTP; TLS_Prio = "%s"; Realm = "%s"; };\n' %
                            (realm, allowed, priority, origin_realm))
            extensions.append((routing, conf))
        peers.start(0, extensions, extra_config=extra, env=environment)
        peers.wait(lambda: "Local server address(es)" in peers.text(0), "client startup")
        if mode in ("routing", "none") and query_log.exists() and query_log.read_text():
            raise AssertionError("discovery began before a routing request")
        if mode == "startup-retry":
            peers.wait(lambda: "Startup discovery for %s application %d failed" % (realm, APP)
                       in peers.text(0), "initial DNS failure", timeout=15)
            ready.touch()
            peers.wait(lambda: re.search(r"'STATE_OPEN'\s+'%s'" %
                                         re.escape(peers.identities[1]), peers.text(0)),
                       "discovery after DNS recovery", timeout=55)
        if mode in ("startup", "startup-retry") and not rejection:
            peers.wait_open(0, 1)
        if mode not in ("startup", "startup-retry"):
            peers.processes[0].send_signal(signal.SIGUSR1)
        if mode == "none":
            peers.wait(lambda: re.search(r"RECV .*Status: (?!2001)\d+", peers.text(0)),
                       "no-route answer", timeout=15)
            time.sleep(1)
            if query_log.exists() and query_log.read_text():
                raise AssertionError("discovery performed without opt-in: " + query_log.read_text())
            if "STATE_OPEN" in peers.text(0):
                raise AssertionError("peer opened without discovery opt-in")
            return
        if rejection:
            failure = ("rt_discover: discovery failed for %s application %d" if mode == "routing"
                       else "Startup discovery for %s application %d failed")
            peers.wait(lambda: failure % (realm, APP)
                       in peers.text(0), "discovery rejection", timeout=40)
            if re.search(r"'STATE_OPEN'\s+'%s'" % re.escape(allowed), peers.text(0)):
                raise AssertionError("unauthorized peer reached STATE_OPEN")
            if rejection == "identity":
                if ("Origin-Host(264)[-M]=\"%s\"" % peers.identities[1] not in peers.text(0)
                        or "save_remote_CE_info" not in peers.text(0)):
                    raise AssertionError("peer identity mismatch was not exercised")
            elif rejection == "origin-realm":
                if "has an unexpected Origin-Realm" not in peers.text(0):
                    raise AssertionError("Origin-Realm restriction was not exercised")
            elif rejection == "priority":
                if "gnutls_priority_set_direct" not in peers.text(0):
                    raise AssertionError("TLS priority override was not exercised")
            elif "fd_cnx_verify_discovery" not in peers.text(0):
                raise AssertionError("certificate domain validation was not exercised")
        else:
            if mode in ("startup", "startup-retry"):
                peers.processes[0].send_signal(signal.SIGUSR1)
            answer = peers.wait(
                lambda: re.search(r"RECV ([0-9a-f]+) \(Ok\) Status: 2001 From '%s' "
                                  r"\('localdomain'\)" %
                                  re.escape(peers.identities[1]), peers.text(0)),
                "successful test_app answer", timeout=25)
            dest_host = peers.identities[1] if realm != REALM else "-"
            sent = re.search(r"SEND ([0-9a-f]+) to '%s' \(%s\)" %
                             (re.escape(realm), re.escape(dest_host)), peers.text(0))
            if not sent or sent.group(1) != answer.group(1):
                raise AssertionError("test_app answer did not echo the sent value")
            peers.wait_open(0, 1)
            peers.wait(lambda: "ECHO Test-Request received from '%s'" % peers.identities[0]
                       in peers.text(1), "test_app server callback")
            if not re.search(r"Connected to '%s' \(TCP,TLS,soc#\d+\)" %
                             re.escape(peers.identities[1]), peers.text(0)):
                raise AssertionError("discovered connection did not use TLS/TCP")
        queries = query_log.read_text().splitlines() if query_log.exists() else []
        srv = ("_naptr._tcp." if dns == "naptr" else "_diameters._tcp.") + realm
        expected = ["35 " + realm, "33 " + srv, "1 srv." + realm]
        if not all(query in queries for query in expected):
            raise AssertionError("missing DNS path queries: %r" % queries)
        if dns == "naptr" and "33 _diameters._tcp." + realm in queries:
            raise AssertionError("unexpected fallback after NAPTR: %r" % queries)


def main():
    def terminate(signum, frame):
        raise SystemExit("test interrupted by signal %d" % signum)

    signal.signal(signal.SIGTERM, terminate)
    if not sys.platform.startswith("linux"):
        print("SKIP: resolver interposition requires Linux", file=sys.stderr)
        return 77
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    daemon, test_app, acl, routing = (str(Path(arg).resolve()) for arg in sys.argv[1:])
    openssl = shutil.which("openssl")
    compiler = shutil.which(os.environ.get("CC", "cc"))
    if not openssl or not compiler:
        raise RuntimeError("openssl and a C compiler are required")
    build_tests = Path(daemon).parent.parent / "tests"
    build_tests.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="discovery-", dir=str(build_tests)) as root:
        root = Path(root)
        fixture = root / "dns_fixture.so"
        source = Path(__file__).with_name("discovery_dns_fixture.c")
        subprocess.run([compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-fPIC",
                        "-shared", str(source), "-ldl", "-o", str(fixture)], check=True)
        for mode, dns, rejection, realm in (
                ("startup", "naptr", None, REALM), ("startup", "fallback", None, REALM),
                ("startup-retry", "naptr", None, REALM),
                ("routing", "naptr", None, REALM), ("routing", "fallback", None, REALM),
                ("routing", "naptr", "origin-realm", REALM),
                ("routing", "naptr", "priority", REALM),
                ("startup", "naptr", "identity", REALM),
                ("startup", "naptr", "certificate", REALM),
                ("startup", "naptr", "origin-realm", REALM),
                ("startup", "naptr", "priority", REALM),
                ("none", "naptr", None, REALM),
                ("startup", "naptr", None, CROSS_REALM),
                ("routing", "naptr", None, CROSS_REALM),
                ("startup", "naptr", "realm-certificate", CROSS_REALM)):
            directory = root / ("%s-%s-%s-%s" % (mode, dns, rejection or "ok", realm))
            directory.mkdir()
            print("Discovery scenario:", mode, dns, rejection or "ok", realm, flush=True)
            scenario(daemon, test_app, acl, routing, fixture, openssl, directory,
                     mode, dns, rejection, realm)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
