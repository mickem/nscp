---
icon: "🔒"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient]
action: conditional
---
**A client target no longer lets a request send its configured credentials to
a destination the request names.** `host=`, `port=` and `address=` used to move
the destination while the target's `password` or `token` came along, so anyone
able to run a client module's `submit_*`/`check_*` command — a REST user in the
seeded `monitoring` role, or an NRPE peer with `allow arguments = true` — could
have the agent send that credential to a host of their choosing. That
combination is now refused. A target with no credential, a request supplying its
own `password=`/`token=`, and a request that does not move the destination are
unaffected. If you relied on one credentialed target plus `host=` to reach
several servers: pass the credential with the request, configure each server as
its own target and select it with `target=` (which now works for queries too),
or set `allow host override = true` on the target. See the
[security notice](../security/notices.md#client-credentials-could-be-sent-to-a-caller-chosen-host).
