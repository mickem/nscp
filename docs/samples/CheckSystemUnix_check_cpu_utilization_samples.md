**CPU utilization broken down by mode:**

```
check_cpu_utilization
OK: user: 1.39% system: 1.54% iowait: 0% steal: 0% idle: 95.73%
L        cli  Performance data: 'cpu_total'=4.27;90;95 'cpu_user'=1.39;0;0 'cpu_system'=1.54;0;0 'cpu_iowait'=0;0;0 'cpu_steal'=0;0;0 'cpu_idle'=95.73;0;0 ...
```

**Custom thresholds on total busy percentage (the default is `total > 90` / `> 95`):**

```
check_cpu_utilization "warn=total > 80" "crit=total > 95"
OK: user: 2.1% system: 1.8% iowait: 0.2% steal: 0% idle: 95.9%
```

**Alert specifically on I/O wait (storage saturation):**

```
check_cpu_utilization "warn=iowait > 20" "crit=iowait > 50"
OK: user: 1.4% system: 1.5% iowait: 0% steal: 0% idle: 95.7%
```

**Alert on steal time (noisy-neighbour on a VM):**

```
check_cpu_utilization "warn=steal > 5" "crit=steal > 15"
OK: user: 1.4% system: 1.5% iowait: 0% steal: 0% idle: 95.7%
```
