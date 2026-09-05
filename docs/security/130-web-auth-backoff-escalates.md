---
title: "Failed WEB authentication now backs off exponentially"
fixed_in: 0.18.1
severity: "Low"
modules: [WEBServer]
action: none
---
The `WEBServer` module blocks a client IP after `auth rate limit max failures`
consecutive failed authentications (default 10) for `auth rate limit block
seconds` (default 60 s). The block used to be a fixed window that reset the
failure counter every time, so an attacker could spend another ten guesses as
soon as it lapsed — about 14 000 guesses per day per source address, sustained
indefinitely — against the admin password and against every credential the
same path carries: Basic auth, the `password` header and the legacy
`?password=` / `?TOKEN=` query-string form Icinga's `check_nscp_api` uses.

The block now doubles for each consecutive block from the same IP — 60 s,
2 min, 4 min, … up to an hour — and only stops growing there. It resets when
that IP authenticates successfully, or when it stays quiet for an hour after
its block expires, so a person mistyping a password still waits only the base
delay. A `block seconds` configured above an hour is never shortened.

This is defense in depth, not a fix for an exploitable weakness: passwords are
stored with PBKDF2-SHA256 (100 000 iterations) and every failure answers with
the same generic 403, so even the old rate was a poor guessing channel against
a strong password. The escalation removes the "forever, at a steady rate" part
of it. It does not defend against an attacker rotating source addresses — the
limiter is per IP by design, keyed on the socket peer rather than on a
spoofable `X-Forwarded-For`.

**What to do:** nothing. If a monitoring system legitimately probes the WEB
server with bad credentials (a health check that expects a 403, say), it will
now be blocked for longer; point it at an endpoint it can authenticate to, or
set `auth rate limit max failures = 0` to disable the limiter for a test
harness.
