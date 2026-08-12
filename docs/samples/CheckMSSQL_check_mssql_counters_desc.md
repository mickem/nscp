#### About `check_mssql_counters`

`check_mssql_counters` reports the **engine performance counters** every SQL
Server health methodology starts with, from `sys.dm_os_performance_counters`
over the same ODBC connection as every other check — so it works for remote
and named instances where the local PDH counter sets
(`SQLServer:Buffer Manager` etc.) are unavailable or renamed.

Most of these counters are cumulative since instance start, so the check takes
**two snapshots one second apart** (a server-side `WAITFOR DELAY`, the check
takes ~1s longer than the others) and reports per-second rates over that
window. The buffer cache hit ratio is likewise computed over the window — the
lifetime ratio converges to ~100% on any long-running instance and hides a
cold or thrashing cache.

Keywords (one row per instance):

| Keyword                | Description                                                            |
|------------------------|------------------------------------------------------------------------|
| `hit_ratio`            | Buffer cache hit ratio in percent over the sampling window             |
| `page_life_expectancy` | Seconds a page stays in the buffer pool without being referenced       |
| `batch_requests`       | Batch requests per second (the instance's workload pulse)              |
| `compilations`         | SQL compilations per second (high vs `batch_requests` = plan-cache misuse) |
| `recompilations`       | SQL re-compilations per second                                         |
| `lazy_writes`          | Lazy-writer pages flushed per second; sustained values mean memory pressure |
| `lock_waits`           | Lock requests per second that had to wait                              |
| `deadlocks`            | Deadlocks per second across all lock types                             |

Any counter reads `-1` when unavailable on the instance.

All counters are emitted as **perfdata by default** (no threshold needed) —
this check is primarily a graphing source. There are **no default alert
thresholds** because healthy values scale with hardware and workload: page
life expectancy scales with buffer pool size (the old "300 seconds" rule
predates large-RAM servers; a common modern rule is 300s per 4 GB of buffer
pool), and batch rates are only meaningful against your own baseline. Typical
starting points:

```
check_mssql_counters "warning=hit_ratio < 95 or page_life_expectancy < 300" "critical=hit_ratio < 85 or page_life_expectancy < 60"
check_mssql_counters "warning=deadlocks > 0.1" "critical=lazy_writes > 20"
```

Rights: `VIEW SERVER STATE`.
