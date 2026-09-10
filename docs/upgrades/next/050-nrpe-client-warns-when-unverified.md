---
icon: "🔒"
modules: [NRPEClient]
action: conditional
---
**The NRPE client now says so when it is not authenticating the server, and
generated certificates carry a usable SAN.** `verify mode` defaults to `none`,
so `ssl = true` alone gives encryption with no peer authentication and
undetectable on-path impersonation. The default is unchanged, but the client
now logs one error line per target at its first check; set
`verify mode = peer-cert` with `ca` pointing at the issuer to silence it and
actually authenticate the server. Generated certificates now name the machine
itself rather than only `localhost`, so verification is possible at all —
regenerate an existing one to use it. See the
[security notice](../security/notices.md#nrpe-and-the-shared-tls-layer-key-permissions-handshake-deadline-and-codec-fixes).
