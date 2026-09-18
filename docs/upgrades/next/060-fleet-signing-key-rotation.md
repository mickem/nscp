---
icon: "🔒"
modules: [core]
action: conditional
---
**The bundle signing key cannot rotate without the old key's endorsement.** A
renewal response that changes `bundle_signing_pub_pem` must also carry
`bundle_signing_pub_sig`, a detached Ed25519 signature over the new key made
with the old one; without it the rotation is refused and the existing key is
kept, so re-enroll the host instead. The fleet guide gained a section stating
what a fleet server can do to an enrolled host, which is worth reading once:
the managed configuration is an ordinary settings include, so a fleet server is
an administrator of every host enrolled with it. See the
[security notice](../security/notices.md#fleet-the-signing-key-cannot-rotate-unendorsed-and-the-trust-boundary-is-written-down).
