Reports system paging (swap) I/O **rates**, sourced from the Windows memory
performance counters `\Memory\Pages Input/sec` and `\Memory\Pages Output/sec`
(sampled over a ~1 second window). Windows has no per-pagefile I/O counter, so
this is a single system-wide aggregate row.

The keyword vocabulary matches the Linux `check_swap_io`, so warning/critical
expressions and detail-syntax port between platforms.

There are no default warning/critical thresholds: sustained paging is workload
dependent, and a default would warn on legitimately busy hosts. Set a threshold
on `swap_in`/`swap_out` (pages/s) or `swap_in_bytes`/`swap_out_bytes` (bytes/s)
for the host in question.

> Note: on Windows these are system-wide paging rates (pages moved between disk
> and physical memory) — the correct analogue of Linux swap-in/out — not literal
> per-pagefile read/write bytes.
