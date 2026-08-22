#### About `check_rds_broker`

`check_rds_broker` reads the "Remote Desktop Connection Broker Counterset"
performance object on an RD Connection Broker and reports one record per
counter (per instance where the counterset has instances). The broker's
counter names vary between Windows Server versions, so the check
**enumerates** whatever this broker exposes instead of hard-coding names —
run it once with `show-all` to see your version's counters, then select with
the `counter` keyword.

Run it on the Connection Broker itself; on any other host the counterset does
not exist and the check returns UNKNOWN with a clear "is this host an RD
Connection Broker?" message.

Pass `averages=true` to collect a second sample after one second so rate
counters carry real values. Typical alerts target the failed/pending request
counters, e.g. `critical=counter like 'Failed' and value > 0`.
