---
icon: "🔒"
modules: [NRPEServer, NRPEClient, NSCAServer, NSClientServer, CheckMKServer, WEBServer, core]
action: required
---
**Generated TLS private keys are now created readable only by the agent, and
a generated CA no longer ships its private key to clients.** Certificates
that NSClient++ generates itself — including the one a default NRPE server
start creates when `certificate` points at a missing file — were written with
`fopen()`, landing at `0644` under a normal systemd unit with an
*unencrypted* private key inside. They are now created `0600` on Unix and
with a DACL restricted to `SYSTEM` and the local `Administrators` on Windows.
A generated CA is split in two: `ca.pem` holds only the certificate (this is
the file `nscp nrpe install` tells you to hand to clients), and the CA
private key goes to `ca-key.pem` beside it. **Existing files are not
touched** — check and fix the permissions of any certificate NSClient++
generated for you:

```
chmod 600 /etc/nscp/security/certificate.pem
```

and, if you distributed a generated `ca.pem` to clients, treat that CA as
compromised: anyone holding the file could issue client certificates that
pass `verify mode = peer-cert`. Regenerate it (delete `ca.pem` and restart)
and re-issue client certificates. See the
[security notice](../security/notices.md#nrpe-and-the-shared-tls-layer-key-permissions-handshake-deadline-and-codec-fixes).
