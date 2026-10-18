---
icon: "🔧 🔒"
modules: [WEBServer, NCPAServer]
action: none
---
**The web server keeps connections alive, and the request body cap is a
setting.** On the Linux packages (the beast web backend) every answer used to
carry `Connection: close`, so the web UI and a monitoring server polling the
REST API paid a TCP and TLS handshake per request; a client that keeps its
connection now gets to reuse it, for up to 30 seconds between requests, on
every platform. Nothing changes for a client that does not. The largest
request body the server accepts was a compile-time constant that differed by
backend - 1 MiB on Linux, 3 MiB on Windows - and a request over it was
dropped without an answer. `[/settings/WEB/server]` gains
`max request size` (default `3MiB`, so Linux now accepts what Windows always
has), written as a byte count with an optional `k`, `m` or `g`; a request
declaring more is answered `413 Payload Too Large` with a body naming the
setting, and logged as `Rejected a request from <ip>: its body is larger than
the N bytes 'max request size' allows`. Raise it for a script upload or a bulk
result submission that is larger. On the mongoose backend (Windows) a value
above 3 MiB is honoured up to 3 MiB, the backend's receive buffer, and the
server says so at startup. The NCPA listener shares the HTTP layer and gains
the keep-alive and the 3 MiB default with it; it has no setting of its own.
Both apply before authentication; see the
[security notice](../security/notices.md#web-server-kept-alive-connections-and-a-configurable-request-body-cap).
