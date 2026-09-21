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
out — hashes its input first. What the table holds for each session is now the
hash, the user name and the creation time, so a crash dump or a debugger
attached to the agent no longer yields the set of live credentials: it shows
that a session exists and whose it is, but not the bearer token that would let
somebody use it.

This is about the session table, not about the whole process. A request being
served still carries its own raw token — in the read buffer it arrived in, and
in the request-scoped context the permission check and the login routes read it
back from — for as long as that request is on the stack. A dump taken while
requests are in flight can therefore still contain the tokens of those
requests. What it can no longer contain is one entry per logged-in user,
readable long after those users stopped making requests.

A build without OpenSSL has no hash function and keeps the table keyed by the
token, as before. In a build that has one, a token whose digest fails is not
issued at all, the same way a token is not issued when the random generator
fails: a session stored under anything but its hash could never be looked up,
or revoked, again.

**What to do:** nothing. Sessions still expire eight hours after login, and
`DELETE /api/v2/login` (the web UI's *log out*) still revokes one immediately.
