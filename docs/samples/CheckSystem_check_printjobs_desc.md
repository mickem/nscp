#### About `check_printjobs`

`check_printjobs` reports the **individual jobs** sitting in the Windows
spooler — one row per job — from `Win32_PrintJob`. Where
[`check_printqueue`](#check_printqueue) tells you *that* a queue is backed up,
this tells you *what* is stuck in it: which document, whose it is, how big it
is, how long it has been waiting and what the spooler says about it.

Units in thresholds:

- `age` takes durations — `age > 30m`, `age > 2h` — and a bare number still
  means seconds.
- `size` takes byte units — `size > 500M`, `size > 2G`. A **bare number is
  rejected** for size keywords, so write `size > 1K` rather than `size > 1024`.

Defaults: **CRITICAL** when `error = 1 or blocked = 1 or user_intervention = 1`
— the three states the spooler cannot get out of by itself — and **WARNING**
when `age > 600` (ten minutes). A paused job is deliberately *not* critical:
someone paused it on purpose. empty-state is **OK**, because an empty spooler is
the normal state; the check then reports "No print jobs queued" and still emits
`count` perfdata so queue depth can be graphed.

Perfdata is keyed `<printer>_<job id>`, so labels change as jobs come and go.
That is fine for alerting; for graphing prefer the always-present `count`, or
`check_printqueue`'s per-printer `jobs` series. **Windows only.**
