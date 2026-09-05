---
icon: "🔒"
modules: [NRPEServer, NRPEClient, NSCAServer, NSClientServer, CheckMKServer, WEBServer, core]
action: required
---
**Generated TLS private keys are now created readable only by the agent, and a
generated CA no longer ships its private key to clients.** Certificates
NSClient++ generates itself — including the one a default NRPE start creates
when `certificate` points at a missing file — used to land at `0644` with an
unencrypted key inside; they are now `0600` (restricted DACL on Windows), and
a generated CA keeps its key in `ca-key.pem` beside the distributable
`ca.pem`. Existing files are not touched: run
`chmod 600 /etc/nscp/security/certificate.pem`, and if you distributed a
generated `ca.pem`, regenerate that CA and re-issue client certificates —
anyone holding the old file can mint certificates that pass
`verify mode = peer-cert`. See the
[security notice](../security/notices.md#nrpe-and-the-shared-tls-layer-key-permissions-handshake-deadline-and-codec-fixes).
