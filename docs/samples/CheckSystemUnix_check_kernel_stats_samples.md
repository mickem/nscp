**Kernel activity — context-switch rate, fork rate and live thread count:**

```
check_kernel_stats
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363
L        cli  Performance data: 'ctxt'=2747325827;8000;10000 'processes'=2888772;8000;10000 'threads'=363;8000;10000
```

**Only the thread count, with a custom threshold:**

```
check_kernel_stats type=threads "warn=current > 100000"
OK - Threads 368
L        cli  Performance data: 'threads'=368;100000;10000
```

**Alert on a runaway context-switch rate:**

```
check_kernel_stats "warn=name = 'ctxt' and rate > 100000" "crit=name = 'ctxt' and rate > 500000"
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363
```

**Only context switches and forks (repeat `type=`):**

```
check_kernel_stats type=ctxt type=processes
OK - Context Switches 57111.0/s, Process Creations 317.0/s
```
