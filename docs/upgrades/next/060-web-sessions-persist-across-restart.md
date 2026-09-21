---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**You stay logged in to the web UI across a restart of the agent.** Restarting
the service, or upgrading it, no longer logs every web user out: a browser tab,
and a script holding a key from `/api/v2/login`, keeps working. Sessions still
expire eight hours after login, and only a hash of each session key is ever
kept, in memory and on disk; see the
[security notice](../security/notices.md#web-sessions-survive-a-restart-of-the-agent).

To end a session, use the **log out** button in the web UI, or call
`DELETE /api/v2/login` with the key. It stops working immediately and does not
come back, not even if the agent is killed rather than stopped cleanly.

Changing a user's password or role also ends that user's sessions, and removing
the user ends theirs — but only from the next start of the agent. The running
service keeps the users it read when it started, so until you restart it the
new password does not work and the old sessions keep working. Reapplying
settings from the UI is not enough; restart the service, or log the sessions
out.

**If you relied on a restart to log everyone out**, set
`persist sessions = false` under `[/settings/WEB/server]`: sessions are then
kept in memory only, as before, and every restart ends every session. Use the
same switch to invalidate every session at once after a suspected leak (restart
once with it off; switch it back on afterwards if you want sessions to
survive the next restart).

```ini
[/settings/WEB/server]
; Keep sessions in memory only, as before: every restart ends every session.
persist sessions = false
```

One limitation: a user whose password is written **in cleartext** in
`[/settings/WEB/server/users/<user>]` still has to log in again after a
restart. Passwords stored in their hashed form (`pbkdf2-sha256$…`, which is
what the agent writes for `admin`) are not affected. To convert a cleartext
password, run `nscp web add-user --user <user> --role <role>` without
`--password`: it keeps the password the user already has and stores it hashed.
