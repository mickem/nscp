#### About `check_kernel_memory`

`check_kernel_memory` reports kernel memory-manager health: kernel-allocation
gauges, cache bytes and page-fault rates. It complements `check_memory`
(used/free/size of physical/committed/virtual) — **kernel-side leaks and
hard-fault storms are the classic server failure modes that free-RAM thresholds
do not catch.** The fault counters are rates, so the check samples a 1-second
window (like `check_swap_io`).

The check returns a single aggregate row. All gauges and rates are always
emitted as perf data (`kernel_cache`, `kernel_page_faults_per_sec`, ...), which
is what makes a slow kernel-allocation leak visible: it is inherently a trend
signal, so let the backend graph it. There are no default thresholds.

`cache` and `page_faults_per_sec` are shared; the kernel-allocation gauges and
the hard-fault rate keep their platform-native names (`pool_*` /
`hard_faults_per_sec` on Windows, `slab_*` / `major_faults_per_sec` on Linux),
the same convention as `hive` vs `manager` in `check_installed_software`.

##### Windows

Sourced from the PDH `Memory` counter set. Keywords: `pool_paged`,
`pool_nonpaged`, `cache`, `page_faults_per_sec`, `transition_faults_per_sec`
and `hard_faults_per_sec`.

`hard_faults_per_sec` counts hard-fault *events* (`Page Reads/sec`), not the
pages they bring in: `check_swap_io` reports the latter as `swap_in`
(`Pages Input/sec`), and a read that pages in a whole cluster makes `swap_in`
several times larger than the fault rate. Read the two side by side to tell a
fault storm from a paging storm.

##### Linux

Sourced from `/proc/meminfo` and `/proc/vmstat`. Keywords: `slab`,
`slab_reclaimable`, `slab_unreclaimable`, `cache`, `page_faults_per_sec` and
`major_faults_per_sec`. `slab_unreclaimable` is the gauge that exposes a slow
kernel-side leak — reclaimable slab grows and shrinks with cache pressure and
is not by itself a problem.

##### macOS

Sourced from the Mach VM statistics. macOS has no slab allocator, so `slab`,
`slab_reclaimable` and `slab_unreclaimable` are `unknown`. Two macOS-only
keywords take their place: `wired` is memory the kernel has pinned, its own
allocations included, and steady growth there is the kernel-side leak signal;
`compressed` is memory held by the memory compressor, which grows under
pressure the host absorbs by compressing rather than swapping. Both are
`unknown` on Linux. `cache` is the file-backed pages, `page_faults_per_sec`
counts every fault and `major_faults_per_sec` the pageins.
