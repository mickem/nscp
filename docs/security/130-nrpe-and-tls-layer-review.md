---
title: "NRPE and the shared TLS layer: key permissions, handshake deadline and codec fixes"
fixed_in: 0.18.1
severity: "High for the generated-key exposure, Low–Medium for the rest"
modules: [NRPEServer, NRPEClient, NSCAServer, NSClientServer, CheckMKServer, WEBServer, core]
action: conditional
---
An end-to-end review of the NRPE path — the wire codec, the server parser and
protocol, both modules, and the shared socket/TLS layer they sit on —
produced the fixes below. None is remotely exploitable for code execution
past `allowed hosts`; the key-permissions issue is the one that needs
operator action.

#### Generated TLS private keys were world-readable

`socket_helpers::write_certs` created the file with a plain
`fopen(cert, "wb")`, so it landed at `0666 & ~umask` — `0644` under a normal
systemd unit — and what it contained was an *unencrypted* PKCS#8 private key
followed by the certificate. Packaging never narrowed it either.

This was not a manual-only path: a default NRPE server start runs
`validate_certificate`, which **generates** the file when it is missing. So a
default install created a world-readable TLS key, and any local account could
read it, decrypt captured NRPE traffic or impersonate the agent.

The CA branch was worse. `write_certs(ca_path, true)` wrote the CA *private
key* into `ca.pem` — the file `nscp nrpe install` tells the operator to
distribute ("the clients need to have a certificate issued from …"). Anyone
who received that file could mint client certificates and walk straight
through `verify mode = peer-cert`, which is NRPE's only real authentication.

Generated key files are now created `0600` on Unix (and re-narrowed when they
already existed) and with a DACL restricted to `SYSTEM` and the local
`Administrators` group on Windows, breaking inheritance so a permissive
parent directory cannot widen them. A generated CA now writes only the
certificate to `ca.pem` and its private key to `ca-key.pem` beside it.

**What to do:** existing files are not modified by the upgrade. Check the
permissions of any certificate NSClient++ generated for you
(`chmod 600 …/security/certificate.pem`). If you distributed a generated
`ca.pem`, treat that CA as compromised — regenerate it and re-issue client
certificates.
