# Extension overview

This catalogue follows the options and descriptions in
[`extensions/CMakeLists.txt`](../../extensions/CMakeLists.txt). Defaults below
are the CMake defaults in a regular build; extensions with `ON` are selected
unless overridden. Each extension is built as a `.fdx` module. See
[`freediameter.conf.sample`](../freediameter.conf.sample) for the main
configuration's `LoadExtension` syntax. The linked samples are starting
points, not complete deployment configurations.

To select an extension at configure time, use its option, for example
`cmake .. -DBUILD_APP_DIAMEAP=ON` from an out-of-tree build directory. Use the corresponding
`BUILD_<EXTENSION>` option to override any default. `ALL_EXTENSIONS=ON`
selects all extensions available for the chosen build type. The project
default for a dictionary or routing extension does not mean that it is
automatically loaded at runtime.

The dependency column lists additional dependencies explicitly declared by
the extension's CMake files; it is not a complete list of freeDiameter's
general build prerequisites. No extra dependency is listed when the extension
CMake file declares none. Required dictionary modules are noted separately.

## Dictionary modules

| Extension | Purpose | Default | Additional build dependency | Sample / notes |
|---|---|---:|---|---|
| `dict_nasreq` | NASREQ (RFC 4005) definitions | ON | — | — |
| `dict_eap` | Diameter EAP (RFC 4072) definitions | ON | — | Requires `dict_nasreq` |
| `dict_dcca` | Diameter Credit-Control (RFC 4006) definitions; incomplete | ON | — | Requires `dict_nasreq` |
| `dict_dcca_3gpp` | 3GPP Credit-Control definitions; incomplete | ON | — | Requires `dict_dcca` |
| `dict_dcca_starent` | Starent Credit-Control definitions | ON | — | Requires `dict_dcca_3gpp` |
| `dict_sip` | Diameter SIP (RFC 4740) definitions | ON | — | Required by `app_sip` and `test_sip` |
| `dict_mip6a` | Mobile IPv6 Authentication definitions | ON | — | Requires `dict_rfc5777` |
| `dict_mip6i` | Mobile IPv6 IKE definitions | ON | — | Requires `dict_rfc5777` |
| `dict_nas_mipv6` | NAS-to-HAAA interaction definitions | ON | — | — |
| `dict_rfc5777` | Classification and QoS (RFC 5777) definitions | ON | — | — |
| `dict_rfc7944` | Routing Message Priority (RFC 7944) definitions | ON | — | — |
| `dict_json` | Load dictionary definitions from JSON | OFF | jsoncpp, json-schema, PCRE++ | [JSON sample](../dict_json.json.sample); not added for `DebianPackage` builds |
| `dict_legacy_xml` | Load dictionary definitions from XML | OFF | libxml2 | [Configuration sample](../dict_legacy_xml.conf.sample) |

## Applications and gateways

| Extension | Purpose | Default | Additional build dependency | Sample / notes |
|---|---|---:|---|---|
| `app_acct` | Store accounting records in a flat database | OFF | PostgreSQL client | [Configuration sample](../app_acct.conf.sample) |
| `app_diameap` | Diameter EAP application server (RFC 4072) | OFF | MySQL client, Gcrypt, GnuTLS | [Configuration sample](../app_diameap.conf.sample), [EAP-TLS sample](../eap_tls_plugin.diameap.conf.sample); requires `dict_eap`. See [DiamEAP README](../../extensions/app_diameap/README). |
| `app_radgw` | RADIUS-client to Diameter-server gateway | OFF | — | [Gateway sample](../app_radgw.conf.sample), [echodrop sample](../echodrop.rgwx.conf.sample) |
| `app_sip` | Diameter SIP authentication and authorization server (RFC 4740) | OFF | MySQL client | [Configuration sample](../app_sip.conf.sample); requires `dict_sip`. Experimental; see [SIP README](../../extensions/app_sip/README). |
| `app_redirect` | Configurable Diameter Redirect server | OFF | — | [Configuration sample](../app_redirect.conf.sample) |

DiamEAP also has method plug-ins: `eap_identity` is enabled by default when
`app_diameap` is built; `eap_md5` and `eap_tls` are disabled by default.
They build as `.emp` modules. EAP-TLS uses GnuTLS. See the DiamEAP README and
configuration sample for setup details.

## Routing

| Extension | Purpose | Default | Sample / notes |
|---|---|---:|---|
| `rt_busypeers` | Handle `TOO_BUSY` messages and relay timeouts | ON | [Configuration sample](../rt_busypeers.conf.sample) |
| `rt_default` | Configurable routing rules | ON | [Configuration sample](../rt_default.conf.sample) |
| `rt_discover` | Explicitly authorized DNS peer discovery on routing failure | OFF | [Configuration sample](../rt_discover.conf.sample) |
| `rt_deny_by_size` | Deny messages over a configured size | ON | [Configuration sample](../rt_deny_by_size.conf.sample) |
| `rt_ereg` | Route using regular-expression matches on AVPs | OFF | [Configuration sample](../rt_ereg.conf.sample) |
| `rt_ignore_dh` | Preserve `Destination-Host` in `Proxy-Info` and restore it from `Origin-Host` in answers | ON | — |
| `rt_load_balance` | Balance requests among equal-scored hosts | ON | — |
| `rt_randomize` | Randomly choose among the highest-scored hosts | ON | — |
| `rt_redirect` | Handle Diameter Redirect messages | ON | — |
| `rt_rewrite` | Convert or remove AVP data | ON | [Configuration sample](../rt_rewrite.conf.sample) |

## Peer authorization

| Extension | Purpose | Default | Sample / notes |
|---|---|---:|---|
| `acl_wl` | Authorize incoming connections using a white list | ON | [Configuration sample](../acl_wl.conf.sample) |

## Debugging and tests

| Extension | Purpose | Default | Additional build dependency | Sample / notes |
|---|---|---:|---|---|
| `dbg_dict_dump` | Log a dictionary dump | ON | — | — |
| `dbg_dict_dump_json` | Dump the dictionary as JSON | OFF | jsoncpp | — |
| `dbg_loglevel` | Read log level from a file for runtime changes | ON | — | [Configuration sample](../dbg_loglevel.conf.sample) |
| `dbg_monitor` | Periodically output status information | ON | — | — |
| `dbg_msg_timings` | Show message timing information | ON | — | — |
| `dbg_msg_dumps` | Show human-readable sent and received messages | ON | — | — |
| `dbg_rt` | Debug routing decisions | ON | — | — |
| `dbg_interactive` | Python-interpreter-based module | OFF | Python 3 and SWIG | [Example script](../dbg_interactive.py.sample); not added for `DebianPackage` builds |
| `test_app` | Send a test Diameter message to another peer | OFF | — | [Configuration sample](../test_app.conf.sample) |
| `test_as` | Receive and display Abort-Session-Requests | OFF | — | — |
| `test_cc` | Receive and display Credit-Control-Requests | ON | — | — |
| `test_ccload` | Generate Credit-Control-Requests and count replies | ON | — | — |
| `test_sip` | Experimental Diameter-SIP client (RFC 4740) | OFF | MySQL client | Requires `dict_sip`; its [historical configuration sample](../test_sip.conf.sample) is not parsed by the current extension |
| `test_netemul` | Proxy emulator for latency, delay variation, and duplicates | OFF | — | [Configuration sample](../test_netemul.conf.sample) |

In `Debug` builds only, CMake also offers these developer/test extensions;
each option defaults to `OFF`:

| Extension | Purpose |
|---|---|
| `_sample` | Demonstrate the extension mechanism |
| `test_acct` | Receive and display Accounting-Requests without storage |
| `test_rt_any` | Send test traffic randomly to an available peer |

This overview intentionally omits per-extension directive inventories. Where
no sample is linked, consult the extension source and its build option; the
repository does not provide a separate configuration sample for that module.
