#### About `check_rds_session_load`

`check_rds_session_load` reports one record per "Terminal Services Session"
counter instance: the console, the session-0 `Services` aggregate, and one
`RDP-Tcp <n>` instance per remote session. It answers "which session is
eating the host?" — the per-session CPU and working set that plain
`check_process` cannot attribute to a user.

Pass `sessions-only=true` to skip the `Services` aggregate (system processes,
not a user session), and `averages=true` to sample CPU over a full second
(the check then takes about a second longer; without it the first PDH sample
is used, which can be noisy).

Counter instances are named after the connection, not the user; correlate the
`RDP-Tcp <n>` number with `quser`/`qwinsta` output to find who it is. The
protocol counters (`total_bytes`) only exist on hosts with the RD Session Host
role; on other machines the keyword reads 0.
