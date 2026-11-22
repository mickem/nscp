---
title: "OpenSSL 3.5.10 — the November 2026 upstream CVEs and the bundled OpenSSL 3.5.9"
fixed_in: 0.27.0
fixed_in_note: "moves to OpenSSL 3.5.10 once upstream publishes it; until then the builds bundle 3.5.9"
severity: "None reachable through NSClient++ (scanners report up to Critical against the bundled library)"
modules: [packaging]
action: none
advisory:
  id: "Upstream ([OpenSSL](https://github.com/openssl/openssl))"
  cve: "[CVE-2026-35190](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-35190), [CVE-2026-107494](https://www.cve.org/CVERecord?id=CVE-2026-107494) and the ten other fixes announced for OpenSSL 3.5.10"
  severity: "None reachable"
  affected: "Windows builds up to 0.26.0 (bundled OpenSSL 3.5.9) carry the affected library, but call none of the affected code"
  summary: "Twelve upstream fixes in QUIC, DTLS, DANE, AES-GCM-SIV, CMS / PKCS#7, a Suite B server flag and a PEM helper; NSClient++ uses none of them."
---
The **Windows builds** of NSClient++ bundle
[OpenSSL](https://github.com/openssl/openssl) 3.5.9. In November 2026
upstream published twelve CVEs against the 3.5 LTS line, all fixed in
OpenSSL 3.5.10: eight in two security advisories
([10 November](https://openssl-library.org/news/secadv/20261110.txt),
[17 November](https://openssl-library.org/news/secadv/20261117.txt)), and
four more on 18 November as CVE records that point at the fixing pull
request and appear in no advisory. As of 20 November 2026 upstream has not
published 3.5.10, so there is no release to move the bundled copy to yet.

Vulnerability scanners that key on the file version of
`libssl-3-x64.dll` / `libcrypto-3-x64.dll` flag 3.5.9 for all twelve in the
meantime. OpenSSL rates the eight advisory CVEs Low and has published no
rating for the other four; the CVSS scores that CVE databases attach to
those (9.1 Critical for CVE-2026-107494 on
[OpenCVE](https://app.opencve.io/cve/CVE-2026-107494), for instance) are
generic scores for an application that uses the affected feature.

None of the twelve sits on a code path NSClient++ calls:

| CVE | Upstream issue | Why NSClient++ does not reach it |
|-----|----------------|----------------------------------|
| [CVE-2026-88822](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-88822) | QUIC uses uninitialised stack bytes during automated peer detection | NSClient++ does not use QUIC; every listener and client speaks TLS over TCP. |
| [CVE-2026-95691](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-95691) | Retransmitted QUIC CRYPTO frames cause a use-after-free read in a QUIC client | No QUIC. |
| [CVE-2026-124709](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-124709) | A QUIC server retains the state of rejected streams without bound | No QUIC. |
| [CVE-2026-88823](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-88823) | AES-GCM-SIV leaves earlier AAD unauthenticated after a zero-length AAD update | NSClient++ does not use AES-GCM-SIV. Its own encryption is AES-256-GCM (fleet bundles) and AES-256-ECB (Gearman), and AES-GCM-SIV is not a TLS cipher suite. |
| [CVE-2026-95692](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-95692) | AES-GCM-SIV reads out of bounds, or truncates, on an IV of the wrong length | No AES-GCM-SIV. |
| [CVE-2026-107496](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-107496) | `CMS_encrypt()` loops forever on a recipient certificate whose EC group order is 1 | NSClient++ does not call the CMS API, and upstream states that TLS does not use this path. |
| [CVE-2026-35190](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-35190) | A TLS 1.3 server with the Suite B certificate flag reads past a fixed group list | Only servers that call `SSL_CTX_set_cert_flags()` with a Suite B flag are affected. NSClient++ never sets it, and no setting exposes it. |
| [CVE-2026-107493](https://openssl-library.org/news/vulnerabilities-3.5/#CVE-2026-107493) | `PEM_X509_INFO_read_bio_ex()` leaves freed entries in a caller-supplied stack when a read fails | Only applications that pass an existing stack are affected. NSClient++ does not call the function, and upstream states that OpenSSL's own callers, including the CA-file loading TLS uses, always pass `NULL`. |
| [CVE-2026-107494](https://www.cve.org/CVERecord?id=CVE-2026-107494) | The QUIC stack queues a `RETIRE_CONNECTION_ID` frame for every retransmitted `NEW_CONNECTION_ID` frame, without bound | No QUIC. |
| [CVE-2026-95690](https://www.cve.org/CVERecord?id=CVE-2026-95690) | Out-of-sequence DTLS handshake fragments make OpenSSL allocate a full reassembly buffer per buffered message | NSClient++ does not use DTLS. |
| [CVE-2026-84786](https://www.cve.org/CVERecord?id=CVE-2026-84786) | A DANE-enabled TLS client dereferences a NULL pointer on an unusual combination of certificate and TLSA records | No TLS client in NSClient++ enables DANE. |
| [CVE-2026-121482](https://www.cve.org/CVERecord?id=CVE-2026-121482) | Verifying a crafted CMS or PKCS#7 SignedData message with CRL checking consumes CPU quadratic in the message size | NSClient++ does not verify CMS or PKCS#7 messages. |

[CVE-2026-121481](https://www.cve.org/CVERecord?id=CVE-2026-121481), the
`EVP_EncodeBlock()` buffer overflow published the same day, affects only
OpenSSL 4.0 and 4.1. The 3.5 line is not in its affected range, and
NSClient++ does not call the function.

Not affected: the Linux packages (DEB/RPM) link the distribution's OpenSSL
and receive these fixes through ordinary OS updates. The macOS tarball
carries the Homebrew `openssl@3` bottle current when it is built.

**What to do:** nothing. A scanner finding against the bundled 3.5.9 DLLs
for these CVEs can be recorded as not exploitable through NSClient++. The
bundled copy moves to 3.5.10 in the first release after upstream publishes
it, which also clears the version-based finding.
