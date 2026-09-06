#### Cross-agent portability keywords

`check_process` keeps a shared keyword vocabulary across the Windows and Linux
agents: `rss` is a straight alias for `working_set` (same bytes and human
value), and `state` accepts `running` as a synonym for `started` (the rendered
value stays `started`), so the same expressions work on both platforms.

#### Process owner (`username` / `uid`)

##### Windows

**`resolve-owner`** (default `false`) turns on owner resolution: it reads each
matching process's token to populate `username`/`uid`. It is opt-in because
`LookupAccountSid` can block for seconds on domain / Azure-AD accounts. Scope the
check to specific processes when using it on a busy host.

```
check_process process=sqlservr.exe resolve-owner=true "crit=username not like 'NT SERVICE'" "detail-syntax=%(exe) owner=%(username)"
check_process process=nginx.exe "warn=state != 'running'" "crit=rss > 2G"
```

##### Linux

`uid` is **always** populated — it is read out of a file the check already
opens, so it costs nothing — and it is numeric, so it can be thresholded
directly:

```
check_process process=* "crit=uid = 0 and working_set > 1G" "detail-syntax=%(exe) uid=%(uid) ws=%(working_set)"
```

```
check_process process=* "filter=uid >= 1000" "warn=count > 200" "top-syntax=%(status): %(count) user processes"
```

**`resolve-owner`** (default `false`) additionally turns each uid into a user
name. It is opt-in because the lookup goes through NSS, which can block for
seconds when it is backed by a remote directory (LDAP/SSSD) — the same reason
the Windows check gates owner resolution. Names are cached per uid for the
lifetime of the agent, so the cost is one lookup per *distinct* owner, not per
process.

```
check_process process=postgres resolve-owner=true "crit=username != 'postgres'" "detail-syntax=%(exe) owner=%(username)"
```

#### Process state: `state` vs `proc_state` (Linux)

Two different questions, two keywords: `state` is the cross-platform
started/stopped verdict (also available on Windows), `proc_state` the raw Linux
scheduler state.

`state` answers "is this process there and alive"; a zombie reports `stopped`
there. `proc_state` answers "what is it *doing*", which is what the two classic
Linux alerts need:

```
check_process process=* "crit=proc_state = 'zombie'" "detail-syntax=%(exe) (pid %(pid)) is a zombie"
```

```
check_process process=* "warn=proc_state = 'disk_sleep'" "detail-syntax=%(exe) is blocked in uninterruptible I/O"
```

`disk_sleep` (`D`) is the useful I/O-hang signal: a process stuck there cannot
be killed and usually means a wedged disk or an unresponsive NFS mount.

For readability the parser also accepts `uninterruptible` for `disk_sleep`,
`defunct` for `zombie` and `traced` for `tracing_stop`.

Note that `proc_state = 'stopped'` means SIGSTOP'd / job-control stopped (`T`),
which is **not** the same as `state = 'stopped'` (process not running).

#### `ppid` (Linux)

The parent process id, so process trees can be expressed:

```
check_process process=* "filter=ppid = 1" "warn=count < 10" "top-syntax=%(status): %(count) processes reparented to init"
```

It is also how kernel threads are excluded: on a standard Linux kernel every
kernel thread is a child of `kthreadd`, which is pid 2, so

```
check_process process=* "filter=ppid != 2 and pid != 2" "warn=count > 500"
```

drops them all. (Note that some environments — WSL, and some container
runtimes — do not run a `kthreadd` at pid 2, so check what pid 2 is on the host
before relying on this.)

#### `elapsed` and `rss` (Linux)

`elapsed` is the "has this been running long enough / too long" check, which
`creation` (an absolute timestamp) makes awkward:

```
check_process process=my-batch-job "crit=elapsed > 3600" "detail-syntax=%(exe) has been running for %(elapsed)s"
```

```
check_process process=nginx "crit=elapsed < 300" "top-syntax=%(status): nginx restarted recently"
```

```
check_process process=* "crit=rss > 2G" "detail-syntax=%(exe) rss=%(rss)"
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

#### `delta=true` and the per-process CPU collector (Windows)

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
