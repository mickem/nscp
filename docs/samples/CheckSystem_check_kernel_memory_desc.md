#### About `check_kernel_memory` (Windows)

`check_kernel_memory` reports kernel memory-manager health from the PDH
`Memory` counter set: pool usage, file cache and page-fault rates. It
complements `check_memory` (used/free/size of physical/committed/virtual) —
**pool exhaustion and hard-fault storms are the classic Windows server failure
modes that free-RAM thresholds do not catch.** The fault counters are rates,
so the check samples a 1-second window (like `check_swap_io`).

Keywords (a single aggregate row):

| Keyword                     | Counter                  | Description                                                              |
|-----------------------------|--------------------------|--------------------------------------------------------------------------|
| `pool_paged`                | Pool Paged Bytes         | Paged pool; size units work (`pool_paged > 2G`), renders human-readable  |
| `pool_nonpaged`             | Pool Nonpaged Bytes      | Nonpaged pool — steady growth is the classic driver-leak signal          |
| `cache`                     | Cache Bytes              | System file-cache working set                                            |
| `page_faults_per_sec`       | Page Faults/sec          | Total faults (soft + hard)                                               |
| `transition_faults_per_sec` | Transition Faults/sec    | The dominant soft-fault kind (resolved without disk I/O)                 |
| `hard_faults_per_sec`       | Page Reads/sec           | Faults that had to read from disk — the fault-storm signal               |

All six are always emitted as perf data (`kernel_pool_paged`,
`kernel_hard_faults_per_sec`, ...), which is what makes the slow nonpaged-pool
leak visible: it is inherently a trend signal, so let the backend graph it.

There are no default thresholds. `hard_faults_per_sec` counts hard-fault
*events* (`Page Reads/sec`), not the pages they bring in: `check_swap_io`
reports the latter as `swap_in` (`Pages Input/sec`), and a read that pages in a
whole cluster makes `swap_in` several times larger than the fault rate. Read
the two side by side to tell a fault storm from a paging storm. On Linux the same command is
provided by the unix CheckSystem module with `slab`/`slab_reclaimable`/
`slab_unreclaimable` as the platform-native gauges and
`major_faults_per_sec` as the hard-fault rate.
