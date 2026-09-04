**Watch only the thread count with custom limits:**

```
check_kernel_stats type=threads "warn=current > 5000" "crit=current > 8000"
OK - Threads 3417|'threads'=3417;5000;8000
```

**Alert on a runaway context-switch rate (baseline the host first):**

```
check_kernel_stats "warn=name = 'ctxt' and rate > 100000" "crit=name = 'ctxt' and rate > 500000"
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363
```

**Select several rows and render the raw values:**

```
check_kernel_stats type=ctxt type=processes "detail-syntax=${name}=${current}"
OK - ctxt=119059, processes=628
```

##### Windows

**Default check (all four rows; thread-count guardrails apply):**

```
check_kernel_stats
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417|'ctxt'=119059;8000;10000 'syscalls'=268703;8000;10000 'processes'=628;8000;10000 'threads'=3417;8000;10000
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_kernel_stats --argument "warn=none" --argument "crit=name = 'threads' and current > 20000"
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417
```

##### Linux

**Default check (context-switch rate, fork rate and live thread count):**

```
check_kernel_stats
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363|'ctxt'=2747325827;8000;10000 'processes'=2888772;8000;10000 'threads'=363;8000;10000
```

Note that on Linux `current` for the rate rows is the *cumulative* counter read
from `/proc/stat`, which is why `ctxt` shows a very large number in perf data
while the message shows the per-second rate.

**Only context switches and forks (repeat `type=`):**

```
check_kernel_stats type=ctxt type=processes
OK - Context Switches 57111.0/s, Process Creations 317.0/s
```
