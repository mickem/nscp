---
title: "Web sessions survive a restart of the agent"
fixed_in: 0.23.0
severity: "Low"
modules: [WEBServer]
action: none
---
Restarting the agent used to log every web UI and REST user out. Sessions now
survive it: what `/api/v2/login` hands out is kept in
`${data-path}/nsclient.db` and read back at the next start. This is the first
time a session leaves the agent's memory, so it is worth knowing what is in
that file and what still ends a session.

Only the SHA-256 of each session key is stored, never a key anyone can use (see
[Web session tokens are only held as hashes](#web-session-tokens-are-only-held-as-hashes)).
Someone who copies `nsclient.db` learns that a session exists and whose it is,
and nothing more. Only `/api/v2/login` creates a session, so nothing is kept
that a client was not given.

#### When a stored session is not brought back

Each session is tied to the credentials it was issued against, and it is not
restored when:

* it was logged out — the web UI's **log out** button, or `DELETE /api/v2/login`;
* it is older than eight hours;
* the user's password or role has changed since it was issued;
* the user no longer exists.

Logging out is written to disk as it happens, so a session that was logged out
stays gone even if the agent is killed rather than stopped cleanly.

A password or role change takes effect when the agent next starts, and that is
when the sessions issued against the old password end. The running service
keeps the users it read at startup, so until you restart it the new password
does not work and the old sessions do not end — reapplying settings from the UI
is not enough.

**What to do:** nothing. To end a session, use the log out button or
`DELETE /api/v2/login`. If you relied on a restart to end every session, set
`persist sessions = false` under `[/settings/WEB/server]` — see the
[upgrade note](../setup/upgrading.md).
