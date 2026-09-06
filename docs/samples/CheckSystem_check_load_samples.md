**Show the system load average (1 / 5 / 15 minutes):**

```
check_load
OK: total load average: 2.33528, 1.84625, 1.74261|'total_load1'=2.33528;0;0 'total_load5'=1.84625;0;0 'total_load15'=1.74261;0;0
```

**Normalise the load per CPU (divide by the core count):**

```
check_load percpu=true
OK: scaled load average: 0.145955, 0.115391, 0.108913|'scaled_load1'=0.14595;0;0 'scaled_load5'=0.11539;0;0 'scaled_load15'=0.10891;0;0
```

**Warn / critical on any load window (`load` is the max of the three):**

```
check_load "warn=load > 20" "crit=load > 40"
OK: total load average: 2.33528, 1.84625, 1.74261|'total_load'=2.33528;20;40 'total_load1'=2.33528;0;0 'total_load5'=1.84625;0;0 'total_load15'=1.74261;0;0
```

**Threshold on a specific window, e.g. the 1-minute average:**

```
check_load "warn=load1 > 4" "crit=load1 > 8"
OK: total load average: 2.33528, 1.84625, 1.74261
```

**Per-CPU thresholds (portable across differently-sized hosts):**

```
check_load percpu=true "warn=load > 1" "crit=load > 2"
OK: scaled load average: 0.145955, 0.115391, 0.108913
```

**Inspect the run-queue counters:**

```
check_load "detail-syntax=run=${procs_running} total=${procs_total}"
OK: run=1 total=11221
```

##### Windows

The synthesised collector exposes three extra keywords — `queue` (the raw
`\System\Processor Queue Length` saturation signal), `cores` and `samples`
(collector ticks folded into the averages so far).

**Inspect the raw saturation signal and the collector state:**

```
check_load "detail-syntax=q=${queue} run=${procs_running} total=${procs_total} cores=${cores} samples=${samples}"
OK: q=0.0294169 run=1 total=11221 cores=16 samples=31
```

**Alert on sustained queueing regardless of utilization (USE-method saturation):**

```
check_load "warn=queue > 16" "crit=queue > 32"
OK: total load average: 2.33528, 1.84625, 1.74261
```
