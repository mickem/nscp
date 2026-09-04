---
title: "CVE-2026-73256 / CVE-2026-73257 — HTTP request smuggling in the bundled Mongoose web server"
fixed_in: 0.16.4
fixed_in_note: "bundles Mongoose 7.23"
severity: "Critical (CVSS 9.1, upstream)"
modules: [WEBServer]
action: none
advisory:
  id: "Upstream ([Cesanta Mongoose](https://github.com/cesanta/mongoose))"
  cve: "[CVE-2026-73256](https://nvd.nist.gov/vuln/detail/CVE-2026-73256), [CVE-2026-73257](https://nvd.nist.gov/vuln/detail/CVE-2026-73257)"
  severity: "Critical (9.1)"
  affected: "Windows builds up to 0.16.3 (bundled Mongoose < 7.22)"
  summary: "HTTP request smuggling in the bundled Mongoose web server; exploitable behind a reverse proxy / WAF."
---
NSClient++ bundles the [Cesanta Mongoose](https://github.com/cesanta/mongoose)
embedded web server as the HTTP engine for the `WEBServer` module (the REST API
and web UI) in the **Windows builds** (`NSCP_WEB_BACKEND=mongoose`, the Windows
default). Two request-smuggling vulnerabilities were published upstream against
Mongoose's HTTP parser, both fixed in Mongoose 7.22:

- **CVE-2026-73256** — a broken HTTP/1.0 detection check in `http_cb()` lets a
  request that combines `Transfer-Encoding: chunked` with conflicting HTTP/1.0
  framing be parsed with different message boundaries than an HTTP/1.0 reverse
  proxy in front of it.
- **CVE-2026-73257** — a request carrying **both** `Content-Length` and
  `Transfer-Encoding: chunked` is accepted instead of rejected, enabling a
  classic CL.TE desynchronization against a Content-Length-preferring front end.

Both are only exploitable when requests reach NSClient++ **through an
intermediary** (reverse proxy, WAF, load balancer) that frames the request
stream differently than Mongoose: an unauthenticated attacker can then smuggle
a second request past the intermediary's access controls (path/method ACLs,
IP restrictions, proxy-level authentication) or inject it into another client's
reused connection. In the common direct client → NSClient++ deployment there is
no front end to desynchronize. NSClient++'s own authentication and WEB
permissions are evaluated per request inside the module, so a smuggled request
still needs valid NSClient++ credentials — what smuggling defeats is any
security control enforced *in front of* NSClient++.

Not affected: the Linux packages (DEB/RPM) build the `WEBServer` module on the
Boost.Beast backend and do not contain Mongoose at all; neither do builds with
the `WEBServer` module disabled.

**What to do:** upgrade Windows installs to 0.16.4 or later, which bundles
Mongoose 7.23 — promptly if NSClient++ sits behind a reverse proxy or WAF. As a
stop-gap, an HTTP/1.1-only front end that itself rejects requests with both
`Content-Length` and `Transfer-Encoding` reduces (but does not remove) the
exposure.
