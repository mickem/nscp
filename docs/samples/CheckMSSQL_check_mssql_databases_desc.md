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
