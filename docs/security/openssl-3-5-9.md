---
title: "OpenSSL 3.5.9 — bundled OpenSSL updated to the 29 September 2026 security release"
fixed_in: next
fixed_in_note: "bundles OpenSSL 3.5.9"
severity: "Low (highest fix reachable through NSClient++)"
modules: [packaging]
action: none
advisory:
  id: "Upstream ([OpenSSL](https://github.com/openssl/openssl))"
  cve: "[CVE-2026-35189](https://nvd.nist.gov/vuln/detail/CVE-2026-35189) and the other OpenSSL 3.5.9 fixes"
  severity: "Low (highest reachable)"
  affected: "Windows builds up to 0.24.1 (bundled OpenSSL 3.5.8)"
  summary: "Thirteen upstream fixes; the only one on a code path NSClient++ calls is a memory-pressure denial of service from a crafted peer certificate."
---
The **Windows builds** of NSClient++ bundle
[OpenSSL](https://github.com/openssl/openssl) as the TLS engine for the
NRPE/NSCA/NSClient server and client modules, the web server / REST API and
every client module that talks HTTPS, for certificate parsing in
`check_certificate`, and for password hashing. Releases up to 0.24.1 bundle
OpenSSL 3.5.8; the upstream security release of 29 September 2026
([advisory](https://openssl-library.org/news/secadv/20260929.txt)) fixes
thirteen CVEs on the 3.5 LTS line, and the next release moves the bundled
copy to 3.5.9. Vulnerability scanners that key on the file version of
`libssl-3-x64.dll` / `libcrypto-3-x64.dll` (Microsoft Defender Vulnerability
Management among them) flag 3.5.8 for all of them (#1665).

Only one of the thirteen sits on a code path NSClient++ calls:

- **[CVE-2026-35189](https://nvd.nist.gov/vuln/detail/CVE-2026-35189)**
  (Low) — a certificate carrying many relative CRL distribution points makes
  OpenSSL allocate disproportionate heap memory when it caches the
  certificate's extensions. Every TLS client in NSClient++ that verifies its
  peer (`NRDPClient`, `IcingaClient`, `GraphiteClient`, `NSCANgClient`,
  `SMTPClient`, `CheckKubernetes`, `check_tcp`, fleet sync) caches the
  server's certificate, and the NRPE and web servers cache a client
  certificate when configured to ask for one. A malicious or spoofed peer
  can therefore drive up the agent's memory use. The impact is a denial of
  service on the agent; there is no memory-safety or confidentiality impact.
- **[CVE-2026-54872](https://nvd.nist.gov/vuln/detail/CVE-2026-54872)**
  (Low) — a timing side-channel in ECDSA signing with curves that have no
  dedicated constant-time implementation. NSClient++ generates RSA and
  Ed25519 keys and its TLS key exchange uses X25519 / P-256, which are not
  affected; it would take an operator-installed server certificate on a
  curve such as brainpool or secp256k1, and a peer timing many handshakes.

The remaining fixes, including the one High-severity entry
([CVE-2026-84782](https://nvd.nist.gov/vuln/detail/CVE-2026-84782), a heap
over-read in DTLS retransmission), are in DTLS, QUIC, CMP and SM2 code and
the `SSL_set_SSL_CTX()` mid-handshake context switch, none of which
NSClient++ uses.

Not affected: the Linux packages (DEB/RPM) link the distribution's OpenSSL
and receive these fixes through ordinary OS updates. The macOS tarball
carries the Homebrew `openssl@3` bottle current when it is built, so a
tarball built after 29 September 2026 already ships 3.5.9 or later.

**What to do:** upgrade Windows installs to the next release. No
configuration change is needed.
