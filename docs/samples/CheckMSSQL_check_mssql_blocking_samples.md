**Default check (healthy — no blocking):**

```
check_mssql_blocking
OK: No blocked sessions
```

**Default check during a blocking incident (warning at 30s, critical at 5m):**

```
check_mssql_blocking
CRITICAL: 2/2 blocked sessions (appdb/app blocked by session 59 (app) for 506s on LCK_M_X, appdb/app blocked by session 60 (app) for 506s on LCK_M_X)|'60_wait_time'=506s;30;300 '61_wait_time'=506s;30;300
```

**Show the whole chain with the root blocker (the session to investigate):**

```
check_mssql_blocking "warning=none" "critical=wait_time > 30m" "top-syntax=${status}: ${list}" "detail-syntax=session ${session_id} (${login}) blocked by ${blocking_session_id} (${blocking_login}), root blocker ${root_blocker}, ${wait_time}s on ${wait_type}"
OK: session 60 (app) blocked by 59 (app), root blocker 59, 506s on LCK_M_X, session 61 (app) blocked by 60 (app), root blocker 59, 506s on LCK_M_X|'60_wait_time'=506s;0;1800 '61_wait_time'=506s;0;1800
```

Here sessions 60 and 61 are both ultimately waiting on session 59 — killing or
committing that one session releases the whole chain.

**Alert only on orphaned transactions (idle blocker holding locks):**

```
check_mssql_blocking "warning=wait_time > 30s and blocker_idle = 1" "critical=wait_time > 30m"
OK: 2 blocked sessions, none over the thresholds|'60_wait_time'=506s;30;1800 '61_wait_time'=506s;30;1800
```

The blocker in this incident was still actively executing (`blocker_idle = 0`),
so the orphaned-transaction warning correctly stays quiet while the generic
`wait_time` critical would still fire at 30 minutes.
