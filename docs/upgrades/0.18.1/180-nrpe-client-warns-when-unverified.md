---
icon: "🔒"
modules: [NRPEClient]
action: conditional
---
**The NRPE client now says so when it is not authenticating the server, and
generated certificates carry a usable SAN.** `verify mode` defaults to `none`
for every NRPE target, so `ssl = true` on its own gives encryption with no
peer authentication — any host that answers the TCP connect is trusted, and
on-path impersonation is undetectable. The default has not changed (most NRPE
servers in the field have no CA to verify against), but the client now logs
one error line per target and `verify mode`, at the first check against it,
naming the endpoint. Set `verify mode = peer-cert` with `ca` pointing at the
issuer of the server's certificate to silence it and actually authenticate
the server. Certificates NSClient++ generates now list the machine's own host
name and address in the subject alternative name instead of only
`localhost`/`127.0.0.1`, so peer verification against a generated certificate
is possible at all — regenerate an existing one (delete it and restart) if
you want to turn verification on. See the
[security notice](../security/notices.md#nrpe-and-the-shared-tls-layer-key-permissions-handshake-deadline-and-codec-fixes).
