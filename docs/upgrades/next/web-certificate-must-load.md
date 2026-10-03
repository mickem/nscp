---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**The WEB server does not start with a certificate that does not load.** If
`certificate` (or `certificate key`) under `[/settings/WEB/server]` points at a
file that exists but holds no usable certificate, no private key, or a key for
a different certificate, the WEB server now logs why and does not start. On
the beast backend (Linux packages) it already refused to start; on Windows it
listened and every TLS handshake failed. Fix the certificate (the default
`${certificate-path}/certificate.pem` is generated when missing), or remove it
and set `allow insecure = true` if cleartext HTTP is really what you want.
