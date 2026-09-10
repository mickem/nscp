---
title: "Two ways past the client host-override guard"
fixed_in: 0.20.0
severity: "Medium"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient]
action: conditional
---
The guard added in 0.19.0 (see
[Client credentials could be sent to a caller-chosen host](notices.md#client-credentials-could-be-sent-to-a-caller-chosen-host))
refuses a request that moves a credentialed target's destination, but it only
recognised a credential in a `password` or `token` key. A secret carried inside
the address instead — `?token=SECRET` in the URL, or `user:password@host` —
looked like no credential at all, so `host=` could redirect it to an attacker's
server. A target that supplied a credential but named no address of its own had
the caller's `host=` recorded as its configured destination, so the guard
compared that host against itself and let the credential travel.

A credential now counts wherever it is written, and a target records only an
address it actually names.

**What to do:** nothing, unless a target of yours keeps its credential inside
`address` *and* callers move its destination with `host=`, `port=` or
`address=`. That is now refused, as it already was for a separate `password` /
`token` key. The remedies are unchanged: pass the credential with the request,
configure each destination as its own target and select it with `target=`, or
set `allow host override = true` on the target.
