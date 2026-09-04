#### About `check_swap_io`

`check_swap_io` measures how fast the system is paging to and from swap. It
samples the underlying counters over a ~1 second window and reports the rate.
Sustained non-zero swap I/O is a strong signal of memory pressure — often more
actionable than swap *usage*, since a box can sit with swap full but idle, or
with little swap used yet thrashing hard.

The keyword vocabulary is identical on both platforms, so warning/critical
expressions and detail-syntax port between them. There are **no default
thresholds**: sustained paging is workload dependent, and a default would warn
on legitimately busy hosts. Set a threshold on `swap_in` / `swap_out` (pages/s)
or `swap_in_bytes` / `swap_out_bytes` (bytes/s) for the host in question.

##### Windows

Sourced from the memory performance counters `\Memory\Pages Input/sec` and
`\Memory\Pages Output/sec`. Windows has no per-pagefile I/O counter, so this is
a single system-wide aggregate row.

> Note: on Windows these are system-wide paging rates (pages moved between disk
> and physical memory) — the correct analogue of Linux swap-in/out — not literal
> per-pagefile read/write bytes.

##### Linux

Reads `pswpin` / `pswpout` from `/proc/vmstat`. On a host with no swap
configured the rates are simply `0`.
