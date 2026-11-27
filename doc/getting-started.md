# Build and local test

## Build

Install the prerequisites described in [INSTALL](../INSTALL). From the
repository root, create a fresh out-of-tree build directory:

```sh
mkdir test.quickstart
cd test.quickstart
cmake .. -DBUILD_MULTI_PEER_TESTS=ON -DBUILD_TEST_APP=ON
cmake --build .
ctest --output-on-failure
```

The multi-peer harness requires Python 3.6 or newer. To run just its basic
two-peer exchange, use `ctest -R '^test_two_peer_test_app$' --output-on-failure`
in the build directory. The default fixture uses TCP without TLS on loopback,
with dynamically selected ports. This is a test, not a secure deployment.
See [test coverage](test-coverage.md) for TLS, SCTP, optional dependencies,
timeouts and skip conditions.

Build options are cached per directory. Inspect them with `cmake -L -N .`,
or use a new `test.*` directory for a different configuration. SCTP is
enabled by default; `-DDISABLE_SCTP=ON` removes that build dependency.
Optional extensions are selected using `BUILD_<NAME>`; see
[extensions](extensions/README.md).

## Run a daemon

The build does not create a deployment configuration or credentials. Start
from [freediameter.conf.sample](freediameter.conf.sample), set an identity,
realm, certificates and trust anchors appropriate to the deployment, and
configure authorized peers and extensions. Run the daemon with
`freeDiameterd --config /absolute/path/to/freeDiameter.conf`; use the built
binary in `test.quickstart/freeDiameterd/` or the installed binary as
appropriate. `freeDiameterd --help` shows its actual options and default
configuration path. See [operations](operations.md) before connecting to a
real network.
