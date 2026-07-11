Reports per-CPU-socket frequency and load, sourced from the `Win32_Processor`
WMI class (one instance per physical socket).

| Keyword | Description |
|---|---|
| `name` | CPU name / model string. |
| `socket_id` | Socket device id (e.g. `CPU0`); use to filter a single socket. |
| `socket` | Socket designation (e.g. `CPU 1`). |
| `current_mhz` | Current clock speed in MHz (perf). |
| `max_mhz` | Maximum clock speed in MHz (perf). |
| `frequency_pct` | Current frequency as a percentage of maximum (perf). |
| `load_pct` | Per-socket CPU load from `Win32_Processor.LoadPercentage` (perf). |
| `cores` | Number of physical cores. |
| `logical_processors` | Number of logical processors (threads). |

There are no default warning/critical thresholds: modern CPUs legitimately clock
far below their maximum at idle, so a `frequency_pct` default would warn on every
idle machine. Use `load_pct` for a per-socket utilisation alert.
