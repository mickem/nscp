---
icon: "🆕 🔒"
modules: [core, WEBServer]
action: conditional
---
**New: what the system looked like when a check failed, on the host and on the fleet server.**
Turn on `capture metrics` under `[/settings/core/failed checks]` and every
failed check also keeps the agent's metrics snapshot from that moment (what
`/api/v2/metrics` was serving, at most one `metrics interval` old). The web
UI's **Failed checks** page shows it as *System at the time* when you open a
row, and `GET /api/v2/failures/<index>/context` serves it. Failures within one
interval share a snapshot, and `metrics context budget` (default 4 MiB) bounds
what is held: the oldest snapshots are released first, and their failures stay
in the log marked *evicted*. `prune metrics = zeros, empty, duplicates`
trims each snapshot before it is kept (zero values, empty strings, and
instances identical to another, such as `_Total` and the only disk) and
lists what it took out, and `prune pattern` leaves out every metric whose
key contains one of a list of text (`VirtualBox, Hyper-V Virtual`). An
enrolled agent also sends *failure context* to a fleet server that asks for
it: at most one report per `[/settings/fleet] failure context interval`
(default `1h`), carrying the newest failure with its snapshot and a count of
the others. It is not a feed of every failed check. Capture is off by
default, so nothing changes until you turn it on. Without it a fleet report
carries the failure and no metrics. Set `failure context = false` under
`[/settings/fleet]` to send no reports. See the
[REST reference](../api/rest/failures.md#metrics-at-the-time-of-the-failure),
[the fleet guide](fleet.md#send-failure-context) and the
[security notice](../security/notices.md#failed-checks-can-carry-a-metrics-snapshot-sent-as-failure-context-to-an-enrolled-fleet-server).
