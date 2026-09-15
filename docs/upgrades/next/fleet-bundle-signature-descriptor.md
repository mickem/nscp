---
icon: "🔒 💥"
modules: [core]
action: required
---
**Fleet bundle signatures changed shape: upgrade the fleet server before the
agents.** Only affects hosts enrolled against a fleet server (`nscp enroll`);
an installation that does not use fleet management is unaffected.

A bundle's Ed25519 signature used to cover the bare SHA-256 digest of its
bytes, which bound nothing about *which* bundle those bytes were — an old
signed blob could be re-advertised under a new id, name or version and still
verify. It now covers a canonical descriptor of the bundle's identity:

```
nsclient-fleet/bundle-sig/v2 \0 tenant_id \0 id \0 name \0 version \0 format \0 sha256
```

signed directly (Ed25519 hashes internally), with the fields taken verbatim
from the desired-state response. The response therefore carries a new
top-level `tenant_id`, which the agent needs to rebuild the descriptor and
cannot derive from its certificate.

The two sides must agree, so:

* **Upgrade the fleet server first**, or at the same time. A server that signs
  the descriptor is already re-signing its stored bundles on startup, so no
  bundle needs re-uploading.
* An agent on this version talking to an older server logs
  `Bundle <id> failed verification: signature verification failed` on every
  poll and keeps the configuration it last applied. Nothing unverified is
  applied and nothing already applied is removed, so the host keeps working —
  it simply stops picking up changes until the server is upgraded.
* An older agent talking to an upgraded server fails the same way, which is
  the case to watch for if you upgrade the server first and the agents later.

A desired-state response that carries bundles but no `tenant_id` is now
rejected as malformed rather than failing later as a bad signature, so the log
names the real problem.

See the [security notice](../security/notices.md#fleet-bundle-signatures-cover-which-bundle-it-is-not-only-its-bytes).
