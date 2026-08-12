#### About `check_mssql_databases`

`check_mssql_databases` enumerates **every database on the instance** from
`sys.databases` (sizes from `sys.master_files`, log usage from
`DBCC SQLPERF(LOGSPACE)`) and produces one row per database, so availability
and capacity policies can be expressed with filter expressions.

Keywords (one row per database):

| Keyword          | Description                                                                     |
|------------------|---------------------------------------------------------------------------------|
| `name`           | Database name                                                                   |
| `state`          | `ONLINE`, `RESTORING`, `RECOVERING`, `RECOVERY_PENDING`, `SUSPECT`, `EMERGENCY` or `OFFLINE` |
| `recovery_model` | `SIMPLE`, `FULL` or `BULK_LOGGED`                                               |
| `is_read_only`   | `1` if the database is read-only                                                |
| `data_size`      | Total data-file size in bytes (accepts units: `data_size > 10G`)                |
| `log_size`       | Total log-file size in bytes (accepts units)                                    |
| `log_used_pct`   | Percentage of the log in use, `-1` if unavailable                               |
| `data_headroom`  | Smallest remaining growth room among the data files in bytes, `-1` if unavailable (accepts units: `data_headroom < 5G`) |
| `log_headroom`   | Same for the log files (accepts units)                                          |

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

`data_headroom`/`log_headroom` answer "how much further can this database
grow before it errors": per file, the distance to `max_size` for capped files
(the log's default 2TB cap counts as one), the free space on the file's
volume (`sys.dm_os_volume_stats`) for uncapped autogrow files, and `0` when
autogrowth is disabled — then the **minimum across the files of each type**,
because SQL Server cannot move allocations between files and the most
constrained file errors first. A threshold like
`"critical=data_headroom < 1G and data_headroom >= 0"` catches both a file
approaching its cap and a volume filling up — the `>= 0` guard excludes the
`-1` unknown sentinel (unlike the plain size keywords, the headroom keywords
accept plain integers as well as units, so `= -1` and `>= 0` work). Note that a fixed-size
pre-allocated file reports headroom `0` by design — free space *inside* the
files is a different measure (`log_used_pct` covers it for logs). Like
`log_used_pct`, the keywords degrade to `-1` when `dm_os_volume_stats` is
unavailable.
