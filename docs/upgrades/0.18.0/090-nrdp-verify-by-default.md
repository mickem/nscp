---
icon: "🔒"
modules: [NRDPClient]
---
**NRDP HTTPS submissions now verify the server certificate by default on
the `nscp client`/REST path.** Previously an `https://` submission made that
way with no `verify mode` set trusted any certificate silently (configured
targets already defaulted to `peer`). If you submit to a self-signed NRDP
endpoint that way and want to keep skipping verification, pass `--verify
none` (or point `--ca` at the certificate) explicitly. Plain `http://` is
unaffected. Two further NRDP hardening fixes ship in the same release — a
malformed server response can no longer crash the agent, and the NRDP token
and any proxy-URL credentials are redacted from the trace log. See
[Security notices](../security/notices.md).
