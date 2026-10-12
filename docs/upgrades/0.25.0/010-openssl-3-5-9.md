---
icon: "🔒"
modules: [packaging]
action: none
---
**The bundled OpenSSL is updated from 3.5.8 to 3.5.9** in the Windows
builds, picking up the thirteen fixes of the 29 September 2026 upstream
security release on the 3.5 LTS line. The only one on a code path
NSClient++ calls is
[CVE-2026-35189](https://nvd.nist.gov/vuln/detail/CVE-2026-35189), a
memory-pressure denial of service from a crafted peer certificate; the rest
are in DTLS, QUIC, CMP and SM2 code the agent never uses. The update also
clears the version-based finding that vulnerability scanners raise against
`libssl-3-x64.dll` / `libcrypto-3-x64.dll` 3.5.8 (#1665). No configuration
change is needed. The Linux packages link the distribution's OpenSSL and
are unaffected; the macOS tarball carries the Homebrew `openssl@3` current
at build time. See the
[security notice](../security/notices.md#openssl-359-bundled-openssl-updated-to-the-29-september-2026-security-release).
