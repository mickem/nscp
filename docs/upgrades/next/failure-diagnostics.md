---
icon: "🆕 🔒"
modules: [core, WEBServer, CheckSystem, CheckSystemUnix]
action: conditional
---
**New: diagnostics - the top processes and the last failures, kept with each failed check.**
Turn on `capture diagnostics` under `[/settings/core/failed checks]` and the
core collects small tables of what is going on - by default with every
metrics round, or every `[/settings/core] diagnostics interval` - and keeps the
latest with each failed check, beside the metrics snapshot. It always holds
`failures.recent` (the last failed checks, newest first); set `processes =
true` under `[/settings/system/windows/diagnostics]` (CheckSystem) or
`[/settings/system/unix/diagnostics]` (CheckSystemUnix) to add
`processes.top_cpu` and `processes.top_memory`: name, pid, CPU share, working
set and private bytes, never a command line. Enabling the tables turns on the
per-process sampling they need. `diagnostics rows` (default 10) caps each
table, and diagnostics snapshots share `metrics context budget` and `max
context size` with the metrics ones. `GET /api/v2/failures/<index>/context`
gains a `diagnostics` object, a fleet failure-context report carries one beside
`context`, `nscp test` gains a `diagnostics [section]` command, and
`GET /api/v2/diagnostics` serves the latest snapshot live. Nothing changes until
you turn capture on. **Custom roles:** the live routes need the new
`diagnostics.list` grant, which no built-in role holds other than `full`. See
the [REST reference](../api/rest/diagnostics.md), the
[fleet guide](fleet.md#send-failure-context) and the
[security notice](../security/notices.md#diagnostics-tables-expose-running-processes-over-rest-and-with-failure-context).
