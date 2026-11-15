---
icon: "📊"
modules: [CheckSystem, CheckSystemUnix, CheckDisk]
action: none
---
**New: pressure metrics - how much the system is waiting, not only how busy it is.**
`/api/v2/metrics` (and so `/api/v2/openmetrics`, the submitters and a failed
check's metrics snapshot) gains the figures that tell a slow host from a
merely busy one:

| Key | Platform | Type | Meaning |
|---|---|---|---|
| `system.pressure.processor_queue_length` | Windows | gauge | Threads ready to run and waiting for a processor, at the last 1 s sample (absent with `disable = load`). |
| `system.pressure.page_faults_per_sec` | Windows | gauge | Page faults per second, soft and hard. |
| `system.pressure.<resource>.some_avg10`, `_avg60`, `_avg300` | Linux | gauge, percent | Share of time at least one task was stalled on the resource - `cpu`, `memory` or `io`, also the `resource` label (pressure stall information); `full_*` for the time every non-idle task was. |
| `system.pressure.<resource>.some_stall_seconds`, `full_stall_seconds` | Linux | counter, seconds | Total stall time since boot. |
| `system.pressure.major_faults` | Linux | counter | Major page faults since boot. |
| `disk.io.<disk>.avg_queue_length` | Windows, Linux | gauge | Average number of requests outstanding on the disk over the interval, next to the instantaneous `queue_length`. |

The stall figures need a kernel with pressure stall information (Linux 4.20+,
not booted with `psi=0`); without it they are absent, not 0. A counter that
cannot be read is likewise left out. `prune metrics = zeros` keeps all of
these at 0, since "nothing was waiting" is the reading that rules a cause out.
No existing key moved: the default PDH counter `disk_queue_length`, which
reads `% Disk Time`, keeps its name. Nothing to do.
