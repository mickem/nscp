---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**The WEB server does not start with a certificate that does not load.** If
`certificate` (or `certificate key`) under `[/settings/WEB/server]` points at a
file that exists but cannot be used - unreadable, no certificate in it, no
private key, or a key for a different certificate - the WEB server now logs
why and does not start. Before, an unreadable certificate or key left it
serving plain HTTP on 8443 (Linux), a mismatched key stopped it (Linux), and
on Windows it listened with every TLS handshake failing. Fix the certificate
(the default `${certificate-path}/certificate.pem` is generated when missing),
or remove it and set `allow insecure = true` if cleartext HTTP is really what
you want. See the
[security notice](../security/notices.md#web-server-no-fallback-to-cleartext-http-when-the-certificate-does-not-load).
