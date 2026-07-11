**Default check:**

```
check_cpu
CPU Load ok
'total 5m load'=0%;80;90 'total 1m load'=0%;80;90 'total 5s load'=7%;80;90
```

**Checking all cores by adding filter=none (disabling the default filter):**

```
check_cpu filter=none "warn=load > 80" "crit=load > 90"
CPU Load ok
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

**Adding kernel times to the check:**

```
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90" "top-syntax=${list}"
core 0 > 3, core 1 > 0, core 2 > 0, core  ... , core 7 > 15, total > 7
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

**Default check via NRPE:**

```
check_nscp --host 192.168.56.103 --command check_cpu
CPU Load ok|'total 5m'=16%;80;90 'total 1m'=13%;80;90 'total 5s'=13%;80;90
```


**Customizing the output syntax to include CPU load in text:**

```
check_cpu "top-syntax=%(status): %(list)"
L        cli OK: OK: 5m: 16%, 1m: 30%, 5s: 23%
```

**Customizing the output syntax to only show CPU load as text:**

```
check_cpu "top-syntax=%(status): Cpu usage is %(list)" time=5m "detail-syntax=%(load) %"
L        cli OK: OK: Cpu usage is 26 %
```

**Full user/system/idle breakdown as perfdata (parity with the Linux `check_cpu_utilization` graph):**

`idle` and `system` now emit perfdata (previously only `total`/`user` did), so the
full breakdown graphs without a custom `top-syntax`. `kernel` is a deprecated alias
of `system` and intentionally emits no separate perf column.

```
check_cpu "warn=idle < 5"
CPU Load ok
'total 5m load'=7%;80;90 'total 5m user'=4%;;; 'total 5m system'=3%;;; 'total 5m idle'=93%;;; ...
```
