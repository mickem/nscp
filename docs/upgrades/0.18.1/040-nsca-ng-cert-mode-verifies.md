---
icon: "🔒"
modules: [NSCANgClient]
action: conditional
---
**NSCA-NG cert mode now actually verifies the server (and presents the
client certificate).** In 0.18.0 the `NSCANgClient` cert mode
(`use psk = false`) applied its TLS configuration to the OpenSSL context
*after* the connection object had been created from it, and OpenSSL copies
the verify mode, client certificate, cipher list and TLS-version bounds out
of the context at creation time — so `verify mode = peer-cert` was silently
ignored, any certificate the server presented was accepted, and the
configured client certificate was never sent. The configuration is applied
before the connection is created now. Two operator-visible consequences:
a cert-mode target that "worked" against a server whose certificate does
not chain to the configured `ca` (or does not match the host name) will now
fail to connect — that is the verification working; fix the server
certificate, or opt out explicitly with `insecure = true` if you accept the
MITM risk. And servers that require a client certificate will start seeing
it. The default PSK mode (`use psk = true`) is unaffected. See
[Security notices](../security/notices.md).
