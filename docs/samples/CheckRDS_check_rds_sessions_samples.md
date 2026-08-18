**Check session counts on a session host:**

```
check_rds_sessions
OK: 1 active, 1 inactive (2 total)|'sessions_active'=1;0;0 'sessions_inactive'=1;0;0 'sessions_total'=2;0;0
```

**Alert when the host approaches its session capacity:**

```
check_rds_sessions "warning=active > 40" "critical=active > 50"
OK: 32 active, 5 inactive (37 total)|'sessions_active'=32;40;50 'sessions_inactive'=5;0;0 'sessions_total'=37;0;0
```

**Alert on disconnected sessions piling up (they still hold memory and CALs):**

```
check_rds_sessions "warning=inactive > 10"
WARNING: 12 active, 14 inactive (26 total)|'sessions_inactive'=14;10;0 'sessions_active'=12;0;0 'sessions_total'=26;0;0
```

**On a host without the counters the check reports UNKNOWN with a clear message:**

```
check_rds_sessions
Remote Desktop Services counters (Terminal Services) not available - is the role installed on this host? (...)
```
