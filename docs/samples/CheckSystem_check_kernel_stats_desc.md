#### About `check_kernel_stats`

`check_kernel_stats` reports system-wide kernel activity as one row per metric.
The rate counters are sampled over a 1-second window. Use `type=` (repeatable)
to restrict which rows are returned; the default is all of them.

Row keywords are the same on both platforms — `name`, `label`, `human`, `rate`
(perf; `0` for the gauge rows) and `current` (perf) — but **`current` does not
mean the same thing on both.** On Windows it is the gauge value, or the rounded
per-second rate for a rate row; on Linux it is the raw cumulative counter read
from `/proc/stat` for a rate row, which only ever grows.

Threshold on `rate` when you mean a per-second value: a threshold written
against `current` from the Windows reading fires permanently on Linux, where the
same keyword is a counter in the millions.

The default thresholds are thread-count guardrails on both platforms:
`warn = name = 'threads' and current > 8000`,
`crit = name = 'threads' and current > 10000`. Override them (`warn=none`) or
threshold the rates explicitly, e.g. `crit=name = 'ctxt' and rate > 500000` —
context-switch storms are workload-relative, so baseline before pinning.

##### Windows

Sourced from the PDH `System` counter set.

| Row (`name`) | Counter              | Kind  | Description                                      |
|--------------|----------------------|-------|--------------------------------------------------|
| `ctxt`       | Context Switches/sec | rate  | Scheduler churn; storms indicate lock contention |
| `syscalls`   | System Calls/sec     | rate  | Kernel-transition rate (Windows only)            |
| `processes`  | Processes            | gauge | Current process count                            |
| `threads`    | Threads              | gauge | Current thread count                             |

Windows exposes no cumulative counter here, so `current` on the rate rows is
the rounded rate rather than a running total. `Processor Queue Length` and
`System Up Time` from the same counter set are deliberately not duplicated —
`check_load` and `check_uptime` own those.

##### Linux

| Row (`name`) | Source                | Kind  | Description                            |
|--------------|-----------------------|-------|----------------------------------------|
| `ctxt`       | `/proc/stat`          | rate  | Context switches per second            |
| `processes`  | `/proc/stat`          | rate  | Process/fork creations per second      |
| `threads`    | `/proc/*/task`        | gauge | Live thread count (instantaneous)      |

##### Platform differences

Linux's `processes` row is a fork *rate*; Windows has no process-creation-rate
counter in this set, so its `processes` row is a *gauge* (current count).
Windows adds the `syscalls` row, which Linux does not have.
