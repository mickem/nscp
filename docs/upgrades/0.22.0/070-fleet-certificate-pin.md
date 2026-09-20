---
icon: "🔒"
modules: [core]
action: conditional
---
**The fleet certificate pin is enforced as a pin.** A pinned PEM that is the
server's leaf certificate is now matched by its public key at every handshake;
one that is itself a CA keeps hostname verification, because a CA certificate
cannot identify a server on its own — previously either kind was simply trusted
for any name. If enrollment handed out a CA rather than the leaf, that
connection now requires the certificate to match the name in `mtls_url`. A
pinned PEM that does not parse is refused rather than quietly falling back to
ordinary verification. See the
[security notice](../security/notices.md#fleet-the-certificate-pin-is-enforced-as-a-pin).
