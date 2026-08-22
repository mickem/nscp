#### About `check_iis_worker_processes`

`check_iis_worker_processes` reports one record per running IIS worker process
(w3wp.exe) from the `W3SVC_W3WP` performance counters. Counter instances are
named `<pid>_<pool>`; the check splits that into the `pid` and `pool`
keywords.

An *empty* result is OK by default: idle application pools spin their workers
down, so "no workers" is a normal state, not a failure (`empty-state=critical`
turns it into an alert for pools that must always be warm). The check goes
UNKNOWN with a clear message when the IIS role (and with it the counter set)
is missing.

There are no default thresholds — a sensible starting point is
`warning=active_requests > 50` scaled to your pools' concurrency.
