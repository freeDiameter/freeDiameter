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
  -DBUILD_DBG_INTERACTIVE=ON -DBUILD_DBG_DICT_DUMP_JSON=ON \
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

The harness starts separate daemons with distinct identities and dynamically
selected ports bound to IPv4 loopback. It waits for connections and exchanges,
applies timeouts, checks daemon shutdown, and prints peer logs on failure.

The default is TCP without TLS. Set either or both options when configuring the
build to run the same scenarios with a different transport:

| CMake option | Effect |
| --- | --- |
| `-DMULTI_PEER_USE_TLS=ON` | Use TLS on a secure port. Requires `openssl`; each scenario generates a temporary CA and identity-specific peer certificates. |
| `-DMULTI_PEER_USE_SCTP=ON` | Use SCTP instead of TCP. Requires SCTP support in the build and host. |

With both options enabled, the daemon uses TLS over SCTP, not DTLS. The tests
check each side of every link's reported connection protocol and security, as
well as the message exchanges; a transport fallback is not accepted. TLS test
credentials and keys are generated in the scenario's temporary directory and
removed afterward. Tests do not contact external peers or exercise reconnection.

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

### `test_multi_peer_relay`

Three peers form a chain: a `test_app` client, a relay with no application
extension, and a `test_app` server. Only adjacent peers are configured to
connect. The test checks both links and their transport, sends a request to
the server through the relay, and verifies the server callback, echoed value,
success result and answer origin. It also checks that the endpoints did not
connect directly. It does not cover routing failures or alternate routes.

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
