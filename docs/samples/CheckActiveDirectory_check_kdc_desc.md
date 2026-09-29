#### About `check_kdc`

`check_kdc` verifies that a Kerberos KDC is actually issuing responses — not
just that port 88 is open. It sends a real (unauthenticated) `AS-REQ` over TCP
and classifies the answer: an `AS-REP` or any `KRB-ERROR` proves a live KDC,
while silence, a reset or a non-Kerberos answer means authentication is down
even though a port probe would still pass. Kerberos failure looks like
"everything is broken" to users, so this is the check to point at every domain
controller.

#### Which account the probe names

The `AS-REQ` names an account that exists: by default this machine's own
(`HOST$`, the account Windows itself authenticates with), which is available
whenever the probed realm is the domain the machine is joined to. The KDC
answers it with `KDC_ERR_PREAUTH_REQUIRED` — the *healthy* result — because the
probe never sends a password: it asks for a ticket, and the KDC asks it to
prove who it is. No password is ever sent and nothing counts against the
account's lockout.

A probe for a name that does not exist would get `KDC_ERR_C_PRINCIPAL_UNKNOWN`
instead. That still proves the KDC is alive, but the DC logs it as a failed
ticket request (event 4768), and a monitor doing that against every DC every
few minutes is exactly the pattern user-enumeration detections alert on. So
the check never invents one: to probe a realm this machine has no account in
(a foreign realm, or from a machine that is not domain-joined), name one with
`principal=`. Pick an account that exists and requires Kerberos
pre-authentication (every account does unless *Do not require Kerberos
preauthentication* is set on it) and is enabled: a disabled or expired account
is refused with its own logged failure. Without `principal=` the check returns
**UNKNOWN** saying so, before anything is sent.

The `principal` keyword shows which account was named.

#### Thresholds and options

Defaults: **WARNING** when `time > 1000`, **CRITICAL** when `responding = 0`.

A host whose name does not resolve never starts an exchange, so it has no
round-trip time to report: `time` renders as `?` and contributes no perf data
rather than putting a sentinel into the series. It still goes **CRITICAL** on
`responding = 0`.

Options: `server=<host>` (repeatable) picks the KDC(s) to probe and
`realm=<REALM>` the realm; both default to what the domain join discovers
(`DsGetDcName`). A discovered realm is uppercased the way Active Directory
reports it; an explicit `realm=` is sent exactly as typed, since Kerberos
realms are case sensitive and a non-AD KDC may serve a lowercase one (max 255
characters). On a machine that is not domain-joined, `server=`, `realm=` and
`principal=` are required and the check says so with **UNKNOWN**.

`timeout=<ms>` (default 5000) bounds the whole probe, name lookups included:
all KDCs are looked up and probed concurrently under one deadline, so it also
bounds the whole check when several KDCs are unreachable. A lookup the DNS
server never answers is reported as `resolve failed: no answer from DNS in
time` when the deadline passes; Windows cannot cancel it, so it finishes on
its own in the background, and until it has, further runs against the same
host report that the previous lookup has not returned rather than starting
another.
