---
icon: "🔒"
modules: [packaging, CheckSecurity]
---
**The bundled OpenSSL is updated from 3.5.4 to 3.5.8** in the Windows
builds, picking up the fixes from four upstream security releases on the
3.5 LTS line. The most relevant fix for NSClient++ is
[CVE-2025-11187](https://nvd.nist.gov/vuln/detail/CVE-2025-11187), a stack
buffer overflow parsing a crafted PKCS#12 file — reachable through
`check_certificate` when it scans certificate files that less-trusted
principals can write to — alongside further PKCS#12/ASN.1 and TLS-stack
fixes. No configuration change is needed. The Linux packages link the
distribution's OpenSSL and are unaffected. See
[Security notices](../security/notices.md).
