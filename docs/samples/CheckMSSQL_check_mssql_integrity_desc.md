#### About `check_mssql_integrity`

`check_mssql_integrity` closes the gap `check_mssql_backup` leaves open: **a
backup of a corrupt database restores a corrupt database.** It reports, per
online database (tempdb excluded):

| Keyword         | Description                                                                       |
|-----------------|------------------------------------------------------------------------------------|
| `name`          | Database name                                                                      |
| `suspect_pages` | Pages in `msdb.dbo.suspect_pages` with unresolved 823/824/825 errors — any value above 0 means the engine has already **seen** corruption |
| `checkdb_age`   | Seconds since the last successful `DBCC CHECKDB` (`dbi_dbccLastKnownGood`), `-1` = never checked, `-2` = unknown/no access (accepts units, e.g. `checkdb_age > 14d`) |

Defaults: **CRITICAL** on `suspect_pages > 0` (corruption has occurred — act
now, while the backups that can repair it still exist), **WARNING** on
`checkdb_age > 14d or checkdb_age = -1` (corruption *would go unnoticed*).
Restored and repaired pages (event types 4/5/7) are excluded from the count,
so the alert clears once the damage is fixed.

`checkdb_age = -2` means the CHECKDB timestamp could not be read and is
deliberately quiet by default — reading it uses `DBCC DBINFO`, which requires
**sysadmin**. The `suspect_pages` half works with `SELECT` on
`msdb.dbo.suspect_pages` alone, so a low-privilege monitoring login still
catches active corruption; add `warning=checkdb_age = -2` if you want missing
access itself flagged. Ages are computed against the **server's own clock**,
so an agent in a different timezone does not skew them.

Note that `DBCC CHECKDB` itself is a heavy operation this check deliberately
never runs — it only reads the timestamp the last run left behind. Schedule
CHECKDB as a maintenance job (see `check_mssql_jobs` to alert when that job
fails or stops running).

Rights: `SELECT` on `msdb.dbo.suspect_pages`; `sysadmin` for `checkdb_age`.
