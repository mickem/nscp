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
