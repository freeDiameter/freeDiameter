# Operations

## Configuration and startup

[freediameter.conf.sample](freediameter.conf.sample) is the configuration
reference and a starting point, not a ready-to-run configuration. It documents
defaults, per-peer options and `LoadExtension` syntax. Copy it to a private
location, provide site-specific identity, realm and TLS credentials, and
replace example paths. Extension configuration formats are independent; see
[extensions](extensions/README.md).

The daemon accepts `--config`, `--daemon`, `--pidfile`, `--syslog` and
logging/debug options; run `freeDiameterd --help` for the built binary's
complete list. A relative main configuration path is tried from the current
working directory and then under the compiled default configuration directory.
Relative extension paths may also be searched under the compiled extension
directory; extension configuration arguments are interpreted by each
extension. Use absolute paths when the working directory is uncertain.
The installed locations are configurable with CMake; inspect the cache rather
than assuming a distribution's paths.

## Peers and transport security

`ConnectPeer` configures a maintained outgoing connection and authorizes the
corresponding identity. Unknown incoming peers are rejected by default unless
an extension authorizes them. Use [acl_wl.conf.sample](acl_wl.conf.sample)
for whitelist syntax. DNS records alone do not authorize a peer.

TLS credentials and trust anchors must match the peer identities used in the
deployment. Protect private keys and any extension configuration containing
passwords. `No_TLS` assumes external transport security such as IPsec; it
does not establish that security. Do not copy cleartext or test credentials
from local fixtures into a production deployment.

SCTP and experimental DTLS/SCTP depend on build and host support.
The sample describes secure port defaults, `ENABLE_DTLS_SCTP`, SCTP-AUTH,
fallback behavior, record-size limits and per-peer security selection.
Do not assume DTLS/SCTP was enabled merely because SCTP was built.
The test PKI in [contrib/PKI](../contrib/README) is not production PKI.

## Discovery

Discovery is opt-in. `DiscoverPeer` starts an explicitly authorized discovery
at startup; the optional `rt_discover` extension initiates it on routing
failure. See [freediameter.conf.sample](freediameter.conf.sample) and
[rt_discover.conf.sample](rt_discover.conf.sample) for their different
retry, authorization and configuration rules. The lower-level
[discovery API](../include/freeDiameter/discovery.h) does not itself retry
failed operations. Discovery requires supported system resolver functionality
and immediate TLS/TCP or DTLS/SCTP; see those references for certificate-name
requirements and failure behavior.

## Signals and shutdown

The daemon catches SIGINT and SIGTERM and requests shutdown with a 60-second
forced-exit watchdog. SIGUSR1 and SIGUSR2 are delivered as extension triggers:
their effects depend on the loaded extensions, not a universal configuration
reload. Check each extension's sample before sending either signal.

The shutdown process attempts to drain admitted work and disconnect peers;
delivery cannot be guaranteed during network or peer failure. For deadline
details and embedding considerations, see [test coverage](test-coverage.md)
and [development](development.md).

## Troubleshooting

Start with the daemon logs and `--help`. Check the selected configuration
path, extension build options and paths, certificate identity and trust,
local port availability, peer authorization, and matching transports. A
successful build does not establish that SCTP, DTLS/SCTP, a database or an
optional test prerequisite is available on the host. For diagnostics and
known test limitations, see [test coverage](test-coverage.md).
