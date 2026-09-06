#### About `submit_remote_nscp`

`submit_remote_nscp` sends a **passive result** to a remote NSClient++ agent over
the NSCP protocol: instead of asking the far end to run a check, it hands it a
result that has already been produced here.

This is how you chain NSClient++ agents. A host that cannot reach the monitoring
server — behind a firewall, in a DMZ, on a management segment — submits its
results to an agent that can, and that agent forwards them onward through
whatever transport the monitoring server expects.

The result is described with `command=` (or its synonym `alias=`, the service
name to report against), `result=` (a number, or `OK` / `WARN` / `CRIT` /
`UNKNOWN`) and `message=`. `batch=` submits several results in one connection as
`command|result|message` records separated by `separator=` (default `|`).

Unlike a passive submission over NRPE, there is no payload ceiling here and
performance data travels as structured data rather than being flattened into the
message, so a full check result survives the hop intact.

Connection, password and TLS options are the same as for
[`check_remote_nscp`](#check_remote_nscp). The usual way to use this command is
to route results to it — give a scheduled check the module's target, rather than
invoking it by hand.
