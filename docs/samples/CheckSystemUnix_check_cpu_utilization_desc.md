#### About `check_cpu_utilization`

`check_cpu_utilization` reports how the CPU's time is being spent, broken down
by mode. It reads the aggregate `cpu` line from `/proc/stat`, waits ~1 second,
reads it again, and reports the delta as percentages — so it measures live
utilization over that sampling window rather than since boot.

All numeric keywords are percentages (0–100).

Default thresholds: **warning** `total > 90`, **critical** `total > 95`. This
differs from [`check_cpu`](#check_cpu), which averages utilization over rolling
time windows (`1m`/`5m`/`15m`) from the background collector; `check_cpu_utilization`
takes a single fresh 1-second sample and exposes the per-mode breakdown, which
is what you want to distinguish user vs. `iowait` vs. `steal` pressure.
