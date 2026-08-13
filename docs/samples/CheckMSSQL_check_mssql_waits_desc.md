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
nothing waited.

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

Keywords (one row per instance):

| Keyword           | Description                                                                  |
|-------------------|-------------------------------------------------------------------------------|
| `runnable_tasks`  | Tasks with CPU work waiting for a scheduler slot (point in time)              |
| `work_queue`      | Tasks queued with **no worker thread at all** — THREADPOOL starvation when > 0 |
| `schedulers`      | Visible online schedulers (compare `runnable_tasks` against this)             |
| `workers`         | Active worker threads                                                         |
| `cpu_waits`       | `SOS_SCHEDULER_YIELD`, `THREADPOOL`, `CX*` (parallelism), ms/s                |
| `io_waits`        | `PAGEIOLATCH_*`, `IO_COMPLETION`, `BACKUPIO`, ms/s                            |
| `log_waits`       | `WRITELOG`, `LOGBUFFER` — transaction-log flush latency, ms/s                 |
| `lock_waits`      | `LCK_M_*`, ms/s                                                               |
| `latch_waits`     | `PAGELATCH_*`, `LATCH_*` (in-memory contention, e.g. tempdb), ms/s            |
| `memory_waits`    | `RESOURCE_SEMAPHORE*` (queries waiting for memory grants), `CMEMTHREAD`, ms/s |
| `network_waits`   | `ASYNC_NETWORK_IO` — usually the client not consuming results, not the network, ms/s |
| `other_waits`     | Everything not covered above (benign waits excluded), ms/s                    |
| `total_waits`     | Total non-benign wait, ms/s                                                   |
| `signal_wait_pct` | Percent of wait time spent runnable **after** the resource arrived — sustained > 20–25% means CPU pressure (`-1` when nothing waited) |

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
