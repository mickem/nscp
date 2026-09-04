---
icon: "🚫"
modules: [core, Scheduler, CheckHelpers]
action: none
---
**A denied check is no longer submitted as a passive result.** The permission
layer answers a denied query as a *successful* query carrying an UNKNOWN
"Permission denied" payload, and both `run_schedules` and `check_and_forward`
forwarded that to the monitoring server — overwriting the last real result
for that service while telling the caller it had succeeded. Both now refuse
with `Permission denied: not allowed to run <command>, nothing was
submitted` and touch no channel. If you restrict what a REST or NRPE identity
may run, expect the denial as an error where you previously saw a stale
UNKNOWN appear on the server. See
[Permissions](../concepts/permissions.md).
