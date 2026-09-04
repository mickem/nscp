---
icon: "🔒"
modules: [WEBServer]
action: none
---
**The bundled Mongoose web server is upgraded to 7.23,** fixing two
critical (CVSS 9.1) HTTP request-smuggling vulnerabilities in its HTTP
parser ([CVE-2026-73256](https://nvd.nist.gov/vuln/detail/CVE-2026-73256),
[CVE-2026-73257](https://nvd.nist.gov/vuln/detail/CVE-2026-73257)). This
affects the **Windows builds** of the `WEBServer` module (REST API / web UI)
and is exploitable when NSClient++ sits behind a reverse proxy or WAF —
upgrade promptly in that topology. The Linux packages use the Boost.Beast
backend and are unaffected. No configuration change is needed. See
[Security notices](../security/notices.md).
