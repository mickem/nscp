# CheckMSSQL

*Available on Windows only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

Check Microsoft SQL Server: connectivity, databases, backups, agent jobs and custom queries.

## Enable module

To enable this module and allow using the commands you need to add `CheckMSSQL = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckMSSQL = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckMSSQL module.

**List of commands:**

A list of all available queries (check commands)

| Command                                                                              | Description                                                                                  |
|--------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------|
| [check_mssql](#check_mssql) *(experimental)*                                         | Check SQL Server connectivity and health (version, edition, uptime).                         |
| [check_mssql_availability_groups](#check_mssql_availability_groups) *(experimental)* | Check Always On availability group replica and database health.                              |
| [check_mssql_backup](#check_mssql_backup) *(experimental)*                           | Check the age of the last full/differential/log backup per database.                         |
| [check_mssql_blocking](#check_mssql_blocking) *(experimental)*                       | Check for blocked sessions and blocking chains.                                              |
| [check_mssql_counters](#check_mssql_counters) *(experimental)*                       | Check engine performance counters: buffer cache, page life expectancy, batch and lock rates. |
| [check_mssql_databases](#check_mssql_databases) *(experimental)*                     | Check database state, recovery model and data/log size.                                      |
| [check_mssql_integrity](#check_mssql_integrity) *(experimental)*                     | Check suspect pages and the age of the last successful DBCC CHECKDB.                         |
| [check_mssql_jobs](#check_mssql_jobs) *(experimental)*                               | Check SQL Server Agent job status.                                                           |
| [check_mssql_query](#check_mssql_query) *(experimental)*                             | Run a custom T-SQL query and apply thresholds to the returned rows.                          |
| [check_mssql_sessions](#check_mssql_sessions) *(experimental)*                       | Check session and connection counts per database and login.                                  |
| [check_mssql_tempdb](#check_mssql_tempdb) *(experimental)*                           | Check tempdb space usage by consumer and volume headroom.                                    |
| [check_mssql_transactions](#check_mssql_transactions) *(experimental)*               | Check for old or leaked open transactions and long-running requests.                         |
| [check_mssql_waits](#check_mssql_waits) *(experimental)*                             | Check wait statistics by category and scheduler pressure.                                    |

### check_mssql

Check SQL Server connectivity and health (version, edition, uptime).

#### About `check_mssql`

`check_mssql` verifies that a Microsoft SQL Server instance is **reachable and
answering queries**: it connects over ODBC, reads `SERVERPROPERTY(...)` and
`sys.dm_os_sys_info` and reports version, patch level, edition and uptime.
A reachable server is OK by default; a failed connection is UNKNOWN with the
stable message prefix `Failed to connect to SQL Server '<server>':` followed by
the ODBC diagnostic (SQLSTATE and native error included).

Defaults: no warning/critical expressions — being able to connect is the health
signal. Add thresholds when needed, e.g. alert after a restart
(`warning=uptime < 1h`) or pin the expected major version
(`critical=version not like '16.'`).

Connection options shared by all CheckMSSQL commands: `server` (host,
`host\INSTANCE` or `host,port`), `database`, `user`/`password` (leave both empty
for Windows integrated authentication as the NSClient++ service account),
`driver`, `connection-string` (raw override), `timeout` (login),
`query-timeout`, `trust-cert` and `encrypt`. Defaults come from the
`/settings/mssql` section, so credentials can be configured once in
`nsclient.ini` instead of per check.

The ODBC driver is auto-detected (newest installed *ODBC Driver NN for SQL
Server* first, falling back to the legacy `SQL Server` driver that ships with
Windows). On the modern drivers `TrustServerCertificate=yes` is added by
default since most instances run with a self-signed certificate; use
`trust-cert=false` (and optionally `encrypt=yes`) when the server has a
properly trusted certificate.

**Jump to section:**

* [Sample Commands](#check_mssql_samples)
* [Command-line Arguments](#check_mssql_options)
* [Filter keywords](#check_mssql_filter_keys)


<a id="check_mssql_samples"></a>
#### Sample Commands

**Default check (local default instance, Windows authentication):**

```
check_mssql
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s
```

**Warn when the server restarted recently (age units supported):**

```
check_mssql "warning=uptime < 1h"
WARNING: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s|'DBSRV01_uptime'=144s;3600;0
```

**Custom output listing edition and patch level:**

```
check_mssql "top-syntax=%(status): %(list)" "detail-syntax=%(server_name) is running %(edition) (%(version) %(product_level))"
OK: DBSRV01 is running Developer Edition (64-bit) (16.0.4265.3 RTM)
```

**Against a named instance or remote host with SQL authentication:**

```
check_mssql "server=db1.example.com,1433" user=monitor password=...
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s
```

**When the server is unreachable (stable UNKNOWN contract):**

```
check_mssql
UNKNOWN: Failed to connect to SQL Server 'localhost': [08001/17] [Microsoft][ODBC SQL Server Driver][DBNETLIB]SQL Server does not exist or access denied., [01000/2] [Microsoft][ODBC SQL Server Driver][DBNETLIB]ConnectionOpen (Connect()).
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_mssql --argument "warning=uptime < 10m"
OK: DBSRV01: SQL Server 16.0.4265.3 RTM Developer Edition (64-bit), uptime 144s
```



<a id="check_mssql_options"></a>
#### Command-line Arguments

<a id="check_mssql_database"></a>
<a id="check_mssql_user"></a>
<a id="check_mssql_password"></a>
<a id="check_mssql_driver"></a>
<a id="check_mssql_connection-string"></a>
<a id="check_mssql_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                      | Default Value | Description                                                                                                    |
|---------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                    |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                        |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                    |               | Password for the SQL login.                                                                                    |
| driver                                      |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                           |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                     |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                      | Default Value                                                                        |
|-------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------|
| <a id="check_mssql_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                      |
| <a id="check_mssql_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                      |
| <a id="check_mssql_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                      |
| <a id="check_mssql_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                      |
| <a id="check_mssql_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                      |
| <a id="check_mssql_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                      |
| <a id="check_mssql_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                |
| <a id="check_mssql_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                |
| <a id="check_mssql_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                              |
| <a id="check_mssql_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                      |
| <a id="check_mssql_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                |
| <a id="check_mssql_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                    |
| <a id="check_mssql_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                   |
| <a id="check_mssql_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                      |
| <a id="check_mssql_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No server information returned                                            |
| <a id="check_mssql_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${server_name}: SQL Server ${version} ${product_level} ${edition}, uptime ${uptime}s |
| <a id="check_mssql_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${server_name}                                                                       |
| <a id="check_mssql_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                      |
| <a id="check_mssql_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                      |
| <a id="check_mssql_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                   |
| <a id="check_mssql_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_filter_keys"></a>
#### Filter keywords

| Option        | Description                                                         |
|---------------|---------------------------------------------------------------------|
| edition       | Edition, e.g. Express Edition (64-bit)                              |
| product_level | Patch level: RTM, SPn or CUn                                        |
| server_name   | Instance name (SERVERPROPERTY('ServerName'))                        |
| uptime        | Seconds since the server started (supports units, e.g. uptime < 1h) |
| version       | Product version, e.g. 16.0.1000.6                                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_availability_groups

Check Always On availability group replica and database health.

#### About `check_mssql_availability_groups`

`check_mssql_availability_groups` reports **Always On availability group
health** from `sys.dm_hadr_availability_replica_states` and
`sys.dm_hadr_database_replica_states`, producing one row per replica and per
availability database on each replica. Replicas that fall out of
synchronisation silently break the RPO the cluster was built for — this check
makes that state page before a failover discovers it.

Defaults: **WARNING** on `PARTIALLY_HEALTHY`, **CRITICAL** on `NOT_HEALTHY`,
`DISCONNECTED`, suspended data movement or a `RESOLVING` role. The health
states already encode Microsoft's own policy evaluation, so the defaults catch
broken replication without tuning; add `redo_queue`/`log_send_queue`
thresholds to alert on lag *before* it degrades health, sized to your RPO/RTO.

**Where to run it:** the primary sees the state of every replica including the
send/redo queues of all secondaries — pointing this check at the AG listener
or the primary gives the full picture. A secondary only exposes its local
replica state (remote replicas without state rows are deliberately omitted
rather than misreported as DISCONNECTED).

empty-state is **OK** (`No availability groups found`) so the check can be
rolled out fleet-wide, including instances without AGs. On hosts where an AG
**must** exist, set `empty-state=critical`: a dropped AG silently removes the
protection it provided.

Rights: `VIEW SERVER STATE`.

**Jump to section:**

* [Sample Commands](#check_mssql_availability_groups_samples)
* [Command-line Arguments](#check_mssql_availability_groups_options)
* [Filter keywords](#check_mssql_availability_groups_filter_keys)


<a id="check_mssql_availability_groups_samples"></a>
#### Sample Commands

**Default check (healthy AG):**

```
check_mssql_availability_groups
OK: All 1 availability replicas/databases are healthy
```

**Data movement suspended (or any NOT_HEALTHY state) — critical by default:**

```
check_mssql_availability_groups
CRITICAL: 1/1 availability replicas/databases (ag1/f80785925e72/agdb: NOT_HEALTHY)
```

**Show role, connection and synchronization state of every replica/database:**

```
check_mssql_availability_groups "warning=none" "critical=health = 'NOT_HEALTHY'" "top-syntax=${status}: ${list}" "detail-syntax=${name}: ${role} ${connected_state} ${health} (${sync_state})"
OK: ag1/f80785925e72/agdb: PRIMARY CONNECTED HEALTHY (SYNCHRONIZED)
```

**Alert on replication lag before it breaks the RPO/RTO (size units):**

```
check_mssql_availability_groups "warning=redo_queue > 500M" "critical=log_send_queue > 1G"
OK: All 1 availability replicas/databases are healthy|'ag1/f80785925e72/agdb_log_send_queue'=0B;0;1073741824 'ag1/f80785925e72/agdb_redo_queue'=0B;524288000;0
```

**No availability groups configured — OK by default, so the check can be
deployed fleet-wide:**

```
check_mssql_availability_groups
OK: No availability groups found
```

**On a host where an AG must exist, make its absence page:**

```
check_mssql_availability_groups empty-state=critical
CRITICAL: No availability groups found
```



<a id="check_mssql_availability_groups_options"></a>
#### Command-line Arguments

<a id="check_mssql_availability_groups_database"></a>
<a id="check_mssql_availability_groups_user"></a>
<a id="check_mssql_availability_groups_password"></a>
<a id="check_mssql_availability_groups_driver"></a>
<a id="check_mssql_availability_groups_connection-string"></a>
<a id="check_mssql_availability_groups_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                                          | Default Value | Description                                                                                                    |
|-----------------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_availability_groups_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                                        |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                            |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                                        |               | Password for the SQL login.                                                                                    |
| driver                                                          |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                               |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_availability_groups_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_availability_groups_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_availability_groups_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                                         |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_availability_groups_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_availability_groups_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_availability_groups_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_availability_groups_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                          | Default Value                                                                                        |
|---------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| <a id="check_mssql_availability_groups_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                      |
| <a id="check_mssql_availability_groups_warning"></a>[warning](../common-options.md#warning)                                     | health = 'PARTIALLY_HEALTHY'                                                                         |
| <a id="check_mssql_availability_groups_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                      |
| <a id="check_mssql_availability_groups_critical"></a>[critical](../common-options.md#critical)                                  | health = 'NOT_HEALTHY' or connected_state = 'DISCONNECTED' or is_suspended = 1 or role = 'RESOLVING' |
| <a id="check_mssql_availability_groups_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                      |
| <a id="check_mssql_availability_groups_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                      |
| <a id="check_mssql_availability_groups_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                |
| <a id="check_mssql_availability_groups_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                |
| <a id="check_mssql_availability_groups_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                                                   |
| <a id="check_mssql_availability_groups_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                      |
| <a id="check_mssql_availability_groups_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                |
| <a id="check_mssql_availability_groups_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                    |
| <a id="check_mssql_availability_groups_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} availability replicas/databases (${problem_list})               |
| <a id="check_mssql_availability_groups_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) availability replicas/databases are healthy                                  |
| <a id="check_mssql_availability_groups_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No availability groups found                                                              |
| <a id="check_mssql_availability_groups_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${health}                                                                                   |
| <a id="check_mssql_availability_groups_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                                              |
| <a id="check_mssql_availability_groups_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                      |
| <a id="check_mssql_availability_groups_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                      |
| <a id="check_mssql_availability_groups_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                   |
| <a id="check_mssql_availability_groups_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_availability_groups_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                            |
|-----------------|----------------------------------------------------------------------------------------------------------------------------------------|
| connected_state | CONNECTED or DISCONNECTED                                                                                                              |
| database        | Availability database name (empty for replica-level rows)                                                                              |
| db_health       | Database synchronization health (empty for replica-level rows)                                                                         |
| group           | Availability group name                                                                                                                |
| health          | Effective health: database health for database rows, replica health otherwise                                                          |
| is_local        | 1 if this replica is the instance being checked                                                                                        |
| is_suspended    | 1 if data movement for the database is suspended                                                                                       |
| log_send_queue  | Log not yet sent to the secondary in bytes, i.e. potential data loss/RPO lag (supports units, e.g. log_send_queue > 100M)              |
| name            | group/replica or group/replica/database                                                                                                |
| redo_queue      | Redo queue on the secondary in bytes: log received but not yet applied, i.e. failover/RTO lag (supports units, e.g. redo_queue > 500M) |
| replica         | Replica server name                                                                                                                    |
| replica_health  | Replica synchronization health: HEALTHY, PARTIALLY_HEALTHY or NOT_HEALTHY                                                              |
| role            | Replica role: PRIMARY, SECONDARY or RESOLVING (no primary, e.g. during failover)                                                       |
| sync_state      | Database synchronization state, e.g. SYNCHRONIZED or SYNCHRONIZING                                                                     |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_backup

Check the age of the last full/differential/log backup per database.

#### About `check_mssql_backup`

`check_mssql_backup` reports the **age of the most recent full, differential
and log backup** for every database, joining `sys.databases` with the backup
history in `msdb.dbo.backupset`. `tempdb` is excluded (it is never backed up).
A backup type that has never been taken is reported as age **-1**, so
"never backed up" can be caught explicitly (`full_age < 0`).

Defaults: **CRITICAL** when a database has never had a full backup or the last
one is older than 7 days (`full_age < 0 or full_age > 7d`), **WARNING** after
3 days (`full_age > 3d`). Log ages are not thresholded by default because they
only apply to `FULL`/`BULK_LOGGED` databases — combine with a filter as in the
log-backup sample. Age expressions accept units (`s`, `m`, `h`, `d`, `w`), and
the `-1` sentinel can be matched directly (`full_age = -1`).

**COPY_ONLY and snapshot backups are ignored by default.** A copy-only backup
(typically taken ad hoc to refresh a dev environment) and a snapshot backup
(VSS or a third-party agent) are not part of the scheduled restore chain, so
counting them would keep `full_age` looking fresh while the real backup job is
failing — the exact situation this check exists to catch. Pass
`include-copy-only=true` and/or `include-snapshot=true` to count them.

Rights: reading backup history requires access to `msdb.dbo.backupset`
(members of `sysadmin` see everything; otherwise grant the monitoring login
`SELECT` on that table). A permission failure surfaces as UNKNOWN with the
`Query failed:` prefix and the ODBC diagnostic.

Note that backup history is recorded by SQL Server itself, so backups taken by
third-party tools appear as long as they use the native `BACKUP` command (VDI
or T-SQL); snapshot-only solutions that bypass it will show as never backed up.

**Jump to section:**

* [Sample Commands](#check_mssql_backup_samples)
* [Command-line Arguments](#check_mssql_backup_options)
* [Filter keywords](#check_mssql_backup_filter_keys)


<a id="check_mssql_backup_samples"></a>
#### Sample Commands

**Default check (full backup required within 7 days, warn after 3):**

```
check_mssql_backup
OK: All 3 databases have recent backups|'model_full_age'=66s;259200;0 'master_full_age'=66s;259200;0 'msdb_full_age'=66s;259200;0
```

**A database that has never been backed up (age is -1):**

```
check_mssql_backup
CRITICAL: 1/4 databases (appdb: last full backup -1s ago)|'appdb_full_age'=-1s;259200;0 'model_full_age'=87s;259200;0 'master_full_age'=87s;259200;0 'msdb_full_age'=87s;259200;0
```

**Log-backup age for FULL-recovery databases:**

```
check_mssql_backup "filter=recovery_model = 'FULL'" "warning=none" "critical=log_age < 0 or log_age > 1h" "detail-syntax=${name}: last log backup ${log_age}s ago"
CRITICAL: 1/1 databases (model: last log backup -1s ago)|'model_log_age'=-1s;0;0
```

**Custom thresholds with age units:**

```
check_mssql_backup "warning=full_age > 25h" "critical=full_age < 0 or full_age > 2d"
OK: All 4 databases have recent backups|'appdb_full_age'=5s;90000;0 'model_full_age'=248s;90000;0 'master_full_age'=248s;90000;0 'msdb_full_age'=248s;90000;0
```

**Catch never-backed-up databases explicitly:**

```
check_mssql_backup "warning=none" "critical=full_age = -1"
CRITICAL: 1/4 databases (appdb: last full backup -1s ago)|'appdb_full_age'=-1s;0;-1 'model_full_age'=248s;0;-1 'master_full_age'=248s;0;-1 'msdb_full_age'=248s;0;-1
```

**A database whose only backup is COPY_ONLY still counts as never backed up:**

```
check_mssql_backup "filter=name = 'appdb'"
CRITICAL: 1/1 databases (appdb: last full backup -1s ago)|'appdb_full_age'=-1s;259200;0
```

**...unless copy-only backups are explicitly included:**

```
check_mssql_backup "filter=name = 'appdb'" include-copy-only=true
OK: All 1 databases have recent backups|'appdb_full_age'=55s;259200;0
```

**Exclude databases that are not backed up on purpose:**

```
check_mssql_backup "filter=name != 'model' and name != 'appdb'"
OK: All 2 databases have recent backups|'master_full_age'=248s;259200;0 'msdb_full_age'=248s;259200;0
```



<a id="check_mssql_backup_options"></a>
#### Command-line Arguments

<a id="check_mssql_backup_database"></a>
<a id="check_mssql_backup_user"></a>
<a id="check_mssql_backup_password"></a>
<a id="check_mssql_backup_driver"></a>
<a id="check_mssql_backup_connection-string"></a>
<a id="check_mssql_backup_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
        
        
| Option                                                     | Default Value | Description                                                                                                                                                                                      |
|------------------------------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [include-copy-only](#check_mssql_backup_include-copy-only) | false         | Count COPY_ONLY backups when computing the ages. Excluded by default: an ad-hoc copy-only backup does not belong to the scheduled restore chain, so counting it would hide a failing backup job. |
| [include-snapshot](#check_mssql_backup_include-snapshot)   | false         | Count snapshot (VSS/third-party agent) backups when computing the ages. Excluded by default for the same reason.                                                                                 |
| [server](#check_mssql_backup_server)                       | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                                                                                                      |
| database                                                   |               | Database (initial catalog) to connect to (default: the login's default database).                                                                                                                |
| user                                                       |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication.                                                                                   |
| password                                                   |               | Password for the SQL login.                                                                                                                                                                      |
| driver                                                     |               | ODBC driver to use (default: newest installed SQL Server driver).                                                                                                                                |
| connection-string                                          |               | Raw ODBC connection string; overrides all other connection options.                                                                                                                              |
| [timeout](#check_mssql_backup_timeout)                     | 10            | Connection (login) timeout in seconds.                                                                                                                                                           |
| [query-timeout](#check_mssql_backup_query-timeout)         | 30            | Query timeout in seconds.                                                                                                                                                                        |
| [trust-cert](#check_mssql_backup_trust-cert)               | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                                                                                                             |
| encrypt                                                    |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                                                                                                     |



<h5 id="check_mssql_backup_include-copy-only">include-copy-only:</h5>

Count COPY_ONLY backups when computing the ages. Excluded by default: an ad-hoc copy-only backup does not belong to the scheduled restore chain, so counting it would hide a failing backup job.

*Default Value:* `false`

<h5 id="check_mssql_backup_include-snapshot">include-snapshot:</h5>

Count snapshot (VSS/third-party agent) backups when computing the ages. Excluded by default for the same reason.

*Default Value:* `false`

<h5 id="check_mssql_backup_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_backup_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_backup_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_backup_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                    |
|--------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| <a id="check_mssql_backup_filter"></a>[filter](../common-options.md#filter)                                        |                                                                  |
| <a id="check_mssql_backup_warning"></a>[warning](../common-options.md#warning)                                     | full_age > 3d                                                    |
| <a id="check_mssql_backup_warn"></a>[warn](../common-options.md#warn)                                              |                                                                  |
| <a id="check_mssql_backup_critical"></a>[critical](../common-options.md#critical)                                  | full_age < 0 or full_age > 7d                                    |
| <a id="check_mssql_backup_crit"></a>[crit](../common-options.md#crit)                                              |                                                                  |
| <a id="check_mssql_backup_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                  |
| <a id="check_mssql_backup_debug"></a>[debug](../common-options.md#debug)                                           | false                                                            |
| <a id="check_mssql_backup_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                            |
| <a id="check_mssql_backup_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                          |
| <a id="check_mssql_backup_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                  |
| <a id="check_mssql_backup_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                            |
| <a id="check_mssql_backup_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                |
| <a id="check_mssql_backup_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} databases (${problem_list}) |
| <a id="check_mssql_backup_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) databases have recent backups            |
| <a id="check_mssql_backup_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No databases found                                    |
| <a id="check_mssql_backup_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: last full backup ${full_age}s ago                       |
| <a id="check_mssql_backup_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                          |
| <a id="check_mssql_backup_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                  |
| <a id="check_mssql_backup_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                  |
| <a id="check_mssql_backup_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                               |
| <a id="check_mssql_backup_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                  |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_backup_filter_keys"></a>
#### Filter keywords

| Option         | Description                                                                                  |
|----------------|----------------------------------------------------------------------------------------------|
| diff_age       | Seconds since the last differential backup finished, -1 = never (supports units)             |
| full_age       | Seconds since the last full backup finished, -1 = never (supports units, e.g. full_age > 7d) |
| log_age        | Seconds since the last log backup finished, -1 = never (supports units, e.g. log_age > 1h)   |
| name           | Database name                                                                                |
| recovery_model | Recovery model: SIMPLE, FULL or BULK_LOGGED                                                  |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_blocking

Check for blocked sessions and blocking chains.

#### About `check_mssql_blocking`

`check_mssql_blocking` reports **currently blocked sessions** from
`sys.dm_exec_requests`, producing one row per blocked session. Blocking chains
are the most common "the application is frozen" root cause on Windows
application stacks, and this check points straight at the session everyone is
waiting on.

A request counts as blocked only when `blocking_session_id` names a *different*
session. A parallel query reports its own session id while waiting on its own
threads (`CXPACKET`/`CXCONSUMER`), and the documented negative values (`-2`
orphaned distributed transaction, `-3` deferred recovery, `-4` latch state
undetermined) name no session at all; neither is one session blocking another,
so neither is reported here. A session with several requests blocked at once (a
`MultipleActiveResultSets` connection) is reported once, with its longest wait.

The check returns one row per blocked session.

Defaults: **WARNING** on `wait_time > 30` (blocking that is already
user-visible), **CRITICAL** on `wait_time > 300` (the application is frozen).
Momentary lock waits below the thresholds are still counted and listed in the
summary but do not alert. empty-state is **OK**: no blocked sessions is the
healthy case.

`root_blocker` resolves chains: when session 70 waits on 60 and 60 waits on
50, both rows report `root_blocker = 50` — kill or investigate that session
to release the whole chain. `blocker_idle = 1` identifies the classic
orphaned-transaction case: the blocker is sleeping while holding locks inside
an open transaction (an application that crashed or forgot to commit), which
never resolves by itself — e.g.
`warning=wait_time > 30s and blocker_idle = 1`.

This is a point-in-time check: deadlocks are resolved by the engine within
seconds and are therefore unlikely to be caught here — use the deadlock rate
of `check_mssql_counters` for that. Sustained blocking, which this check is
for, is exactly what deadlock detection does **not** resolve.

Rights: `VIEW SERVER STATE`.

**Jump to section:**

* [Sample Commands](#check_mssql_blocking_samples)
* [Command-line Arguments](#check_mssql_blocking_options)
* [Filter keywords](#check_mssql_blocking_filter_keys)


<a id="check_mssql_blocking_samples"></a>
#### Sample Commands

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



<a id="check_mssql_blocking_options"></a>
#### Command-line Arguments

<a id="check_mssql_blocking_database"></a>
<a id="check_mssql_blocking_user"></a>
<a id="check_mssql_blocking_password"></a>
<a id="check_mssql_blocking_driver"></a>
<a id="check_mssql_blocking_connection-string"></a>
<a id="check_mssql_blocking_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                               | Default Value | Description                                                                                                    |
|------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_blocking_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                             |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                 |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                             |               | Password for the SQL login.                                                                                    |
| driver                                               |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                    |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_blocking_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_blocking_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_blocking_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                              |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_blocking_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_blocking_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_blocking_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_blocking_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                                                                                        |
|----------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------|
| <a id="check_mssql_blocking_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                      |
| <a id="check_mssql_blocking_warning"></a>[warning](../common-options.md#warning)                                     | wait_time > 30                                                                                                       |
| <a id="check_mssql_blocking_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                      |
| <a id="check_mssql_blocking_critical"></a>[critical](../common-options.md#critical)                                  | wait_time > 300                                                                                                      |
| <a id="check_mssql_blocking_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                      |
| <a id="check_mssql_blocking_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                      |
| <a id="check_mssql_blocking_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                |
| <a id="check_mssql_blocking_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                |
| <a id="check_mssql_blocking_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                                                                   |
| <a id="check_mssql_blocking_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                      |
| <a id="check_mssql_blocking_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                |
| <a id="check_mssql_blocking_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                    |
| <a id="check_mssql_blocking_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} blocked sessions (${problem_list})                                              |
| <a id="check_mssql_blocking_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(count) blocked sessions, none over the thresholds                                                       |
| <a id="check_mssql_blocking_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No blocked sessions                                                                                       |
| <a id="check_mssql_blocking_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${database}/${login} blocked by session ${blocking_session_id} (${blocking_login}) for ${wait_time}s on ${wait_type} |
| <a id="check_mssql_blocking_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${session_id}                                                                                                        |
| <a id="check_mssql_blocking_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                      |
| <a id="check_mssql_blocking_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                      |
| <a id="check_mssql_blocking_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                   |
| <a id="check_mssql_blocking_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_blocking_filter_keys"></a>
#### Filter keywords

| Option              | Description                                                                |
|---------------------|----------------------------------------------------------------------------|
| blocker_idle        | 1 if the direct blocker is idle (holding locks with no active request)     |
| blocking_login      | Login of the direct blocker                                                |
| blocking_session_id | Session id of the direct blocker                                           |
| command             | Command the blocked request is executing, e.g. UPDATE                      |
| database            | Database the blocked request runs in (empty if unavailable)                |
| login               | Login of the blocked session                                               |
| root_blocker        | Session id at the head of this blocking chain                              |
| session_id          | Session id of the blocked request                                          |
| wait_time           | Seconds the request has been blocked (supports units, e.g. wait_time > 5m) |
| wait_type           | Wait type of the blocked request, e.g. LCK_M_X                             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_counters

Check engine performance counters: buffer cache, page life expectancy, batch and lock rates.

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

Any counter reads `-1` when unavailable on the instance or reset during the
sampling window. Guard low-value
thresholds with `>= 0` so missing data does not become a false low-value alert.
Use a separate `= -1` condition if missing data should alert.

All counters are emitted as **perfdata by default** (no threshold needed) —
this check is primarily a graphing source. There are **no default alert
thresholds** because healthy values scale with hardware and workload: page
life expectancy scales with buffer pool size (the old "300 seconds" rule
predates large-RAM servers; a common modern rule is 300s per 4 GB of buffer
pool), and batch rates are only meaningful against your own baseline. Typical
starting points:

```
check_mssql_counters "warning=(hit_ratio >= 0 and hit_ratio < 95) or (page_life_expectancy >= 0 and page_life_expectancy < 300)" "critical=(hit_ratio >= 0 and hit_ratio < 85) or (page_life_expectancy >= 0 and page_life_expectancy < 60)"
check_mssql_counters "warning=deadlocks > 0.1" "critical=lazy_writes > 20"
```

Rights: `VIEW SERVER STATE`.

**Jump to section:**

* [Sample Commands](#check_mssql_counters_samples)
* [Command-line Arguments](#check_mssql_counters_options)
* [Filter keywords](#check_mssql_counters_filter_keys)


<a id="check_mssql_counters_samples"></a>
#### Sample Commands

**Default check (informational, all counters emitted as perfdata):**

```
check_mssql_counters
OK: hit ratio 100%, PLE 4010s, 155.819 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=155.81854;0;0 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0 'mssql_hit_ratio'=100%;0;0 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_page_life_expectancy'=4010s;0;0 'mssql_recompilations'=0;0;0
```

The check samples the cumulative counters twice, one second apart, so it takes
about a second longer than the other CheckMSSQL commands.

**Alert on memory-pressure symptoms:**

```
check_mssql_counters "warning=(hit_ratio >= 0 and hit_ratio < 95) or (page_life_expectancy >= 0 and page_life_expectancy < 300)" "critical=(hit_ratio >= 0 and hit_ratio < 85) or (page_life_expectancy >= 0 and page_life_expectancy < 60)"
OK: hit ratio 100%, PLE 4012s, 148.368 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=148.36795;0;0 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0 'mssql_hit_ratio'=100%;95;85 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_page_life_expectancy'=4012s;300;60 'mssql_recompilations'=0;0;0
```

**A workload spike trips a batch-rate threshold:**

```
check_mssql_counters "warning=batch_requests > 100" "critical=batch_requests > 10000"
WARNING: hit ratio 100%, PLE 4061s, 154.303 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=154.30267;100;10000 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0 'mssql_hit_ratio'=100%;0;0 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_page_life_expectancy'=4061s;0;0 'mssql_recompilations'=0;0;0
```

**Watch locking health (pairs with `check_mssql_blocking`):**

```
check_mssql_counters "warning=lock_waits > 50" "critical=deadlocks > 0.5"
OK: hit ratio 100%, PLE 4102s, 0 batches/s, 0 compilations/s, 0 lazy writes/s, 0 lock waits/s, 0 deadlocks/s|'mssql_batch_requests'=0;0;0 'mssql_compilations'=0;0;0 'mssql_deadlocks'=0;0;0.5 'mssql_hit_ratio'=100%;0;0 'mssql_lazy_writes'=0;0;0 'mssql_lock_waits'=0;50;0 'mssql_page_life_expectancy'=4102s;0;0 'mssql_recompilations'=0;0;0
```



<a id="check_mssql_counters_options"></a>
#### Command-line Arguments

<a id="check_mssql_counters_database"></a>
<a id="check_mssql_counters_user"></a>
<a id="check_mssql_counters_password"></a>
<a id="check_mssql_counters_driver"></a>
<a id="check_mssql_counters_connection-string"></a>
<a id="check_mssql_counters_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                               | Default Value | Description                                                                                                    |
|------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_counters_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                             |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                 |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                             |               | Password for the SQL login.                                                                                    |
| driver                                               |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                    |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_counters_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_counters_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_counters_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                              |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_counters_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_counters_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_counters_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_counters_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                                                                                                                                                                          |
|----------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| <a id="check_mssql_counters_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                                                                                                  |
| <a id="check_mssql_counters_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                                                                                                  |
| <a id="check_mssql_counters_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                                                                                                                |
| <a id="check_mssql_counters_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                                                                                                  |
| <a id="check_mssql_counters_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                                                                                                      |
| <a id="check_mssql_counters_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                                                                                                     |
| <a id="check_mssql_counters_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No performance counters found                                                                                                                                                               |
| <a id="check_mssql_counters_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | hit ratio ${hit_ratio}%, PLE ${page_life_expectancy}s, ${batch_requests} batches/s, ${compilations} compilations/s, ${lazy_writes} lazy writes/s, ${lock_waits} lock waits/s, ${deadlocks} deadlocks/s |
| <a id="check_mssql_counters_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | mssql                                                                                                                                                                                                  |
| <a id="check_mssql_counters_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                                                                                                        |
| <a id="check_mssql_counters_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                                                                                                     |
| <a id="check_mssql_counters_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                                                                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_counters_filter_keys"></a>
#### Filter keywords

| Option               | Description                                                                                     |
|----------------------|-------------------------------------------------------------------------------------------------|
| batch_requests       | Batch requests per second (-1 if unavailable)                                                   |
| compilations         | SQL compilations per second (-1 if unavailable)                                                 |
| deadlocks            | Deadlocks per second across all lock types (-1 if unavailable)                                  |
| hit_ratio            | Buffer cache hit ratio in percent over the sampling window (-1 if unavailable)                  |
| lazy_writes          | Lazy writer pages flushed per second, sustained values mean memory pressure (-1 if unavailable) |
| lock_waits           | Lock requests per second that had to wait (-1 if unavailable)                                   |
| page_life_expectancy | Seconds a page stays in the buffer pool without being referenced (-1 if unavailable)            |
| recompilations       | SQL re-compilations per second (-1 if unavailable)                                              |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_databases

Check database state, recovery model and data/log size.

#### About `check_mssql_databases`

`check_mssql_databases` enumerates **every database on the instance** from
`sys.databases` (sizes from `sys.master_files`, log usage from
`DBCC SQLPERF(LOGSPACE)`) and produces one row per database, so availability
and capacity policies can be expressed with filter expressions.

Defaults: **CRITICAL** on broken states
(`state = 'SUSPECT' or state = 'EMERGENCY' or state = 'RECOVERY_PENDING'`),
**WARNING** on transitional or offline states
(`state = 'RESTORING' or state = 'RECOVERING' or state = 'OFFLINE'`).
If taking databases offline is routine, filter them away
(`filter=state != 'OFFLINE'`). empty-state is **UNKNOWN** (system databases
always exist, so an empty result indicates a broken query).

`log_used_pct` comes from `DBCC SQLPERF(LOGSPACE)`; if the login lacks
permission for it (requires `VIEW SERVER STATE`), the check still works and
reports `-1`. Perfdata is emitted for the size keywords referenced in your
warning/critical expressions.

`data_headroom`/`log_headroom` answer "how much further can this database grow
before it errors". The room is whichever limit binds first, the file's own
`max_size` cap or the volume it has to grow into (`sys.dm_os_volume_stats`) — a
log file with unlimited growth still carries the engine's 2TB cap, which is no
comfort on a volume with a gigabyte left. Files with autogrowth disabled
contribute `0`. Those per-file limits roll up in two steps:

- **Files sharing a volume** can only add that volume's free space between them,
  counted once. A default `tempdb` has one data file per core on a single
  volume, and counting its free space per file would report eight times the
  disk.
- **Within a filegroup** the volumes are summed, because proportional fill keeps
  allocating in sibling files until every file in the group is full — one
  legacy fixed-size file does not pin the whole filegroup — and files on
  separate volumes really do add up. Log files all roll up as one group.
- **Across filegroups** the most constrained one wins, because a full filegroup
  fails writes to its own objects however much room another filegroup has.

A threshold like
`"critical=data_headroom < 1G and data_headroom >= 0"` catches both a file
approaching its cap and a volume filling up — the `>= 0` guard excludes the
`-1` unknown sentinel. Size keywords accept plain byte counts and fractional
units, so `= -1`, `>= 0` and `< 1.5G` all work. Note that a
filegroup of fixed-size pre-allocated files reports headroom `0` by design —
free space *inside* the files is a different measure (`log_used_pct` covers it
for logs). Like `log_used_pct`, the keywords degrade to `-1` when
`dm_os_volume_stats` is unavailable, and a single file with no volume
information makes its whole database report `-1` rather than guess.

Volume statistics are collected only when a headroom keyword appears in a
filter, threshold, output template or extra perfdata request. Those checks read
file sizes and volume statistics together; ordinary state/size checks avoid
the per-file volume lookups.

**Jump to section:**

* [Sample Commands](#check_mssql_databases_samples)
* [Command-line Arguments](#check_mssql_databases_options)
* [Filter keywords](#check_mssql_databases_filter_keys)


<a id="check_mssql_databases_samples"></a>
#### Sample Commands

**Default check (all databases healthy):**

```
check_mssql_databases
OK: All 4 databases are ONLINE
```

**Size perfdata per database (size keywords accept units):**

```
check_mssql_databases "warning=data_size > 100G"
OK: All 4 databases are ONLINE|'master_data'=4456448B;107374182400;0 'model_data'=8388608B;107374182400;0 'msdb_data'=15925248B;107374182400;0 'tempdb_data'=67108864B;107374182400;0
```

**Report state, recovery model and log usage for every database:**

```
check_mssql_databases "top-syntax=${status}: ${list}" "detail-syntax=${name}: ${state} ${recovery_model} log used ${log_used_pct}%" "warning=none" "critical=none" show-all
OK: appdb: ONLINE FULL log used 5%, master: ONLINE SIMPLE log used 30%, model: ONLINE FULL log used 12%, msdb: ONLINE SIMPLE log used 66%, tempdb: ONLINE SIMPLE log used 7%
```

**Alert on log usage in FULL-recovery databases:**

```
check_mssql_databases "filter=recovery_model = 'FULL'" "warning=log_used_pct > 80" "critical=log_used_pct > 90"
OK: All 2 databases are ONLINE|'appdb_log_used_pct'=6%;80;90 'model_log_used_pct'=12%;80;90
```

**Ignore an intentionally offline database:**

```
check_mssql_databases "filter=name != 'archive2019'"
OK: All 5 databases are ONLINE
```

**Alert before a file hits its max_size or fills its volume (`>= 0` excludes
the `-1` unknown sentinel):**

```
check_mssql_databases "warning=data_headroom < 1K and data_headroom >= 0" "critical=log_headroom < 1K and log_headroom >= 0"
OK: All 5 databases are ONLINE|'appdb_data_headroom'=96468992B;1024;0 'appdb_log_headroom'=991888719872B;0;1024 'master_data_headroom'=991888719872B;1024;0 'master_log_headroom'=991888719872B;0;1024 'model_data_headroom'=991888719872B;1024;0 'model_log_headroom'=991888719872B;0;1024 'msdb_data_headroom'=991888719872B;1024;0 'msdb_log_headroom'=991888719872B;0;1024 'tempdb_data_headroom'=991888719872B;1024;0 'tempdb_log_headroom'=991888719872B;0;1024
```

Headroom is whichever runs out first, the file's `max_size` cap or the volume it
grows into. `appdb` here is capped at 100MB with ~92MB left, so its cap binds;
everything else is bounded by the ~924GB free on the volume — including the log
files, which carry the engine's 2TB cap but cannot reach it on this disk.

**A database approaching its size cap trips the threshold:**

```
check_mssql_databases "warning=data_headroom < 200M and data_headroom >= 0" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=${name}: data headroom ${data_headroom}B, log headroom ${log_headroom}B"
WARNING: appdb: data headroom 96468992B, log headroom 991888719872B, master: data headroom 991888719872B, log headroom 991888719872B, model: data headroom 991888719872B, log headroom 991888719872B, msdb: data headroom 991888719872B, log headroom 991888719872B, tempdb: data headroom 991888719872B, log headroom 991888719872B|'appdb_data_headroom'=96468992B;209715200;0 'master_data_headroom'=991888719872B;209715200;0 'model_data_headroom'=991888719872B;209715200;0 'msdb_data_headroom'=991888719872B;209715200;0 'tempdb_data_headroom'=991888719872B;209715200;0
```

**Flag hosts where headroom cannot be determined at all:**

```
check_mssql_databases "warning=data_headroom = -1" "critical=none"
OK: All 5 databases are ONLINE|'appdb_data_headroom'=96468992B;-1;0 'master_data_headroom'=991888719872B;-1;0 'model_data_headroom'=991888719872B;-1;0 'msdb_data_headroom'=991888719872B;-1;0 'tempdb_data_headroom'=991888719872B;-1;0
```



<a id="check_mssql_databases_options"></a>
#### Command-line Arguments

<a id="check_mssql_databases_database"></a>
<a id="check_mssql_databases_user"></a>
<a id="check_mssql_databases_password"></a>
<a id="check_mssql_databases_driver"></a>
<a id="check_mssql_databases_connection-string"></a>
<a id="check_mssql_databases_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                                | Default Value | Description                                                                                                    |
|-------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_databases_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                              |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                  |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                              |               | Password for the SQL login.                                                                                    |
| driver                                                |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                     |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_databases_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_databases_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_databases_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                               |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_databases_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_databases_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_databases_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_databases_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                | Default Value                                                          |
|-----------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------|
| <a id="check_mssql_databases_filter"></a>[filter](../common-options.md#filter)                                        |                                                                        |
| <a id="check_mssql_databases_warning"></a>[warning](../common-options.md#warning)                                     | state = 'RESTORING' or state = 'RECOVERING' or state = 'OFFLINE'       |
| <a id="check_mssql_databases_warn"></a>[warn](../common-options.md#warn)                                              |                                                                        |
| <a id="check_mssql_databases_critical"></a>[critical](../common-options.md#critical)                                  | state = 'SUSPECT' or state = 'EMERGENCY' or state = 'RECOVERY_PENDING' |
| <a id="check_mssql_databases_crit"></a>[crit](../common-options.md#crit)                                              |                                                                        |
| <a id="check_mssql_databases_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                        |
| <a id="check_mssql_databases_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                  |
| <a id="check_mssql_databases_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                  |
| <a id="check_mssql_databases_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                |
| <a id="check_mssql_databases_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                        |
| <a id="check_mssql_databases_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                  |
| <a id="check_mssql_databases_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                      |
| <a id="check_mssql_databases_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} databases (${problem_list})       |
| <a id="check_mssql_databases_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) databases are ONLINE                           |
| <a id="check_mssql_databases_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No databases found                                          |
| <a id="check_mssql_databases_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${state}                                                      |
| <a id="check_mssql_databases_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                |
| <a id="check_mssql_databases_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                        |
| <a id="check_mssql_databases_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                        |
| <a id="check_mssql_databases_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                     |
| <a id="check_mssql_databases_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_databases_filter_keys"></a>
#### Filter keywords

| Option         | Description                                                                                                                                                                                                                                                                                                                                          |
|----------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| data_headroom  | Remaining growth room for the data files in bytes: each file's room is the distance to its max_size but never more than the free space on its volume, summed per filegroup, and the most constrained filegroup wins; 0 when autogrowth is off, -1 if unavailable (supports units and plain integers, e.g. data_headroom < 5G and data_headroom >= 0) |
| data_size      | Total size of the data files in bytes (supports units, e.g. data_size > 10G)                                                                                                                                                                                                                                                                         |
| is_read_only   | 1 if the database is read-only                                                                                                                                                                                                                                                                                                                       |
| log_headroom   | Remaining growth room for the log files in bytes, same semantics as data_headroom - note that a log file with unlimited growth still carries the engine's 2TB cap, so this reports the volume's free space until the log approaches 2TB (supports units)                                                                                             |
| log_size       | Total size of the log files in bytes (supports units, e.g. log_size > 1G)                                                                                                                                                                                                                                                                            |
| log_used_pct   | Percentage of the log in use (-1 if unavailable)                                                                                                                                                                                                                                                                                                     |
| name           | Database name                                                                                                                                                                                                                                                                                                                                        |
| recovery_model | Recovery model: SIMPLE, FULL or BULK_LOGGED                                                                                                                                                                                                                                                                                                          |
| state          | Database state: ONLINE, RESTORING, RECOVERING, RECOVERY_PENDING, SUSPECT, EMERGENCY or OFFLINE                                                                                                                                                                                                                                                       |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_integrity

Check suspect pages and the age of the last successful DBCC CHECKDB.

#### About `check_mssql_integrity`

`check_mssql_integrity` closes the gap `check_mssql_backup` leaves open: **a
backup of a corrupt database restores a corrupt database.** It reports suspect
pages and the age of the last successful CHECKDB for each online database
(tempdb excluded).

Defaults: **CRITICAL** on `suspect_pages > 0` (corruption has occurred — act
now, while the backups that can repair it still exist), **WARNING** on
`checkdb_age > 14d or checkdb_age = -1` (corruption *would go unnoticed*).
Restored and repaired pages (event types 4/5/7) are excluded from the count,
so the alert clears once the damage is fixed.

Both halves degrade independently, and both sentinels are deliberately quiet by
default, because missing permission is not a finding. `checkdb_age = -2` means
the timestamp could not be read — that uses `DBCC DBINFO`, which requires
**sysadmin**, and it degrades per database, so a login with rights to some
databases still reports ages for those. `suspect_pages = -1` means
`msdb.dbo.suspect_pages` was out of reach, as on Azure SQL Database or for a
login with no msdb user; the CHECKDB half keeps working regardless. Add
`warning=checkdb_age = -2` or `warning=suspect_pages = -1` if you want missing
access itself flagged. Ages are computed against the **server's own clock**, so
an agent in a different timezone does not skew them.

The CHECKDB timestamps are collected in a single server-side batch (one `DBCC
DBINFO` per database, executed on the server), so an instance with hundreds of
databases costs one round trip rather than hundreds. If that batch fails, the
check logs a warning before falling back to individual queries; this slower
fallback can exceed the timeout on a large instance.

Note that `DBCC CHECKDB` itself is a heavy operation this check deliberately
never runs — it only reads the timestamp the last run left behind. Schedule
CHECKDB as a maintenance job (see `check_mssql_jobs` to alert when that job
fails or stops running).

Rights: `SELECT` on `msdb.dbo.suspect_pages`; `sysadmin` for `checkdb_age`.

**Jump to section:**

* [Sample Commands](#check_mssql_integrity_samples)
* [Command-line Arguments](#check_mssql_integrity_options)
* [Filter keywords](#check_mssql_integrity_filter_keys)


<a id="check_mssql_integrity_samples"></a>
#### Sample Commands

**Default check on an instance that never ran CHECKDB — warns:**

```
check_mssql_integrity
WARNING: 4/4 databases (appdb: checkdb age -1s, 0 suspect pages, master: checkdb age -1s, 0 suspect pages, model: checkdb age -1s, 0 suspect pages, msdb: checkdb age -1s, 0 suspect pages)|'appdb_checkdb_age'=-1s;1209600;0 'appdb_suspect_pages'=0;0;0 'master_checkdb_age'=-1s;1209600;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=-1s;1209600;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=-1s;1209600;0 'msdb_suspect_pages'=0;0;0
```

**After `DBCC CHECKDB` has run — ages track the last successful check:**

```
check_mssql_integrity
OK: No integrity problems found in 4 databases|'appdb_checkdb_age'=229s;1209600;0 'appdb_suspect_pages'=0;0;0 'master_checkdb_age'=230s;1209600;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=230s;1209600;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=230s;1209600;0 'msdb_suspect_pages'=0;0;0
```

**Tighter age for a nightly CHECKDB job (time units):**

```
check_mssql_integrity "warning=checkdb_age > 7d" "critical=suspect_pages > 0"
OK: No integrity problems found in 4 databases|'appdb_checkdb_age'=229s;604800;0 'appdb_suspect_pages'=0;0;0 'master_checkdb_age'=230s;604800;0 'master_suspect_pages'=0;0;0 'model_checkdb_age'=230s;604800;0 'model_suspect_pages'=0;0;0 'msdb_checkdb_age'=230s;604800;0 'msdb_suspect_pages'=0;0;0
```

**A monitoring login without sysadmin or msdb access — reports what it cannot
determine instead of failing the check:**

```
check_mssql_integrity "warning=none" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=${name}: age ${checkdb_age} pages ${suspect_pages}" show-all
OK: appdb: age -2 pages -1, master: age -1 pages -1, model: age -2 pages -1, msdb: age -1 pages -1
```

`checkdb_age` is `-2` for the databases whose boot page the login may not read
(`DBCC DBINFO` needs sysadmin) and `suspect_pages` is `-1` when
`msdb.dbo.suspect_pages` is out of reach. Neither sentinel trips the default
thresholds, so a least-privilege login degrades quietly rather than alerting or
turning the whole check UNKNOWN.



<a id="check_mssql_integrity_options"></a>
#### Command-line Arguments

<a id="check_mssql_integrity_database"></a>
<a id="check_mssql_integrity_user"></a>
<a id="check_mssql_integrity_password"></a>
<a id="check_mssql_integrity_driver"></a>
<a id="check_mssql_integrity_connection-string"></a>
<a id="check_mssql_integrity_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                                | Default Value | Description                                                                                                    |
|-------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_integrity_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                              |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                  |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                              |               | Password for the SQL login.                                                                                    |
| driver                                                |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                     |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_integrity_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_integrity_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_integrity_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                               |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_integrity_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_integrity_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_integrity_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_integrity_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                | Default Value                                                        |
|-----------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------|
| <a id="check_mssql_integrity_filter"></a>[filter](../common-options.md#filter)                                        |                                                                      |
| <a id="check_mssql_integrity_warning"></a>[warning](../common-options.md#warning)                                     | checkdb_age > 1209600 or checkdb_age = -1                            |
| <a id="check_mssql_integrity_warn"></a>[warn](../common-options.md#warn)                                              |                                                                      |
| <a id="check_mssql_integrity_critical"></a>[critical](../common-options.md#critical)                                  | suspect_pages > 0                                                    |
| <a id="check_mssql_integrity_crit"></a>[crit](../common-options.md#crit)                                              |                                                                      |
| <a id="check_mssql_integrity_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                      |
| <a id="check_mssql_integrity_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                |
| <a id="check_mssql_integrity_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                |
| <a id="check_mssql_integrity_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                              |
| <a id="check_mssql_integrity_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                      |
| <a id="check_mssql_integrity_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                |
| <a id="check_mssql_integrity_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                    |
| <a id="check_mssql_integrity_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} databases (${problem_list})     |
| <a id="check_mssql_integrity_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): No integrity problems found in %(count) databases         |
| <a id="check_mssql_integrity_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No databases found                                        |
| <a id="check_mssql_integrity_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: checkdb age ${checkdb_age}s, ${suspect_pages} suspect pages |
| <a id="check_mssql_integrity_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                              |
| <a id="check_mssql_integrity_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                      |
| <a id="check_mssql_integrity_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                      |
| <a id="check_mssql_integrity_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                   |
| <a id="check_mssql_integrity_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_integrity_filter_keys"></a>
#### Filter keywords

| Option        | Description                                                                                                                                              |
|---------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
| checkdb_age   | Seconds since the last successful DBCC CHECKDB, -1 = never checked, -2 = unknown/no access (supports units, e.g. checkdb_age > 14d)                      |
| name          | Database name                                                                                                                                            |
| suspect_pages | Pages in msdb.dbo.suspect_pages with unresolved 823/824/825 errors - any value above 0 means the engine has seen corruption, -1 = unknown/no msdb access |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_jobs

Check SQL Server Agent job status.

#### About `check_mssql_jobs`

`check_mssql_jobs` reports the **outcome of the last run of every SQL Server
Agent job** from `msdb.dbo.sysjobs` and the job-outcome rows of
`msdb.dbo.sysjobhistory`, producing one row per job. Disabled jobs are
excluded by the default filter (`enabled = 1`).

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

**Jump to section:**

* [Sample Commands](#check_mssql_jobs_samples)
* [Command-line Arguments](#check_mssql_jobs_options)
* [Filter keywords](#check_mssql_jobs_filter_keys)


<a id="check_mssql_jobs_samples"></a>
#### Sample Commands

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



<a id="check_mssql_jobs_options"></a>
#### Command-line Arguments

<a id="check_mssql_jobs_database"></a>
<a id="check_mssql_jobs_user"></a>
<a id="check_mssql_jobs_password"></a>
<a id="check_mssql_jobs_driver"></a>
<a id="check_mssql_jobs_connection-string"></a>
<a id="check_mssql_jobs_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                           | Default Value | Description                                                                                                    |
|--------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_jobs_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                         |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                             |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                         |               | Password for the SQL login.                                                                                    |
| driver                                           |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_jobs_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_jobs_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_jobs_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                          |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_jobs_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_jobs_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_jobs_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_jobs_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                                               |
|------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------|
| <a id="check_mssql_jobs_filter"></a>[filter](../common-options.md#filter)                                        | enabled = 1                                                 |
| <a id="check_mssql_jobs_warning"></a>[warning](../common-options.md#warning)                                     | last_run_status = 'canceled' or last_run_status = 'retry'   |
| <a id="check_mssql_jobs_warn"></a>[warn](../common-options.md#warn)                                              |                                                             |
| <a id="check_mssql_jobs_critical"></a>[critical](../common-options.md#critical)                                  | last_run_status = 'failed'                                  |
| <a id="check_mssql_jobs_crit"></a>[crit](../common-options.md#crit)                                              |                                                             |
| <a id="check_mssql_jobs_ok"></a>[ok](../common-options.md#ok)                                                    |                                                             |
| <a id="check_mssql_jobs_debug"></a>[debug](../common-options.md#debug)                                           | false                                                       |
| <a id="check_mssql_jobs_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                       |
| <a id="check_mssql_jobs_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                          |
| <a id="check_mssql_jobs_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                             |
| <a id="check_mssql_jobs_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                       |
| <a id="check_mssql_jobs_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                           |
| <a id="check_mssql_jobs_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} jobs (${problem_list}) |
| <a id="check_mssql_jobs_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) jobs succeeded                      |
| <a id="check_mssql_jobs_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No enabled SQL Agent jobs found                  |
| <a id="check_mssql_jobs_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${last_run_status}                                 |
| <a id="check_mssql_jobs_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                     |
| <a id="check_mssql_jobs_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                             |
| <a id="check_mssql_jobs_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                             |
| <a id="check_mssql_jobs_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                          |
| <a id="check_mssql_jobs_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                             |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_jobs_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                   |
|------------------|-----------------------------------------------------------------------------------------------|
| enabled          | 1 if the job is enabled                                                                       |
| is_running       | 1 if the job is executing right now                                                           |
| last_run_age     | Seconds since the last run finished, -1 = never ran (supports units, e.g. last_run_age > 25h) |
| last_run_outcome | Raw msdb run_status code of the last completed run (-1 = never ran)                           |
| last_run_status  | Outcome of the last completed run: failed, succeeded, retry, canceled or never                |
| name             | Job name                                                                                      |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_query

Run a custom T-SQL query and apply thresholds to the returned rows.

#### About `check_mssql_query`

`check_mssql_query` runs a **user-supplied T-SQL statement** and turns every
returned column into a filter keyword, so warning/critical expressions and
perfdata can be built from any query result — the SQL Server counterpart of
`check_wmi`. Each returned row is matched against the filter separately.

Every column of the result set is available as a keyword under its own name,
usable as string or number, and the whole row is available as the `line`
keyword, rendered as `column=value` pairs.

Numeric columns can be thresholded directly (`warning=sessions > 50`) and are
emitted as perfdata when referenced. Alias columns in SQL (`SELECT COUNT(*) AS
sessions ...`) to give keywords stable, expression-friendly names — avoid
spaces and punctuation in column aliases.

Defaults: no warning/critical expressions and `empty-state=ignored`; set
`empty-state=ok` (plus `top-syntax=${status}: ${list}`) for queries where "no
rows" means healthy, as in the long-running-requests example.

Only the first result set that has columns is read. A batch whose earlier
statements return row counts rather than rows (`UPDATE …; SELECT …` without
`SET NOCOUNT ON`) works — those are skipped — but later result sets are not
visible, so a query returning several is reduced to the first. A statement that
produces no result set at all is reported as UNKNOWN
(`Query returned no result set`) rather than a misleading empty OK.

The query runs with the connection's default database unless `database=` is
given; qualify object names (`msdb.dbo...`) or set `database=` when querying a
specific catalog. The statement runs under `query-timeout` (default 30s) so a
runaway query cannot hang the agent. The login used only needs SELECT/VIEW
SERVER STATE permissions appropriate to the query — prefer a low-privilege
monitoring login over `sa`.

**Jump to section:**

* [Sample Commands](#check_mssql_query_samples)
* [Command-line Arguments](#check_mssql_query_options)
* [Filter keywords](#check_mssql_query_filter_keys)


<a id="check_mssql_query_samples"></a>
#### Sample Commands

**List rows returned by a query (each column becomes a keyword):**

```
check_mssql_query "query=SELECT name, database_id FROM sys.databases"
name=master, database_id=1, name=tempdb, database_id=2, name=model, database_id=3, name=msdb, database_id=4
```

**Threshold on a computed value (user sessions):**

```
check_mssql_query "query=SELECT COUNT(*) AS sessions FROM sys.dm_exec_sessions WHERE is_user_process = 1" "warning=sessions > 50" "critical=sessions > 100" "top-syntax=${status}: ${list}"
OK: sessions=4
```

**Alert on rows matching a condition (long-running requests):**

```
check_mssql_query "query=SELECT session_id, total_elapsed_time FROM sys.dm_exec_requests WHERE total_elapsed_time > 60000" "critical=total_elapsed_time > 60000" "empty-state=ok" "top-syntax=${status}: ${list}" "detail-syntax=session ${session_id}: ${total_elapsed_time}ms" "empty-syntax=%(status): no long-running requests"
OK: no long-running requests
```

**A batch whose first statement returns a row count instead of rows:**

```
check_mssql_query "query=CREATE TABLE #t(i int); INSERT INTO #t VALUES(1),(2); SELECT COUNT(*) AS n FROM #t;" "top-syntax=${status}: ${list}"
OK: n=2
```

**Missing query (stable error contract):**

```
check_mssql_query
UNKNOWN: No query specified (use query=<T-SQL>)
```

**A statement that returns no result set at all:**

```
check_mssql_query "query=DECLARE @i int = 1;"
UNKNOWN: Query returned no result set (the statement produced no columns)
```



<a id="check_mssql_query_options"></a>
#### Command-line Arguments

<a id="check_mssql_query_query"></a>
<a id="check_mssql_query_database"></a>
<a id="check_mssql_query_user"></a>
<a id="check_mssql_query_password"></a>
<a id="check_mssql_query_driver"></a>
<a id="check_mssql_query_connection-string"></a>
<a id="check_mssql_query_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
        
| Option                                            | Default Value | Description                                                                                                    |
|---------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| query                                             |               | The T-SQL query to execute.                                                                                    |
| [server](#check_mssql_query_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                          |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                              |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                          |               | Password for the SQL login.                                                                                    |
| driver                                            |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                 |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_query_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_query_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_query_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                           |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_query_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_query_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_query_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_query_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                            | Default Value |
|-------------------------------------------------------------------------------------------------------------------|---------------|
| <a id="check_mssql_query_filter"></a>[filter](../common-options.md#filter)                                        |               |
| <a id="check_mssql_query_warning"></a>[warning](../common-options.md#warning)                                     |               |
| <a id="check_mssql_query_warn"></a>[warn](../common-options.md#warn)                                              |               |
| <a id="check_mssql_query_critical"></a>[critical](../common-options.md#critical)                                  |               |
| <a id="check_mssql_query_crit"></a>[crit](../common-options.md#crit)                                              |               |
| <a id="check_mssql_query_ok"></a>[ok](../common-options.md#ok)                                                    |               |
| <a id="check_mssql_query_debug"></a>[debug](../common-options.md#debug)                                           | false         |
| <a id="check_mssql_query_show-all"></a>[show-all](../common-options.md#show-all)                                  | false         |
| <a id="check_mssql_query_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored       |
| <a id="check_mssql_query_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |               |
| <a id="check_mssql_query_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false         |
| <a id="check_mssql_query_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,             |
| <a id="check_mssql_query_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${list}       |
| <a id="check_mssql_query_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |               |
| <a id="check_mssql_query_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |               |
| <a id="check_mssql_query_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | %(line)       |
| <a id="check_mssql_query_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         |               |
| <a id="check_mssql_query_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |               |
| <a id="check_mssql_query_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |               |
| <a id="check_mssql_query_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1            |
| <a id="check_mssql_query_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_query_filter_keys"></a>
#### Filter keywords

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_sessions

Check session and connection counts per database and login.

#### About `check_mssql_sessions`

`check_mssql_sessions` reports **session and connection counts aggregated per
database and login** from `sys.dm_exec_sessions` and `sys.dm_exec_connections`,
producing one row per (database, login) pair. Only user sessions are counted
(`is_user_process = 1`); system tasks are excluded. Connection-pool exhaustion
and runaway session counts precede most application outages, and this check
shows the growth per application login before the hard limit is hit.

There are **no default thresholds**: healthy session counts are entirely
workload-specific, so the check lists the pairs and stays OK until you add
thresholds, e.g. `warning=sessions > 100` sized to your application's
connection-pool limit, or `critical=max_idle > 12h` to catch leaked
connections that were never returned to the pool. Only sleeping/dormant
sessions count towards `max_idle` — a session busy executing a long request is
working, not leaked. `max_idle` is `-1` when the group has no idle session
that has completed a request (e.g. just-opened or all-running connections).

The check's own monitoring connection counts as one session (typically
`master/<monitoring login>`), so a live server always reports at least one
pair. The default label omits the database and slash when the database name
is unavailable. Physical connections exclude logical MARS connections.

Rights: `VIEW SERVER STATE` is required to see sessions other than your own;
without it the check still works but only reports the monitoring session. A
permission failure on the DMVs themselves surfaces as UNKNOWN with the
`Query failed:` prefix.

**Jump to section:**

* [Sample Commands](#check_mssql_sessions_samples)
* [Command-line Arguments](#check_mssql_sessions_options)
* [Filter keywords](#check_mssql_sessions_filter_keys)


<a id="check_mssql_sessions_samples"></a>
#### Sample Commands

**Default check (informational listing per database/login):**

```
check_mssql_sessions
OK: appdb/app: 3 sessions (3 running), master/NT AUTHORITY\SYSTEM: 1 sessions (0 running), master/sa: 1 sessions (1 running)
```

**Alert before the connection pool is exhausted (with perfdata):**

```
check_mssql_sessions "warning=sessions > 100" "critical=sessions > 200"
OK: appdb/app: 3 sessions (3 running), master/NT AUTHORITY\SYSTEM: 1 sessions (0 running), master/sa: 1 sessions (1 running)|'appdb/app_sessions'=3;100;200 'master/NT AUTHORITY\SYSTEM_sessions'=1;100;200 'master/sa_sessions'=1;100;200
```

**A runaway session count trips the threshold:**

```
check_mssql_sessions "warning=sessions > 2"
WARNING: appdb/app: 3 sessions (3 running), master/NT AUTHORITY\SYSTEM: 1 sessions (0 running), master/sa: 1 sessions (1 running)|'appdb/app_sessions'=3;2;0 'master/NT AUTHORITY\SYSTEM_sessions'=1;2;0 'master/sa_sessions'=1;2;0
```

**Catch leaked connections that have been idle for half a day (time units):**

```
check_mssql_sessions "critical=max_idle > 12h"
OK: appdb/app: 3 sessions (0 running), master/sa: 1 sessions (1 running)|'appdb/app_max_idle'=139s;0;43200 'master/sa_max_idle'=-1s;0;43200
```

Only sleeping/dormant sessions count towards `max_idle`: the `master/sa` group
is the monitoring session itself (running, so excluded) and reports the `-1`
unknown sentinel, while the three idle `app` sessions report a real idle age.

**Watch a single application login:**

```
check_mssql_sessions "filter=login = 'app'" "warning=sessions > 100"
OK: appdb/app: 3 sessions (3 running)|'appdb/app_sessions'=3;100;0
```



<a id="check_mssql_sessions_options"></a>
#### Command-line Arguments

<a id="check_mssql_sessions_database"></a>
<a id="check_mssql_sessions_user"></a>
<a id="check_mssql_sessions_password"></a>
<a id="check_mssql_sessions_driver"></a>
<a id="check_mssql_sessions_connection-string"></a>
<a id="check_mssql_sessions_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                               | Default Value | Description                                                                                                    |
|------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_sessions_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                             |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                 |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                             |               | Password for the SQL login.                                                                                    |
| driver                                               |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                    |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_sessions_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_sessions_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_sessions_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                              |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_sessions_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_sessions_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_sessions_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_sessions_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                      |
|----------------------------------------------------------------------------------------------------------------------|----------------------------------------------------|
| <a id="check_mssql_sessions_filter"></a>[filter](../common-options.md#filter)                                        |                                                    |
| <a id="check_mssql_sessions_warning"></a>[warning](../common-options.md#warning)                                     |                                                    |
| <a id="check_mssql_sessions_warn"></a>[warn](../common-options.md#warn)                                              |                                                    |
| <a id="check_mssql_sessions_critical"></a>[critical](../common-options.md#critical)                                  |                                                    |
| <a id="check_mssql_sessions_crit"></a>[crit](../common-options.md#crit)                                              |                                                    |
| <a id="check_mssql_sessions_ok"></a>[ok](../common-options.md#ok)                                                    |                                                    |
| <a id="check_mssql_sessions_debug"></a>[debug](../common-options.md#debug)                                           | false                                              |
| <a id="check_mssql_sessions_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                              |
| <a id="check_mssql_sessions_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                            |
| <a id="check_mssql_sessions_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                    |
| <a id="check_mssql_sessions_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                              |
| <a id="check_mssql_sessions_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                  |
| <a id="check_mssql_sessions_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                 |
| <a id="check_mssql_sessions_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                    |
| <a id="check_mssql_sessions_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No user sessions found                  |
| <a id="check_mssql_sessions_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${sessions} sessions (${running} running) |
| <a id="check_mssql_sessions_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                            |
| <a id="check_mssql_sessions_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                    |
| <a id="check_mssql_sessions_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                    |
| <a id="check_mssql_sessions_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                 |
| <a id="check_mssql_sessions_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                    |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_sessions_filter_keys"></a>
#### Filter keywords

| Option      | Description                                                                                                                                                      |
|-------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| connections | Number of physical connections for this database/login pair                                                                                                      |
| database    | Database the sessions are connected to (empty if unavailable)                                                                                                    |
| idle        | Sessions that are sleeping or dormant                                                                                                                            |
| login       | Login name the sessions authenticated as                                                                                                                         |
| max_idle    | Seconds since the most idle sleeping/dormant session last completed a request (running sessions are excluded), -1 = unknown (supports units, e.g. max_idle > 2h) |
| name        | Database/login label, or just the login when the database is unavailable                                                                                         |
| running     | Sessions currently executing a request                                                                                                                           |
| sessions    | Number of sessions for this database/login pair                                                                                                                  |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_tempdb

Check tempdb space usage by consumer and volume headroom.

#### About `check_mssql_tempdb`

`check_mssql_tempdb` reports **tempdb space usage split by consumer** from
`tempdb.sys.dm_db_file_space_usage`, plus the free space on the tempdb
volume. tempdb is the instance-wide shared resource: when it fills, every
database on the instance starts failing — and the split tells you *what* is
filling it: version-store growth can mean a long snapshot transaction, internal
objects can mean query spills, and user objects cover temp tables and table
variables.

All values are emitted as **perfdata by default** — trending the split is how
tempdb sizing problems are diagnosed. `volume_free` uses `MIN` across the
data-file volumes because the files on the fullest volume hit the wall first.

There are **no default alert thresholds**: `used_pct` measures the *current*
allocation, which is soft while autogrowth is enabled, and pre-sized tempdb
capacity is a sizing decision. Meaningful starting points:

```
check_mssql_tempdb "warning=used_pct > 80" "critical=used_pct > 90 or volume_free < 1G and volume_free >= 0"
check_mssql_tempdb "warning=version_store > 5G"
```

For a pre-sized tempdb (autogrowth off) `used_pct` is a hard limit and the
80/90 thresholds are appropriate. With autogrowth on, `volume_free` is the
real ceiling. The `volume_free >= 0` guard keeps the `-1` unknown sentinel
(no `sys.dm_os_volume_stats` access) from tripping the low-space threshold. A
growing `version_store` is best cross-checked with `check_mssql_transactions`
— the pinning transaction shows up there.

Rights: `VIEW SERVER STATE` (the `volume_free` keyword additionally uses
`sys.dm_os_volume_stats`; if unavailable the check still works and reports
`-1`).

**Jump to section:**

* [Sample Commands](#check_mssql_tempdb_samples)
* [Command-line Arguments](#check_mssql_tempdb_options)
* [Filter keywords](#check_mssql_tempdb_filter_keys)


<a id="check_mssql_tempdb_samples"></a>
#### Sample Commands

**Default check (informational, everything as perfdata):**

```
check_mssql_tempdb
OK: tempdb 4% used of 67108864B (version store 0B, user 1572864B, internal 0B)|'tempdb_free'=64094208B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=67108864B;0;0 'tempdb_used'=3014656B;0;0 'tempdb_used_pct'=4%;0;0 'tempdb_user_objects'=1572864B;0;0 'tempdb_version_store'=0B;0;0 'tempdb_volume_free'=992251518976B;0;0
```

**Usage and volume thresholds:**

```
check_mssql_tempdb "warning=used_pct > 80" "critical=used_pct > 90 or volume_free < 1G and volume_free >= 0"
OK: tempdb 4% used of 67108864B (version store 0B, user 1507328B, internal 0B)|'tempdb_free'=64225280B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=67108864B;0;0 'tempdb_used'=2883584B;0;0 'tempdb_used_pct'=4%;80;90 'tempdb_user_objects'=1507328B;0;0 'tempdb_version_store'=0B;0;0 'tempdb_volume_free'=991920017408B;0;1073741824
```

**During temp-table pressure — the split shows the consumer:**

```
check_mssql_tempdb "warning=used_pct > 80" "critical=used_pct > 95"
OK: tempdb 24% used of 603979776B (version store 0B, user 144310272B, internal 0B)|'tempdb_free'=457900032B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=603979776B;0;0 'tempdb_used'=146079744B;0;0 'tempdb_used_pct'=24%;80;95 'tempdb_user_objects'=144310272B;0;0 'tempdb_version_store'=0B;0;0 'tempdb_volume_free'=991714615296B;0;0
```

Here a session holding a large temp table grew tempdb from 64MB to 576MB and
`user_objects` accounts for nearly all of the usage — a temp-table problem,
not a version-store or spill problem.

**Catch a snapshot transaction pinning the version store:**

```
check_mssql_tempdb "warning=version_store > 5G"
OK: tempdb 24% used of 603979776B (version store 0B, user 144310272B, internal 0B)|'tempdb_free'=457900032B;0;0 'tempdb_internal_objects'=0B;0;0 'tempdb_size'=603979776B;0;0 'tempdb_used'=146079744B;0;0 'tempdb_used_pct'=24%;0;0 'tempdb_user_objects'=144310272B;0;0 'tempdb_version_store'=0B;5368709120;0 'tempdb_volume_free'=991714598912B;0;0
```



<a id="check_mssql_tempdb_options"></a>
#### Command-line Arguments

<a id="check_mssql_tempdb_database"></a>
<a id="check_mssql_tempdb_user"></a>
<a id="check_mssql_tempdb_password"></a>
<a id="check_mssql_tempdb_driver"></a>
<a id="check_mssql_tempdb_connection-string"></a>
<a id="check_mssql_tempdb_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                             | Default Value | Description                                                                                                    |
|----------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_tempdb_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                           |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                               |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                           |               | Password for the SQL login.                                                                                    |
| driver                                             |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                  |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_tempdb_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_tempdb_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_tempdb_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                            |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_tempdb_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_tempdb_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_tempdb_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_tempdb_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                                                                                |
|--------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------|
| <a id="check_mssql_tempdb_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                              |
| <a id="check_mssql_tempdb_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                              |
| <a id="check_mssql_tempdb_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                              |
| <a id="check_mssql_tempdb_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                                              |
| <a id="check_mssql_tempdb_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                              |
| <a id="check_mssql_tempdb_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                              |
| <a id="check_mssql_tempdb_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                        |
| <a id="check_mssql_tempdb_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                        |
| <a id="check_mssql_tempdb_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                                      |
| <a id="check_mssql_tempdb_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                              |
| <a id="check_mssql_tempdb_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                        |
| <a id="check_mssql_tempdb_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                            |
| <a id="check_mssql_tempdb_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                           |
| <a id="check_mssql_tempdb_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                              |
| <a id="check_mssql_tempdb_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No tempdb information returned                                                                                    |
| <a id="check_mssql_tempdb_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | tempdb ${used_pct}% used of ${size}B (version store ${version_store}B, user ${user_objects}B, internal ${internal_objects}B) |
| <a id="check_mssql_tempdb_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | tempdb                                                                                                                       |
| <a id="check_mssql_tempdb_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                              |
| <a id="check_mssql_tempdb_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                              |
| <a id="check_mssql_tempdb_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                           |
| <a id="check_mssql_tempdb_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                              |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_tempdb_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                                                                                        |
|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| free             | Unallocated bytes within the tempdb files (supports units)                                                                                                         |
| internal_objects | Bytes held by internal objects: sort/hash spills and work tables (supports units)                                                                                  |
| size             | Allocated tempdb data-file bytes (supports units, e.g. size > 50G)                                                                                                 |
| used             | Bytes in use within the tempdb files (supports units)                                                                                                              |
| used_pct         | Percent of the current tempdb allocation in use                                                                                                                    |
| user_objects     | Bytes held by user objects: temp tables and table variables (supports units)                                                                                       |
| version_store    | Bytes held by the version store; growth means a long-running snapshot transaction is pinning it (supports units)                                                   |
| volume_free      | Free bytes on the most constrained volume holding a tempdb data file, -1 = unknown (supports units and plain integers, e.g. volume_free < 5G and volume_free >= 0) |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_transactions

Check for old or leaked open transactions and long-running requests.

#### About `check_mssql_transactions`

`check_mssql_transactions` reports **open user transactions** from
`sys.dm_tran_session_transactions` / `sys.dm_tran_active_transactions`, one
row per session. An old open transaction blocks log truncation (the log
grows until the disk fills) and pins version-store cleanup (tempdb grows) —
it is the *precursor* to two different outages, hours before either happens,
and `check_mssql_blocking` only sees it once another session collides with it.

A session holding more than one open transaction (a distributed transaction
enlisting several, for instance) is reported once, with its **oldest** — that is
the one pinning log truncation and the version store. Likewise, a session
running several concurrent requests on one transaction (a
`MultipleActiveResultSets` connection) is one row, described by its
longest-running request.

The check returns one row per session with an open transaction.

Defaults: **WARNING** on `transaction_age > 1800 or is_idle = 1 and
transaction_age > 300`, **CRITICAL** on `transaction_age > 7200`. The idle
case gets the much shorter fuse deliberately: an open transaction whose
session is not executing anything is the classic **leaked transaction** — an
application that crashed, timed out, or forgot to `COMMIT` — and it never
resolves by itself; the working case gets half an hour before it warns.
Legitimate long batch jobs can be excluded with a filter, e.g.
`"filter=login != 'etl_service'"`.

A long-running *query* also shows up here (every user request runs inside a
transaction), with `request_age` telling you how long the current statement
has been executing versus how long the transaction has been open.

The check excludes its own session, so an idle server reports
`OK: No open transactions`.

Rights: `VIEW SERVER STATE`.

**Jump to section:**

* [Sample Commands](#check_mssql_transactions_samples)
* [Command-line Arguments](#check_mssql_transactions_options)
* [Filter keywords](#check_mssql_transactions_filter_keys)


<a id="check_mssql_transactions_samples"></a>
#### Sample Commands

**Default check (healthy — nothing open):**

```
check_mssql_transactions
OK: No open transactions
```

**Default check with a leaked transaction — the idle 5-minute fuse fires while
an equally old but actively working transaction stays quiet:**

```
check_mssql_transactions
WARNING: 1/2 open transactions (session 54 (appdb/sa) open for 349s (idle: 1))|'54_transaction_age'=349s;1800;7200 '55_transaction_age'=349s;1800;7200
```

**List everything with full detail (idle flag, request age and command):**

```
check_mssql_transactions "warning=none" "critical=transaction_age > 4h" "top-syntax=${status}: ${list}" "detail-syntax=session ${session_id} (${database}/${login}): ${transaction_name} open ${transaction_age}s, idle=${is_idle}, request=${request_age}s ${command}"
OK: session 54 (appdb/sa): user_transaction open 349s, idle=1, request=-1s , session 55 (master/sa): user_transaction open 349s, idle=0, request=349s WAITFOR|'54_transaction_age'=349s;0;14400 '55_transaction_age'=349s;0;14400
```

Session 54 is the leak (open transaction, no active request); session 55 is a
long-running but working request.

**Tighter thresholds (time units):**

```
check_mssql_transactions "warning=transaction_age > 2m or is_idle = 1 and transaction_age > 1m" "critical=transaction_age > 2h"
WARNING: 2/2 open transactions (session 54 (appdb/sa) open for 349s (idle: 1), session 55 (master/sa) open for 349s (idle: 0))|'54_transaction_age'=349s;120;7200 '55_transaction_age'=349s;120;7200
```

**Page only on leaked transactions:**

```
check_mssql_transactions "warning=none" "critical=is_idle = 1 and transaction_age > 1m" "detail-syntax=${database}/${login} session ${session_id} idle in transaction for ${transaction_age}s"
CRITICAL: 1/2 open transactions (appdb/sa session 54 idle in transaction for 349s)|'54_transaction_age'=349s;0;60 '55_transaction_age'=349s;0;60
```



<a id="check_mssql_transactions_options"></a>
#### Command-line Arguments

<a id="check_mssql_transactions_database"></a>
<a id="check_mssql_transactions_user"></a>
<a id="check_mssql_transactions_password"></a>
<a id="check_mssql_transactions_driver"></a>
<a id="check_mssql_transactions_connection-string"></a>
<a id="check_mssql_transactions_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                                   | Default Value | Description                                                                                                    |
|----------------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_transactions_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                                 |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                                     |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                                 |               | Password for the SQL login.                                                                                    |
| driver                                                   |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                        |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_transactions_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_transactions_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_transactions_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                                  |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_transactions_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_transactions_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_transactions_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_transactions_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                   | Default Value                                                                                |
|--------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------|
| <a id="check_mssql_transactions_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                              |
| <a id="check_mssql_transactions_warning"></a>[warning](../common-options.md#warning)                                     | transaction_age > 1800 or is_idle = 1 and transaction_age > 300                              |
| <a id="check_mssql_transactions_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                              |
| <a id="check_mssql_transactions_critical"></a>[critical](../common-options.md#critical)                                  | transaction_age > 7200                                                                       |
| <a id="check_mssql_transactions_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                              |
| <a id="check_mssql_transactions_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                              |
| <a id="check_mssql_transactions_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                        |
| <a id="check_mssql_transactions_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                        |
| <a id="check_mssql_transactions_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                                           |
| <a id="check_mssql_transactions_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                              |
| <a id="check_mssql_transactions_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                        |
| <a id="check_mssql_transactions_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                            |
| <a id="check_mssql_transactions_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_count}/${count} open transactions (${problem_list})                     |
| <a id="check_mssql_transactions_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(count) open transactions, none over the thresholds                              |
| <a id="check_mssql_transactions_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No open transactions                                                              |
| <a id="check_mssql_transactions_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | session ${session_id} (${database}/${login}) open for ${transaction_age}s (idle: ${is_idle}) |
| <a id="check_mssql_transactions_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${session_id}                                                                                |
| <a id="check_mssql_transactions_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                              |
| <a id="check_mssql_transactions_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                              |
| <a id="check_mssql_transactions_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                           |
| <a id="check_mssql_transactions_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                              |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_transactions_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                             |
|------------------|-----------------------------------------------------------------------------------------|
| command          | Command of the active request (empty when the session is idle)                          |
| database         | Database context of the session                                                         |
| is_idle          | 1 if the transaction is open but the session has no active request (leaked transaction) |
| login            | Login that owns the transaction                                                         |
| request_age      | Seconds the current request has been executing, -1 = no active request (supports units) |
| session_id       | Session id owning the transaction                                                       |
| transaction_age  | Seconds since the transaction began (supports units, e.g. transaction_age > 30m)        |
| transaction_name | Transaction name, e.g. user_transaction or implicit_transaction                         |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mssql_waits

Check wait statistics by category and scheduler pressure.

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

**Jump to section:**

* [Sample Commands](#check_mssql_waits_samples)
* [Command-line Arguments](#check_mssql_waits_options)
* [Filter keywords](#check_mssql_waits_filter_keys)


<a id="check_mssql_waits_samples"></a>
#### Sample Commands

**Default check (informational, full wait profile as perfdata):**

```
check_mssql_waits
OK: 0 runnable tasks on 16 schedulers, 0 queued; waits ms/s: cpu 0, io 0, log 28.6807, lock 0, memory 0, signal 9.375%|'mssql_cpu_waits'=0;0;0 'mssql_io_waits'=0;0;0 'mssql_latch_waits'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_log_waits'=28.68068;0;0 'mssql_memory_waits'=0;0;0 'mssql_network_waits'=0;0;0 'mssql_other_waits'=1.91204;0;0 'mssql_runnable_tasks'=0;0;0 'mssql_signal_wait_pct'=9.375%;0;0 'mssql_total_waits'=30.59273;0;0 'mssql_work_queue'=0;0;0 'mssql_workers'=44;0;0
```

Here a write-heavy workload shows up as transaction-log waits (`WRITELOG`,
~29 ms of wait per second) while every other category is quiet — a storage
question, not a locking or CPU one. The check samples the cumulative wait
statistics twice, one second apart, so it takes about a second longer than the
other CheckMSSQL commands.

**Alert on CPU pressure and worker starvation:**

```
check_mssql_waits "warning=work_queue > 0 or signal_wait_pct > 25" "critical=work_queue > 10"
OK: 0 runnable tasks on 16 schedulers, 0 queued; waits ms/s: cpu 0, io 0, log 23.8569, lock 0, memory 0, signal 7.40741%|'mssql_cpu_waits'=0;0;0 'mssql_io_waits'=0;0;0 'mssql_latch_waits'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_log_waits'=23.85685;0;0 'mssql_memory_waits'=0;0;0 'mssql_network_waits'=0;0;0 'mssql_other_waits'=2.9821;0;0 'mssql_runnable_tasks'=0;0;0 'mssql_signal_wait_pct'=7.4074%;25;0 'mssql_total_waits'=26.83896;0;0 'mssql_work_queue'=0;0;10 'mssql_workers'=44;0;0
```

**Alert on storage pressure:**

```
check_mssql_waits "warning=io_waits > 500 or log_waits > 200" "critical=io_waits > 2000"
OK: 0 runnable tasks on 16 schedulers, 0 queued; waits ms/s: cpu 0, io 0, log 29.8211, lock 0, memory 0, signal 6.45161%|'mssql_cpu_waits'=0;0;0 'mssql_io_waits'=0;500;2000 'mssql_latch_waits'=0;0;0 'mssql_lock_waits'=0;0;0 'mssql_log_waits'=29.82107;200;0 'mssql_memory_waits'=0;0;0 'mssql_network_waits'=0;0;0 'mssql_other_waits'=0.99403;0;0 'mssql_runnable_tasks'=0;0;0 'mssql_signal_wait_pct'=6.45161%;0;0 'mssql_total_waits'=30.8151;0;0 'mssql_work_queue'=0;0;0 'mssql_workers'=45;0;0
```



<a id="check_mssql_waits_options"></a>
#### Command-line Arguments

<a id="check_mssql_waits_database"></a>
<a id="check_mssql_waits_user"></a>
<a id="check_mssql_waits_password"></a>
<a id="check_mssql_waits_driver"></a>
<a id="check_mssql_waits_connection-string"></a>
<a id="check_mssql_waits_encrypt"></a>

        
        
        
        
        
        
        
        
        
        
| Option                                            | Default Value | Description                                                                                                    |
|---------------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------|
| [server](#check_mssql_waits_server)               | localhost     | SQL Server to connect to: host, host\INSTANCE or host,port.                                                    |
| database                                          |               | Database (initial catalog) to connect to (default: the login's default database).                              |
| user                                              |               | SQL login to authenticate with; leave empty (together with password) to use Windows integrated authentication. |
| password                                          |               | Password for the SQL login.                                                                                    |
| driver                                            |               | ODBC driver to use (default: newest installed SQL Server driver).                                              |
| connection-string                                 |               | Raw ODBC connection string; overrides all other connection options.                                            |
| [timeout](#check_mssql_waits_timeout)             | 10            | Connection (login) timeout in seconds.                                                                         |
| [query-timeout](#check_mssql_waits_query-timeout) | 30            | Query timeout in seconds.                                                                                      |
| [trust-cert](#check_mssql_waits_trust-cert)       | true          | Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).                           |
| encrypt                                           |               | Force connection encryption on or off: yes or no (modern ODBC drivers only).                                   |



<h5 id="check_mssql_waits_server">server:</h5>

SQL Server to connect to: host, host\INSTANCE or host,port.

*Default Value:* `localhost`

<h5 id="check_mssql_waits_timeout">timeout:</h5>

Connection (login) timeout in seconds.

*Default Value:* `10`

<h5 id="check_mssql_waits_query-timeout">query-timeout:</h5>

Query timeout in seconds.

*Default Value:* `30`

<h5 id="check_mssql_waits_trust-cert">trust-cert:</h5>

Trust the server certificate (TrustServerCertificate=yes, modern ODBC drivers only).

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                            | Default Value                                                                                                                                                                                                              |
|-------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| <a id="check_mssql_waits_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                                                                                                                      |
| <a id="check_mssql_waits_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                                                                                                                      |
| <a id="check_mssql_waits_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                                                                                                                                    |
| <a id="check_mssql_waits_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                                                                                                                      |
| <a id="check_mssql_waits_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                                                                                                                          |
| <a id="check_mssql_waits_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                                                                                                                         |
| <a id="check_mssql_waits_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No scheduler information returned                                                                                                                                                                               |
| <a id="check_mssql_waits_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${runnable_tasks} runnable tasks on ${schedulers} schedulers, ${work_queue} queued; waits ms/s: cpu ${cpu_waits}, io ${io_waits}, log ${log_waits}, lock ${lock_waits}, memory ${memory_waits}, signal ${signal_wait_pct}% |
| <a id="check_mssql_waits_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | mssql                                                                                                                                                                                                                      |
| <a id="check_mssql_waits_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                                                                                                                            |
| <a id="check_mssql_waits_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                                                                                                                         |
| <a id="check_mssql_waits_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                                                                                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_waits_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                                          |
|-----------------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| cpu_waits       | CPU/parallelism wait ms per second (SOS_SCHEDULER_YIELD, THREADPOOL, CX*)                                                                            |
| io_waits        | Data-file I/O wait ms per second (PAGEIOLATCH_*, IO_COMPLETION, BACKUPIO)                                                                            |
| latch_waits     | Latch wait ms per second (PAGELATCH_*, LATCH_*)                                                                                                      |
| lock_waits      | Lock wait ms per second (LCK_M_*)                                                                                                                    |
| log_waits       | Transaction-log wait ms per second (WRITELOG, LOGBUFFER)                                                                                             |
| memory_waits    | Memory wait ms per second (RESOURCE_SEMAPHORE*, CMEMTHREAD)                                                                                          |
| network_waits   | Network wait ms per second (ASYNC_NETWORK_IO - usually the client not consuming results, not the network)                                            |
| other_waits     | Wait ms per second not covered by the categories (benign waits excluded)                                                                             |
| runnable_tasks  | Tasks that have CPU work but are waiting for a scheduler slot; sustained values above the core count mean CPU pressure                               |
| schedulers      | Visible online schedulers (compare runnable_tasks against this)                                                                                      |
| signal_wait_pct | Percent of wait time spent runnable, i.e. waiting for CPU after the resource arrived; sustained > 20-25% means CPU pressure (-1 when nothing waited) |
| total_waits     | Total non-benign wait ms per second                                                                                                                  |
| work_queue      | Tasks queued with no worker thread at all (THREADPOOL starvation when > 0)                                                                           |
| workers         | Active worker threads                                                                                                                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                                  | Description |
|-------------------------------------------------|-------------|
| [/settings/mssql](#/settings/mssql)             |             |
| [/settings/mssql/facts](#/settings/mssql/facts) |             |


### /settings/mssql <a id="/settings/mssql"></a>



| Key                                     | Default Value | Description       |
|-----------------------------------------|---------------|-------------------|
| [connection string](#connection-string) |               | CONNECTION STRING |
| [database](#database)                   |               | DATABASE          |
| [driver](#odbc-driver)                  |               | ODBC DRIVER       |
| [hostname](#sql-server)                 | localhost     | SQL SERVER        |
| [password](#sql-password)               |               | SQL PASSWORD      |
| [query timeout](#query-timeout)         | 30            | QUERY TIMEOUT     |
| [timeout](#login-timeout)               | 10            | LOGIN TIMEOUT     |
| [user](#sql-user)                       |               | SQL USER          |


```ini
# 
[/settings/mssql]
hostname=localhost
query timeout=30
timeout=10
```

#### CONNECTION STRING <a id="/settings/mssql/connection string"></a>

Raw ODBC connection string; overrides all other connection settings.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | connection string                   |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mssql]
# CONNECTION STRING
connection string=
```

#### DATABASE <a id="/settings/mssql/database"></a>

Default database (initial catalog) to connect to.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | database                            |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mssql]
# DATABASE
database=
```

#### ODBC DRIVER <a id="/settings/mssql/driver"></a>

ODBC driver used to connect; leave empty to auto-detect the newest installed SQL Server driver.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | driver                              |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mssql]
# ODBC DRIVER
driver=
```

#### SQL SERVER <a id="/settings/mssql/hostname"></a>

Default SQL Server to connect to: host, host\\INSTANCE or host,port.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | hostname                            |
| Default value: | `localhost`                         |


**Sample:**

```
[/settings/mssql]
# SQL SERVER
hostname=localhost
```

#### SQL PASSWORD <a id="/settings/mssql/password"></a>

Password for the SQL login.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | password                            |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mssql]
# SQL PASSWORD
password=
```

#### QUERY TIMEOUT <a id="/settings/mssql/query timeout"></a>

Query timeout in seconds.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | query timeout                       |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `30`                                |


**Sample:**

```
[/settings/mssql]
# QUERY TIMEOUT
query timeout=30
```

#### LOGIN TIMEOUT <a id="/settings/mssql/timeout"></a>

Connection (login) timeout in seconds.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | timeout                             |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `10`                                |


**Sample:**

```
[/settings/mssql]
# LOGIN TIMEOUT
timeout=10
```

#### SQL USER <a id="/settings/mssql/user"></a>

SQL login used to authenticate; leave user and password empty to use Windows integrated authentication.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mssql](#/settings/mssql) |
| Key:           | user                                |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mssql]
# SQL USER
user=
```

### /settings/mssql/facts <a id="/settings/mssql/facts"></a>



| Key                                       | Default Value | Description           |
|-------------------------------------------|---------------|-----------------------|
| [mssql](#mssql-server-facts)              | false         | MSSQL SERVER FACTS    |
| [mssql.databases](#mssql-databases-facts) | false         | MSSQL DATABASES FACTS |


```ini
# 
[/settings/mssql/facts]
mssql=false
mssql.databases=false
```

#### MSSQL SERVER FACTS <a id="/settings/mssql/facts/mssql"></a>

Collect the server record of the \`mssql\` fact set: the instance name (the same value check_mssql calls \`server_name\`), the machine and instance it is, the version, patch level, update level and edition, the engine edition, the server collation, the authentication mode and whether it is clustered or has Always On enabled. Not its uptime: that is monitoring, and it lives in check_mssql. One connection and one SERVERPROPERTY query per facts round, made with the connection configured in this section's parent (Windows authentication, or the user and password): a facts round has no request to take them from. Nothing is collected while this is off.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/mssql/facts](#/settings/mssql/facts) |
| Key:           | mssql                                           |
| Default value: | `false`                                         |


**Sample:**

```
[/settings/mssql/facts]
# MSSQL SERVER FACTS
mssql=false
```

#### MSSQL DATABASES FACTS <a id="/settings/mssql/facts/mssql.databases"></a>

Collect the \`mssql.databases\` fact set: one record per database the login may see, system databases included - its name (the record id, the same value check_mssql_databases calls \`name\`), its recovery model, collation, compatibility level, creation date and whether it is read-only. Not its state or size: those are monitoring, and they live in check_mssql_databases. One query of sys.databases per facts round, over the same connection as the server record. Nothing is collected while this is off.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/mssql/facts](#/settings/mssql/facts) |
| Key:           | mssql.databases                                 |
| Default value: | `false`                                         |


**Sample:**

```
[/settings/mssql/facts]
# MSSQL DATABASES FACTS
mssql.databases=false
```
