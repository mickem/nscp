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

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword                | Description                                                     |
|------------------------|-----------------------------------------------------------------|
| `result`               | `ok`, `parse_error`, `http_<code>` or `error: <message>`        |
| `pool`                 | Name of the FPM pool                                            |
| `process_manager`      | `static`, `dynamic` or `ondemand`                               |
| `active_processes`     | Workers currently serving requests                              |
| `idle_processes`       | Idle (spare) workers                                            |
| `total_processes`      | Total workers in the pool                                       |
| `max_active_processes` | Peak simultaneously-active workers since start                  |
| `listen_queue`         | Requests currently waiting for a free worker                    |
| `max_listen_queue`     | Peak listen queue length since start                            |
| `listen_queue_len`     | Size of the socket listen queue                                 |
| `max_children_reached` | Times the pool hit `pm.max_children` since start                |
| `slow_requests`        | Requests exceeding `request_slowlog_timeout` since start        |
| `accepted_conn`        | Connections accepted since start                                |
| `code`                 | HTTP status code of the response                                 |
| `url`, `host`, `port`  | The requested endpoint                                          |

`max_children_reached`, `slow_requests`, `max_listen_queue` and
`accepted_conn` are cumulative since the pool (re)started; a threshold on them
stays raised until the counter resets on reload.
