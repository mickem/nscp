#### About `check_apache_status`

`check_apache_status` fetches Apache httpd's
[mod_status](https://httpd.apache.org/docs/current/mod/mod_status.html) page in
its machine-readable form (`/server-status?auto`) and exposes the reported
values as filter keywords. The `?auto` parameter is appended automatically when
the URL does not already carry it, so `url=http://host/server-status` is
enough. `ExtendedStatus On` (the default since Apache 2.3.6) is required for
the request/byte counters; `BusyWorkers`/`IdleWorkers` are always present.

The check emits a single record. By default it goes **critical** when the
endpoint cannot be fetched or does not look like a mod_status page
(`result != 'ok'`); worker/traffic thresholds are opt-in.

Connection options match `check_http` where applicable: `timeout`, `username`
/ `password` (Basic auth), and for https `tls-version`, `verify` and `ca`.

Note that `requests_per_sec`/`bytes_per_sec` are lifetime averages computed by
Apache itself, not a current rate; for spiky load, alert on `busy_workers`
instead.
