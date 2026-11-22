---
title: "Mongoose web backend: headers past the 30th were silently ignored"
fixed_in: 0.27.0
fixed_in_note: "bundles Mongoose 7.24"
severity: "Medium behind a reverse proxy or WAF, Low for direct connections (our assessment; upstream lists the fix without a rating)"
modules: [WEBServer, NCPAServer]
action: conditional
---
NSClient++ bundles the [Cesanta Mongoose](https://github.com/cesanta/mongoose)
embedded web server as the HTTP engine of the `WEBServer` and `NCPAServer`
modules in the **Windows builds** (`NSCP_WEB_BACKEND=mongoose`, the Windows
default). Mongoose parses request headers into a fixed table of 30 entries.
Up to 7.23, a request with more headers than that was accepted and served
with everything past the 30th dropped: the application never saw those
headers, and neither did Mongoose's own framing logic.

On its own that only meant a client could have a header ignored. Behind an
**intermediary** (reverse proxy, WAF, load balancer) that does read every
header, the two ends could disagree about the same request - a
`Content-Length` or `Transfer-Encoding` in position 31 or later framed the
body for the proxy but not for Mongoose, which then read the body bytes as the
next request on the connection. That is the same desynchronization primitive
as the request-smuggling issues fixed in 0.16.4: it lets a request skip
controls enforced *in front of* NSClient++ (path or method ACLs, proxy-level
authentication). NSClient++'s own authentication and permissions are evaluated
per request inside the module, so a smuggled request still needs valid
NSClient++ credentials.

Mongoose 7.24 refuses such a request instead: it fails to parse and the
connection is closed unanswered. Upstream lists this as *HTTP header-table
bounds validation* among the security fixes of 7.24. The other security fixes
in that release are in components NSClient++ does not compile in or never
reaches - the built-in TCP/IP and TLS stacks (NSClient++ uses the operating
system's sockets and OpenSSL), PPP, MQTT, WebSocket (CVE-2026-106024; the
modules never upgrade a connection), and Mongoose's static-file, directory
listing and SSI serving (NSClient++ serves files through its own controller).

Not affected: the Linux and macOS packages, which build both modules on the
Boost.Beast backend and do not contain Mongoose.

**What to do:** upgrade Windows installs that sit behind a reverse proxy or
WAF. If that proxy adds enough headers to push a request past 30, requests
through it are now dropped - see the
[upgrade note](../setup/upgrading.md); reduce the headers it forwards.
