#### Filtering by account SID (`user` / `sid`)

Each event carries the security identifier (SID) of the account associated with
it. The `user` keyword (alias `sid`) exposes that SID as a string (e.g.
`S-1-5-18` for `LOCAL SYSTEM`), so you can scope a check to a specific account.

```
check_eventlog file=Security "filter=user = 'S-1-5-18'" "detail-syntax=${id}: ${message}"
```

`user`/`sid` is populated by the modern Windows Event Log API
(`EvtSystemUserID`). On the legacy `ReadEventLog` API (pre-Vista, or when the
modern API is unavailable) it renders as an empty string.

#### "Since last check" scanning

To avoid re-scanning and double-counting the same events on every poll, use the
`bookmark` option. With `bookmark=auto` (or a shared bookmark name) the check
resumes after the last event it saw, rather than re-walking the whole
`scan-range` window each time:

```
check_eventlog file=Application bookmark=auto "filter=level = 'error'"
```

The bookmark position is persisted across restarts (stored under
`eventlog.bookmarks`), so a monitoring cycle only ever reports genuinely new
events. Without a bookmark the check falls back to the time-window `scan-range`
(default `-24h`), which is stateless and may re-report events inside the window.
