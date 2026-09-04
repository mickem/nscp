#### About `check_process_history`

`check_process_history` reports the processes NSClient++ has seen running since
the agent started — including ones that have long since exited. It answers the
question a point-in-time `check_process` cannot: *did this ever run?*

One record is returned per distinct executable, carrying `exe`, whether it is
`running` right now, `first_seen` / `last_seen` timestamps and `times_seen`
(how many collector ticks observed it).

##### It must be turned on first

The history is kept by the CheckSystem background collector, which does **not**
track it by default. Enable it once:

```ini
[/settings/system/windows]
process history = true
```

(`[/settings/system/unix]` on Linux.) Until it is enabled the history is empty
and the check reports zero processes rather than an error, which is easy to
mistake for "nothing ran".

##### The window is the agent's uptime

History lives in memory and starts empty when the agent starts. A restarted
agent has no history, and there is no persistence across restarts — so
`first_seen` means "first seen since this agent process started", not "first
seen on this machine". Read the results alongside
[`check_uptime`](#check_uptime) or `check_nscp`'s `uptime` when the window
matters.

##### What it is good for

The typical uses are verifying that scheduled work actually ran, and catching
things that ran when they should not have:

```
check_process_history process=backup.exe "crit=times_seen = 0"
check_process_history "crit=exe like 'psexec'" "top-syntax=${status}: ${problem_list}"
```

`process=` restricts the check to specific executable names (repeatable,
case-insensitive); with no `process=` every process in the history is reported.
There are no default thresholds, and the default empty state is OK.

For the narrower question "what appeared *recently*", use
[`check_process_history_new`](#check_process_history_new), which takes a `time=`
window and reports only processes first seen inside it.
