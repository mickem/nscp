---
icon: "📊"
modules: [CheckMSSQL]
action: none
---
**SQL Server: average latch wait time.** `check_mssql_counters` gained
`latch_wait_ms`: the average time, in milliseconds, that a latch request which
had to wait spent waiting, measured over the check's one-second sampling
window (`0` when nothing waited, `-1` when the counters are unavailable). Use
it as `warning=latch_wait_ms > 10`. The default output and perfdata are
unchanged; the value is added to the perfdata once a threshold references it.
