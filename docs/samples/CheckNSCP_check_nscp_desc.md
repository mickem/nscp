#### About `check_nscp`

`check_nscp` reports on the health of the agent itself: whether it has crashed,
whether it has logged errors, and how long it has been running. It is the check
you point at NSClient++ to answer "is my monitoring agent healthy?", as opposed
to `check_nscp_version` (which version is this?) and `check_nscp_update` (is a
newer one available?).

It is a normal filter check, so the usual `filter` / `warning` / `critical`,
`top-syntax` / `detail-syntax` and `perf-config` options all apply. Exactly one
object is fed to the filter — the agent — so `${list}` is a single line.

The default thresholds keep the historical verdict: **any** crash report or
**any** logged error makes the agent CRITICAL.

```
check_nscp
OK: 0 crash(es), 0 error(s), uptime 0
```

#### What the keywords are measured against

| Keyword | Where it comes from |
|---------|---------------------|
| `crashes`, `last_crash`, `crash_age` | The crash archive folder, i.e. the `archive folder` key in `[/settings/crash]`. Files ending in `.crash`, `.dmp` or `.txt` count as crash reports. |
| `errors`, `last_error` | Messages logged at ERROR or CRITICAL level, counted since the module was loaded. The counter is not reset by a restart of the check, only by a restart of the agent. |
| `uptime` | Time since the CheckNSCP module was loaded, which for a normally configured agent is the agent's own uptime. |
| `version`, `date` | The running build, same values `check_nscp_version` reports. |

The three crash-report extensions are historical. 0.6.10 and later write one
plain-text `<timestamp>.crash` per crash, naming the exception, the faulting
address and the module it landed in. Up to 0.6.9 the agent used Google
Breakpad, which left a `<guid>.dmp` minidump and a `<guid>.dmp.txt` description
behind; breakpad was dropped because its vendored submodule and build machinery
had become a dependency burden. All three extensions are counted, so an archive
that predates the change still reports correctly.

`uptime` and `crash_age` accept units in thresholds, so you can write
`crit=uptime < 5m` or `warn=crash_age < 7d` rather than converting to seconds
yourself. Rendered through `${uptime}` / `${crash_age}` they come out as a
human-readable duration whose largest unit is controlled by `max-unit`.

#### Crash reports are a Windows concept

NSClient++ only archives crash reports on Windows — there is no crash handler in
the Unix builds. On Linux the crash archive folder therefore stays empty and
`crashes` is always `0`; that is the documented result, not a failure. The
`errors` and `uptime` keywords work identically on both platforms.

#### Alerting on a crash that has since been cleaned up

`crashes` counts every report still in the archive folder, so an agent that
crashed a year ago and was never tidied up stays CRITICAL forever. To alert only
on a *recent* crash, threshold on `crash_age` instead and let the count alone go
unremarked:

```
check_nscp "crit=crash_age < 7d" "warn=errors > 0"
```

`crash_age` is `none` when the archive holds no report at all, and every numeric
comparison against `none` is false — so the expression above is quietly OK on an
agent that has never crashed.
