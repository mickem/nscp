---
title: "Web sessions: tokens hashed in memory and at rest, bound to a credential fingerprint"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: none
---
Web sessions now survive a restart of the agent, which means a session token
leaves the process for the first time. The token handling was reworked ahead of
that so it does not: what is kept is only a hash, only for the sessions a client
was actually handed, and only for as long as the credentials they were issued
against stay the same.

#### Only a SHA-256 of a session token is stored

The session table used to be keyed by the 32-character bearer token itself. It
is now keyed by the token's SHA-256 (hex), and every lookup — validate, resolve
the user, revoke — hashes its input first. The raw token exists in the HTTP
response that issued it and in the client that holds it, and nowhere else: not
in the agent's memory, and not in `${data-path}/nsclient.db`, where the
sessions are written at a clean shutdown. A copy of `nsclient.db` (or a core
dump) therefore no longer hands anybody a working credential, only the fact
that a session exists and which user it belongs to.

A build without OpenSSL has no hash function. It keys the table by the raw
token, as before, and persists nothing at all — such a build cannot serve TLS
either, so there is no session there worth carrying across a restart. In a
build that has one, a token whose digest fails is not issued at all, the same
way a token is not issued when the random generator fails: a session stored
under anything but its hash could never be looked up, or revoked, again.

#### Only the sessions a client was handed are written out

Every request that authenticates with Basic auth creates a session (the token
comes back as a cookie), so a monitoring system polling with Basic auth mints
one per poll and never looks at any of them. Those stay in memory, capped and
swept as before, and are never written to disk. Only a session whose key went
through `/api/v2/login` — the one endpoint that returns a key to a client — is
marked as held, and only marked sessions are exported. The whole table is one
row in `nsclient.db`, replaced at every shutdown, so a session that expired or
was logged out leaves nothing behind.

#### A session is bound to the credentials it was issued against

Each session records a *credential fingerprint*: the SHA-256 of the user's role
and the password value stored for them. Changing the password or the role
moves it, and on the way back in at boot a stored session whose fingerprint no
longer matches the configuration is dropped rather than restored (in memory,
re-adding a user still revokes their tokens outright, as it always did). A
session for a user who has been removed from the configuration entirely is
dropped the same way. Where no hash function is available the fingerprint is
simply absent — never the underlying material, which in such a build would be
the cleartext password.

This preserves the property that a stolen token does not survive a password
change, and extends it across a restart, which is where an in-memory-only
revocation list would otherwise have lost it.

#### Nothing else about sessions changed

Sessions still expire eight hours after login, wherever they are stored: an
expired record is refused on import and is never written out. `DELETE
/api/v2/login` still revokes immediately, and because the shutdown export reads
the live table, a session that was logged out is simply not written back.

**What to do:** nothing, unless a restart was your way of ending every session
at once. `persist sessions = false` under `[/settings/WEB/server]` keeps
sessions in memory only, as before; a password change ends one user's
sessions; `DELETE /api/v2/login` ends one. Users whose password is configured
in cleartext in the INI have to log in again after a restart — see the
[upgrade note](../setup/upgrading.md).
