#### About `check_load`

`check_load` reports 1/5/15-minute load averages —
utilization tells you how busy the CPUs are, load tells you how much work is
*queued for* them, which is the saturation signal utilization alone cannot
give (100% CPU with an empty queue is a busy box; 100% with a deep queue is an
overloaded one).

##### Linux

The averages come straight from `/proc/loadavg` — the kernel's own 1-, 5-
and 15-minute run-queue averages.

##### Windows

Windows has no kernel-maintained load average, so the CheckSystem background
collector synthesises one: every second it folds the instantaneous value

```
load = processor queue length + busy cores
```

into three exponential moving averages (the Linux loadavg formula sampled at
1 Hz). Each fold decays over the interval actually measured rather than an
assumed second, so the averages stay correct when a collector tick overruns
the 1-second cadence — which is exactly what happens on the loaded hosts this
check exists for. The queue length is the PDH counter `\System\Processor Queue Length`
(threads ready to run but not running, system-wide) and busy cores is
`cores x CPU busy%` from the same tick. This reproduces Linux semantics —
running + runnable tasks — so a fully-busy 8-core box reads ~8.0 and a
saturated one reads above it, and the familiar threshold conventions
(`warn=load > <cores>`, or `percpu=true` with `warn=load > 1`) transfer as-is.

##### Common behaviour

The check returns a single aggregate row and the keyword vocabulary is identical
on both platforms, so warning/critical expressions and detail-syntax port
between them. With `percpu=true` each figure is divided by the number of CPUs so
thresholds port across hosts with different core counts (the row's `type` then
reads `scaled` instead of `total`).

There are no default thresholds; the three averages are always emitted as perf
data (`total_load1` etc., `scaled_*` with `percpu=true`). `queue` is never
divided by `percpu` — it is an absolute thread count.

**Windows caveats:** the averages live in the collector, so the check reports
*"Load average data is not available yet"* right after service start. 
If the `\System\Processor Queue Length` counter is unavailable (corrupt perflib), 
the load degrades to the CPU-utilization component and a warning is logged. 
Some hypervisors report a small nonzero queue on idle guests — the smoothing 
absorbs the noise, but baseline before alerting tightly on `queue`. 
Load sampling can be turned off with `disable = load` in 
`/settings/system/windows` (the check then reports data-unavailable rather 
than zeros).
