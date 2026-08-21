# CheckMSSQL

*Available on Windows only.*

Check Microsoft SQL Server: connectivity, databases, backups, agent jobs and custom queries.

## Enable module

To enable this module and and allow using the commands you need to ass `CheckMSSQL = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckMSSQL = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckMSSQL module.

**List of commands:**

A list of all available queries (check commands)

| Command                                         | Description                                                          |
|-------------------------------------------------|----------------------------------------------------------------------|
| [check_mssql](#check_mssql)                     | Check SQL Server connectivity and health (version, edition, uptime). |
| [check_mssql_backup](#check_mssql_backup)       | Check the age of the last full/differential/log backup per database. |
| [check_mssql_databases](#check_mssql_databases) | Check database state, recovery model and data/log size.              |
| [check_mssql_jobs](#check_mssql_jobs)           | Check SQL Server Agent job status.                                   |
| [check_mssql_query](#check_mssql_query)         | Run a custom T-SQL query and apply thresholds to the returned rows.  |

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


| Option                                                                                       | Default Value                                                                        |
|----------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------|
| <a id="check_mssql_filter"></a>[filter](../common-options.md#filter)                         |                                                                                      |
| <a id="check_mssql_warning"></a>[warning](../common-options.md#warning)                      |                                                                                      |
| <a id="check_mssql_warn"></a>[warn](../common-options.md#warn)                               |                                                                                      |
| <a id="check_mssql_critical"></a>[critical](../common-options.md#critical)                   |                                                                                      |
| <a id="check_mssql_crit"></a>[crit](../common-options.md#crit)                               |                                                                                      |
| <a id="check_mssql_ok"></a>[ok](../common-options.md#ok)                                     |                                                                                      |
| <a id="check_mssql_debug"></a>[debug](../common-options.md#debug)                            | false                                                                                |
| <a id="check_mssql_show-all"></a>[show-all](../common-options.md#show-all)                   | false                                                                                |
| <a id="check_mssql_empty-state"></a>[empty-state](../common-options.md#empty-state)          | unknown                                                                              |
| <a id="check_mssql_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                                                                      |
| <a id="check_mssql_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                                                                                |
| <a id="check_mssql_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                                                                    |
| <a id="check_mssql_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${status}: ${list}                                                                   |
| <a id="check_mssql_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                |                                                                                      |
| <a id="check_mssql_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): No server information returned                                            |
| <a id="check_mssql_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${server_name}: SQL Server ${version} ${product_level} ${edition}, uptime ${uptime}s |
| <a id="check_mssql_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${server_name}                                                                       |


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


| Option                                                                                              | Default Value                                                    |
|-----------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| <a id="check_mssql_backup_filter"></a>[filter](../common-options.md#filter)                         |                                                                  |
| <a id="check_mssql_backup_warning"></a>[warning](../common-options.md#warning)                      | full_age > 3d                                                    |
| <a id="check_mssql_backup_warn"></a>[warn](../common-options.md#warn)                               |                                                                  |
| <a id="check_mssql_backup_critical"></a>[critical](../common-options.md#critical)                   | full_age < 0 or full_age > 7d                                    |
| <a id="check_mssql_backup_crit"></a>[crit](../common-options.md#crit)                               |                                                                  |
| <a id="check_mssql_backup_ok"></a>[ok](../common-options.md#ok)                                     |                                                                  |
| <a id="check_mssql_backup_debug"></a>[debug](../common-options.md#debug)                            | false                                                            |
| <a id="check_mssql_backup_show-all"></a>[show-all](../common-options.md#show-all)                   | false                                                            |
| <a id="check_mssql_backup_empty-state"></a>[empty-state](../common-options.md#empty-state)          | unknown                                                          |
| <a id="check_mssql_backup_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                                                  |
| <a id="check_mssql_backup_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                                                            |
| <a id="check_mssql_backup_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                                                |
| <a id="check_mssql_backup_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${status}: ${problem_count}/${count} databases (${problem_list}) |
| <a id="check_mssql_backup_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                | %(status): All %(count) databases have recent backups            |
| <a id="check_mssql_backup_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): No databases found                                    |
| <a id="check_mssql_backup_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${name}: last full backup ${full_age}s ago                       |
| <a id="check_mssql_backup_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${name}                                                          |


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


| Option                                                                                                 | Default Value                                                          |
|--------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------|
| <a id="check_mssql_databases_filter"></a>[filter](../common-options.md#filter)                         |                                                                        |
| <a id="check_mssql_databases_warning"></a>[warning](../common-options.md#warning)                      | state = 'RESTORING' or state = 'RECOVERING' or state = 'OFFLINE'       |
| <a id="check_mssql_databases_warn"></a>[warn](../common-options.md#warn)                               |                                                                        |
| <a id="check_mssql_databases_critical"></a>[critical](../common-options.md#critical)                   | state = 'SUSPECT' or state = 'EMERGENCY' or state = 'RECOVERY_PENDING' |
| <a id="check_mssql_databases_crit"></a>[crit](../common-options.md#crit)                               |                                                                        |
| <a id="check_mssql_databases_ok"></a>[ok](../common-options.md#ok)                                     |                                                                        |
| <a id="check_mssql_databases_debug"></a>[debug](../common-options.md#debug)                            | false                                                                  |
| <a id="check_mssql_databases_show-all"></a>[show-all](../common-options.md#show-all)                   | false                                                                  |
| <a id="check_mssql_databases_empty-state"></a>[empty-state](../common-options.md#empty-state)          | unknown                                                                |
| <a id="check_mssql_databases_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                                                        |
| <a id="check_mssql_databases_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                                                                  |
| <a id="check_mssql_databases_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                                                      |
| <a id="check_mssql_databases_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${status}: ${problem_count}/${count} databases (${problem_list})       |
| <a id="check_mssql_databases_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                | %(status): All %(count) databases are ONLINE                           |
| <a id="check_mssql_databases_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): No databases found                                          |
| <a id="check_mssql_databases_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${name}: ${state}                                                      |
| <a id="check_mssql_databases_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${name}                                                                |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_databases_filter_keys"></a>
#### Filter keywords

| Option         | Description                                                                                    |
|----------------|------------------------------------------------------------------------------------------------|
| data_size      | Total size of the data files in bytes (supports units, e.g. data_size > 10G)                   |
| is_read_only   | 1 if the database is read-only                                                                 |
| log_size       | Total size of the log files in bytes (supports units, e.g. log_size > 1G)                      |
| log_used_pct   | Percentage of the log in use (-1 if unavailable)                                               |
| name           | Database name                                                                                  |
| recovery_model | Recovery model: SIMPLE, FULL or BULK_LOGGED                                                    |
| state          | Database state: ONLINE, RESTORING, RECOVERING, RECOVERY_PENDING, SUSPECT, EMERGENCY or OFFLINE |

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


| Option                                                                                            | Default Value                                               |
|---------------------------------------------------------------------------------------------------|-------------------------------------------------------------|
| <a id="check_mssql_jobs_filter"></a>[filter](../common-options.md#filter)                         | enabled = 1                                                 |
| <a id="check_mssql_jobs_warning"></a>[warning](../common-options.md#warning)                      | last_run_status = 'canceled' or last_run_status = 'retry'   |
| <a id="check_mssql_jobs_warn"></a>[warn](../common-options.md#warn)                               |                                                             |
| <a id="check_mssql_jobs_critical"></a>[critical](../common-options.md#critical)                   | last_run_status = 'failed'                                  |
| <a id="check_mssql_jobs_crit"></a>[crit](../common-options.md#crit)                               |                                                             |
| <a id="check_mssql_jobs_ok"></a>[ok](../common-options.md#ok)                                     |                                                             |
| <a id="check_mssql_jobs_debug"></a>[debug](../common-options.md#debug)                            | false                                                       |
| <a id="check_mssql_jobs_show-all"></a>[show-all](../common-options.md#show-all)                   | false                                                       |
| <a id="check_mssql_jobs_empty-state"></a>[empty-state](../common-options.md#empty-state)          | ok                                                          |
| <a id="check_mssql_jobs_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                                             |
| <a id="check_mssql_jobs_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                                                       |
| <a id="check_mssql_jobs_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                                           |
| <a id="check_mssql_jobs_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${status}: ${problem_count}/${count} jobs (${problem_list}) |
| <a id="check_mssql_jobs_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                | %(status): All %(count) jobs succeeded                      |
| <a id="check_mssql_jobs_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): No enabled SQL Agent jobs found                  |
| <a id="check_mssql_jobs_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${name}: ${last_run_status}                                 |
| <a id="check_mssql_jobs_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${name}                                                     |


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


| Option                                                                                             | Default Value |
|----------------------------------------------------------------------------------------------------|---------------|
| <a id="check_mssql_query_filter"></a>[filter](../common-options.md#filter)                         |               |
| <a id="check_mssql_query_warning"></a>[warning](../common-options.md#warning)                      |               |
| <a id="check_mssql_query_warn"></a>[warn](../common-options.md#warn)                               |               |
| <a id="check_mssql_query_critical"></a>[critical](../common-options.md#critical)                   |               |
| <a id="check_mssql_query_crit"></a>[crit](../common-options.md#crit)                               |               |
| <a id="check_mssql_query_ok"></a>[ok](../common-options.md#ok)                                     |               |
| <a id="check_mssql_query_debug"></a>[debug](../common-options.md#debug)                            | false         |
| <a id="check_mssql_query_show-all"></a>[show-all](../common-options.md#show-all)                   | false         |
| <a id="check_mssql_query_empty-state"></a>[empty-state](../common-options.md#empty-state)          | ignored       |
| <a id="check_mssql_query_perf-config"></a>[perf-config](../common-options.md#perf-config)          |               |
| <a id="check_mssql_query_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false         |
| <a id="check_mssql_query_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,             |
| <a id="check_mssql_query_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${list}       |
| <a id="check_mssql_query_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                |               |
| <a id="check_mssql_query_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       |               |
| <a id="check_mssql_query_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | %(line)       |
| <a id="check_mssql_query_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          |               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mssql_query_filter_keys"></a>
#### Filter keywords

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                      | Description |
|-------------------------------------|-------------|
| [/settings/mssql](#/settings/mssql) |             |


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
