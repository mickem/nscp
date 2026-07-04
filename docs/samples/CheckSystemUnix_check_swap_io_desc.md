#### About `check_swap_io`

`check_swap_io` measures how fast the system is paging to and from swap. It
reads `pswpin`/`pswpout` from `/proc/vmstat`, waits ~1 second, reads them again,
and reports the rate. Sustained non-zero swap I/O is a strong signal of memory
pressure — often more actionable than swap *usage*, since a box can sit with
swap full but idle, or with little swap used yet thrashing hard.

Keywords:

| Keyword          | Description                                       |
|------------------|---------------------------------------------------|
| `swap_in`        | Pages paged in from swap per second               |
| `swap_out`       | Pages paged out to swap per second                |
| `swap_in_bytes`  | Bytes paged in per second (pages × page size)     |
| `swap_out_bytes` | Bytes paged out per second                        |
| `swap_count`     | Number of active swap devices                     |

There are **no default thresholds** — a bare `check_swap_io` reports the current
rate as OK. Supply `warning=` / `critical=` on `swap_in` / `swap_out` (or the
`*_bytes` variants). On a host with no swap configured the rates are simply `0`.
