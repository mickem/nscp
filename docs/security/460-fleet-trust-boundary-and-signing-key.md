---
title: "Fleet: the signing key cannot rotate unendorsed, and the trust boundary is written down"
fixed_in: next
severity: "Low"
modules: [core, docs]
action: conditional
---
Two findings from a review of the fleet agent.

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
certificate is unchanged in this respect — it authenticates the channel, the
pin authenticated the channel the renewal arrived on, and refusing it would
break ordinary certificate rotation.

Refusing the key is not refusing the renewal. The rest of the response — the new
client certificate, the CA and the server pin — is certificate material the host
needs to keep talking to the fleet at all, it is signed by the fleet CA, and it
arrived over the pinned channel. So the certificate is taken, the signing key
stays as it was, and the refusal is logged as an error for the operator.
Abandoning the whole renewal instead would be the more expensive failure: a host
facing a server that got the rotation wrong would stop renewing altogether and
fall off the fleet when its certificate expired — over a key it was keeping
either way.

Two details of the check follow from the same reasoning. Keys are compared as
keys, not as PEM text, so a server that re-serialises the key it already gave
this host — different line wrapping, a trailing newline gained or lost — is not
mistaken for a rotation and refused. And a host whose state carries *no* signing
key does not get one from a renewal either: with nothing to endorse a new key
with, whoever answered that renewal would be choosing what verifies every bundle
from then on, which is the authority the offline key exists to keep away from the
server. A key is adopted at enrollment; such a host re-enrolls. Nothing is
loosened by the refusal: a host with no signing key already fails verification
on every bundle it is offered, so it stays fail-closed rather than trusting a
key the server chose.

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

**What to do:** nothing required on a working fleet. If your fleet server
rotates the bundle signing key at renewal, it must now send
`bundle_signing_pub_sig` alongside it; until it does, re-enroll the hosts
instead. Certificates keep renewing meanwhile, so watch the log for the refusal
rather than for renewal failures.
