---
applyTo: "**"
---

# freeDiameter project instructions

## Project overview

freeDiameter is a C implementation of the Diameter protocol, used to carry
Authentication, Authorization, and Accounting (AAA) traffic.

- `libfdproto` provides shared functionality for manipulating Diameter messages,
  dictionaries, and related data.
- `libfdcore` implements the framework's core behavior, including peer
  connections, Diameter base-protocol tasks, and extension loading.
- `freeDiameterd` is the daemon that initializes and runs the framework.
- `extensions/` contains optional application, routing, dictionary, debugging,
  and testing modules.

## Architecture and APIs

- Follow the existing component boundaries and conventions. Use the public
  headers in `include/freeDiameter/` for library and extension interfaces.
- Implement extensions using the existing extension entry-point and build
  conventions. Add or update the relevant CMake configuration when introducing
  a source file, extension, or test.
- If a public library API changes, increment `FD_PROJECT_VERSION_API`.
  `libfdproto.h` requires the increment for any edit, including comments;
  this also changes the shared libraries' SONAMEs.

## Implementation rules

- Keep changes focused and consistent with the surrounding C and CMake style.
  Maintain the POSIX.1-2001 compatibility target stated in the root
  `CMakeLists.txt`.
- Reuse the project's established error, logging, and tracing helpers where
  appropriate, and follow existing resource ownership and cleanup patterns.
- Preserve existing copyright and license notices, including file-specific
  attribution.
- Update relevant tests and documentation when behavior, configuration, or
  supported usage changes.
- Keep `doc/` configuration samples synchronized with the implementation.
  When code changes affect configuration syntax, options, defaults, or behavior,
  update the corresponding samples and documentation in the same change.

## Documentation maintenance

- Start at `doc/README.md` and update the relevant guide, reference and sample
  together. Do not copy long lists of options into multiple documents.
- Treat public header contracts, parser grammars and initialization defaults,
  CMake definitions, and registered tests as the sources for API behavior,
  configuration, build options, and test coverage respectively. Check the
  implementation when comments or examples disagree.
- When changing API ownership, callbacks or lifecycle, update public header
  comments and `doc/development.md`. For configuration or transport changes,
  check `doc/operations.md`, the affected samples and extension documentation.
  For build or packaging changes, check `INSTALL*`, extension documentation
  and `contrib/README`. For test changes, check `doc/test-coverage.md`.
- Validate changed examples and local links where practical. State prerequisites,
  skipped tests, security implications and unverified platforms explicitly.
  Test fixtures, legacy contributions, and experimental features must not be
  presented as production-ready. Preserve license and attribution notices.
- Run `python3 tests/check_doc_links.py` after changing Markdown links; this
  checks relative inline links in `doc/`, the instruction file and the
  Markdown-formatted contribution and extension READMEs, not anchors,
  external sites or non-Markdown configuration samples.

## Build and test

Build out of tree with CMake in a repository-root directory named `test.*`.
Use `test.build` for a general build, or choose a descriptive suffix for a
specific test run (for example, `test.diameap`). From the repository root:

```sh
mkdir test.build
cd test.build
cmake ..
cmake --build .
make test
```

Building the daemon does not create its runtime configuration. The samples in
`doc/` are useful starting points: copy the needed files into the selected
`test.*` directory and adapt them for the build and test environment. For a
two-instance local communication test, use `doc/single_host/` as a template
for the two peer configurations. Update paths to the built daemon's extensions
and any required certificates or other runtime files; keep generated or
modified test configs and assets in the `test.*` directory rather than
overwriting the tracked samples.

The project requires a C compiler, CMake, Flex, and Bison, along with the
libraries listed in the platform-specific installation instructions. Tests are
registered through CTest; use a focused test when possible and run the test
suite for broader changes.

Extensions are configurable. Check `extensions/CMakeLists.txt` for each
extension's `BUILD_*` option and use `ALL_EXTENSIONS` when a change needs all
extensions built. Enable any optional extension-specific test prerequisites
before relying on those tests.
