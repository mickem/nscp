#### About `check_mssql_jobs`

`check_mssql_jobs` reports the **outcome of the last run of every SQL Server
Agent job** from `msdb.dbo.sysjobs` and the job-outcome rows of
`msdb.dbo.sysjobhistory`, producing one row per job. Disabled jobs are
excluded by the default filter (`enabled = 1`).

Keywords (one row per job):

| Keyword            | Description                                                                    |
|--------------------|--------------------------------------------------------------------------------|
| `name`             | Job name                                                                       |
| `enabled`          | `1` if the job is enabled                                                      |
| `is_running`       | `1` if the job is executing right now                                          |
| `last_run_status`  | Outcome of the last **completed** run: `failed`, `succeeded`, `retry`, `canceled` or `never` |
| `last_run_outcome` | Raw msdb `run_status` code of the last completed run (`-1` = never ran)        |
| `last_run_age`     | Seconds since the last run **finished**, `-1` = never ran (accepts units)      |

Defaults: **CRITICAL** on `last_run_status = 'failed'`, **WARNING** on
`canceled` or `retry`. empty-state is **OK**: an instance without Agent jobs —
including Express edition, which has no SQL Agent at all — is healthy, not an
error. Jobs that have never run report `last_run_status = 'never'` and are not
alerted on by default; add `warning=last_run_status = 'never'` to catch
schedules that never fire, or `warning=last_run_age > 25h` to catch a nightly
job that stopped running.

`last_run_age` is measured from the moment the run **finished** (the start time
from `sysjobhistory` plus that run's duration), so a threshold like
`last_run_age > 25h` is not skewed by how long the job itself takes.

In-flight runs are reported through `is_running`, taken from
`msdb.dbo.sysjobactivity`. SQL Agent only writes a job's outcome row when the
run **completes**, so a job that is still executing keeps the
`last_run_status` of its previous run (or `never` on a first-ever run) — use
`is_running` to reason about the current execution, for example
`critical=is_running = 1 and last_run_age > 6h` to catch a job that is stuck.

Rights: reading job status requires msdb access — membership in
`SQLAgentReaderRole` (or `sysadmin`). A permission failure surfaces as UNKNOWN
with the `Query failed:` prefix and the ODBC diagnostic.
