---
title: "Diagnostics tables expose running processes over REST and with failure context"
fixed_in: next
severity: "Low"
modules: [core, WEBServer, CheckSystem, CheckSystemUnix]
action: conditional
---
Diagnostics are a new kind of host data: small tables of what is going on
right now, collected by the core on an interval and kept with each failed
check. This release ships three tables - the last failed checks
(`failures.recent`), and the processes using the most CPU and the most memory
(`processes.top_cpu`, `processes.top_memory`) - and they reach readers three
ways:

* **Live, over REST.** `GET /api/v2/diagnostics` and
  `GET /api/v2/diagnostics/<section>` serve the latest snapshot under a new
  grant, `diagnostics.list`, which no built-in role holds other than `full`
  (through `*`).
* **With a failed check, over REST.** `GET /api/v2/failures/<index>/context`
  serves the diagnostics a failure was recorded with under the existing
  `failures.list` grant. A caller holding `failures.list` can therefore read
  the process names that were running when a check failed, as it can already
  read the metrics snapshot.
* **To an enrolled fleet server.** A failure-context report carries the same
  tables, to the server the host is enrolled with, over the pinned mTLS
  channel, only when that server asks (`X-Failure-Context: accept`) and at
  most once per `failure context interval`.

What leaves the host is new: the names, pids, CPU share and memory of the
processes at the top of each table (a view of what runs on the machine), and
the recent failed checks' command, status, caller and first line of output,
which the failed-check log already holds. Command lines are never a column:
they carry credentials far more often than check arguments do. Every text cell
passes through the failed-check log's redaction, so a `password=` or `token=`
pair loses its value.

Everything is off by default, twice over: nothing is collected until
`capture diagnostics = true` under `[/settings/core/failed checks]`, and a
process table only exists once `processes = true` is set in the producing
module's own section (`[/settings/system/windows/diagnostics]`,
`[/settings/system/unix/diagnostics]`).

**What to do:** nothing on a default install. Before turning `capture
diagnostics` on, decide whether the roles holding `failures.list` may see
which processes run on the host, grant `diagnostics.list` only to the roles
that need the live tables, and use `prune pattern` to leave out a table the
fleet server should not receive (`prune pattern = processes.top_memory`). To
keep failed checks off the fleet server altogether, set
`failure context = false` under `[/settings/fleet]`.
