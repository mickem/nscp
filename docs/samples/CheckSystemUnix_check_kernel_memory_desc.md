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

The check returns a single aggregate row. All gauges and rates are always emitted as perf data (`kernel_slab`,
`kernel_major_faults_per_sec`, ...), which is what makes a slow unreclaimable-
slab leak visible: it is inherently a trend signal, so let the backend graph
it.

