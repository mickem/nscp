---
icon: "🔒"
modules: [NRDPClient, IcingaClient, SMTPClient, NSCAClient, NSCANgClient, NSCPClient]
action: conditional
---
**A client target no longer lets a request send its configured credentials to
a destination the request names.** The outbound client modules load their
`default` target — password or token included — and then apply the request's
arguments on top, so anyone able to run the module's `submit_*`/`check_*`
commands (a REST user in the seeded `monitoring` role, or an NRPE peer with
`allow arguments = true`) could move the destination with `host=`, `port=` or
`address=` and have the agent send the configured credentials to a host of
their choosing. That combination is now refused with an error naming the
target.

Only that combination: a target with no credentials, a request that supplies
its own `password=`/`token=`, and a request that does not move the destination
all behave as before. If you relied on one credentialed target plus `host=` to
reach several servers, either pass the credential with the request, configure
each server as its own target and select it with `target=` (which now works
for queries too, see below), or set the new `allow host override = true` on
the target to keep the old behaviour. See the
[security notice](../security/notices.md#client-credentials-stay-with-their-target-private-script-upload-staging-and-a-junction-proof-shared-folder).
