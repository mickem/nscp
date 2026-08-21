#### Process owner (`username` / `uid`)

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

#### Process state: `state` vs `proc_state`

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

#### `ppid`

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

#### `elapsed` and `rss`

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
