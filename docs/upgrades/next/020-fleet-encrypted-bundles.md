---
icon: "🔒"
modules: [core]
action: conditional
---
**Fleet: encrypted bundles are now opened by the agent.** Nothing to do unless
you use the fleet server's *Encryption key* feature. A bundle the operator seals
in the browser (`format: enc-v1`) used to be refused by the agent as an
unreadable archive; the agent now opens it with a key you hand it out of band,
and the server never sees that key. Give the key at enrollment with
`nscp enroll --bundle-key <key>` or the `FLEET_BUNDLE_KEY` installer property,
or to an already enrolled host with `nscp enroll --update-bundle-keys
--bundle-key <key>` (several `--bundle-key` while rotating; on Windows,
re-running the installer with only `FLEET_BUNDLE_KEY` does the same). The key
is stored in the enrollment manifest (`agent-state.json`) beside the host's
private key, never in `nsclient.ini`. `nscp enroll --require-encrypted-bundles`
(or `FLEET_REQUIRE_ENCRYPTED_BUNDLES=1`) refuses every bundle that is not
sealed, for a fleet server you do not trust with plaintext configuration; it
is stored in the manifest too, so the server cannot switch it off. See
[Central management with NSClient Fleet](fleet.md#encrypted-bundles).
