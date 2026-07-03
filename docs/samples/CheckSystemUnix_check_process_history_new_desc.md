#### About `check_process_history_new`

`check_process_history_new` reports processes that were **first seen within a
recent time window** — i.e. processes that started (or first appeared to the
agent) recently. It is useful for spotting unexpected launches, flapping
services that keep restarting, or confirming that a scheduled job actually ran.

It relies on the background **process-history collector**, which must be enabled:

```ini
[/settings/system/unix]
process history = true
```

Until that is set the check returns **UNKNOWN** with
`Process history is not enabled (set 'process history = true' under /settings/system/unix)`.

Arguments and keywords:

| Name                | Description                                                       |
|---------------------|-------------------------------------------------------------------|
| `time` (arg)        | How far back "new" reaches, e.g. `30s`, `5m`, `1h` (default `5m`) |
| `exe`               | Executable path of the process                                    |
| `first_seen`        | Unix timestamp the process was first observed                     |
| `last_seen`         | Unix timestamp the process was last observed                      |
| `times_seen`        | How many times it has been observed running                       |
| `currently_running` | `1` when the process is still running                             |
| `count`             | Number of matching (new) processes                                |

There are no default thresholds; the empty result is `OK: No new processes
found.` Threshold on `count` to alert on *any* new process, or filter by `exe`
to watch for a specific program starting. See also the companion
`check_process_history` (full history rather than just recently-new).
