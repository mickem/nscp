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
trims each snapshot before it is kept (zero values, empty strings, values
that restate others such as memory `avail` and `%` next to `used` and
`total`, and instances identical to another, such as `_Total` and the only
disk) and
lists what it took out, and `prune pattern` leaves out every metric whose
key contains one of a list of text (`VirtualBox, Hyper-V Virtual`). An
enrolled agent also sends *failure context* to a fleet server that asks for
it: at most one report per `[/settings/fleet] failure context interval`
(default `1h`), carrying the newest failure with its snapshot and a count of
the others. It is there to enrich an alert your monitoring system already
holds, not to be a feed of every failed check, and nothing is sent for a
failure that has no snapshot. Capture is off by default, so nothing changes
and nothing is sent until you turn it on. Set `failure context = false` under
`[/settings/fleet]` to send no reports. See the
[REST reference](../api/rest/failures.md#metrics-at-the-time-of-the-failure),
[the fleet guide](fleet.md#send-failure-context) and the
[security notice](../security/notices.md#failed-checks-can-carry-a-metrics-snapshot-sent-as-failure-context-to-an-enrolled-fleet-server).
