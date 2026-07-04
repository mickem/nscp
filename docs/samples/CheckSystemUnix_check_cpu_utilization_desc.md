#### About `check_cpu_utilization`

`check_cpu_utilization` reports how the CPU's time is being spent, broken down
by mode. It reads the aggregate `cpu` line from `/proc/stat`, waits ~1 second,
reads it again, and reports the delta as percentages — so it measures live
utilization over that sampling window rather than since boot.

Keywords (all whole-percent, 0–100):

| Keyword    | Description                                                   |
|------------|--------------------------------------------------------------|
| `user`     | Time in user space (incl. nice)                              |
| `system`   | Time in the kernel                                          |
| `iowait`   | Time waiting for I/O to complete                            |
| `irq`      | Time servicing hardware interrupts                         |
| `softirq`  | Time servicing soft interrupts                             |
| `steal`    | Time stolen by the hypervisor (VM guests)                  |
| `guest`    | Time running a guest under this kernel                     |
| `idle`     | Idle time                                                  |
| `total`    | Overall busy percentage (`100 − idle − iowait`)            |

Default thresholds: **warning** `total > 90`, **critical** `total > 95`. This
differs from [`check_cpu`](#check_cpu), which averages utilization over rolling
time windows (`1m`/`5m`/`15m`) from the background collector; `check_cpu_utilization`
takes a single fresh 1-second sample and exposes the per-mode breakdown, which
is what you want to distinguish user vs. `iowait` vs. `steal` pressure.
