**Show the swap paging rate:**

```
check_swap_io
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s
L        cli  Performance data: 'io_swap_in'=0;0;0 'io_swap_out'=0;0;0 'io_swap_in_bytes'=0;0;0 'io_swap_out_bytes'=0;0;0
```

**Alert on sustained swapping (pages/second):**

```
check_swap_io "warn=swap_out > 100" "crit=swap_out > 1000"
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s
```

**Alert on swap-in as well (thrashing pulls pages back in):**

```
check_swap_io "warn=swap_in > 100 or swap_out > 100" "crit=swap_in > 1000 or swap_out > 1000"
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s
```

**Threshold on throughput in bytes/second instead of pages:**

```
check_swap_io "warn=swap_out_bytes > 1048576" "crit=swap_out_bytes > 10485760"
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s
```
