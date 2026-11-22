---
title: "Diagnostics tables expose processes, services and log messages over REST and with failure context"
fixed_in: 0.27.0
severity: "Low"
modules: [core, WEBServer, CheckSystem, CheckSystemUnix, CheckEventLog, CheckDisk]
action: conditional
---
Diagnostics are a new kind of host data: small tables of what is going on
right now, collected by the core on an interval and kept with each failed
check. This release ships seven tables - the last failed checks
(`failures.recent`), the processes using the most CPU and the most memory
(`processes.top_cpu`, `processes.top_memory`), the services that are not
running as they should and the service manager's crash events
(`services.problems`, `services.events`), and the last errors written to the
system logs (`eventlog.errors`) and the size of the places that usually fill a
disk (`disk.growth_spots`) - and they reach readers three ways:

* **Live, over REST.** `GET /api/v2/diagnostics` and
  `GET /api/v2/diagnostics/<section>` serve the latest snapshot under a new
  grant, `diagnostics.list`, which no built-in role holds other than `full`
  (through `*`). The web UI's **Diagnostics** page reads these routes and
  the **Failed checks** page the one below, under the same grants.
* **With a failed check, over REST.** `GET /api/v2/failures/<index>/context`
  serves the diagnostics a failure was recorded with under the existing
  `failures.list` grant. A caller holding `failures.list` can therefore read
  the process names, services and log messages of the moment a check failed,
  as it can already read the metrics snapshot.
* **To an enrolled fleet server.** A failure-context report carries the same
  tables, to the server the host is enrolled with, over the pinned mTLS
  channel, only when that server asks (`X-Failure-Context: accept`) and at
  most once per `failure context interval`.

What leaves the host is new:

* the names, pids, CPU share and memory of the processes at the top of each
  process table - a view of what runs on the machine;
* the names, states and exit codes of the services in trouble, with **the
  path of their executable and the account they run as**, and their start
  time and restart count;
* **event log and journal message text**: the service manager's crash
  messages and every critical or error event in the Application and System
  logs (Windows) or every journal entry at priority `err` or worse (Linux).
  Applications fill those messages with whatever they like - file paths, user
  names, host names, sometimes a connection string;
* **directory paths and sizes**: the temp and log directories, including the
  agent's own and - through `TEMP`/`TMPDIR` - the account it runs as, the page
  and swap files, and the shadow-copy storage per volume, with the size of
  each and a note when a folder could not be read. A directory's total says
  how much was written there, not what; no file name is reported;
* the recent failed checks' command, status, caller and first line of output,
  which the failed-check log already holds.

Command lines are never a column, and a service's executable is cut from its
arguments (the Windows image path, the systemd `ExecStart`), because both
carry credentials far more often than check arguments do. Every text cell
passes through the failed-check log's redaction, so a `password=` or `token=`
pair loses its value - which catches the common shapes, not every secret an
application might write into a log message.

Everything is off by default, twice over: nothing is collected until
`capture diagnostics = true` under `[/settings/core/failed checks]`, and each
table only exists once it is switched on in the producing module's own section
(`[/settings/system/windows/diagnostics]`, `[/settings/system/unix/diagnostics]`,
`[/settings/eventlog/diagnostics]`, `[/settings/disk/diagnostics]`).

**What to do:** nothing on a default install. Before turning `capture
diagnostics` on, decide whether the roles holding `failures.list` may see
which processes and services run on the host and what its logs say, grant
`diagnostics.list` only to the roles that need the live tables, switch on only
the tables you want, and use `prune pattern` to leave out a table the fleet
server should not receive (`prune pattern = eventlog.errors`). To keep failed
checks off the fleet server altogether, set `failure context = false` under
`[/settings/fleet]`.
