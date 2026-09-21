---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**You stay logged in to the web UI across a restart of the agent.** Restarting
the service — or reapplying settings from the UI — no longer logs every web
user out. A browser tab, and a script holding a key from `/api/v2/login`,
keeps working across a service restart or an upgrade. Sessions still expire
eight hours after login, and only a hash of each session key is ever kept, in
memory and on disk; see the
[security notice](../security/notices.md#web-sessions-survive-a-restart-of-the-agent).

To end a session, use the **log out** button in the web UI, or call
`DELETE /api/v2/login` with the key. That revokes it immediately, and a session
that was logged out is not brought back by a restart. Changing a user's
password or role also ends all of that user's sessions, and so does removing
the user.

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
