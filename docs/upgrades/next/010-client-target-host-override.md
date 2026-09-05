---
icon: "🔒"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NRPEClient, NSCPClient, GraphiteClient, SyslogClient, CheckMKClient]
action: conditional
---
**A client target that carries credentials now refuses `host=`, `port=` and
`address=` from the request.** Every outbound client module used to load the
`default` target — password or token included — and then let the request
move the destination, so anyone able to run the module's `submit_*`/`check_*`
commands (a REST user in the seeded `monitoring` role, or an NRPE peer with
`allow arguments = true`) could have the agent send the configured
credentials to a host of their choosing. A target with a `password` or
`token` now answers such a request with an error naming the target. Targets
without credentials, and every call that does not override the destination,
are unchanged. If you relied on one credentialed target plus `host=` to
reach several servers, configure each server as its own target and select it
with `target=`, or set the new `allow host override = true` on the target to
restore the old behaviour explicitly. See the
[security notice](../security/notices.md#client-credentials-stay-with-their-target-private-script-upload-staging-and-a-junction-proof-shared-folder).
