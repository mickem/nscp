#### About `check_load` (Windows)

`check_load` reports Unix-style 1/5/15-minute load averages on Windows —
utilization tells you how busy the CPUs are, load tells you how much work is
*queued for* them, which is the saturation signal utilization alone cannot
give (100% CPU with an empty queue is a busy box; 100% with a deep queue is an
overloaded one).

Windows has no kernel-maintained load average, so the CheckSystem background
collector synthesises one: every second it folds the instantaneous value

```
load = processor queue length + busy cores
```

into three exponential moving averages (the Linux loadavg formula sampled at
1 Hz). The queue length is the PDH counter `\System\Processor Queue Length`
(threads ready to run but not running, system-wide) and busy cores is
`cores x CPU busy%` from the same tick. This reproduces Linux semantics —
running + runnable tasks — so a fully-busy 8-core box reads ~8.0 and a
saturated one reads above it, and the familiar threshold conventions
(`warn=load > <cores>`, or `percpu=true` with `warn=load > 1`) transfer as-is.

Keywords (a single aggregate row, matching the Linux `check_load`):

| Keyword         | Description                                                                     |
|-----------------|---------------------------------------------------------------------------------|
| `load1`         | Load average over the last 1 minute                                             |
| `load5`         | Load average over the last 5 minutes                                            |
| `load15`        | Load average over the last 15 minutes                                           |
| `load`          | The largest of the three (threshold "any window")                               |
| `type`          | `total`, or `scaled` with `percpu=true` (averages divided by the core count)    |
| `queue`         | Smoothed (1-minute) processor queue length alone — the pure saturation signal   |
| `procs_running` | Last tick's instantaneous runnable + running estimate                           |
| `procs_total`   | Total threads on the system (scheduling entities)                               |
| `cores`         | Logical processor count                                                         |
| `samples`       | Collector ticks folded into the averages                                        |

There are no default thresholds; the three averages are always emitted as perf
data (`total_load1` etc., `scaled_*` with `percpu=true`). `queue` is never
divided by `percpu` — it is an absolute thread count.

**Caveats:** the averages live in the collector, so the check reports
*"Load average data is not available yet"* right after service start. 
If the `\System\Processor Queue Length` counter is unavailable (corrupt perflib), 
the load degrades to the CPU-utilization component and a warning is logged. 
Some hypervisors report a small nonzero queue on idle guests — the smoothing 
absorbs the noise, but baseline before alerting tightly on `queue`. 
Load sampling can be turned off with `disable = load` in 
`/settings/system/windows` (the check then reports data-unavailable rather 
than zeros).
