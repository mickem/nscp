# SQL Server Monitoring

**Goal:** Monitor a Microsoft SQL Server instance end to end — the Windows
services, connectivity, database state, data and log disk usage, memory
pressure, backups, Agent jobs and the workload itself — from the agent that is
already running on the database server.

This scenario combines the dedicated [`CheckMSSQL`](../reference/windows/CheckMSSQL.md)
module with the general Windows checks. The SQL-specific outputs below were
captured against a real SQL Server 2022 instance.

---

## Prerequisites

```ini
[/modules]
CheckMSSQL     = enabled
CheckSystem    = enabled   ; services, memory, performance counters
CheckDisk      = enabled   ; data/log volume free space
CheckEventLog  = enabled   ; SQL Server error log events
NRPEServer     = enabled   ; if using NRPE (active monitoring)
```

Or from the command line:

```
nscp settings --activate-module CheckMSSQL CheckSystem CheckDisk CheckEventLog
```

### Connection and credentials

By default `CheckMSSQL` connects to `localhost` using **Windows integrated
authentication**, i.e. as the account the NSClient++ service runs under. If that
account has access to SQL Server, no credentials need to be configured at all.

To point every check at a different instance, or to use SQL authentication, set
the defaults once:

```ini
[/settings/mssql]
hostname = localhost\SQLEXPRESS   ; host, host\INSTANCE or host,port
; user     = nscp_monitor         ; omit user+password for Windows auth
; password = secret               ; stored as a masked setting
```

Any check can still override these per invocation (`server=`, `user=`,
`password=`, `database=`, `timeout=`).

### Permissions

Create a dedicated low-privilege login rather than using `sa`:

| What you want to monitor | Grant                                                   |
|--------------------------|---------------------------------------------------------|
| Instance health, databases, sizes | `VIEW SERVER STATE` and `VIEW ANY DATABASE`    |
| Backup ages              | `SELECT` on `msdb.dbo.backupset` (or `db_datareader` in msdb) |
| Agent jobs               | `SQLAgentReaderRole` in `msdb`                          |

A permission failure is reported as UNKNOWN with a `Query failed:` prefix and
the SQL error, so it is easy to tell apart from a real outage.

---

## 1. Are the SQL Server services running?

The first thing to check is that the service is up at all — this catches a
stopped instance faster and more cheaply than a failing connection.

```
check_service service=MSSQLSERVER service=SQLSERVERAGENT
```

For a **named instance** the service names are `MSSQL$<INSTANCE>` and
`SQLAgent$<INSTANCE>`:

```
check_service "service=MSSQL$SQLEXPRESS"
```

<!-- @formatter:off -->
!!! note
    SQL Server Agent is not available in Express edition, so leave
    `SQLSERVERAGENT` out of the check on Express hosts — otherwise it will
    always alert as missing.
<!-- @formatter:on -->

See [Service & Process Monitoring](service-monitoring.md) for the full
`check_service` syntax.

---

## 2. Is the instance answering?

A running service is not the same as a usable instance — it can be up but
refusing connections while a database recovers. `check_mssql` connects, runs a
query and reports what it found.

### Command

```
check_mssql
```

### Expected output (healthy)

```
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 31s
```

### Expected output (alert)

```
UNKNOWN: Failed to connect to SQL Server 'localhost': [08001/17] [Microsoft][ODBC SQL Server Driver][DBNETLIB]SQL Server does not exist or access denied.
```

### Customisation

**Alert if the instance restarted recently** (uptime accepts units):

```
check_mssql "warning=uptime < 1h"
```

**Check a named instance or a remote host:**

```
check_mssql "server=DBSRV01\SQLEXPRESS"
check_mssql "server=db1.example.com,1433" user=nscp_monitor password=...
```

---

## 3. Database state

```
check_mssql_databases
```

```
OK: All 4 databases are ONLINE
```

The default thresholds go **CRITICAL** on `SUSPECT`, `EMERGENCY` and
`RECOVERY_PENDING`, and **WARNING** on `RESTORING`, `RECOVERING` and `OFFLINE`.
If you deliberately keep databases offline, filter them out:

```
check_mssql_databases "filter=name != 'archive2019'"
```

---

## 4. Disk: data and log files

Log growth is the classic way a SQL Server fills a volume, so this deserves two
layers: the volume itself, and the log usage inside each database.

### The volumes

Data and log files usually live on dedicated drives. Check them with the normal
disk check:

```
check_drivesize drive=D: drive=L: "warn=free < 20%" "crit=free < 10%"
```

See [Disk Space Alerting](disk-space.md) for more options.

### Log usage inside each database

A transaction log that is 95% full will grow (or fail) even when the volume has
plenty of space — most often because a database is in `FULL` recovery but nobody
is taking log backups.

```
check_mssql_databases "warning=log_used_pct > 80" "critical=log_used_pct > 90" "detail-syntax=${name}: ${state} log ${log_used_pct}%" show-all
```

```
OK: master: ONLINE log 46%, model: ONLINE log 10%, msdb: ONLINE log 38%, tempdb: ONLINE log 7%
'master_log_used_pct'=46%;80;90 'model_log_used_pct'=10%;80;90 'msdb_log_used_pct'=38%;80;90 'tempdb_log_used_pct'=7%;80;90
```

### Log files approaching their growth cap

A log file with a `MAXSIZE` will stop the database dead when it reaches it. This
finds files nearing their cap and stays quiet when everything is set to
unlimited growth:

```
check_mssql_query "query=SELECT DB_NAME(database_id) AS db, CAST(100.0*size/NULLIF(NULLIF(max_size,-1),0) AS int) AS pct_of_max FROM sys.master_files WHERE type = 1 AND max_size > 0" "warning=pct_of_max > 80" "critical=pct_of_max > 90" "top-syntax=${status}: ${list}" "detail-syntax=${db}: ${pct_of_max}% of max log size" "empty-syntax=%(status): all log files are set to unlimited growth"
```

```
OK: msdb: 0% of max log size
```

### Current file sizes

Useful as a graphing metric rather than an alert (`max=-1` means unlimited
growth):

```
check_mssql_query "query=SELECT DB_NAME(database_id) AS db, name AS logical_name, CAST(size AS bigint)*8/1024 AS size_mb, CASE WHEN max_size = -1 THEN -1 ELSE CAST(max_size AS bigint)*8/1024 END AS max_mb FROM sys.master_files WHERE type = 1" "top-syntax=${status}: ${list}" "detail-syntax=${db}/${logical_name}: ${size_mb}MB max=${max_mb}"
```

```
OK: master/mastlog: 2MB max=-1, tempdb/templog: 8MB max=-1, model/modellog: 8MB max=-1, msdb/MSDBLog: 1MB max=2097152
```

---

## 5. Memory

SQL Server deliberately consumes as much memory as its `max server memory`
setting allows, so **host memory usage alone is a poor signal** — a healthy
instance will look "full". Monitor the host for the operating system's sake, and
SQL Server's own memory health separately.

### Host memory

```
check_memory "warn=used > 90%" "crit=used > 95%"
```

Set SQL Server's `max server memory` so the OS keeps a few GB, then this check
is meaningful again. See [Windows Server Health](windows-server-health.md).

### Page life expectancy

How long a page stays in the buffer pool. A sharp, sustained drop means SQL
Server is churning the cache and reading from disk.

```
check_mssql_query "query=SELECT cntr_value AS page_life_expectancy FROM sys.dm_os_performance_counters WHERE counter_name = 'Page life expectancy' AND object_name LIKE '%Buffer Manager%'" "warning=page_life_expectancy < 300" "critical=page_life_expectancy < 180" "top-syntax=${status}: ${list}"
```

```
CRITICAL: page_life_expectancy=31
```

The output above is from a **freshly started instance** — page life expectancy
counts up from zero after every restart, so expect it to alert for the first few
minutes after a service restart and treat a sustained low value as the real
signal.

### Buffer cache hit ratio

```
check_mssql_query "query=SELECT CAST(100.0 * (SELECT cntr_value FROM sys.dm_os_performance_counters WHERE counter_name = 'Buffer cache hit ratio' AND object_name LIKE '%Buffer Manager%') / NULLIF((SELECT cntr_value FROM sys.dm_os_performance_counters WHERE counter_name = 'Buffer cache hit ratio base' AND object_name LIKE '%Buffer Manager%'),0) AS int) AS cache_hit_pct" "warning=cache_hit_pct < 95" "critical=cache_hit_pct < 90" "top-syntax=${status}: ${list}"
```

```
OK: cache_hit_pct=100
```

### How much memory the SQL Server process is using

```
check_mssql_query "query=SELECT physical_memory_in_use_kb/1024 AS sql_memory_mb, memory_utilization_percentage AS mem_util_pct FROM sys.dm_os_process_memory" "top-syntax=${status}: ${list}"
```

```
OK: sql_memory_mb=4178, mem_util_pct=100
```

### Reading the same counters over PDH

Everything above is also exposed as Windows performance counters, which
`check_pdh` can read without a SQL login. Use the `SQLServer:` object prefix for
a default instance and `MSSQL$<INSTANCE>:` for a named one:

```
check_pdh "counter=\SQLServer:Buffer Manager\Page life expectancy" "warn=value < 300" "crit=value < 180"
```

<!-- @formatter:off -->
!!! tip
    Add `resolution=english` when the server runs a localised Windows — SQL
    Server's counter object names are translated, and the English-name lookup
    keeps your check portable. See [Performance Counter Monitoring](counters.md).
<!-- @formatter:on -->

---

## 6. Backups

`check_mssql_backup` reports the age of the last full, differential and log
backup for every database, straight from the backup history in `msdb`.

```
check_mssql_backup
```

```
CRITICAL: 3/3 databases (model: last full backup -1s ago, master: last full backup -1s ago, msdb: last full backup -1s ago)
'model_full_age'=-1s;259200;0 'master_full_age'=-1s;259200;0 'msdb_full_age'=-1s;259200;0
```

An age of **-1 means never backed up**, which the defaults treat as CRITICAL
along with any full backup older than 7 days (warning at 3 days).

**Log backups for FULL-recovery databases** — the check that catches a log
growing without bound:

```
check_mssql_backup "filter=recovery_model = 'FULL'" "warning=none" "critical=log_age < 0 or log_age > 1h" "detail-syntax=${name}: last log backup ${log_age}s ago"
```

<!-- @formatter:off -->
!!! note
    COPY_ONLY and snapshot backups are **ignored by default**, so an ad-hoc
    backup taken to refresh a dev environment cannot make a database look
    protected while the scheduled job is failing. Pass `include-copy-only=true`
    or `include-snapshot=true` to count them.
<!-- @formatter:on -->

---

## 7. Agent jobs

```
check_mssql_jobs
```

```
OK: No enabled SQL Agent jobs found
```

Only enabled jobs are considered, and the default thresholds go **CRITICAL** on
a failed last run and **WARNING** on `canceled` or `retry`. An instance with no
Agent jobs — including Express, which has no Agent at all — is OK, not an error.

**Catch a job that stopped running:**

```
check_mssql_jobs "warning=last_run_age > 25h"
```

**Catch a job that is stuck:**

```
check_mssql_jobs "warning=none" "critical=is_running = 1 and last_run_age > 6h"
```

`last_run_age` is measured from when the run *finished*, and `is_running`
reports jobs executing right now — a job in flight has not written its outcome
yet, so it keeps the status of its previous run.

---

## 8. Workload

These use `check_mssql_query`, which turns any query's columns into thresholds
and performance data.

**Connection count:**

```
check_mssql_query "query=SELECT COUNT(*) AS connections FROM sys.dm_exec_sessions WHERE is_user_process = 1" "warning=connections > 500" "critical=connections > 800" "top-syntax=${status}: ${list}"
```

```
OK: connections=4
```

**Blocked sessions:**

```
check_mssql_query "query=SELECT COUNT(*) AS blocked FROM sys.dm_exec_requests WHERE blocking_session_id <> 0" "warning=blocked > 0" "critical=blocked > 5" "top-syntax=${status}: ${list}"
```

```
OK: blocked=0
```

**Longest-running request:**

```
check_mssql_query "query=SELECT ISNULL(MAX(total_elapsed_time)/1000, 0) AS longest_query_s FROM sys.dm_exec_requests WHERE session_id > 50" "warning=longest_query_s > 300" "critical=longest_query_s > 900" "top-syntax=${status}: ${list}"
```

```
OK: longest_query_s=59
```

---

## 9. SQL Server errors in the event log

SQL Server writes severe errors to the Windows Application log under the source
`MSSQLSERVER` (or `MSSQL$<INSTANCE>`). This catches corruption, failed logins
and I/O errors that no status check will show:

```
check_eventlog file=Application "filter=provider = 'MSSQLSERVER' and level in ('error', 'critical') and written > -1h" "warn=count > 0"
```

See [Event Log Monitoring](event-log.md) for scanning intervals and how to avoid
re-alerting on the same event.

---

## Putting it all together

A complete `nsclient.ini` for a database server:

```ini
[/modules]
CheckMSSQL    = enabled
CheckSystem   = enabled
CheckDisk     = enabled
CheckEventLog = enabled
NRPEServer    = enabled

[/settings/mssql]
hostname = localhost
; Windows integrated auth as the service account - no credentials needed.

[/settings/NRPE/server]
allowed hosts = 10.0.0.1    ; IP of your monitoring server
port          = 5666
```

Service checks on the monitoring server:

```
check_nrpe -H <agent-ip> -c check_service --argument "service=MSSQLSERVER" --argument "service=SQLSERVERAGENT"
check_nrpe -H <agent-ip> -c check_mssql
check_nrpe -H <agent-ip> -c check_mssql_databases
check_nrpe -H <agent-ip> -c check_mssql_backup
check_nrpe -H <agent-ip> -c check_mssql_jobs
check_nrpe -H <agent-ip> -c check_drivesize --argument "drive=D:" --argument "drive=L:"
```

<!-- @formatter:off -->
!!! tip
    Backup and job checks only need to run a few times a day — schedule them far
    less often than the connectivity check to keep load off `msdb`. To run checks
    on a schedule and push the results instead of being polled, see
    [Passive Monitoring](passive-monitoring-nsca.md).
<!-- @formatter:on -->

### Suggested check intervals

| Check                     | Interval    | Why                                                     |
|---------------------------|-------------|----------------------------------------------------------|
| `check_service`, `check_mssql` | 1 min  | Outage detection                                        |
| `check_mssql_databases`   | 5 min       | State changes are rare but urgent                       |
| Memory / workload queries | 5 min       | Trends, not spikes                                      |
| `check_drivesize`         | 5–15 min    | Volumes fill gradually                                  |
| `check_mssql_jobs`        | 15 min      | Job outcomes only change when a job finishes            |
| `check_mssql_backup`      | 1–6 hours   | Scans backup history in `msdb`; thresholds are in days  |

---

## Next Steps

- [CheckMSSQL reference](../reference/windows/CheckMSSQL.md) — every command, keyword and option
- [Windows Server Health](windows-server-health.md) — CPU, memory, disk baseline for the host
- [Service & Process Monitoring](service-monitoring.md) — more `check_service` options
- [Performance Counter Monitoring](counters.md) — averaging SQL counters over time
- [Checks In Depth: Filters](../concepts/checks.md#3-filters-choosing-what-to-check) — the filter and threshold syntax used throughout
