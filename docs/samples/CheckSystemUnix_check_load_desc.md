#### About `check_load`

`check_load` reports the Linux system load average from `/proc/loadavg` — the
1-, 5- and 15-minute run-queue averages. With `percpu=true` each figure is
divided by the number of CPUs so thresholds port across hosts with different
core counts (the row's `type` then reads `scaled` instead of `total`).

There are **no default thresholds** — a bare `check_load` always returns OK and
just reports the numbers. Supply `warning=` / `critical=` (typically on `load`,
or on `load5`/`load15` to ignore momentary spikes). `load1`/`load5`/`load15` are
emitted as performance data by default.
