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
