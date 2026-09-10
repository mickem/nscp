---
icon: "🔒 ⏱️"
modules: [NRPEServer, NSCAServer, NSClientServer, CheckMKServer, core]
action: none
---
**Inbound TLS handshakes are now bounded by the listener's `timeout`.** The
connection deadline was armed only after the handshake had completed, so the
handshake phase itself was unbounded: a host permitted by `allowed hosts`
could open sockets, send nothing, and pin one connection object, file
descriptor and buffer each, indefinitely. The plain-TCP path was bounded by
`timeout` (30 s by default) all along; the SSL path — the NRPE default — was
not. A client on a slow or lossy link that cannot complete a handshake within
`timeout` is now dropped instead of lingering; raise `timeout` on the listener
if that is too tight for your network. See the
[security notice](../security/notices.md#nrpe-and-the-shared-tls-layer-key-permissions-handshake-deadline-and-codec-fixes).
