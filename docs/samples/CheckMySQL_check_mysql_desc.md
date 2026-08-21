#### About `check_mysql`

`check_mysql` verifies that a MySQL-compatible server is reachable and healthy.
It connects (TCP by default, or via the `socket` setting), reads the
server version and a couple of global status values, and reports them. Being
able to connect is the health signal: a reachable server is **OK** unless you
add thresholds, and any connection failure is reported as **UNKNOWN** with the
driver's error message ("Access denied ...", "Can't connect ...").

The check works identically against MySQL, MariaDB and Percona; the `flavor`
keyword tells them apart when you need to (e.g. `warning=flavor != 'mariadb'`
to catch an unplanned migration).

Common connection options that can be passed per request (shared by all
CheckMySQL commands, defaults come from `/settings/mysql`): `host=`, `port=`,
`user=`, `password=`, `database=`, `tls=true`, `timeout=`, `query-timeout=`.

The `socket`, `defaults file` and `plugin dir` connection parameters are set
**only** in `/settings/mysql`, not per request. Each names a local resource the
service loads or reads as its own account — a plugin directory the connector
loads a DLL/`.so` from, a named pipe that can point off-box, an option file read
for credentials — so accepting them from a check request would let any caller
who can run a check load code or exfiltrate credentials as the agent. They are
deployment properties; configure them once in `nsclient.ini`.

Notes:

* `host=localhost` connects over **TCP** like any other host name; set the
  `socket` setting when you want the local socket / named pipe (the check does
  not inherit the mysql client's silent localhost-means-socket behaviour).
* MySQL 8 accounts default to the `caching_sha2_password` auth plugin, which
  the connector loads from its plugin directory; if that fails set the
  `plugin dir` setting to point at it.
* Give the monitoring user as few privileges as possible; `USAGE` is enough
  for `check_mysql`.
