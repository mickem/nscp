#### About `check_process_history_new`

`check_process_history_new` reports processes that were **first seen within a
recent time window** — i.e. processes that started (or first appeared to the
agent) recently. It is useful for spotting unexpected launches, flapping
services that keep restarting, or confirming that a scheduled job actually ran.

It relies on the background **process-history collector**, which must be enabled:

```ini
[/settings/system/windows]
process history = true
```

(`[/settings/system/unix]` on Linux.) Until that is set the check returns
**UNKNOWN** and names the setting, quoting the module's own settings path — so
the message reads `/settings/system/windows` on Windows and
`/settings/system/unix` on Linux:

```
check_process_history_new
UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/windows)
```

`time=` sets how far back "new" reaches — `30s`, `5m`, `1h` — and defaults to
`5m`.

There are no default thresholds; the empty result is `OK: No new processes
found.` Threshold on `count` to alert on *any* new process, or filter by `exe`
to watch for a specific program starting. See also the companion
`check_process_history` (full history rather than just recently-new).
