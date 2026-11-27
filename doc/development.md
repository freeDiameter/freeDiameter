# Development

## Components and build

`libfdproto` provides dictionaries, Diameter messages, sessions, dispatch and
utility structures. `libfdcore` adds connections, peer management, routing,
extension loading and framework lifecycle. `freeDiameterd` is the executable
that configures and starts the framework. Extensions add dictionaries,
applications, routing, authorization and diagnostics. See
[extensions](extensions/README.md) for selection and loading.

Build and test out of tree as described in [getting started](getting-started.md).
The public headers and actual implementation are the reference for function
contracts; tests show specific use cases, not exhaustive protocol compliance.

## Using the C API

Include the generated `<freeDiameter/freeDiameter-host.h>` before
`<freeDiameter/libfdproto.h>` or `<freeDiameter/libfdcore.h>`. Extensions may
include `<freeDiameter/extension.h>`, which includes the core headers.
Use the generated header from the same build as the libraries.
`FD_PROJECT_VERSION_API` also determines the libraries' SONAMEs. When it
changes, applications and extensions linked to an earlier SONAME need
matching libraries or a rebuild against the new ones.

Standalone `libfdproto` users call `fd_libproto_init()` before other library
functions and `fd_libproto_fini()` on termination. When using `libfdcore`,
`fd_core_initialize()` initializes `libfdproto` as well. The daemon calls
`fd_core_initialize()`, `fd_core_parseconf()` and `fd_core_start()`, then
waits for shutdown. Embedders should handle each returned error and follow
the lifecycle documented in [libfdcore.h](../include/freeDiameter/libfdcore.h).

`fd_core_shutdown()` requests shutdown without a forced-exit watchdog;
`fd_core_shutdown_timeout()` can terminate the entire process if the chosen
deadline expires, including in an embedding application. `-1` disables that
watchdog, not the shorter best-effort drain deadlines. Wait for completion
with `fd_core_wait_shutdown_complete()`. See
[test coverage](test-coverage.md) for the daemon's deadline and shutdown limits.

Ownership and callback rules differ by API. For example, `fd_peer_add()`
copies peer information but moves nonempty endpoint lists; `fd_msg_send()`
queues a message asynchronously and its shutdown error can leave ownership
with the caller. Answer and expiry callbacks have different message ownership
rules. Follow the function-specific comments and signatures in
[libfdcore.h](../include/freeDiameter/libfdcore.h), rather than applying a
general rule to all callbacks. The discovery callback can run before
`fd_discovery_start()` returns and must not stop its own operation; see
[discovery.h](../include/freeDiameter/discovery.h) for synchronization,
callback-data lifetime and stop requirements.

## Writing an extension

Study [the sample extension](../extensions/_sample/sample.c), its
[cleanup](../extensions/_sample/fini.c) and
[CMake definition](../extensions/_sample/CMakeLists.txt). Include
`extension.h`, declare an entry point with `EXTENSION_ENTRY`, register the
required callbacks or dictionary entries, and release owned resources in
`fd_ext_fini` where needed. Entry points return standard error codes.
The sample target is available only in Debug configurations and must be
enabled with `BUILD__SAMPLE`. Runtime extension dependencies and load order
must be considered separately from enabling build targets.

Callbacks and asynchronous work may outlive their initiator. Follow the
relevant registration, unregistration and ownership contracts in the public
headers, and ensure extension cleanup is safe during shutdown. For dictionary,
message and dispatch examples, see the tests and existing extensions rather
than assuming all APIs have the same lifetime rules.

## Testing and documentation changes

CTest registrations and conditions are in [tests/CMakeLists.txt](../tests/CMakeLists.txt).
[Test coverage](test-coverage.md) describes the default, optional and
multi-peer plans, assertions and limitations. The
[container guide](../contrib/containers/README) describes disposable database
fixtures. Do not run database fixtures against production data.

When changing public behavior, update the corresponding header comments,
configuration samples, guides and focused tests together. See the
[documentation index](README.md) and
[contributor instructions](../.github/instructions/fd.instructions.md).
