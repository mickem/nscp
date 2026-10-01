---
icon: "🔒"
modules: [core]
action: conditional
---
**The bundle signing key cannot rotate without the old key's endorsement.** A
renewal response that changes `bundle_signing_pub_pem` must also carry
`bundle_signing_pub_sig`, a detached Ed25519 signature over the new key made
with the old one; without it the rotation is refused and the existing key is
kept, so re-enroll the host instead. Only the key is refused: the certificate,
the CA and the server pin in the same response are still taken, so the host
keeps renewing and the refusal appears in the log as an error rather than as a
renewal failure. Keys are compared as keys, so a server that re-serialises the
same key (different line wrapping, a trailing newline) is not mistaken for a
rotation. A host whose state carries no signing key at all does not get one from
a renewal either — a key is adopted at enrollment, so re-enroll such a host.
The fleet guide gained a section stating
what a fleet server can do to an enrolled host, which is worth reading once:
the managed configuration is an ordinary settings include, so a fleet server is
an administrator of every host enrolled with it. See the
[security notice](../security/notices.md#fleet-the-signing-key-cannot-rotate-unendorsed-and-the-trust-boundary-is-written-down).
