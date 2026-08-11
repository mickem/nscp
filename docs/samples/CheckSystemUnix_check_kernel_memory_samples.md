**Default check (inventory of the kernel memory gauges and fault rates):**

```
check_kernel_memory
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s|'kernel_cache'=4294967296;0;0 'kernel_major_faults_per_sec'=2;0;0 'kernel_page_faults_per_sec'=25000;0;0 'kernel_slab'=536870912;0;0 'kernel_slab_reclaimable'=402653184;0;0 'kernel_slab_unreclaimable'=134217728;0;0
```

**Detect an unreclaimable-slab leak (baseline the host, then pin absolute bytes):**

```
check_kernel_memory "warn=slab_unreclaimable > 1G" "crit=slab_unreclaimable > 2G"
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s
```

**Alert on a major-fault storm (memory pressure forcing disk reads):**

```
check_kernel_memory "warn=major_faults_per_sec > 200" "crit=major_faults_per_sec > 1000"
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s
```

**Inspect the fault breakdown (total vs major):**

```
check_kernel_memory "detail-syntax=faults=${page_faults_per_sec}/s (major ${major_faults_per_sec}/s), slab=${slab}"
OK: faults=25000/s (major 2/s), slab=512MB
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_kernel_memory --argument "crit=major_faults_per_sec > 1000"
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s
```
