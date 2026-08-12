#### About `check_mssql_tempdb`

`check_mssql_tempdb` reports **tempdb space usage split by consumer** from
`tempdb.sys.dm_db_file_space_usage`, plus the free space on the tempdb
volume. tempdb is the instance-wide shared resource: when it fills, every
database on the instance starts failing — and the split tells you *what* is
filling it before you have to guess:

| Keyword            | Description                                                              |
|--------------------|---------------------------------------------------------------------------|
| `size`             | Allocated tempdb data-file bytes (accepts units)                          |
| `free`             | Unallocated bytes within the files                                        |
| `used` / `used_pct`| Bytes / percent of the current allocation in use                          |
| `version_store`    | Held by the version store — growth means a **long-running snapshot transaction** is pinning cleanup |
| `user_objects`     | Held by user objects — **temp tables and table variables**                |
| `internal_objects` | Held by internal objects — **sort/hash spills and work tables** (under-estimated memory grants) |
| `volume_free`      | Free bytes on the most constrained volume holding a tempdb data file, `-1` = unknown |

All values are emitted as **perfdata by default** — trending the split is how
tempdb sizing problems are diagnosed. `volume_free` uses `MIN` across the
data-file volumes because the files on the fullest volume hit the wall first.

There are **no default alert thresholds**: `used_pct` measures the *current*
allocation, which is soft while autogrowth is enabled, and pre-sized tempdb
capacity is a sizing decision. Meaningful starting points:

```
check_mssql_tempdb "warning=used_pct > 80" "critical=used_pct > 90 or volume_free < 1G"
check_mssql_tempdb "warning=version_store > 5G"
```

For a pre-sized tempdb (autogrowth off) `used_pct` is a hard limit and the
80/90 thresholds are appropriate. With autogrowth on, `volume_free` is the
real ceiling. A growing `version_store` is best cross-checked with
`check_mssql_transactions` — the pinning transaction shows up there.

Rights: `VIEW SERVER STATE` (the `volume_free` keyword additionally uses
`sys.dm_os_volume_stats`; if unavailable the check still works and reports
`-1`).
