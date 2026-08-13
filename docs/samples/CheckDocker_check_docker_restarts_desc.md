#### About `check_docker_restarts`

`check_docker_restarts` detects crash-looping and OOM-killed containers via
the inspect endpoint (`RestartCount`, `State.OOMKilled`, `State.StartedAt`,
`State.ExitCode`). It always includes stopped containers: a crash-looping
container spends most of its time not-running, and a final crash leaves it
exited — both must stay visible.

Default thresholds encode the crash-loop signature: **warning** when
`restart_count > 3 and started < 15m and started >= 0` (many restarts *and* a
recent start — a container that has been up for a month is fine no matter how
bumpy its past), **critical** when `oom_killed = 1` (the last exit was an
out-of-memory kill).

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword           | Description                                                           |
|-------------------|-----------------------------------------------------------------------|
| `names`           | Container name(s), comma separated                                    |
| `image`           | Image the container was created from                                  |
| `container_state` | `created`, `restarting`, `running`, `removing`, `paused`, `exited`, `dead` |
| `restart_count`   | Restarts since the container was created                              |
| `started`         | Seconds since the last start, `-1` if it never started; supports units (`started < 10m`) |
| `exit_code`       | Exit code of the last exit (0 while running fine)                     |
| `oom_killed`      | `1` when the last exit was an out-of-memory kill, else `0`            |

`container=<name>` (repeatable) scopes the check to specific containers.
Note that `restart_count` is cumulative for the container's lifetime; that is
why the default warning also requires a recent start before it fires.
