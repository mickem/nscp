---
icon: "🔧"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient, NRPEClient, GraphiteClient, SyslogClient, CheckMKClient, CollectdClient, Op5Client]
action: none
---
**`target=` now selects a configured target on the query path as well.** The
shared client machinery only ever applied `target=` / `-t` when a command was
run as an *exec*; as a query — which is what a REST or NRPE caller gets for
`check_*` and `submit_*` — the argument was accepted and then ignored, so the
call silently went to the `default` target. It is applied on both paths now,
with any explicit option (`host=`, `timeout=`, …) still winning over what the
selected target says. A query that passed `target=` and relied on reaching
`default` anyway will now reach the target it named.
