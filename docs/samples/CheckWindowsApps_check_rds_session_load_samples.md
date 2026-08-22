**Show per-session resource usage (one record per counter instance):**

```
check_rds_session_load
OK: Console: 6.064% cpu, 18833346560B working set, Services: 1.68039% cpu, 6785593344B working set|'Console_working_set'=18833346560B;0;0 'Services_working_set'=6785593344B;0;0
```

**Only real sessions (skip the session-0 'Services' aggregate) and sample CPU over a second:**

```
check_rds_session_load sessions-only=true averages=true
OK: Console: 7.88642% cpu, 18829905920B working set|'Console_working_set'=18829905920B;0;0
```

**Find the runaway session eating the host:**

```
check_rds_session_load sessions-only=true averages=true "warning=cpu > 50" "critical=cpu > 80"
WARNING: RDP-Tcp 55: 63.2% cpu, 4831838208B working set|'RDP-Tcp 55_cpu'=63.2%;50;80 'RDP-Tcp 55_working_set'=4831838208B;0;0 ...
```

**Alert on per-session memory:**

```
check_rds_session_load sessions-only=true "warning=working_set > 8000000000"
OK: Console: 6.1% cpu, 4833346560B working set|'Console_working_set'=4833346560B;8000000000;0
```

**On a host without the counters the check reports UNKNOWN with a clear message:**

```
check_rds_session_load
Remote Desktop Services counters (Terminal Services Session) not available - is the role installed on this host? (...)
```
