#### About `check_mssql_backup`

`check_mssql_backup` reports the **age of the most recent full, differential
and log backup** for every database, joining `sys.databases` with the backup
history in `msdb.dbo.backupset`. `tempdb` is excluded (it is never backed up).
A backup type that has never been taken is reported as age **-1**, so
"never backed up" can be caught explicitly (`full_age < 0`).

Keywords (one row per database):

| Keyword          | Description                                                       |
|------------------|-------------------------------------------------------------------|
| `name`           | Database name                                                     |
| `recovery_model` | `SIMPLE`, `FULL` or `BULK_LOGGED`                                 |
| `full_age`       | Seconds since the last full backup finished, `-1` = never (accepts units) |
| `diff_age`       | Seconds since the last differential backup finished, `-1` = never |
| `log_age`        | Seconds since the last log backup finished, `-1` = never          |

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
