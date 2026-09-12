---
title: "Fleet: encrypted bundles are opened by the agent, not the server"
fixed_in: next
severity: "Low"
modules: [core]
action: conditional
---
A fleet server distributes configuration, scripts and other files to its
agents as signed bundles. The signature and the published SHA-256 establish
that a bundle arrived as the operator uploaded it, but the server stores and
serves its contents, so anything in a bundle — credentials in a generated
`nsclient.ini` fragment, a script carrying a token — was readable by whoever
could read the server's storage or stand in front of it with a valid
certificate.

The agent can now open a bundle the operator sealed in the browser before
upload (`enc-v1`: AES-256-GCM under a 32-byte key, the bundle's name and
version authenticated alongside the ciphertext). The key reaches the agent out
of band and is never sent to the server:

* `nscp enroll --bundle-key <key>` at enrollment, the `FLEET_BUNDLE_KEY`
  installer property, or `nscp enroll --update-bundle-keys` on an already
  enrolled host. Several keys may be configured so a key can be rotated
  without a window where neither key works.
* The key is stored in the enrollment manifest (`agent-state.json`), beside
  the host's private key and under the same permissions — not in
  `nsclient.ini`.
* A sealed bundle served under another name or version fails to open rather
  than being applied, and a bundle that names a configured key but fails to
  authenticate is refused outright instead of being retried against the
  remaining keys.
* Plaintext exists only in the sync's staging directory and is removed on
  every exit path.

`nscp enroll --require-encrypted-bundles` (or
`FLEET_REQUIRE_ENCRYPTED_BUNDLES=1`) additionally refuses every bundle that is
not sealed, for a fleet server that is trusted to distribute configuration but
not to read it. It is held in the manifest rather than in the synced
configuration, so a compromised server cannot switch it off by sending a
bundle that turns it off.

**What to do:** nothing required — an unsealed bundle is still applied unless
you ask for the requirement, and an agent with no key configured behaves
exactly as before. If you use the fleet server's *Encryption key* feature,
hand each agent the key as above; agents without it refuse the sealed bundle
rather than applying something they cannot read.
