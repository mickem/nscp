#### About `check_mysql`

`check_mysql` verifies that a MySQL-compatible server is reachable and healthy.
It connects (TCP by default, or a socket/named pipe via `socket=`), reads the
server version and a couple of global status values, and reports them. Being
able to connect is the health signal: a reachable server is **OK** unless you
add thresholds, and any connection failure is reported as **UNKNOWN** with the
driver's error message ("Access denied ...", "Can't connect ...").

The check works identically against MySQL, MariaDB and Percona; the `flavor`
keyword tells them apart when you need to (e.g. `warning=flavor != 'mariadb'`
to catch an unplanned migration).

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword             | Description                                                        |
|---------------------|--------------------------------------------------------------------|
| `version`           | Server version, e.g. `10.11.14-MariaDB-ubu2404` or `8.4.3`         |
| `version_comment`   | Server version comment (distribution/build description)            |
| `flavor`            | `mysql`, `mariadb` or `percona`, derived from the version           |
| `uptime`            | Seconds since the server started; supports units (`uptime < 1h`)    |
| `threads_connected` | Currently open connections (`Threads_connected`)                    |
| `max_connections`   | Configured connection limit (`max_connections`)                     |
| `connections_pct`   | Open connections as a percentage of `max_connections`               |

Common connection options (shared by all CheckMySQL commands, defaults come
from `/settings/mysql`): `host=`, `port=`, `socket=`, `user=`, `password=`,
`database=`, `defaults-file=`, `plugin-dir=`, `tls=true`, `timeout=`,
`query-timeout=`.

Notes:

* `host=localhost` connects over **TCP** like any other host name; use
  `socket=` when you want the local socket / named pipe (the check does not
  inherit the mysql client's silent localhost-means-socket behaviour).
* MySQL 8 accounts default to the `caching_sha2_password` auth plugin, which
  the connector loads from its plugin directory; if that fails use
  `plugin-dir=` (or the `plugin dir` setting) to point at it.
* Give the monitoring user as few privileges as possible; `USAGE` is enough
  for `check_mysql`.
