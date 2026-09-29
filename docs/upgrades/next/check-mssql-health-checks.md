---
icon: "🧪"
modules: [CheckMSSQL, filters]
action: none
---
**Eight more SQL Server checks, and growth headroom in
`check_mssql_databases` (experimental).** Nothing to do on an upgrade: existing
checks keep their defaults. The new commands use the module's existing
connection settings:

| Command | Checks | Rights |
|---|---|---|
| `check_mssql_sessions` | session and connection counts per database and login | `VIEW SERVER STATE` |
| `check_mssql_blocking` | blocked sessions and blocking chains | `VIEW SERVER STATE` |
| `check_mssql_transactions` | old or leaked open transactions and long-running requests | `VIEW SERVER STATE` |
| `check_mssql_counters` | buffer cache, page life expectancy, batch and lock rates | `VIEW SERVER STATE` |
| `check_mssql_waits` | wait statistics by category and scheduler pressure | `VIEW SERVER STATE` |
| `check_mssql_tempdb` | tempdb space by consumer and volume headroom | `VIEW SERVER STATE` |
| `check_mssql_availability_groups` | Always On replica and database health | `VIEW SERVER STATE` |
| `check_mssql_integrity` | suspect pages and the age of the last successful CHECKDB | `SELECT` on `msdb.dbo.suspect_pages`; `sysadmin` for `checkdb_age` |

`check_mssql_counters` samples twice, one second apart, so it takes about a
second longer than the others. `check_mssql_integrity` reads when CHECKDB last
succeeded; it never runs CHECKDB itself. A missing permission degrades a
keyword to a `-1`/`-2` sentinel, or returns UNKNOWN, rather than raising an
alert.

`check_mssql_databases` gains `data_headroom` and `log_headroom`: how much
further a database can grow before it reaches a file's `max_size` or fills the
volume it grows into. Files that share a volume share its free space, so the
headroom is not counted once per file:

```
check_mssql_databases "critical=data_headroom < 1G and data_headroom >= 0"
```

One fix reaches every check: an integer keyword compared against a size
(`1G`) or another integer is no longer converted through a floating-point
number on the way, so values above 2^53 bytes compare exactly.
