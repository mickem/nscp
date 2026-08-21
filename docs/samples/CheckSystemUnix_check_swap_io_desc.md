#### About `check_swap_io`

`check_swap_io` measures how fast the system is paging to and from swap. It
reads `pswpin`/`pswpout` from `/proc/vmstat`, waits ~1 second, reads them again,
and reports the rate. Sustained non-zero swap I/O is a strong signal of memory
pressure — often more actionable than swap *usage*, since a box can sit with
swap full but idle, or with little swap used yet thrashing hard.

There are **no default thresholds** — a bare `check_swap_io` reports the current
rate as OK. Supply `warning=` / `critical=` on `swap_in` / `swap_out` (or the
`*_bytes` variants). On a host with no swap configured the rates are simply `0`.
