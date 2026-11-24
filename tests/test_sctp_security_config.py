import subprocess
import sys
import tempfile
from pathlib import Path


daemon, cmake, source, build, enabled_dtls = sys.argv[1:6]


def rejected(command, message, cwd=None):
    result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            universal_newlines=True, timeout=30, cwd=cwd)
    if result.returncode == 0 or message not in result.stdout:
        raise AssertionError("Expected rejection containing %r; exit %d:\n%s" %
                             (message, result.returncode, result.stdout))


with tempfile.TemporaryDirectory(dir=Path(build) / "tests") as temporary:
    directory = Path(temporary)
    cases = [
        ("SctpSec3436 = 15658;", "syntax error"),
        ("SctpSec3436 = 0;", "syntax error"),
        ("SctpSecDTLS;", "syntax error"),
    ]
    conflicts = [
        ('ConnectPeer = "peer.localdomain" { SctpSecDTLS; SctpSec3436; };',
         "SctpSec3436 conflicts with SctpSecDTLS."),
        ('ConnectPeer = "peer.localdomain" { SctpSec3436; SctpSecDTLS; };',
         "SctpSecDTLS conflicts with SctpSec3436."),
        ('ConnectPeer = "peer.localdomain" { SctpSecDTLS; No_TLS; };',
         "SctpSecDTLS requires immediate security, not No_TLS or TLS_old_method."),
        ('ConnectPeer = "peer.localdomain" { No_TLS; SctpSecDTLS; };',
         "SctpSecDTLS requires immediate security, not No_TLS or TLS_old_method."),
        ('ConnectPeer = "peer.localdomain" { SctpSecDTLS; TLS_old_method; };',
         "SctpSecDTLS requires immediate security, not No_TLS or TLS_old_method."),
        ('ConnectPeer = "peer.localdomain" { TLS_old_method; SctpSecDTLS; };',
         "SctpSecDTLS requires immediate security, not No_TLS or TLS_old_method."),
        ('TLS_old_method; ConnectPeer = "peer.localdomain" { SctpSecDTLS; };',
         "SctpSecDTLS requires immediate security, not No_TLS or TLS_old_method."),
        ('ConnectPeer = "peer.localdomain" { SctpSecDTLS; }; TLS_old_method;',
         "TLS_old_method conflicts with a ConnectPeer directive SctpSecDTLS."),
    ]
    if enabled_dtls == "0":
        unsupported = "SctpSecDTLS requires a build with ENABLE_DTLS_SCTP."
        cases.append(('ConnectPeer = "peer.localdomain" { SctpSecDTLS; };', unsupported))
        cases.extend((config, unsupported) for config, _ in conflicts)
    else:
        cases.extend(conflicts)
    for index, (config, message) in enumerate(cases):
        path = directory / ("invalid%d.conf" % index)
        path.write_text(config + "\n")
        rejected([daemon, "-c", str(path)], message)

    cmake_build = directory / "build"
    cmake_build.mkdir()
    rejected([cmake, source,
              "-DENABLE_DTLS_SCTP=ON", "-DDISABLE_SCTP=ON"],
             "ENABLE_DTLS_SCTP requires SCTP support", cwd=str(cmake_build))
