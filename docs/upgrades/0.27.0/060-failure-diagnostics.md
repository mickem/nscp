---
icon: "🆕 🔒"
modules: [core, WEBServer, CheckSystem, CheckSystemUnix, CheckEventLog, CheckDisk]
action: conditional
---
**New: diagnostics - the top processes, the services in trouble, the last log errors, the size of the usual growth spots and the last failures, kept with each failed check.**
Turn on `capture diagnostics` under `[/settings/core/failed checks]` and the
core collects small tables of what is going on - by default with every
metrics round, or every `[/settings/core] diagnostics interval` - and keeps the
latest with each failed check, beside the metrics snapshot. It always holds
`failures.recent` (the last failed checks, newest first). Each other table is
switched on in the module that produces it:

| Table | Windows | Linux |
|---|---|---|
| `processes.top_cpu`, `processes.top_memory` | `processes = true` under `[/settings/system/windows/diagnostics]` | `processes = true` under `[/settings/system/unix/diagnostics]` |
| `services.problems` | `services = true` under `[/settings/system/windows/diagnostics]` | `services = true` under `[/settings/system/unix/diagnostics]` |
| `services.events` | `services = true` under `[/settings/eventlog/diagnostics]` | (the same `services` key) |
| `eventlog.errors` | `errors = true` under `[/settings/eventlog/diagnostics]` | `errors = true` under `[/settings/system/unix/diagnostics]` |
| `disk.growth_spots` | `growth spots = true` under `[/settings/disk/diagnostics]` | (the same key) |

The process tables hold name, pid, CPU share, working set and private bytes,
never a command line; `services.problems` the services that are not running
as they should, with their exit code, executable (never its arguments),
account, start time and - on Linux - restart count; `services.events` the
service manager's crash and restart events; `eventlog.errors` the last
critical and error events of the Application and System logs, or the
journal's entries at priority `err` or worse; `disk.growth_spots` the size of
the temp and log directories, the page and swap files and (Windows) the
shadow-copy storage in use, with a note when a time limit or an unreadable
folder cut a size short. Enabling a table turns on what
it needs: the per-process sampling, a 30-second service refresh, an event log
subscription of its own (separate from the real-time filters), or - on Linux,
where the service and log tables need systemd - `systemctl` and `journalctl`
on a thread of their own, or a walker thread that measures the growth spots
every `growth spots interval` (10 minutes) at background priority. The event tables start from the last ten minutes of
the logs. `diagnostics rows` (default 10) caps each table, and diagnostics
snapshots share `metrics context budget` and `max context size` with the
metrics ones. `GET /api/v2/failures/<index>/context` gains a `diagnostics`
object, a fleet failure-context report carries one beside `context`, `nscp
test` gains a `diagnostics [section]` command, and `GET /api/v2/diagnostics`
serves the latest snapshot live. In the web UI, an opened entry on the
**Failed checks** page shows *Diagnostics at the time* under the metrics, one
table per section, and a new **Diagnostics** page shows the live tables and
the switches that turn capture and each table on.
Nothing changes until you turn capture on.
**Custom roles:** the live routes need the new `diagnostics.list` grant, which
no built-in role holds other than `full`. See the
[REST reference](../api/rest/diagnostics.md), the
[fleet guide](fleet.md#send-failure-context) and the
[security notice](../security/notices.md#diagnostics-tables-expose-processes-services-and-log-messages-over-rest-and-with-failure-context).
