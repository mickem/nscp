#### About `check_load`

`check_load` reports the Linux system load average from `/proc/loadavg` — the
1-, 5- and 15-minute run-queue averages. With `percpu=true` each figure is
divided by the number of CPUs so thresholds port across hosts with different
core counts (the row's `type` then reads `scaled` instead of `total`).

Keywords:

| Keyword         | Description                                              |
|-----------------|---------------------------------------------------------|
| `load1`         | 1-minute load average                                   |
| `load5`         | 5-minute load average                                   |
| `load15`        | 15-minute load average                                  |
| `load`          | The largest of `load1`/`load5`/`load15` (convenience)   |
| `procs_running` | Number of currently runnable kernel scheduling entities |
| `procs_total`   | Total number of scheduling entities                     |
| `type`          | `total`, or `scaled` when `percpu=true`                 |

There are **no default thresholds** — a bare `check_load` always returns OK and
just reports the numbers. Supply `warning=` / `critical=` (typically on `load`,
or on `load5`/`load15` to ignore momentary spikes). `load1`/`load5`/`load15` are
emitted as performance data by default.
