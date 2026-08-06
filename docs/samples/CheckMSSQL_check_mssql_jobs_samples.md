**Default check (a job failed its last run):**

```
check_mssql_jobs
CRITICAL: 1/2 jobs (Refresh reporting cache: failed)
```

**All enabled jobs succeeded:**

```
check_mssql_jobs
OK: All 2 jobs succeeded
```

**Exclude a known-noisy job:**

```
check_mssql_jobs "filter=enabled = 1 and name not like 'Refresh'"
OK: All 1 jobs succeeded
```

**Also alert when a nightly job has not run for over a day:**

```
check_mssql_jobs "warning=last_run_age > 25h"
OK: All 2 jobs succeeded|'Refresh reporting cache_last_run_age'=5s;90000;0 'Nightly index maintenance_last_run_age'=229s;90000;0
```

**No SQL Agent (Express edition) — not a problem:**

```
check_mssql_jobs
OK: No enabled SQL Agent jobs found
```
