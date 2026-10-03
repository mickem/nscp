#### About `check_iis_worker_processes`

`check_iis_worker_processes` reports one record per running IIS worker process
(w3wp.exe) from the `W3SVC_W3WP` performance counters. Counter instances are
named `<pid>_<pool>`; the check splits that into the `pid` and `pool`
keywords.

An *empty* result is OK by default: idle application pools spin their workers
down (after 20 minutes in a default pool), so "no workers" is a normal state,
not a failure (`empty-state=critical` turns it into an alert for pools that
must always be warm). With no worker alive the `W3SVC_W3WP` counter object has
no instances at all; the check reports that as `No IIS worker processes
running`, and only goes UNKNOWN with a clear message when the IIS role (and
with it the counter set itself) is missing.

There are no default thresholds — a sensible starting point is
`warning=active_requests > 50` scaled to your pools' concurrency.
