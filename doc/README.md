# freeDiameter documentation

These pages describe the source tree, not necessarily every downstream package.
Features and extensions depend on build options and platform prerequisites.

## Start here

- [Build and local test](getting-started.md): configure, build and run a
  two-peer test without changing system configuration.
- [Operations](operations.md): daemon configuration, peers, security,
  discovery, signals and troubleshooting.
- [Extensions](extensions/README.md): select, load and configure extensions.
- [Development](development.md): component boundaries, public APIs, extension
  lifecycle and tests.
- [Test coverage](test-coverage.md): what the test plans exercise and their
  limits; [database tests](../tests/README.database) and the
  [container environment](../contrib/containers/README) are optional.
- [Installation guides](../INSTALL): platform and packaging details.
- [Contributed material](../contrib/README): status and caveats for optional
  tools, dictionaries and test PKI.

## Reference and ownership

- [Daemon configuration sample](freediameter.conf.sample) documents syntax,
  options and defaults alongside examples. Its TLS paths are placeholders;
  copy and adapt it before use.
- Other [configuration samples](./) document extension-specific syntax.
  [single_host](single_host/) is a test-only example, not a deployment recipe.
- The public C contracts are in
  [libfdproto.h](../include/freeDiameter/libfdproto.h),
  [libfdcore.h](../include/freeDiameter/libfdcore.h),
  [discovery.h](../include/freeDiameter/discovery.h) and
  [extension.h](../include/freeDiameter/extension.h). The generated
  `freeDiameter-host.h` supplies build-specific definitions.
- The definitive build options and defaults are in
  [CMakeLists.txt](../CMakeLists.txt),
  [include/freeDiameter/CMakeLists.txt](../include/freeDiameter/CMakeLists.txt)
  and [extensions/CMakeLists.txt](../extensions/CMakeLists.txt).
  Use `cmake -L -N` in a configured build directory to inspect its cache.

When documentation disagrees with behavior, consult the relevant parser,
initialization code and tests, then update both the reference and the example.
See the [contributor instructions](../.github/instructions/fd.instructions.md)
for the maintenance checklist.
