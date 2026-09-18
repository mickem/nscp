---
title: "Web sessions: tokens hashed in memory and at rest, bound to a credential fingerprint"
fixed_in: next
severity: "Low"
modules: [WEBServer]
action: none
---
Web sessions now survive a restart of the agent, which means a session token
leaves the process for the first time. The token handling was reworked ahead of
that so it does not: what is kept is only a hash, and only for as long as the
credentials it was issued against stay the same.

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
either, so there is no session there worth carrying across a restart.

#### A session is bound to the credentials it was issued against

Each session records a *credential fingerprint*: the SHA-256 of the user's role
and the password value stored for them. Re-applying an unchanged user (which
every settings reload and every restart does) leaves the fingerprint alone and
the session alive; changing the password or the role moves it, and the session
is revoked — in memory, and on the way back in at boot, where a stored session
whose fingerprint no longer matches the configuration is dropped rather than
restored. A session for a user who has been removed from the configuration
entirely is dropped the same way.

This preserves the property that a stolen token does not survive a password
change, and extends it across a restart, which is where an in-memory-only
revocation list would otherwise have lost it.

#### Nothing else about sessions changed

Sessions still expire eight hours after login, wherever they are stored: an
expired record is refused on import and is never written out. `DELETE
/api/v2/login` still revokes immediately, and because the shutdown export reads
the live table, a session that was logged out is simply not written back.

**What to do:** nothing. Users whose password is configured in cleartext in the
INI have to log in again after a restart — see the
[upgrade note](../setup/upgrading.md).
