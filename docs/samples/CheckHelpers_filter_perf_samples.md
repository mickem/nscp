**Run a check unchanged (no sorting, no limit):**

```
filter_perf command=check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.24464GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used'=29.77734MB;36.98125;41.6039;0;46.22656 '/opt/env-runner used %'=64%;80;90;0;100
```

**Keep only the largest values (`sort=normal`, biggest first):**

```
filter_perf command=check_drivesize sort=normal limit=3
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100 '/opt/env-runner used %'=64%;80;90;0;100
```

**Or the smallest (`sort=reversed`):**

```
filter_perf command=check_drivesize sort=reversed limit=2
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.24916GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100
```

**Top N processes by memory:**

The typical use — a check that emits one counter per process would otherwise
produce hundreds of series.

```
filter_perf sort=normal limit=10 command=check_process arguments "filter=working_set > 0" "warn=working_set > 3G" "crit=working_set > 5G" "detail-syntax=%(exe) ws=%(working_set)"
WARNING: WARNING: clion64.exe=started
'clion64.exe ws_size'=3.30851GB;3;5 'Rider.Backend.exe ws_size'=1.80017GB;3;5 'clangd.exe ws_size'=1.4822GB;3;5 'devenv.exe ws_size'=1.14938GB;3;5 'msedge.exe ws_size'=0.5757GB;3;5
```

**The status is not affected by `limit`:**

Warning and critical are still evaluated against *every* matching item, so the
alert fires even when the offending series is not among the ones shown.

```
filter_perf command=check_drivesize sort=reversed limit=1 "arguments=crit=used > 50%"
CRITICAL: CRITICAL /opt/claude-code: 202.746MB/229.949MB used, /opt/env-runner: 29.777MB/46.227MB used
'/ used'=8.30583GB;201.57782;125.98613;0;251.97227
```

Note that sorting only ever considers numeric counters; entries without a
numeric value keep their original position.
