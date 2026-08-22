#### About `check_phpfpm_status`

`check_phpfpm_status` fetches a PHP-FPM pool's status page (the default text
format) and exposes the reported values as filter keywords. The page must be
enabled in the pool configuration (`pm.status_path = /status`) and the location
routed to FPM in the web server in front of it (or served via `fastcgi` on a
dedicated port).

The check emits a single record. By default it goes **warning** when requests
are waiting in the listen queue (`listen_queue > 0` — the pool has no free
worker to pick them up) and **critical** when the endpoint cannot be fetched
or does not look like an FPM status page (`result != 'ok'`).

`max_children_reached`, `slow_requests`, `max_listen_queue` and
`accepted_conn` are cumulative since the pool (re)started; a threshold on them
stays raised until the counter resets on reload.
