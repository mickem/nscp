---
icon: "📊"
modules: [CheckDisk]
action: none
---
**New: how much each volume grew over the last hour and day, as metrics and as `check_drivesize` keywords.**
The CheckDisk collector now keeps a small in-memory history of used space per
volume (one sample a minute for an hour, one an hour for a day) and publishes
the change:

| Where | Name | Meaning |
|---|---|---|
| `/api/v2/metrics` (gauge, bytes, `drive` label) | `disk.free.<drive>.used_change_1h`, `disk.free.<drive>.used_change_24h` | Used bytes now minus those of the newest sample at least an hour (a day) old - negative when space was freed. |
| `check_drivesize` keyword | `used_change_1h`, `used_change_24h` | The same values. Thresholds take size units (`warn=used_change_1h > 2G`); they render with a sign (`+1.5GB`) and emit perfdata in bytes. |

Neither exists until there is a sample a full window old: an hour (a day)
after the agent started, or after the volume appeared or changed size. Until
then the metrics are absent rather than 0, and the keywords render `unknown`,
fire no threshold and emit no perfdata, like `rate`; a one-shot `nscp client`
query never has them. A settings reload keeps the history, a restart starts it
over. The hour covers 60 to 70 minutes and the day 24 to 26 hours; a longer
gap in the collector's sampling makes them absent again instead of stretching
them.
`prune metrics = zeros` keeps them at 0, since "not growing" rules a volume
out. No existing key moved. Nothing to do.
