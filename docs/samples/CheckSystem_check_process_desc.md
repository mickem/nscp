#### Showing only the top processes (sorting and limiting)

`check_process` does not sort or limit its output: every matching process is evaluated and
returned. To report only the few most interesting processes (for example the 10 biggest CPU
or memory consumers) wrap the check in
[`filter_perf`](../check/CheckHelpers.md#filter_perf), which post-processes the performance
data produced by a check, sorting it (`sort=normal`, biggest first) and limiting it
(`limit=N`).

For example, the top 10 processes by working set (RAM), excluding SQL Server:

```
filter_perf sort=normal limit=10 command=check_process arguments "filter=working_set > 0 and exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G" "detail-syntax=%(exe) ws=%(working_set)"
```

...and the top 10 by CPU usage (see `--delta` below):

```
filter_perf sort=normal limit=10 command=check_process arguments --delta "filter=time > 0" "warn=time > 50" "crit=time > 80" "detail-syntax=%(exe) cpu=%(time)%"
```

Note that `limit` only trims the performance data; the warning/critical status is still
evaluated against every matching process, so an alert is raised even if the offending
process is not among the items shown.

#### Measuring CPU usage (`--delta`)

By default the `kernel`, `user` and `time` keywords report the *accumulated* CPU time each
process has used since it started, in seconds. To measure *current* CPU usage instead, pass
`--delta`: the check samples the CPU times, waits one second, samples again and reports the
difference.

In `--delta` mode these three keywords are no longer seconds but a **percentage of the total
CPU capacity of the whole machine** (the sum of all cores / vCPUs). As a consequence the same
load reads differently depending on how many cores the machine has: a single-threaded process
saturating one core shows roughly `100 / number_of_cores` percent — about 50% on a 2-vCPU
machine but only ~8% on a 12-vCPU machine. Keep this in mind when choosing thresholds,
especially when the same threshold is reused across machines with different core counts.
