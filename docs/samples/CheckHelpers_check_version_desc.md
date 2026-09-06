#### About `check_version`

`check_version` returns the running NSClient++ version string as an OK result.
It takes no options and never fails, which makes it the cheapest possible
"is the agent alive and answering?" probe — useful as a heartbeat check, and as
the first thing to run when verifying a new NRPE/REST connection.

It reports only the version *string*. If you want to threshold on the version —
"alert when this fleet member falls behind" — use
[`check_nscp_version`](CheckNSCP.md#check_nscp_version) from the CheckNSCP
module, which exposes `major`, `minor`, `release` and `build` as filterable
keywords, or [`check_nscp_update`](CheckNSCP.md#check_nscp_update) to compare
against the latest release published on GitHub.

The legacy alias `CheckVersion` is accepted for backwards compatibility.
