---
title: "Failed WEB authentication now backs off exponentially"
fixed_in: 0.18.1
severity: "Low"
modules: [WEBServer]
action: none
---
The block that follows `auth rate limit max failures` consecutive failed
authentications (default 10) used to be a fixed `auth rate limit block
seconds` window (default 60 s) that reset the counter, so an attacker could
resume guessing as soon as it lapsed — roughly 14 000 guesses a day per source
address, indefinitely, against every credential that path carries: Basic auth,
the `password` header and the legacy `?password=` / `?TOKEN=` form Icinga's
`check_nscp_api` uses.

Each further block from the same IP now doubles the wait, up to an hour, and
resets after a successful authentication or an hour of quiet. This is defense
in depth rather than a fix for an exploitable weakness — PBKDF2-SHA256
(100 000 iterations) and a uniform 403 already made this a poor guessing
channel. It does not address an attacker rotating source addresses; the
limiter is per IP by design, keyed on the socket peer rather than a spoofable
`X-Forwarded-For`.

**What to do:** nothing. A monitoring probe that deliberately authenticates
with bad credentials will now be blocked for longer; point it at an endpoint
it can authenticate to, or set `auth rate limit max failures = 0` for a test
harness.
