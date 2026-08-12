**Default check (informational listing per database/login):**

```
check_mssql_sessions
OK: appdb/app: 3 sessions (3 running), master/NT AUTHORITY\SYSTEM: 1 sessions (0 running), master/sa: 1 sessions (1 running)
```

**Alert before the connection pool is exhausted (with perfdata):**

```
check_mssql_sessions "warning=sessions > 100" "critical=sessions > 200"
OK: appdb/app: 3 sessions (3 running), master/NT AUTHORITY\SYSTEM: 1 sessions (0 running), master/sa: 1 sessions (1 running)|'appdb/app_sessions'=3;100;200 'master/NT AUTHORITY\SYSTEM_sessions'=1;100;200 'master/sa_sessions'=1;100;200
```

**A runaway session count trips the threshold:**

```
check_mssql_sessions "warning=sessions > 2"
WARNING: appdb/app: 3 sessions (3 running), master/NT AUTHORITY\SYSTEM: 1 sessions (0 running), master/sa: 1 sessions (1 running)|'appdb/app_sessions'=3;2;0 'master/NT AUTHORITY\SYSTEM_sessions'=1;2;0 'master/sa_sessions'=1;2;0
```

**Catch leaked connections that have been idle for half a day (time units):**

```
check_mssql_sessions "critical=max_idle > 12h"
OK: appdb/app: 3 sessions (0 running), master/sa: 1 sessions (1 running)|'appdb/app_max_idle'=139s;0;43200 'master/sa_max_idle'=-1s;0;43200
```

Only sleeping/dormant sessions count towards `max_idle`: the `master/sa` group
is the monitoring session itself (running, so excluded) and reports the `-1`
unknown sentinel, while the three idle `app` sessions report a real idle age.

**Watch a single application login:**

```
check_mssql_sessions "filter=login = 'app'" "warning=sessions > 100"
OK: appdb/app: 3 sessions (3 running)|'appdb/app_sessions'=3;100;0
```
