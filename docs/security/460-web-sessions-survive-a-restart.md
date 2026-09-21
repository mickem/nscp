---
title: "Web sessions survive a restart of the agent"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: none
---
Web UI and REST sessions now survive a restart of the agent: the session keys
handed out by `/api/v2/login` are saved to `${data-path}/nsclient.db` when the
service stops and loaded again when it starts. This is the first time a
session leaves the agent's memory.

#### What is stored

Only the SHA-256 hash of each session key, never the key itself — the same
hash the agent already keeps in memory (see
[Web session tokens are only held as hashes](#web-session-tokens-are-only-held-as-hashes)).
Someone who obtains a copy of `nsclient.db` learns that a session exists and
which user it belongs to, but cannot use it to log in. Only `/api/v2/login`
creates a session; a request that authenticates with a username and password
on any other route creates none, so nothing is saved that a client was not
given.

#### When a saved session is not brought back

Each saved session is tied to the credentials it was issued against. A session
is not restored, and so ends at the restart, when:

* it was logged out (the web UI's **log out** button, or `DELETE /api/v2/login`);
* it is older than eight hours;
* the user's password or role has changed in the meantime;
* the user no longer exists.

So a leaked key does not outlive a password change, whether or not the agent
was restarted in between.

**What to do:** nothing. To end a session, use the log out button or
`DELETE /api/v2/login`. If you relied on a restart to end every session, set
`persist sessions = false` under `[/settings/WEB/server]` — see the
[upgrade note](../setup/upgrading.md).
