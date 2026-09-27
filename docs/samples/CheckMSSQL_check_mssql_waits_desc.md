#### About `check_mssql_waits`

`check_mssql_waits` reports **wait statistics by category and scheduler
pressure** from `sys.dm_os_wait_stats` and `sys.dm_os_schedulers`. Wait
categories are how you tell a storage problem from a CPU or locking problem
without a DBA: the category with the highest rate is where the instance's
time is going.

`sys.dm_os_wait_stats` is cumulative since instance start, so the check takes
**two snapshots one second apart** (a server-side `WAITFOR DELAY`; the check
takes ~1s longer than the others) and reports each category as **milliseconds
of wait accumulated per second of wall clock** over that window. Idle
housekeeping waits (`LAZYWRITER_SLEEP`, `CHECKPOINT_QUEUE`, `XE_*`, the
`HADR_` housekeeping timers, and the other community benign-wait suspects —
including this check's own `WAITFOR`) are excluded, so `0` really means
nothing waited. A wait whose counters reset during the sampling window is
excluded from the rates and signal-wait percentage.

Two families are deliberately **not** excluded wholesale, because the waits that
matter most hide inside them. `HADR_SYNC_COMMIT` counts towards
`other_waits`/`total_waits`, so synchronous availability-group commit latency
stays visible while the `HADR_` housekeeping timers are filtered. And of the
`PREEMPTIVE_*` family — SQLOS calling out of the engine, mostly idle — these
external stalls keep counting: `PREEMPTIVE_OS_WRITEFILEGATHER` and
`PREEMPTIVE_OS_FLUSHFILEBUFFERS` (file growth and flushes, reported under
`io_waits`), plus `PREEMPTIVE_HTTP_REQUEST`, `PREEMPTIVE_OS_AUTHENTICATIONOPS`,
`PREEMPTIVE_OS_CRYPTOPS`, `PREEMPTIVE_ODBCOPS` and `PREEMPTIVE_OLEDBOPS` (backup
to URL, domain lookups, key operations and linked servers, under `other_waits`).
Autogrow on slow storage or a hanging backup would otherwise leave every
category reading zero in the middle of the incident.

The check returns one row per instance.

The whole profile is emitted as **perfdata by default** — like
`check_mssql_counters` this is primarily a graphing source, and wait rates
only mean something against the workload's own baseline. Two thresholds are
meaningful without a baseline and make good starting points:

```
check_mssql_waits "warning=work_queue > 0 or signal_wait_pct > 25" "critical=work_queue > 10"
```

`work_queue > 0` (worker starvation) and a sustained high `signal_wait_pct`
(CPU pressure) are abnormal on any healthy instance; `runnable_tasks`
sustained above the scheduler count points the same way.

Rights: `VIEW SERVER STATE`.
