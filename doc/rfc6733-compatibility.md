# RFC 6733 and verified errata compatibility

This page records implementation behavior relevant to RFC 6733 and its
verified errata. It is an engineering assessment, not a claim of complete
protocol certification or interoperability with every Diameter implementation.
RFC 6733 obsoletes RFC 3588; verified erratum
[4808](https://www.rfc-editor.org/errata/eid4808) clarifies that RFC 6733 is
not fully backward compatible with RFC 3588.

## Current implementation

- The normal Diameter and secure Diameter ports are 3868 and 5868. The secure
  port follows the IANA `diameters` assignment and verified
  [erratum 3997](https://www.rfc-editor.org/errata/eid3997), which corrects
  RFC 6733's 5658 typo. DNS discovery uses 5868 for secure NAPTR address
  records.
- Outbound message encoding pads AVPs to 32-bit boundaries. The buffer is
  zero-initialized, and message-length calculation includes the padded sizes
  of AVPs, including children of Grouped AVPs. The message tests exercise
  OctetString and nested Grouped AVP lengths and check a serialized padding
  byte.
- Inbound parsing is more permissive: it can accept an AVP when the declared
  data is present but its alignment padding is absent. This is a receive-side
  validation limitation, not an outbound encoding issue. RFC 6733 Section 4.1
  specifies AVP padding; this assessment does not establish a separate
  receiver-side MUST to reject missing padding. Verified
  [erratum 3805](https://www.rfc-editor.org/errata/eid3805) clarifies that the
  message length is measured in octets and is a multiple of four. The encoder
  generates aligned messages; the parser does not separately enforce that
  alignment.
- TLS/TCP is available in the normal build. DTLS/SCTP is an optional,
  experimental feature, disabled by default and currently restricted to
  Linux with the required GnuTLS and SCTP APIs
  ([build options](../include/freeDiameter/CMakeLists.txt)). RFC 6733
  Section 13 requires implementations to support TLS/TCP and DTLS/SCTP, so
  an unmodified default build should not be described as satisfying that
  specific requirement.
- DTLS/SCTP sends each Diameter message as a single DTLS record and rejects
  messages larger than the negotiated data MTU. This is a practical
  interoperability limitation. RFC 6083 Section 4.1 specifies minimum SCTP
  user-message support; the relationship between that requirement and the
  configured GnuTLS data MTU should be verified before making a transport
  profile conformance claim.
- DNS peer discovery is available through configuration or the discovery API
  and supports secure NAPTR/SRV discovery. It is opt-in. The implementation
  delegates DNS queries to the system resolver and does not itself validate
  DNSSEC; whether answers are validated depends on the resolver configuration.
  DNS records do not, on their own, authorize a peer.
- `No_TLS` relies on external security such as IPsec. It does not configure or
  verify that protection; deployments must ensure that Diameter traffic is
  protected as required by RFC 6733 Section 13.

## Relevant verified errata

| Erratum | Compatibility note |
| --- | --- |
| [3805](https://www.rfc-editor.org/errata/eid3805) | Clarifies message-length units and four-octet alignment. Generated messages are aligned; inbound enforcement is not explicit. |
| [3997](https://www.rfc-editor.org/errata/eid3997) | Corrects the secure port to 5868; the implementation default and secure DNS address-record path use 5868. |
| [4615](https://www.rfc-editor.org/errata/eid4615) | Clarifies that one `Failed-AVP` can contain multiple offending AVPs. The base command rules allow at most one `Failed-AVP`. |
| [4808](https://www.rfc-editor.org/errata/eid4808) | Removes the claim of full backward compatibility with RFC 3588. A build-time workaround can relax Vendor-Id multiplicity in `Vendor-Specific-Application-Id`; this is not a general RFC-version switch. |
| [4887](https://www.rfc-editor.org/errata/eid4887) | Adds `Origin-Realm` to locally generated answers. The core origin helper adds both `Origin-Host` and `Origin-Realm`; callers that construct answers directly remain responsible for the required AVPs. |
| [3806](https://www.rfc-editor.org/errata/eid3806), [3942](https://www.rfc-editor.org/errata/eid3942), [4803](https://www.rfc-editor.org/errata/eid4803), [6171](https://www.rfc-editor.org/errata/eid6171) | Terminology, spelling, or editorial clarifications. They do not by themselves establish additional wire-format changes in this implementation. |

RFC 7075 defines application-specific realm-based redirection; it is not a
universal base-protocol feature requirement. Applications that require it
should be assessed against that specification separately.

## Conformance statement

The codebase implements substantial RFC 6733 base-protocol functionality, and
focused tests cover message encoding, peer operation, discovery, and optional
secure transports. The default C test suite is not a complete RFC conformance
suite, and local integration tests do not establish interoperability with
independent implementations. Use the more precise description “RFC 6733-based
implementation with optional DTLS/SCTP and documented limitations”; do not
claim unconditional compliance with RFC 6733 and all related transport
requirements without a requirement-by-requirement audit and external
interoperability testing.
