---
title: "Fleet: the certificate pin is enforced as a pin"
fixed_in: 0.22.0
severity: "Low"
modules: [core]
action: conditional
---
`mtls_server_cert_pem` is whatever the server returns at enrollment. The agent
added it to the trust store and skipped hostname verification, which *is* a pin
when the PEM is the server's own leaf certificate. Hand out an intermediate or a
public CA instead and the same code accepted **any** certificate chaining to it,
for **any** name — weaker than ordinary verification rather than stronger, and
the agent could not tell the difference.

The pin is verified as a pin now. The pinned PEM is parsed once: for a leaf, the
handshake requires the peer's SubjectPublicKeyInfo digest to be the pinned one
(the SPKI rather than the whole certificate, so a server renewing with the same
key keeps matching); for a PEM that is itself a CA, hostname verification stays
on, because a CA certificate cannot speak for identity on its own. A pinned PEM
that will not parse is refused outright rather than silently falling back.

**What to do:** nothing required on a working fleet. If enrollment handed out a
CA certificate rather than the server's leaf as the pin, hostname verification
now applies to that connection — the certificate has to match the name in
`mtls_url`.
