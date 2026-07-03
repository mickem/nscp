**Show the system load average (1 / 5 / 15 minutes):**

```
check_load
OK: total load average: 0.96, 0.79, 0.75
L        cli  Performance data: 'total_load1'=0.95999;0;0 'total_load5'=0.79;0;0 'total_load15'=0.75;0;0
```

**Normalise the load per CPU (divide by the core count):**

```
check_load percpu=true
OK: scaled load average: 0.048, 0.0395, 0.0375
L        cli  Performance data: 'scaled_load1'=0.048;0;0 'scaled_load5'=0.0395;0;0 'scaled_load15'=0.03749;0;0
```

**Warn / critical on any load window (`load` is the max of the three):**

```
check_load "warn=load > 5" "crit=load > 10"
OK: total load average: 0.82, 0.77, 0.75
L        cli  Performance data: 'total_load'=0.81999;5;10 ...
```

**Threshold on a specific window, e.g. the 1-minute average:**

```
check_load "warn=load1 > 4" "crit=load1 > 8"
OK: total load average: 0.82, 0.77, 0.75
```

**Per-CPU thresholds (useful for portability across differently-sized hosts):**

```
check_load percpu=true "warn=load > 1" "crit=load > 2"
OK: scaled load average: 0.05, 0.04, 0.04
```
