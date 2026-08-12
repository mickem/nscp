#### About `check_docker_stats`

`check_docker_stats` samples per-container resource usage the way
`docker stats` does: CPU as a percentage of the host (delta of the container's
CPU time over the host's, scaled by online CPUs) and memory usage with the
page cache excluded, against the container's memory limit.

Sampling uses the daemon's `stream=false` stats mode, which takes two readings
about a second apart so the CPU delta is meaningful — expect roughly **one
second per sampled container**. On hosts with many containers, scope the check
with `container=<name>` (repeatable) or accept the added latency.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword        | Description                                                        |
|----------------|--------------------------------------------------------------------|
| `names`        | Container name(s), comma separated                                 |
| `image`        | Image the container was created from                               |
| `cpu_pct`      | CPU usage in percent of the host (like `docker stats`)             |
| `memory_used`  | Memory used in bytes, page cache excluded; size units in thresholds (`memory_used > 200M`) |
| `memory_limit` | Memory limit in bytes (the host's total memory when unlimited)     |
| `memory_pct`   | Memory usage in percent of the limit                               |
| `memory`       | Human readable usage, e.g. `45.2MB of 256MB` (display only)        |

Size-typed thresholds need a unit (`1b`, `64k`, `200M`, `1G`); a bare number
is rejected. No default thresholds: an idle container is OK until you say
otherwise.
