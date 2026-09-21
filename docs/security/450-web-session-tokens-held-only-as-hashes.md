---
title: "Web session tokens are only held as hashes"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: none
---
The web server's session table used to be keyed by the session token itself:
the 32-character key that `/api/v2/login` hands out and that the web UI sends
as a bearer token on every request. It is now keyed by the SHA-256 of that
token, and every lookup — validating a request, resolving the user, logging
out — hashes its input first. The token exists in the login response and in
the client that holds it, and nowhere else: a crash dump or a debugger
attached to the agent no longer contains a usable credential, only the fact
that a session exists and which user it belongs to.

A build without OpenSSL has no hash function and keeps the table keyed by the
token, as before. In a build that has one, a token whose digest fails is not
issued at all, the same way a token is not issued when the random generator
fails: a session stored under anything but its hash could never be looked up,
or revoked, again.

**What to do:** nothing. Sessions still expire eight hours after login, and
`DELETE /api/v2/login` (the web UI's *log out*) still revokes one immediately.
