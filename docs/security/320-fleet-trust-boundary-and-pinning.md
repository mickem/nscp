---
title: "Fleet: the certificate pin is a pin, the signing key cannot rotate unendorsed, and the trust boundary is written down"
fixed_in: next
severity: "Low"
modules: [core, CheckWMI, CheckMySQL, CheckMSSQL, CheckDisk, docs]
action: conditional
---
Three findings from a review of the fleet agent and the checks that connect
somewhere else.

#### The "pin" was a trust anchor with the hostname check turned off

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

#### A renewal could replace the bundle signing key

Bundles are signed offline precisely so a compromised fleet server cannot forge
one. The renewal response carried a new `bundle_signing_pub_pem` and the agent
installed it, so the server could hand out a new verification key and sign
whatever it liked from then on — over a channel authenticated by the server
itself, which is the authority in question.

A rotation is now accepted only when the key being replaced endorses its
replacement: `bundle_signing_pub_sig`, a detached Ed25519 signature over the new
PEM made with the key this host already holds. A server that sends no
endorsement cannot rotate the key; the operator re-enrolls, which is a
deliberate act by whoever holds the offline key. The server's own TLS
certificate is unchanged in this respect — it authenticates the channel, the old
pin authenticated the channel the renewal arrived on, and refusing it would
break ordinary certificate rotation.

#### What a fleet server can do to a host was not written down anywhere

`fleet.ini` is an ordinary INI include of the settings store, and the desired
state the server sends is rendered into it verbatim and unsigned. A
configuration file for this agent can enable script execution, define the
scripts, add includes pointing anywhere and rewrite the fleet settings
themselves: **a fleet server can run code as `SYSTEM` or `root` on every host
enrolled with it.** That is the same trust any configuration-management system
carries, and it is a legitimate design — but no page said so, while the
installer text implied executable content was separately authorised, and the
fleet guide's claim that `--require-encrypted-bundles` means "nothing the server
sends is applied unless a key holder produced it" overstated what the flag
covers. The flag covers bundles; the desired state is not a bundle.

The fleet guide now has a section stating the boundary plainly, the encrypted
bundles section says what the flag does and does not reach, and the installer
documentation carries the same warning at the point where enrollment is
configured.

#### Checks that connect somewhere else are documented as such

`check_wmi`, `check_mysql`, `check_mssql` and `check_uncpath` take the host, the
credentials and sometimes the whole connection string as arguments. Where the
caller supplies those, they are server-side request forgery from the agent's
network position, and an outbound authentication attempt made on the caller's
behalf. That is by design and the controls are the ones that decide whether a
caller may pass arguments at all — now stated as such in the securing guide and
in [Restricting what a check may read](../concepts/check-access.md). `check_wmi`
remains the only one with a named-target gate of its own.

**What to do:** nothing required on a working fleet. If your fleet server rotates
the bundle signing key at renewal, it must now send `bundle_signing_pub_sig`
alongside it; until it does, re-enroll the hosts instead. If enrollment handed
out a CA certificate rather than the server's leaf as the pin, hostname
verification now applies to that connection — the certificate has to match the
name in `mtls_url`.
