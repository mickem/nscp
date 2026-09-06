**Before the history is turned on:**

The history is kept by the CheckSystem background collector, which is off by
default.

```
check_process_history
UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/unix)
```

Enable it once (`[/settings/system/windows]` on Windows):

```ini
[/settings/system/unix]
process history = true
```

**Default check (an inventory of everything seen since the agent started):**

There are no default thresholds, so a bare call is always OK and reports the
count.

```
check_process_history
OK: 91 processes in history.
```

**Restrict to specific executables (`process=`, repeatable, case-insensitive):**

```
check_process_history process=nscp process=make
OK: 2 processes in history.
```

**Show the detail for a process:**

The default `top-syntax` renders only the *problem* list, so with no threshold
set you get the OK summary. Ask for the full list to see the per-process detail.

```
check_process_history process=nscp "top-syntax=${status}: ${list}" "detail-syntax=${exe} running=${running} seen=${times_seen}"
OK: nscp running=true seen=1
```

`first_seen` / `last_seen` are timestamps and support date comparisons:

```
check_process_history process=nscp "detail-syntax=${exe} running=${running} seen=${times_seen} first=${first_seen}" show-all
OK: nscp running=true seen=1 first=2026-09-04 12:56:43
```

**Count only what is still running:**

```
check_process_history "filter=running = 'true'" "top-syntax=${status}: ${count} still running"
OK: 79 still running
```

**Alert on something that should never have run:**

Note that `like` is a substring match, so a short pattern matches more than you
expect — here `'nc'` matches a kernel worker thread.

```
check_process_history "crit=exe like 'nc'" "top-syntax=${status}: ${problem_list}"
CRITICAL: kworker/R-sync_wq (true)
```

Anchor the pattern or use `=` for an exact name when you mean one binary.

**Verify that a scheduled job actually ran:**

```
check_process_history process=backup.exe "crit=times_seen = 0"
CRITICAL: backup.exe (false)
```

**The window is the agent's uptime:**

History lives in memory and starts empty at agent start, so `first_seen` means
"first seen since this agent process started", not "first seen on this machine".
A restarted agent reports an empty history:

```
check_process_history
OK: 0 processes in history.
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_process_history --arguments "process=backup.exe" --arguments "crit=times_seen = 0"
OK: 1 processes in history.
```
