#### Process owner and cross-agent portability keywords

For portability `check_process` also exposes:

| Keyword    | Description                                                                           |
|------------|---------------------------------------------------------------------------------------|
| `rss`      | Resident set size — a straight alias for `working_set` (same bytes / human value)     |
| `username` | Process owner as `DOMAIN\name` (empty unless `resolve-owner=true`)                    |
| `uid`      | Process owner SID string — the Windows analogue of a Unix uid (empty unless resolved) |
| `state`    | Accepts `running` as a synonym for `started` (the rendered value stays `started`)     |

**`resolve-owner`** (default `false`) turns on owner resolution: it reads each
matching process's token to populate `username`/`uid`. It is opt-in because
`LookupAccountSid` can block for seconds on domain / Azure-AD accounts. Scope the
check to specific processes when using it on a busy host.

```
check_process process=sqlservr.exe resolve-owner=true "crit=username not like 'NT SERVICE'" "detail-syntax=%(exe) owner=%(username)"
```

```
check_process process=nginx.exe "warn=state != 'running'" "crit=rss > 2G"
```

#### Showing only the top processes (sorting and limiting)

`check_process` does not sort or limit its output: every matching process is evaluated and
returned. To report only the few most interesting processes (for example the 10 biggest
memory consumers) wrap the check in
[`filter_perf`](../check/CheckHelpers.md#filter_perf), which post-processes the performance
data produced by a check, sorting it (`sort=normal`, biggest first) and limiting it
(`limit=N`).

For example, the top 10 processes by working set (RAM), excluding SQL Server:

```
filter_perf sort=normal limit=10 command=check_process arguments "filter=working_set > 0 and exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G" "detail-syntax=%(exe) ws=%(working_set)"
```

The same approach works for CPU usage. Pass `delta=true` so that `%(time)` (and
`%(kernel)` / `%(user)`) report CPU usage over a one second window as a whole
percentage of total CPU, instead of the cumulative CPU seconds since the process
started, for example the top 10 processes by CPU:

```
filter_perf sort=normal limit=10 command=check_process arguments delta=true "warn=time > 50" "crit=time > 90" "detail-syntax=%(exe) cpu=%(time)%"
```

Note that `limit` only trims the performance data; the warning/critical status is still
evaluated against every matching process, so an alert is raised even if the offending
process is not among the items shown.

#### `delta=true` and the per-process CPU collector

Unlike earlier releases, `delta=true` no longer samples, sleeps a second, then
samples again inside the check. Instead the CPU percentage is taken from a
background collector that diffs the system process table once a second, so the
check returns immediately with a always-fresh rolling one-second reading (and
memory/handle fields report their real absolute values, not a one-second change).

Because that collector is off by default, you must enable it once:

```ini
[/settings/system/windows]
process cpu = true
```

Until it is enabled, `check_process delta=true` returns `UNKNOWN` with a message
naming the setting (it fails fast on the flag, whether or not `time`/`kernel`/
`user` appear in the syntax) rather than reporting misleading numbers.
Cumulative CPU seconds (`delta` omitted) need no collector and are unaffected.
