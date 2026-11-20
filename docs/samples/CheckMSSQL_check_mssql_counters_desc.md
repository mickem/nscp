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

The check returns one row per instance.

Any counter except `latch_wait_ms` reads `-1` when unavailable on the instance or reset during the
sampling window. Guard low-value
thresholds with `>= 0` so missing data does not become a false low-value alert.
Use a separate `= -1` condition if missing data should alert.

`latch_wait_ms` is the average time, in whole milliseconds, that a latch request
which had to wait spent waiting. It is computed over the same sampling window
from `Average Latch Wait Time (ms)` and its base counter, and reads `0` when no
latch request waited during the window — unlike the hit ratio it never falls
back to the average since instance start, which would keep reporting an old
incident. A sustained high value points at contention on in-memory structures
(typically `tempdb` allocation pages or a hot index page); read it next to
`check_mssql_waits`. When the latch counters are unavailable or were reset
during the window it has no value rather than `-1`: it renders as `unknown`,
emits no perfdata and compares false against every number, so a threshold such
as `latch_wait_ms > 10` needs no guard. Add `or latch_wait_ms = 'unknown'` if
missing data should alert.

All counters except `latch_wait_ms` are emitted as **perfdata by default** (no
threshold needed) — this check is primarily a graphing source.
`latch_wait_ms` joins the perfdata as soon as a threshold references it. There are **no default alert
thresholds** because healthy values scale with hardware and workload: page
life expectancy scales with buffer pool size (the old "300 seconds" rule
predates large-RAM servers; a common modern rule is 300s per 4 GB of buffer
pool), and batch rates are only meaningful against your own baseline. Typical
starting points:

```
check_mssql_counters "warning=(hit_ratio >= 0 and hit_ratio < 95) or (page_life_expectancy >= 0 and page_life_expectancy < 300)" "critical=(hit_ratio >= 0 and hit_ratio < 85) or (page_life_expectancy >= 0 and page_life_expectancy < 60)"
check_mssql_counters "warning=deadlocks > 0.1" "critical=lazy_writes > 20"
check_mssql_counters "warning=latch_wait_ms > 10" "critical=latch_wait_ms > 100"
```

Rights: `VIEW SERVER STATE`.
