---
title: "Fleet: bundle signatures cover which bundle it is, not only its bytes"
fixed_in: 0.22.0
severity: "Low"
modules: [core]
action: required
---
A fleet server distributes configuration and scripts to its agents as signed
bundles. The Ed25519 signature used to cover the bare SHA-256 digest of the
bundle's bytes and nothing else, so it bound none of the bundle's id, name,
version or format. The digest already pins the content, which means a
signature over it alone said little more than "this tenant's server saw this
blob once".

The consequence is that a signed blob could be re-advertised as a *different*
bundle and still verify. Anyone who could write to the server's database — or
otherwise choose what a desired state advertises — could take an old, properly
signed bundle and offer it under a new id, name or version, and the agent
would accept it as that bundle. Only the encrypted format's additional
authenticated data closed name and version, and only for bundles that were
sealed.

The signature now covers a canonical descriptor of the bundle's identity
together with its digest: a version prefix followed by the tenant id, bundle
id, name, version, format and SHA-256, NUL-separated. Every field is a ULID,
an integer or a token from a grammar with no NUL in it, so no two distinct
bundles can produce the same signing bytes. Priority is deliberately absent —
it belongs to a group assignment rather than to the bundle, and the same
bundle legitimately carries different priorities in different groups.

Because the agent needs the tenant id to rebuild the descriptor and cannot
derive it (its certificate carries the tenant *slug*), the desired-state
response now carries one. There is nothing to trust in that value: the
verifying key is per tenant, so a wrong one simply fails verification.

This is a protocol change on both sides. An agent on this version refuses a
bundle whose signature covers only the digest, which is what a fleet server
that predates the descriptor produces.

**What to do:** upgrade the fleet server to a version that signs the
descriptor before (or together with) upgrading agents. An agent talking to a
server that still signs the old way reports `signature verification failed`
for every bundle and keeps the configuration it last applied, so a stale
agent is left running its previous configuration rather than an unverified
one. See the [upgrade note](../setup/upgrading.md) for the operational
sequence.
