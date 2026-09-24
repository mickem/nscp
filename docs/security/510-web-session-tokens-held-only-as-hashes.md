---
title: "Web session tokens are only held as hashes"
fixed_in: 0.23.0
severity: "Low"
modules: [WEBServer]
action: none
---
A crash dump, a core file or an attached debugger used to reveal the web
session token of every logged-in user, sitting in the agent's session table
ready to be replayed against the REST API. The table now holds only a SHA-256
of each token, so a dump shows that a session exists and whose it is, but not a
token anyone can reuse.

One limit worth knowing: a request being served still carries its own token in
memory for as long as it is being handled, so a dump captured under load can
still expose the sessions of the requests in flight at that instant. What it no
longer exposes is every session that is currently open.

A build made without OpenSSL has no hash function and keeps the previous
behaviour.

**What to do:** nothing. Sessions still expire eight hours after login, and the
web UI's *log out* still revokes one immediately.
