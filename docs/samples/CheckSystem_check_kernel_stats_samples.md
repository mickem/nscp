**Default check (all four rows; thread-count guardrails apply):**

```
check_kernel_stats
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417|'ctxt_current'=119059;8000;10000 'syscalls_current'=268703;8000;10000 'processes_current'=628;8000;10000 'threads_current'=3417;8000;10000
```

**Threshold a context-switch storm (baseline the host first):**

```
check_kernel_stats "warn=none" "crit=name = 'ctxt' and rate > 500000"
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417
```

**Watch only the thread count with custom limits:**

```
check_kernel_stats type=threads "warn=current > 5000" "crit=current > 8000"
OK - Threads 3417
```

**Select several rows and render the raw values:**

```
check_kernel_stats type=ctxt type=processes "detail-syntax=${name}=${current}"
OK - ctxt=119059, processes=628
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_kernel_stats --argument "warn=none" --argument "crit=name = 'threads' and current > 20000"
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417
```
