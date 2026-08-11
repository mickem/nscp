**Default check (inventory of the kernel memory gauges and fault rates):**

```
check_kernel_memory
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s|'kernel_cache'=298504192;0;0 'kernel_hard_faults_per_sec'=57.2;0;0 'kernel_page_faults_per_sec'=16617.16;0;0 'kernel_pool_nonpaged'=2760646656;0;0 'kernel_pool_paged'=1809305600;0;0 'kernel_transition_faults_per_sec'=4624.68;0;0
```

Note the shape of a healthy host: five-digit total faults/s (soft) but only a
handful of hard faults/s.

**Detect a nonpaged-pool leak (baseline the host, then pin absolute bytes):**

```
check_kernel_memory "warn=pool_nonpaged > 3G" "crit=pool_nonpaged > 4G"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

**Alert on a hard-fault storm (memory pressure forcing disk reads):**

```
check_kernel_memory "warn=hard_faults_per_sec > 200" "crit=hard_faults_per_sec > 1000"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

**Combine pool and fault policy in one check:**

```
check_kernel_memory "warn=pool_paged > 4G or pool_nonpaged > 3G" "crit=hard_faults_per_sec > 1000"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

**Inspect the fault breakdown (soft vs hard):**

```
check_kernel_memory "detail-syntax=faults=${page_faults_per_sec}/s (soft ${transition_faults_per_sec}/s, hard ${hard_faults_per_sec}/s)"
OK: faults=16617.16/s (soft 4624.68/s, hard 57.2/s)
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_kernel_memory --argument "crit=hard_faults_per_sec > 1000"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```
