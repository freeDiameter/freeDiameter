# Test coverage

This document describes the behaviors exercised by the CTest plans. It is not
a statement of line, branch, or complete protocol coverage. Tests available in
a build depend on its CMake options and installed prerequisites.

## Default plan

With `BUILD_TESTING` enabled, `ctest --output-on-failure` runs the registered
tests in the build directory. The opt-in multi-peer tests below are not registered
unless explicitly enabled.

| Test | Main coverage |
| --- | --- |
| `testsctp` | SCTP socket operations and message streams; it does no SCTP work when built with `DISABLE_SCTP`. |
| `testostr` | Octet-string hashing and Diameter identity operations. |
| `testfifo` | FIFO operations, timeouts, concurrent access, cancellation, thresholds, and queue limits. |
| `testpeers` | Peer creation and identity lookup. |
| `testdict` | Dictionary objects, searches, rules, and deletion. |
| `testmesg` | Message and AVP construction, serialization, parsing, dictionary and rule validation, and answers. |
| `testmesg_stress` | Repeated message construction, parsing, answer creation, and serialization. |
| `testsess` | Session identifiers, state, handlers, and timeouts. |
| `testdisp` | Dispatch handlers, matching, callback ordering, and application support. |
| `testcnx` | Connection operations including TCP and TLS, plus SCTP where enabled. |
| `testloadext` | Loading built extension libraries and resolving their entry points; it does not execute the entry points. |
| `test_dbg_interactive` | When `BUILD_DBG_INTERACTIVE` is enabled: script and console operation, callbacks, synchronization, failure handling, and execution of the Python sample. |

`testappacct` is separately opt-in and requires a configured PostgreSQL
database; see [tests/CMakeLists.txt](../tests/CMakeLists.txt) and
[testappacct.c](../tests/testappacct.c).

## Opt-in multi-peer plan

For example, from the repository root:

```sh
mkdir test.multi-peer
cd test.multi-peer
cmake .. -DBUILD_MULTI_PEER_TESTS=ON -DBUILD_TEST_APP=ON \
  -DBUILD_TEST_NETEMUL=ON -DBUILD_DBG_INTERACTIVE=ON -DBUILD_DBG_DICT_DUMP_JSON=ON \
  -DBUILD_RT_EREG=ON -DBUILD_APP_REDIRECT=ON
cmake --build .
ctest -L multi-peer --output-on-failure
```

The plan requires Python 3.6 or newer. The current application scenarios
require `test_app`. Extension-specific scenarios are registered only when
their required targets are built. `dbg_interactive` requires Python development
files and SWIG; `dbg_dict_dump_json` requires jsoncpp. The redirect scenario
also requires `app_redirect`, `rt_default`, and `dbg_msg_dumps`; the rewrite
scenario requires `dbg_msg_dumps`. The balancing scenarios require `dbg_rt`
and `dbg_loglevel`, and the debug scenarios require `dbg_loglevel`. Scenarios
that inspect debug routing output are not registered in `MaxPerformance` builds,
which strip that output.
GitHub Actions runs the multi-peer plan on Ubuntu for all four combinations of
TCP/SCTP and TLS enabled/disabled.

The separate AddressSanitizer job runs all the C tests, including `testcnx`, and
the multi-peer scenarios with leak detection enabled, using TCP without TLS and
with SCTP disabled. The sanitizer job disables only ODR
violation detection because extensions intentionally export the same
`fd_ext_depends` symbol; it does not disable leak detection.

The harness starts separate daemons with distinct identities and dynamically
selected ports bound to IPv4 loopback. It waits for connections and exchanges,
applies timeouts, checks daemon shutdown, and prints peer logs on failure.
It allows each peer 25 seconds to stop. If `FD_MULTI_PEER_DIAGNOSTICS` is set
and a peer exceeds that deadline, the harness runs GDB for an all-thread
backtrace before killing it and saves the backtrace and peer logs in that
directory. GDB must be installed and permitted to attach; capture failures
are reported without preventing cleanup. The multi-peer CI job enables
debugger attachment on its GitHub-hosted runner and uploads these diagnostics
as a matrix-specific artifact. The logs and backtraces may contain test data.

The default is TCP without TLS. Set either or both options when configuring the
build to run the same scenarios with a different transport:

| CMake option | Effect |
| --- | --- |
| `-DMULTI_PEER_USE_TLS=ON` | Use TLS on a secure port. Requires `openssl`; each scenario generates a temporary CA and identity-specific peer certificates. |
| `-DMULTI_PEER_USE_SCTP=ON` | Use SCTP instead of TCP. Requires SCTP support in the build and host. |

With both options enabled, the daemon uses TLS over SCTP unless
`ENABLE_DTLS_SCTP=ON` enables DTLS over SCTP. The tests
check each side of every link's reported connection protocol and security, as
well as the message exchanges; a transport fallback is not accepted. TLS test
credentials and keys are generated in the scenario's temporary directory and
removed afterward. Tests do not contact external peers or exercise reconnection.

With `ENABLE_DTLS_SCTP`, `MULTI_PEER_USE_TLS`, and `MULTI_PEER_USE_SCTP`
enabled, additional tests exercise:

- DTLS 1.2 handshakes with OpenSSL as client and server, followed by
  application data. OpenSSL must be built with SCTP support. If the normal
  `openssl` lacks it, set `FD_DTLS_SCTP_OPENSSL` to an SCTP-enabled executable
  when running CTest. The OpenSSL command-line tools are not Diameter peers;
  these tests do not establish a Diameter peer connection.
- Packet loss followed by delay and reordering using `tc netem` in a private
  user and network namespace. The test requires `unshare`, `ip`, `tc`,
  permission to create those namespaces and raw sockets, and SCTP-AUTH. It
  never changes the host's network configuration. It checks that Linux send-queue accounting
  remains nonzero while an acknowledgment is delayed or a gap acknowledgment
  arrives first, then drains, and checks Diameter recovery and exchanges. It
  is complemented by ACK-barrier tests that hold acknowledgments during client
  and server handshakes and shutdown, and check key activation, Finished,
  old-key deletion and close-notify ordering after the queue drains. Separate
  cases keep ACKs blocked to check handshake and shutdown timeouts.
  The tests exercise one Linux kernel and local loopback paths, not vendor or
  cross-kernel interoperability.
- With `dbg_msg_dumps` built, pending-request shutdown tests briefly drop
  traffic while a request is queued, then apply 100 ms delay as the requesting
  daemon or both daemons shut down. They check bounded process termination and
  report whether the request was answered, sent to failover, or its answer was
  dropped during shutdown. These characterize the outcome; they do not
  guarantee application-level delivery.

These tests report as skipped when their prerequisites are unavailable. For
example, the GitHub Actions AUTH-enabled TLS/SCTP job builds an SCTP-enabled
OpenSSL to run the interoperability cases. To run them locally:

```sh
FD_DTLS_SCTP_OPENSSL=/path/to/openssl \
  ctest -R '^test_sctp_(openssl_|netem|ack_|shutdown_pending_)' --output-on-failure
```

### Adding a multi-peer scenario

[multi_peer_harness.py](../tests/multi_peer_harness.py) provides a reusable
`Peers` fixture. It allocates ports and a temporary directory, starts and
stops daemons in reverse start order, waits with deadlines, and prints logs on
failure. Scenarios live in separate scripts; see
[test_two_peer_test_app.py](../tests/test_two_peer_test_app.py) and
[test_two_peer_dbg_interactive.py](../tests/test_two_peer_dbg_interactive.py).

To add an extension combination:

1. Write a scenario function taking `peers`, and invoke it with, for example,
   `run_scenario(daemon, scenario, 3, [(0, 1), (1, 2)])` for a three-peer
   chain. The count supplies identities `peer1.localdomain`, etc.; each link
   is an undirected pair of zero-based peer indexes. Only linked peers are
   configured to connect to and accept each other. Arguments supplied by CTest
   start with the daemon path.
2. Call `peers.start(index, extensions, extra_config="", env=None)` for each
   peer, in the order required by the scenario. `index` identifies the peer;
   `extensions` is a list of `(extension_path, config_path_or_None)` tuples,
   so each peer can load multiple extensions. Write per-scenario configuration
   and coordination files under `peers.directory`.
   Set `connect_neighbors=False` for a passive peer that must accept incoming
   connections without preconfigured `ConnectPeer` entries.
3. Use `peers.wait()` for readiness and scenario-specific results. Use
   `peers.wait_open(index, remote)` and `peers.wait_all_open()` to check
   directed peer states, and `peers.wait_transport()` to check all links.
   The accepting peer can remain in `STATE_OPEN_NEW` until it receives a
   message, so do not wait for both peers to reach `STATE_OPEN` before
   triggering that message.
4. Register scenario-specific failure files with `peers.watch_error_file()`,
   cleanup actions with `peers.on_stop()`, and checks that need flushed logs
   with `peers.after_stop()`. The fixture reports cleanup failures and still
   stops the daemons if a cleanup action fails.
5. In [tests/CMakeLists.txt](../tests/CMakeLists.txt), condition registration
   on the required extension targets and call `ADD_MULTI_PEER_TEST` with the
   test name, script name, and `$<TARGET_FILE:...>` for each extension. The
   helper supplies the daemon path, the `multi-peer` label, and a CTest timeout.
   Update this document with the scenario's actual assertions and limits.

The fixture uses the selected TCP/SCTP and TLS options with IPv4 loopback;
tests requiring other transports or security must extend that setup.
`test_multi_peer_harness` checks multiple extension declarations, topology
validation and configuration, link state checks, TLS/SCTP configuration and
fallback detection, failure handling and cleanup, and protection against using
previous `test_app` exchange output.

### Compiled dictionary extension scenarios

The `test_dictionary_dict_*` tests load each compiled dictionary extension,
its required dictionary dependencies, and `dbg_dict_dump_json` into a daemon.
Each checks one representative AVP's name, code and vendor in the resulting
dictionary. They cover `dict_nasreq`, `dict_eap`, `dict_dcca`,
`dict_dcca_3gpp`, `dict_dcca_starent`, `dict_sip`, `dict_rfc5777`,
`dict_mip6a`, `dict_mip6i`, `dict_nas_mipv6` and `dict_rfc7944`. These
checks do not validate every AVP, rule, or command.
`test_dictionary_combined` loads all eleven extensions together in dependency
order and checks the same representative AVPs in the combined dictionary.

### `test_multi_peer_acl_wl`

An unconfigured incoming peer is allowed by an exact or wildcard whitelist
entry and can exchange a `test_app` message. An unlisted peer is rejected.
The test also checks that a matching entry without `ALLOW_IPSEC` accepts TLS
but rejects a clear connection, and that a reloaded whitelist accepts a
subsequent connection. Cleartext `ALLOW_IPSEC` cases exercise the policy flag
only; the fixture does not establish an IPsec tunnel.

### `test_multi_peer_relay`

Three peers form a chain: a `test_app` client, a relay with no application
extension, and a `test_app` server. Only adjacent peers are configured to
connect. The test checks both links and their transport, sends a request to
the server through the relay, and verifies the server callback, echoed value,
success result and answer origin. It also checks that the endpoints did not
connect directly. It does not cover routing failures or alternate routes.

### `test_multi_peer_netemul`

A relay running `test_netemul` forwards a `test_app` request with 1000 ms
latency, zero deviation and duplication disabled. The test verifies a
successful echoed answer and a minimum 900 ms elapsed time. A second exchange
sets duplication probability to one and verifies the server receives multiple
requests. The elapsed-time check can also include unrelated scheduling delays.

The accompanying `test_netemul_process` test controls the random inputs to
check varying latency and deadline-based queue reordering, as well as duplicate
counts, retransmission flags, source identities and copied AVP values. It does
not check the statistical distribution or observe reordering over a live
network connection.

### Routing extension scenarios

These scenarios use a client, a relay, and one or two servers. They check
connections and transport as well as the following extension behavior:

| Test | Behavioral assertions |
| --- | --- |
| `test_multi_peer_rt_default`, `test_multi_peer_rt_ereg` | A User-Name rule selects one of two servers; after reloading the rule, the other server answers. Both answers echo the request value. |
| `test_multi_peer_rt_deny_by_size` | A request above the configured limit receives `DIAMETER_UNABLE_TO_COMPLY` with the relay's origin identity and does not reach the server; after increasing the limit and reloading, an exchange succeeds. |
| `test_multi_peer_rt_busypeers` | A suspended server triggers a relay timeout and `DIAMETER_TOO_BUSY`; after resuming the server, a subsequent exchange succeeds. |
| `test_multi_peer_rt_redirect` | A redirecting peer returns a redirect indication, the relay retries to the indicated connected server, and the server returns the echoed value. |
| `test_multi_peer_rt_rewrite` | A User-Name AVP is mapped to a Class AVP with the same bytes and removed from the forwarded request. |
| `test_multi_peer_rt_ignore_dh` | A request addressed to a virtual host reaches a different server, and the answer returns with the virtual host restored as Origin-Host. |
| `test_multi_peer_rt_randomize`, `test_multi_peer_rt_load_balance` | With two idle, equally scored servers, the observed routing scores show the expected boost and the higher-scored server answers. |

These are focused examples, not exhaustive tests of routing rules, busy-peer
retries, redirect caching, rewrite operations, or balancing under unequal
load. The size test checks rejection and acceptance at two configured limits,
not the precise boundary.

### Debug extension scenarios

The three-peer debug scenarios exchange an application request through the
instrumented relay. The dictionary dump tests inspect base AVPs in the text
or parsed JSON output; the loglevel test checks a runtime reload; and the
monitor test checks its signal-triggered config, extension, and dictionary
dumps. The message dump, timing, and routing tests check for corresponding
records during a forwarded exchange. They do not exhaustively validate every
dump format, hook, timing value, or monitoring interval.

### `test_two_peer_test_app`

Both daemons load `test_app` in client-and-server mode. The test:

1. Waits for peer 2 to reach `STATE_OPEN` for peer 1.
2. Uses `SIGUSR1` to request an application message from peer 2 to peer 1.
   It checks the server callback, the echoed `Test-AVP` value, result code
   `DIAMETER_SUCCESS` (2001), and the answer's origin host and realm.
3. Checks that both peers have reached `STATE_OPEN` for one another. The
   accepting peer may enter `STATE_OPEN_NEW` first and reach `STATE_OPEN`
   after receiving the application message.
4. Triggers and verifies the same application exchange in the reverse
   direction, with peer 1 as the client and peer 2 as the server.

This exercises the default test application identifiers, explicit
destination-host routing, signal-triggered clients, and both server callbacks.
It does not cover benchmark mode, configurable identifiers, long payloads,
error answers, or all configuration options.

### `test_two_peer_dbg_interactive`

Peer 1 loads [dbg_interactive.py.sample](dbg_interactive.py.sample), and peer 2
loads `test_app`. After the sample registers its application, the harness
establishes the connection and verifies:

1. A `test_app` request from peer 2 is handled by the sample's Python dispatch
   callback, with a successful answer and matching test value.
2. Both peers reach `STATE_OPEN`.
3. The sample's Python client sends a request to peer 2 and its answer callback
   checks the echoed value, success result, origin host, and origin realm.

The sample's optional timeout, peer-addition, and other network-dependent
examples are not exercised by this test.

### `test_multi_peer_rt_ignore_dh`

With `rt_ignore_dh` and `dbg_loglevel` built, a client sends to a virtual
destination through a relay. The test checks the server's echoed answer, the
restored Origin-Host, and the stowed and restored host names in the relay's
debug logs.
