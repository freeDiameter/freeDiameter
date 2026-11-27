# Single-host test fixture

These are test-only examples, not production configurations. Both peer links
use `No_TLS`: Diameter messages are sent in cleartext without TLS peer
authentication. The listeners are not restricted to loopback by default;
adapt the configuration and run only in an isolated test environment.

From an out-of-tree build directory with `test_app` built, copy the two
`freeDiameter-*.conf` and `test_app*.conf` sample files into that directory, then run
`sh ../doc/single_host/make_certs.sh`. The script requires OpenSSL and creates
`single-host-certs/` in the current directory; the sample configurations
reference it. It refuses to overwrite an existing directory. If generation
fails, inspect and remove that directory before retrying. Certificates expire
after two days; the CA and private keys are solely for this fixture.

Adjust ports, listener addresses, and extension paths for the build and host,
then run two daemon instances using the corresponding configuration files.
The default ports may conflict with other services. Protect and discard the
generated private keys with the rest of the test directory.
