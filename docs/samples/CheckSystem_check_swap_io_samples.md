**Default check:**

```
check_swap_io
OK: 1 page file(s), in 0 pages/s, out 0 pages/s
'io_swap_in'=0;;; 'io_swap_out'=0;;; 'io_swap_in_bytes'=0B;;; 'io_swap_out_bytes'=0B;;;
```

**Alert on sustained paging (pages/s):**

```
check_swap_io "warn=swap_in > 1000" "crit=swap_in > 5000"
OK: 1 page file(s), in 42 pages/s, out 7 pages/s
'io_swap_in'=42;1000;5000; 'io_swap_out'=7;;; 'io_swap_in_bytes'=172032B;;; 'io_swap_out_bytes'=28672B;;;
```

**Threshold on throughput (bytes/s) with a custom output line:**

```
check_swap_io "crit=swap_out_bytes > 10485760" "detail-syntax=in ${swap_in_bytes}B/s, out ${swap_out_bytes}B/s"
OK: in 172032B/s, out 28672B/s
'io_swap_in_bytes'=172032B;;; 'io_swap_out_bytes'=28672B;;10485760;
```
