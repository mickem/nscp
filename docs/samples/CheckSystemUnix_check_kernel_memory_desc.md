#### About `check_kernel_memory`

`check_kernel_memory` reports kernel memory-manager health from `/proc/meminfo`
and `/proc/vmstat`: slab allocator usage, page-cache bytes and page-fault
rates. It complements `check_memory` — kernel-side leaks and major-fault storms
are the failure modes free-RAM thresholds do not catch. The fault counters are
cumulative, so the check samples a 1-second window (like `check_swap_io`).

It is the unix counterpart to the Windows `check_kernel_memory`: `cache` and
the fault-rate keywords are shared; the kernel-allocation gauges keep their
platform-native names (`slab_*` here, `pool_*` on Windows), the same
convention as `hive` vs `manager` in `check_installed_software`.

Keywords (a single aggregate row):

| Keyword                | Source                    | Description                                                         |
|------------------------|---------------------------|---------------------------------------------------------------------|
| `slab`                 | `/proc/meminfo` Slab      | Total kernel slab bytes; size units work (`slab > 2G`)              |
| `slab_reclaimable`     | SReclaimable              | Slab the kernel can drop under pressure (dentry/inode caches)       |
| `slab_unreclaimable`   | SUnreclaim                | Pinned kernel slab — steady growth is the kernel/driver leak signal |
| `cache`                | Cached                    | Page-cache bytes                                                    |
| `page_faults_per_sec`  | `/proc/vmstat` pgfault    | Total faults (soft + hard)                                          |
| `major_faults_per_sec` | `/proc/vmstat` pgmajfault | Faults that had to read from disk — the fault-storm signal          |

All gauges and rates are always emitted as perf data (`kernel_slab`,
`kernel_major_faults_per_sec`, ...), which is what makes a slow unreclaimable-
slab leak visible: it is inherently a trend signal, so let the backend graph
it.

