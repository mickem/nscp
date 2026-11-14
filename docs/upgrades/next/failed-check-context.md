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
in the log marked *evicted*. An enrolled agent also reports failed checks to a
fleet server that asks for them: at most one report per
`[/settings/fleet] failure context interval` (default `1h`), carrying the
newest failure with its snapshot and a count of the others. Capture is off by
default, so nothing changes until you turn it on. Without it a fleet report
carries the failure and no metrics. Set `failure context = false` under
`[/settings/fleet]` to send no reports. See the
[REST reference](../api/rest/failures.md#metrics-at-the-time-of-the-failure),
[the fleet guide](fleet.md#report-failed-checks) and the
[security notice](../security/notices.md#failed-checks-can-carry-a-metrics-snapshot-and-are-reported-to-an-enrolled-fleet-server).
