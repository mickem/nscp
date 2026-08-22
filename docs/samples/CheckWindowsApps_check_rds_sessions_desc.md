#### About `check_rds_sessions`

`check_rds_sessions` reads the "Terminal Services" performance counters
(Active/Inactive/Total Sessions) and reports the session-count picture of a
Remote Desktop session host in one record. The counter object exists on every
Windows SKU (the console counts as a session), so the check also works on
plain servers — the numbers only become interesting on session hosts.

There are no default thresholds; all three values are emitted as perfdata so
capacity can be graphed even on an all-OK check.

Disconnected (`inactive`) sessions still hold memory, licenses and (for
per-device CALs) a seat, so `warning=inactive > <n>` is a useful signal that
idle-session limits are not configured or not working. For per-session
resource usage see `check_rds_session_load`; for CAL exhaustion see
`check_rds_licenses`.
