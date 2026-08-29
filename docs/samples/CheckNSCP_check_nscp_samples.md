**Check that the agent is healthy**

With no arguments the defaults apply: any crash report or any logged error is
CRITICAL.

```
check_nscp
OK: 0 crash(es), 0 error(s), uptime 4d 02:17|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0
```

**An agent that has crashed**

```
check_nscp
CRITICAL: 2 crash(es), 0 error(s), uptime 03:12|'nscp_crashes'=2;0;0 'nscp_errors'=0;0;0
```

**Name the crash report and how old it is**

```
check_nscp "detail-syntax=last=${last_crash} age=${crash_age}"
CRITICAL: last=2026-01-02-08-15-31.crash age=0:31|'nscp_crashes'=2;0;0 'nscp_errors'=0;0;0
```

**Only alert on a recent crash**

`crash_age` accepts units, so the window is written the way you think about it.
An agent that crashed half an hour ago trips a 7-day window:

```
check_nscp "crit=crash_age < 7d"
CRITICAL: 2 crash(es), 0 error(s), uptime 03:12|'nscp_crash_age'=1878s;0;604800
```

…and one that has never crashed does not, because every numeric comparison
against `none` is false. There is no value to graph either, so the check emits
no performance data at all:

```
check_nscp "crit=crash_age < 7d"
OK: 0 crash(es), 0 error(s), uptime 4d 02:17
```

**Warn before the crash archive piles up**

A threshold on a keyword also produces that keyword's performance data, so this
graphs the crash count with both thresholds attached — and nothing else.

```
check_nscp "warn=crashes > 5" "crit=crashes > 10"
OK: 0 crash(es), 0 error(s), uptime 4d 02:17|'nscp_crashes'=0;5;10
```

**Alert on a freshly restarted agent**

`uptime` takes units too. The default `critical` still applies here, so the
crash and error metrics come along with the uptime one.

```
check_nscp "warn=uptime < 5m"
WARNING: 0 crash(es), 0 error(s), uptime 42s|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0 'nscp_uptime'=42s;300;0
```

**Change the granularity of the rendered durations**

`max-unit` controls the largest unit `${uptime}` and `${crash_age}` use.

```
check_nscp "max-unit=h" "detail-syntax=up=${uptime}"
OK: up=98:17|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0
```

**Report the last error the agent logged**

`errors` counts messages logged at ERROR or CRITICAL level since the agent
started; `${last_error}` is the most recent of them.

```
check_nscp "detail-syntax=${errors} error(s): ${last_error}"
CRITICAL: 1 error(s): Failed to load module: CheckWMI|'nscp_crashes'=0;0;0 'nscp_errors'=1;0;0
```

**Custom top-level output**

```
check_nscp "top-syntax=agent is ${status}"
agent is OK|'nscp_crashes'=0;0;0 'nscp_errors'=0;0;0
```
