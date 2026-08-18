#### About `check_tomcat_status`

`check_tomcat_status` fetches Apache Tomcat's manager status page in XML form
(`/manager/status?XML=true`) and reports one record per connector plus the JVM
heap numbers. The `?XML=true` parameter is appended automatically when the URL
does not already carry it.

The manager application must be deployed and the account used needs the
`manager-status` role (or `manager-gui`, which includes it) in
`conf/tomcat-users.xml`; pass it with `username=` / `password=`.

One record is emitted per connector (e.g. `http-nio-8080`, `ajp-nio-8009`).
By default the check goes **warning** at 75% and **critical** at 90% thread
pool usage, and **critical** when the page cannot be fetched or parsed
(`result != 'ok'` — including `http_401` for missing credentials).

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword           | Description                                                        |
|-------------------|--------------------------------------------------------------------|
| `result`          | `ok`, `parse_error`, `http_<code>` or `error: <message>`           |
| `connector`       | Connector name (e.g. `http-nio-8080`)                              |
| `threads_busy`    | Threads currently serving requests                                 |
| `threads_current` | Threads currently alive in the pool                                |
| `threads_max`     | Maximum size of the thread pool                                    |
| `thread_usage`    | `threads_busy` as a percentage of `threads_max`                    |
| `request_count`   | Requests served since start                                        |
| `error_count`     | Requests that ended in an error since start                        |
| `processing_time` | Total request processing time (ms) since start                     |
| `max_time`        | Slowest request (ms) since start                                   |
| `bytes_received` / `bytes_sent` | Traffic since start                                  |
| `memory_free` / `memory_total` / `memory_max` | JVM heap in bytes (same on every record) |
| `code`            | HTTP status code of the response                                   |
| `url`, `host`, `port` | The requested endpoint                                         |

Use `filter=connector like 'http'` to scope the check to specific connectors.
The JVM heap keywords repeat on every connector record, so combine them with a
`filter` to avoid the same memory alert firing once per connector.
