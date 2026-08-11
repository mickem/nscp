#### About `check_kernel_stats` (Windows)

`check_kernel_stats` reports system-wide kernel activity from the PDH `System`
counter set — the Windows counterpart to the unix `check_kernel_stats`
(`/proc/stat`). The rate counters are sampled over a 1-second window.

It emits one row per metric, selected with `type=` (repeatable; default all):

| Row (`name`) | Counter              | Kind  | Description                                     |
|--------------|----------------------|-------|--------------------------------------------------|
| `ctxt`       | Context Switches/sec | rate  | Scheduler churn; storms indicate lock contention |
| `syscalls`   | System Calls/sec     | rate  | Kernel-transition rate (Windows only)            |
| `processes`  | Processes            | gauge | Current process count                            |
| `threads`    | Threads              | gauge | Current thread count                             |

Row keywords match the unix check: `name`, `label`, `human`, `rate` (perf,
0 for the gauge rows) and `current` (perf; the gauge value, or the *rounded
rate* for the rate rows — Windows exposes no cumulative counter).

**Platform differences:** unix's `processes` row is a fork *rate*
(creations/sec from `/proc/stat`); Windows has no process-creation-rate counter
in this set, so its `processes` row is a *gauge* (current count). Windows adds
the `syscalls` row; unix does not have it. `Processor Queue Length` and
`System Up Time` from the same counter set are deliberately not duplicated
here — `check_load` and `check_uptime` own those.

The default thresholds are the same thread-count guardrails as the unix check:
`warn = name = 'threads' and current > 8000`,
`crit = name = 'threads' and current > 10000`. Override them (`warn=none`) or
threshold the rates explicitly, e.g. `crit=name = 'ctxt' and rate > 500000` —
context-switch storms are workload-relative, so baseline before pinning.
