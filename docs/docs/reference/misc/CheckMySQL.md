# CheckMySQL

Check MySQL, MariaDB, Percona and other MySQL-compatible servers: connectivity, health and custom queries.

CheckMySQL checks MySQL, MariaDB, Percona and other MySQL-compatible database
servers on both Windows and Linux. It connects with MariaDB Connector/C
(speaking the native protocol, so one module covers the whole MySQL family)
and is only built/shipped when the connector is available.

Default connection settings live under `/settings/mysql` (host, port, socket,
user, password, TLS, timeouts) and every command accepts the network overrides
per request (`host=`, `port=`, `user=`, `password=`, `tls=true`, ...). The
parameters that name a local resource the service loads or reads on its own
account — `socket`, `plugin dir` and `defaults file` — are configured only in
`/settings/mysql`, never per request, so a check caller cannot make the agent
load a plugin or read a file it chooses. Credentials can be kept out of
`nsclient.ini` entirely via `defaults file` pointing at a permission-protected
my.cnf-style file.


## Enable module

To enable this module and and allow using the commands you need to ass `CheckMySQL = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckMySQL = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckMySQL module.

**List of commands:**

A list of all available queries (check commands)

| Command                                 | Description                                                                                |
|-----------------------------------------|--------------------------------------------------------------------------------------------|
| [check_mysql](#check_mysql)             | Check MySQL/MariaDB server connectivity and health (version, flavor, uptime, connections). |
| [check_mysql_query](#check_mysql_query) | Run a custom SQL query and apply thresholds to the returned rows.                          |

### check_mysql

Check MySQL/MariaDB server connectivity and health (version, flavor, uptime, connections).

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

**Jump to section:**

* [Sample Commands](#check_mysql_samples)
* [Command-line Arguments](#check_mysql_options)
* [Filter keywords](#check_mysql_filter_keys)


<a id="check_mysql_samples"></a>
#### Sample Commands

**Check that a MySQL/MariaDB server is up (connecting is the health signal):**

```
check_mysql host=127.0.0.1 user=monitor password=secret
OK: mariadb 11.8.8-MariaDB-ubu2404, uptime 771002s, connections 3/151 (1%)
```

**Warn when the server restarted recently (uptime keyword supports time units):**

```
check_mysql host=127.0.0.1 user=monitor password=secret "warning=uptime < 15m"
WARNING: mariadb 11.8.8-MariaDB-ubu2404, uptime 2s, connections 1/151 (0%)|'mariadb_uptime'=2s;900;0
```

**Alert when the connection pool is close to max_connections:**

```
check_mysql host=127.0.0.1 user=monitor password=secret "warning=connections_pct > 60" "critical=connections_pct > 80"
OK: mariadb 11.8.8-MariaDB-ubu2404, uptime 771002s, connections 3/151 (1%)|'mariadb_connections_pct'=1%;60;80
```

**An unreachable or refusing server is clearly reported (UNKNOWN):**

```
check_mysql host=127.0.0.1 port=9999 timeout=2
Failed to connect to MySQL server '127.0.0.1:9999': Can't connect to server on '127.0.0.1' (110)
```

```
check_mysql host=127.0.0.1 user=root password=wrong
Failed to connect to MySQL server '127.0.0.1:3306': Access denied for user 'root'@'192.168.127.1' (using password: YES)
```

**Connect through the local socket instead of TCP:**

```
check_mysql socket=/run/mysqld/mysqld.sock user=monitor password=secret
OK: mariadb 11.8.8-MariaDB-ubu2404, uptime 771002s, connections 3/151 (1%)
```

**MySQL 8 with a non-default client-plugin directory (caching_sha2_password):**

```
check_mysql host=db1 user=monitor password=secret plugin-dir=C:\Program Files\MariaDB\MariaDB Connector C 64-bit\lib\plugin
OK: mysql 8.4.11, uptime 1011s, connections 1/151 (0%)
```



<a id="check_mysql_options"></a>
#### Command-line Arguments

<a id="check_mysql_socket"></a>
<a id="check_mysql_database"></a>
<a id="check_mysql_user"></a>
<a id="check_mysql_password"></a>
<a id="check_mysql_defaults-file"></a>
<a id="check_mysql_plugin-dir"></a>

| Option                                      | Default Value | Description                                                                                                                                |
|---------------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| [host](#check_mysql_host)                   | localhost     | MySQL/MariaDB server to connect to.                                                                                                        |
| [port](#check_mysql_port)                   | 3306          | TCP port of the server.                                                                                                                    |
| socket                                      |               | Unix socket path (or Windows named pipe) to connect through instead of TCP.                                                                |
| database                                    |               | Default database (schema) to connect to.                                                                                                   |
| user                                        |               | User to authenticate with.                                                                                                                 |
| password                                    |               | Password to authenticate with.                                                                                                             |
| defaults-file                               |               | my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.                               |
| plugin-dir                                  |               | Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong). |
| [tls](#check_mysql_tls)                     | false         | Require TLS on the connection.                                                                                                             |
| [timeout](#check_mysql_timeout)             | 10            | Connection timeout in seconds.                                                                                                             |
| [query-timeout](#check_mysql_query-timeout) | 30            | Query (read/write) timeout in seconds.                                                                                                     |



<h5 id="check_mysql_host">host:</h5>

MySQL/MariaDB server to connect to.

*Default Value:* `localhost`

<h5 id="check_mysql_port">port:</h5>

TCP port of the server.

*Default Value:* `3306`

<h5 id="check_mysql_tls">tls:</h5>

Require TLS on the connection.

*Default Value:* `false`

<h5 id="check_mysql_timeout">timeout:</h5>

Connection timeout in seconds.

*Default Value:* `10`

<h5 id="check_mysql_query-timeout">query-timeout:</h5>

Query (read/write) timeout in seconds.

*Default Value:* `30`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                       | Default Value                                                                                                      |
|----------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------|
| <a id="check_mysql_filter"></a>[filter](../common-options.md#filter)                         |                                                                                                                    |
| <a id="check_mysql_warning"></a>[warning](../common-options.md#warning)                      |                                                                                                                    |
| <a id="check_mysql_warn"></a>[warn](../common-options.md#warn)                               |                                                                                                                    |
| <a id="check_mysql_critical"></a>[critical](../common-options.md#critical)                   |                                                                                                                    |
| <a id="check_mysql_crit"></a>[crit](../common-options.md#crit)                               |                                                                                                                    |
| <a id="check_mysql_ok"></a>[ok](../common-options.md#ok)                                     |                                                                                                                    |
| <a id="check_mysql_debug"></a>[debug](../common-options.md#debug)                            | false                                                                                                              |
| <a id="check_mysql_show-all"></a>[show-all](../common-options.md#show-all)                   | false                                                                                                              |
| <a id="check_mysql_empty-state"></a>[empty-state](../common-options.md#empty-state)          | unknown                                                                                                            |
| <a id="check_mysql_perf-config"></a>[perf-config](../common-options.md#perf-config)          |                                                                                                                    |
| <a id="check_mysql_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false                                                                                                              |
| <a id="check_mysql_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,                                                                                                                  |
| <a id="check_mysql_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${status}: ${list}                                                                                                 |
| <a id="check_mysql_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                |                                                                                                                    |
| <a id="check_mysql_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       | %(status): No server information returned                                                                          |
| <a id="check_mysql_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | ${flavor} ${version}, uptime ${uptime}s, connections ${threads_connected}/${max_connections} (${connections_pct}%) |
| <a id="check_mysql_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          | ${flavor}                                                                                                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mysql_filter_keys"></a>
#### Filter keywords

| Option            | Description                                                         |
|-------------------|---------------------------------------------------------------------|
| connections_pct   | Open connections as a percentage of max_connections                 |
| flavor            | Server flavor derived from the version: mysql, mariadb or percona   |
| max_connections   | Configured connection limit (max_connections)                       |
| threads_connected | Currently open connections (Threads_connected)                      |
| uptime            | Seconds since the server started (supports units, e.g. uptime < 1h) |
| version           | Server version, e.g. 10.11.14-MariaDB-ubu2404 or 8.4.3              |
| version_comment   | Server version comment (distribution/build description)             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_mysql_query

Run a custom SQL query and apply thresholds to the returned rows.

#### About `check_mysql_query`

`check_mysql_query` runs an arbitrary SQL query and applies thresholds to the
returned rows, CheckWMI-style: every column of the result set is registered as
a filter keyword, so `filter=`, `warning=` and `critical=` expressions can
reference columns by name, and `detail-syntax` can render them with
`%(column)`. The built-in `line` keyword renders a whole row as
`column=value` pairs.

Notes:

* Each row of the result set is matched separately, so a query returning one
  row per database/queue/job gives per-item results and `${problem_list}`
  works as usual.
* Columns are compared numerically when the threshold side is a number
  (decimal text such as `99.6` is rounded) and as strings otherwise.
* Like other generic query checks, performance data is emitted once you choose
  a `perf-syntax` (there is no meaningful default alias for arbitrary
  queries); the thresholded columns then appear as perf values.
* Statements that produce no result set (a lone `UPDATE`, `SET`, ...) are
  reported as UNKNOWN rather than silently OK — the check is for reading
  state, not mutating it.
* Use a read-only monitoring account: the query runs with whatever privileges
  the configured user has.

**Jump to section:**

* [Sample Commands](#check_mysql_query_samples)
* [Command-line Arguments](#check_mysql_query_options)
* [Filter keywords](#check_mysql_query_filter_keys)


<a id="check_mysql_query_samples"></a>
#### Sample Commands

**List rows returned by a custom query (each column becomes a keyword):**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SELECT table_schema AS db, COUNT(*) AS tables_count FROM information_schema.tables GROUP BY table_schema" "detail-syntax=%(db)=%(tables_count)" "top-syntax=${list}"
information_schema=85, mysql=31, performance_schema=81, sys=102
```

**Threshold on a column value (e.g. long-running queries):**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SELECT COUNT(*) AS slow_queries FROM information_schema.processlist WHERE time > 60 AND command != 'Sleep'" "critical=slow_queries > 0" "top-syntax=${status}: ${list}" "detail-syntax=%(slow_queries) slow queries"
OK: 0 slow queries
```

**Emit the thresholded column as performance data (set a perf-syntax):**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SELECT COUNT(*) AS tables_count FROM information_schema.tables" "critical=tables_count > 5000" "perf-syntax=tables" "top-syntax=${status}: ${list}" "detail-syntax=%(tables_count) tables"
OK: 299 tables|'tables_counttables'=299;0;5000
```

**A statement that returns no result set is reported instead of a silent OK:**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret "query=SET @x = 1"
Query returned no result set (the statement produced no columns)
```

**A missing query is rejected with a clear message:**

```
check_mysql_query host=127.0.0.1 user=monitor password=secret
No query specified (use query=<SQL>)
```



<a id="check_mysql_query_options"></a>
#### Command-line Arguments

<a id="check_mysql_query_query"></a>
<a id="check_mysql_query_socket"></a>
<a id="check_mysql_query_database"></a>
<a id="check_mysql_query_user"></a>
<a id="check_mysql_query_password"></a>
<a id="check_mysql_query_defaults-file"></a>
<a id="check_mysql_query_plugin-dir"></a>

| Option                                            | Default Value | Description                                                                                                                                |
|---------------------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| query                                             |               | The SQL query to execute.                                                                                                                  |
| [host](#check_mysql_query_host)                   | localhost     | MySQL/MariaDB server to connect to.                                                                                                        |
| [port](#check_mysql_query_port)                   | 3306          | TCP port of the server.                                                                                                                    |
| socket                                            |               | Unix socket path (or Windows named pipe) to connect through instead of TCP.                                                                |
| database                                          |               | Default database (schema) to connect to.                                                                                                   |
| user                                              |               | User to authenticate with.                                                                                                                 |
| password                                          |               | Password to authenticate with.                                                                                                             |
| defaults-file                                     |               | my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.                               |
| plugin-dir                                        |               | Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong). |
| [tls](#check_mysql_query_tls)                     | false         | Require TLS on the connection.                                                                                                             |
| [timeout](#check_mysql_query_timeout)             | 10            | Connection timeout in seconds.                                                                                                             |
| [query-timeout](#check_mysql_query_query-timeout) | 30            | Query (read/write) timeout in seconds.                                                                                                     |



<h5 id="check_mysql_query_host">host:</h5>

MySQL/MariaDB server to connect to.

*Default Value:* `localhost`

<h5 id="check_mysql_query_port">port:</h5>

TCP port of the server.

*Default Value:* `3306`

<h5 id="check_mysql_query_tls">tls:</h5>

Require TLS on the connection.

*Default Value:* `false`

<h5 id="check_mysql_query_timeout">timeout:</h5>

Connection timeout in seconds.

*Default Value:* `10`

<h5 id="check_mysql_query_query-timeout">query-timeout:</h5>

Query (read/write) timeout in seconds.

*Default Value:* `30`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                             | Default Value |
|----------------------------------------------------------------------------------------------------|---------------|
| <a id="check_mysql_query_filter"></a>[filter](../common-options.md#filter)                         |               |
| <a id="check_mysql_query_warning"></a>[warning](../common-options.md#warning)                      |               |
| <a id="check_mysql_query_warn"></a>[warn](../common-options.md#warn)                               |               |
| <a id="check_mysql_query_critical"></a>[critical](../common-options.md#critical)                   |               |
| <a id="check_mysql_query_crit"></a>[crit](../common-options.md#crit)                               |               |
| <a id="check_mysql_query_ok"></a>[ok](../common-options.md#ok)                                     |               |
| <a id="check_mysql_query_debug"></a>[debug](../common-options.md#debug)                            | false         |
| <a id="check_mysql_query_show-all"></a>[show-all](../common-options.md#show-all)                   | false         |
| <a id="check_mysql_query_empty-state"></a>[empty-state](../common-options.md#empty-state)          | ignored       |
| <a id="check_mysql_query_perf-config"></a>[perf-config](../common-options.md#perf-config)          |               |
| <a id="check_mysql_query_escape-html"></a>[escape-html](../common-options.md#escape-html)          | false         |
| <a id="check_mysql_query_list-separator"></a>[list-separator](../common-options.md#list-separator) | ,             |
| <a id="check_mysql_query_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)             | ${list}       |
| <a id="check_mysql_query_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                |               |
| <a id="check_mysql_query_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)       |               |
| <a id="check_mysql_query_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)    | %(line)       |
| <a id="check_mysql_query_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)          |               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_mysql_query_filter_keys"></a>
#### Filter keywords

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                      | Description |
|-------------------------------------|-------------|
| [/settings/mysql](#/settings/mysql) |             |


### /settings/mysql <a id="/settings/mysql"></a>



| Key                             | Default Value | Description        |
|---------------------------------|---------------|--------------------|
| [database](#database)           |               | DATABASE           |
| [defaults file](#defaults-file) |               | DEFAULTS FILE      |
| [hostname](#mysql-server)       | localhost     | MYSQL SERVER       |
| [password](#mysql-password)     |               | MYSQL PASSWORD     |
| [plugin dir](#plugin-directory) |               | PLUGIN DIRECTORY   |
| [port](#mysql-port)             | 3306          | MYSQL PORT         |
| [query timeout](#query-timeout) | 30            | QUERY TIMEOUT      |
| [socket](#mysql-socket)         |               | MYSQL SOCKET       |
| [timeout](#connection-timeout)  | 10            | CONNECTION TIMEOUT |
| [tls](#tls)                     | false         | TLS                |
| [user](#mysql-user)             |               | MYSQL USER         |


```ini
# 
[/settings/mysql]
hostname=localhost
port=3306
query timeout=30
timeout=10
tls=false
```

#### DATABASE <a id="/settings/mysql/database"></a>

Default database (schema) to connect to.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | database                            |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mysql]
# DATABASE
database=
```

#### DEFAULTS FILE <a id="/settings/mysql/defaults file"></a>

my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | defaults file                       |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mysql]
# DEFAULTS FILE
defaults file=
```

#### MYSQL SERVER <a id="/settings/mysql/hostname"></a>

Default MySQL/MariaDB server to connect to.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | hostname                            |
| Default value: | `localhost`                         |


**Sample:**

```
[/settings/mysql]
# MYSQL SERVER
hostname=localhost
```

#### MYSQL PASSWORD <a id="/settings/mysql/password"></a>

Password used to authenticate.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | password                            |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mysql]
# MYSQL PASSWORD
password=
```

#### PLUGIN DIRECTORY <a id="/settings/mysql/plugin dir"></a>

Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong).


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | plugin dir                          |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mysql]
# PLUGIN DIRECTORY
plugin dir=
```

#### MYSQL PORT <a id="/settings/mysql/port"></a>

Default TCP port of the server.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | port                                |
| Default value: | `3306`                              |


**Sample:**

```
[/settings/mysql]
# MYSQL PORT
port=3306
```

#### QUERY TIMEOUT <a id="/settings/mysql/query timeout"></a>

Query (read/write) timeout in seconds.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | query timeout                       |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `30`                                |


**Sample:**

```
[/settings/mysql]
# QUERY TIMEOUT
query timeout=30
```

#### MYSQL SOCKET <a id="/settings/mysql/socket"></a>

Unix socket path (or Windows named pipe) to connect through instead of TCP.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | socket                              |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mysql]
# MYSQL SOCKET
socket=
```

#### CONNECTION TIMEOUT <a id="/settings/mysql/timeout"></a>

Connection timeout in seconds.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | timeout                             |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `10`                                |


**Sample:**

```
[/settings/mysql]
# CONNECTION TIMEOUT
timeout=10
```

#### TLS <a id="/settings/mysql/tls"></a>

Require TLS on the connection.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | tls                                 |
| Advanced:      | Yes (means it is not commonly used) |
| Default value: | `false`                             |


**Sample:**

```
[/settings/mysql]
# TLS
tls=false
```

#### MYSQL USER <a id="/settings/mysql/user"></a>

User used to authenticate.


| Key            | Description                         |
|----------------|-------------------------------------|
| Path:          | [/settings/mysql](#/settings/mysql) |
| Key:           | user                                |
| Default value: | _N/A_                               |


**Sample:**

```
[/settings/mysql]
# MYSQL USER
user=
```
