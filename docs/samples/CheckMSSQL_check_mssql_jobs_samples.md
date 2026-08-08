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

**Show the run state of every job, including in-flight runs:**

```
check_mssql_jobs "warning=none" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=${name}: status=${last_run_status} running=${is_running} age=${last_run_age}" show-all
OK: Long running job: status=never running=1 age=-1, Quick job: status=succeeded running=0 age=33
```

**Alert on a job that is stuck running:**

```
check_mssql_jobs "warning=none" "critical=is_running = 1" "detail-syntax=${name} is still running"
CRITICAL: 1/2 jobs (Long running job is still running)
```

**No SQL Agent (Express edition) — not a problem:**

```
check_mssql_jobs
OK: No enabled SQL Agent jobs found
```
