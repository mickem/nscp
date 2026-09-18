---
icon: "🔒"
modules: [core]
action: conditional
---
**The fleet certificate pin is enforced as a pin, and the bundle signing key
cannot rotate without the old key's endorsement.** A pinned PEM that is the
server's leaf certificate is now matched by its public key at every handshake;
one that is itself a CA keeps hostname verification, because a CA certificate
cannot identify a server on its own — previously either kind was simply trusted
for any name. If enrollment handed out a CA rather than the leaf, that
connection now requires the certificate to match the name in `mtls_url`. A
renewal response that changes `bundle_signing_pub_pem` must also carry
`bundle_signing_pub_sig`, a detached Ed25519 signature over the new key made
with the old one; without it the rotation is refused and the existing key is
kept, so re-enroll the host instead. The fleet guide gained a section stating
what a fleet server can do to an enrolled host, which is worth reading once:
the managed configuration is an ordinary settings include, so a fleet server is
an administrator of every host enrolled with it. See the
[security notice](../security/notices.md#fleet-the-certificate-pin-is-a-pin-the-signing-key-cannot-rotate-unendorsed-and-the-trust-boundary-is-written-down).
