---
icon: "🔒"
modules: [WEBServer]
action: none
---
**Web sessions now survive a restart of the agent.** Nothing to do. Restarting
the service used to log every web UI and REST user out, because session tokens
lived only in the web server's memory. They are now written to
`${data-path}/nsclient.db` at a clean shutdown and read back at the next start,
so a browser tab — or a script holding a bearer key — keeps working across a
service restart or an upgrade. What is stored is only the SHA-256 of each
token, never the token itself, in memory as well as on disk; see the
[security notice](../security/notices.md#web-sessions-tokens-hashed-in-memory-and-at-rest-bound-to-a-credential-fingerprint).

Everything that ended a session before still ends it:

* Sessions still expire **eight hours after login**, and an expired one is
  neither written out nor read back in.
* `DELETE /api/v2/login` (the web UI's *log out*) still revokes immediately,
  and a revoked session is not written back at shutdown.
* Changing a user's **password or role** still revokes their sessions, now
  across a restart too: each session records a fingerprint of the credentials
  it was issued against, and one that no longer matches the configuration is
  dropped instead of restored. Removing the user from the configuration drops
  them as well.

One limitation, worth knowing if you see users having to log in again anyway:
**a user whose password is configured in cleartext** under
`[/settings/WEB/server/users/<user>]` is re-hashed with a fresh salt on every
boot, so their credential fingerprint changes and their sessions are not
restored. A password already stored in its hashed form (`pbkdf2-sha256$…`,
which is what `nscp web install` writes for `admin`) is stable and its sessions
do survive. `nscp web add-user --user <user> --role <role>` rewrites an existing
cleartext password as a hash (leave `--password` out to keep the password the
user already has), if you want the same for a user you added by hand.
