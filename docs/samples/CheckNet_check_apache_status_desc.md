#### About `check_apache_status`

`check_apache_status` fetches Apache httpd's
[mod_status](https://httpd.apache.org/docs/current/mod/mod_status.html) page in
its machine-readable form (`/server-status?auto`) and exposes the reported
values as filter keywords. The `?auto` parameter is appended automatically when
the URL does not already carry it, so `url=http://host/server-status` is
enough. `ExtendedStatus On` (the default since Apache 2.3.6) is required for
the request/byte counters; `BusyWorkers`/`IdleWorkers` are always present.

The check emits a single record; worker/traffic thresholds are opt-in.
If the endpoint cannot be fetched (`invalid_url`, `http_<code>`, `error: ...`)
or does not look like a mod_status page, there is no data to judge: the check
returns **UNKNOWN** with `<url>: <reason>` as the message and no performance
data, so a graph shows a gap rather than a drop to zero.

Connection options match `check_http` where applicable: `timeout`, `username`
/ `password` (Basic auth), and for https `tls-version`, `verify` and `ca`.

Note that `requests_per_sec`/`bytes_per_sec` are lifetime averages computed by
Apache itself, not a current rate; for spiky load, alert on `busy_workers`
instead.
