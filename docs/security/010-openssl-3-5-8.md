---
title: "OpenSSL 3.5.5–3.5.8 — bundled OpenSSL updated past four upstream security releases"
fixed_in: 0.18.0
fixed_in_note: "bundles OpenSSL 3.5.8"
severity: "Moderate (highest fix reachable through NSClient++)"
modules: [packaging, CheckSecurity, WEBServer]
advisory:
  id: "Upstream ([OpenSSL](https://github.com/openssl/openssl))"
  cve: "[CVE-2025-11187](https://nvd.nist.gov/vuln/detail/CVE-2025-11187) and the other OpenSSL 3.5.5–3.5.8 fixes"
  severity: "Moderate (highest reachable)"
  affected: "Windows builds up to 0.17.0 (bundled OpenSSL ≤ 3.5.4)"
  summary: "Crafted-input flaws in the bundled OpenSSL; the most severe reachable one is a stack overflow parsing a hostile PKCS#12 file via `check_certificate`."
---
The **Windows builds** of NSClient++ statically bundle
[OpenSSL](https://github.com/openssl/openssl) as the TLS engine for the
NRPE/NSCA/NSClient server and client modules and the web server / REST API,
for certificate parsing in `check_certificate`, and for password hashing.
Releases up to 0.17.0 bundled OpenSSL 3.5.4; the four upstream security
releases since then — 3.5.5 (27 Jan 2026), 3.5.6 (7 Apr 2026), 3.5.7
(9 Jun 2026) and 3.5.8 (25 Aug 2026) — fix some forty CVEs on the 3.5 LTS
line, and 0.18.0 moves the bundled copy to 3.5.8.

The fixes most relevant to NSClient++ sit on code paths it actually calls:

- **[CVE-2025-11187](https://nvd.nist.gov/vuln/detail/CVE-2025-11187)**
  (Moderate) — missing validation of PBMAC1 parameters in PKCS#12 files can
  trigger a stack buffer overflow or invalid pointer dereference during MAC
  verification. `check_certificate` parses operator-specified certificate
  files and falls back to PKCS#12 (`.pfx`/`.p12`) parsing, so a hostile
  certificate file scanned by a check reaches this code.
- Several further low-severity fixes on the same PKCS#12/ASN.1 parsing paths
  ([CVE-2025-69421](https://nvd.nist.gov/vuln/detail/CVE-2025-69421),
  [CVE-2026-22795](https://nvd.nist.gov/vuln/detail/CVE-2026-22795),
  [CVE-2026-34181](https://nvd.nist.gov/vuln/detail/CVE-2026-34181),
  [CVE-2026-7383](https://nvd.nist.gov/vuln/detail/CVE-2026-7383),
  [CVE-2026-34180](https://nvd.nist.gov/vuln/detail/CVE-2026-34180)) and in
  the TLS stack
  ([CVE-2025-66199](https://nvd.nist.gov/vuln/detail/CVE-2025-66199)
  TLS 1.3 `CompressedCertificate` memory growth,
  [CVE-2025-15468](https://nvd.nist.gov/vuln/detail/CVE-2025-15468)).

The highest-severity upstream fixes in this span —
[CVE-2026-45447](https://nvd.nist.gov/vuln/detail/CVE-2026-45447) (High,
use-after-free verifying a PKCS#7/S/MIME signature) and
[CVE-2025-15467](https://nvd.nist.gov/vuln/detail/CVE-2025-15467) (High,
stack overflow parsing CMS `AuthEnvelopedData`) — are in CMS / S/MIME code
NSClient++ never calls, and the QUIC, CMP and DTLS fixes likewise do not
apply.

Not affected: the Linux packages (DEB/RPM) link the distribution's OpenSSL
and receive these fixes through ordinary OS updates.

**What to do:** upgrade Windows installs to 0.18.0 or later — promptly if
`check_certificate` is pointed at certificate files or directories that
less-trusted principals can write to. No configuration change is needed.
