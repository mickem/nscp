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
