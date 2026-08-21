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

<a id="check_mysql_warn"></a>
<a id="check_mysql_crit"></a>
<a id="check_mysql_help"></a>
<a id="check_mysql_help-pb"></a>
<a id="check_mysql_show-default"></a>
<a id="check_mysql_help-short"></a>
<a id="check_mysql_socket"></a>
<a id="check_mysql_database"></a>
<a id="check_mysql_user"></a>
<a id="check_mysql_password"></a>
<a id="check_mysql_defaults-file"></a>
<a id="check_mysql_plugin-dir"></a>

| Option                                        | Default Value                                                                                                      | Description                                                                                                                                |
|-----------------------------------------------|--------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_mysql_filter)                 |                                                                                                                    | Filter which marks interesting items.                                                                                                      |
| [warning](#check_mysql_warning)               |                                                                                                                    | Filter which marks items which generates a warning state.                                                                                  |
| warn                                          |                                                                                                                    | Short alias for warning                                                                                                                    |
| [critical](#check_mysql_critical)             |                                                                                                                    | Filter which marks items which generates a critical state.                                                                                 |
| crit                                          |                                                                                                                    | Short alias for critical.                                                                                                                  |
| [ok](#check_mysql_ok)                         |                                                                                                                    | Filter which marks items which generates an ok state.                                                                                      |
| [debug](#check_mysql_debug)                   | false                                                                                                              | Show debugging information in the log                                                                                                      |
| [show-all](#check_mysql_show-all)             | false                                                                                                              | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                           |
| [empty-state](#check_mysql_empty-state)       | unknown                                                                                                            | Return status to use when nothing matched filter.                                                                                          |
| [perf-config](#check_mysql_perf-config)       |                                                                                                                    | Performance data generation configuration                                                                                                  |
| [escape-html](#check_mysql_escape-html)       | false                                                                                                              | Escape any < and > characters to prevent HTML encoding                                                                                     |
| [list-separator](#check_mysql_list-separator) | ,                                                                                                                  | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).                  |
| help                                          | N/A                                                                                                                | Show help screen (this screen)                                                                                                             |
| help-pb                                       | N/A                                                                                                                | Show help screen as a protocol buffer payload                                                                                              |
| show-default                                  | N/A                                                                                                                | Show default values for a given command                                                                                                    |
| help-short                                    | N/A                                                                                                                | Show help screen (short format).                                                                                                           |
| [top-syntax](#check_mysql_top-syntax)         | ${status}: ${list}                                                                                                 | Top level syntax.                                                                                                                          |
| [ok-syntax](#check_mysql_ok-syntax)           |                                                                                                                    | ok syntax.                                                                                                                                 |
| [empty-syntax](#check_mysql_empty-syntax)     | %(status): No server information returned                                                                          | Empty syntax.                                                                                                                              |
| [detail-syntax](#check_mysql_detail-syntax)   | ${flavor} ${version}, uptime ${uptime}s, connections ${threads_connected}/${max_connections} (${connections_pct}%) | Detail level syntax.                                                                                                                       |
| [perf-syntax](#check_mysql_perf-syntax)       | ${flavor}                                                                                                          | Performance alias syntax.                                                                                                                  |
| [host](#check_mysql_host)                     | localhost                                                                                                          | MySQL/MariaDB server to connect to.                                                                                                        |
| [port](#check_mysql_port)                     | 3306                                                                                                               | TCP port of the server.                                                                                                                    |
| socket                                        |                                                                                                                    | Unix socket path (or Windows named pipe) to connect through instead of TCP.                                                                |
| database                                      |                                                                                                                    | Default database (schema) to connect to.                                                                                                   |
| user                                          |                                                                                                                    | User to authenticate with.                                                                                                                 |
| password                                      |                                                                                                                    | Password to authenticate with.                                                                                                             |
| defaults-file                                 |                                                                                                                    | my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.                               |
| plugin-dir                                    |                                                                                                                    | Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong). |
| [tls](#check_mysql_tls)                       | false                                                                                                              | Require TLS on the connection.                                                                                                             |
| [timeout](#check_mysql_timeout)               | 10                                                                                                                 | Connection timeout in seconds.                                                                                                             |
| [query-timeout](#check_mysql_query-timeout)   | 30                                                                                                                 | Query (read/write) timeout in seconds.                                                                                                     |



<h5 id="check_mysql_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_mysql_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_mysql_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_mysql_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_mysql_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `false`

<h5 id="check_mysql_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `false`

<h5 id="check_mysql_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_mysql_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_mysql_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `false`

<h5 id="check_mysql_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_mysql_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_mysql_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_mysql_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No server information returned`

<h5 id="check_mysql_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${flavor} ${version}, uptime ${uptime}s, connections ${threads_connected}/${max_connections} (${connections_pct}%)`

<h5 id="check_mysql_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${flavor}`

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

**Common options for all checks:**

| Option        | Description                                                                                                                                                                                                                                                           |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                                                                                                                                                                                                                  |
| crit_count    | Number of items matched the critical criteria.                                                                                                                                                                                                                        |
| crit_list     | A list of all items which matched the critical criteria.                                                                                                                                                                                                              |
| detail_list   | A special list with critical, then warning and finally ok.                                                                                                                                                                                                            |
| list          | A list of all items which matched the filter.                                                                                                                                                                                                                         |
| ok_count      | Number of items matched the ok criteria.                                                                                                                                                                                                                              |
| ok_list       | A list of all items which matched the ok criteria.                                                                                                                                                                                                                    |
| problem_count | Number of items matched either warning or critical criteria.                                                                                                                                                                                                          |
| problem_list  | A list of all items which matched either the critical or the warning criteria.                                                                                                                                                                                        |
| sep           | The decoded list-separator, for use in the top-syntax: templates are never escape-decoded (a literal C:\temp must stay a literal C:\temp), so reference %(sep) to break the line before the first list item, e.g. top-syntax=%(status): %(count) items:%(sep)%(list). |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                                                                                                                                                                                                           |
| total         | Total number of items.                                                                                                                                                                                                                                                |
| warn_count    | Number of items matched the warning criteria.                                                                                                                                                                                                                         |
| warn_list     | A list of all items which matched the warning criteria.                                                                                                                                                                                                               |

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

<a id="check_mysql_query_warn"></a>
<a id="check_mysql_query_crit"></a>
<a id="check_mysql_query_help"></a>
<a id="check_mysql_query_help-pb"></a>
<a id="check_mysql_query_show-default"></a>
<a id="check_mysql_query_help-short"></a>
<a id="check_mysql_query_query"></a>
<a id="check_mysql_query_socket"></a>
<a id="check_mysql_query_database"></a>
<a id="check_mysql_query_user"></a>
<a id="check_mysql_query_password"></a>
<a id="check_mysql_query_defaults-file"></a>
<a id="check_mysql_query_plugin-dir"></a>

| Option                                              | Default Value | Description                                                                                                                                |
|-----------------------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_mysql_query_filter)                 |               | Filter which marks interesting items.                                                                                                      |
| [warning](#check_mysql_query_warning)               |               | Filter which marks items which generates a warning state.                                                                                  |
| warn                                                |               | Short alias for warning                                                                                                                    |
| [critical](#check_mysql_query_critical)             |               | Filter which marks items which generates a critical state.                                                                                 |
| crit                                                |               | Short alias for critical.                                                                                                                  |
| [ok](#check_mysql_query_ok)                         |               | Filter which marks items which generates an ok state.                                                                                      |
| [debug](#check_mysql_query_debug)                   | false         | Show debugging information in the log                                                                                                      |
| [show-all](#check_mysql_query_show-all)             | false         | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                           |
| [empty-state](#check_mysql_query_empty-state)       | ignored       | Return status to use when nothing matched filter.                                                                                          |
| [perf-config](#check_mysql_query_perf-config)       |               | Performance data generation configuration                                                                                                  |
| [escape-html](#check_mysql_query_escape-html)       | false         | Escape any < and > characters to prevent HTML encoding                                                                                     |
| [list-separator](#check_mysql_query_list-separator) | ,             | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).                  |
| help                                                | N/A           | Show help screen (this screen)                                                                                                             |
| help-pb                                             | N/A           | Show help screen as a protocol buffer payload                                                                                              |
| show-default                                        | N/A           | Show default values for a given command                                                                                                    |
| help-short                                          | N/A           | Show help screen (short format).                                                                                                           |
| [top-syntax](#check_mysql_query_top-syntax)         | ${list}       | Top level syntax.                                                                                                                          |
| [ok-syntax](#check_mysql_query_ok-syntax)           |               | ok syntax.                                                                                                                                 |
| [empty-syntax](#check_mysql_query_empty-syntax)     |               | Empty syntax.                                                                                                                              |
| [detail-syntax](#check_mysql_query_detail-syntax)   | %(line)       | Detail level syntax.                                                                                                                       |
| [perf-syntax](#check_mysql_query_perf-syntax)       |               | Performance alias syntax.                                                                                                                  |
| query                                               |               | The SQL query to execute.                                                                                                                  |
| [host](#check_mysql_query_host)                     | localhost     | MySQL/MariaDB server to connect to.                                                                                                        |
| [port](#check_mysql_query_port)                     | 3306          | TCP port of the server.                                                                                                                    |
| socket                                              |               | Unix socket path (or Windows named pipe) to connect through instead of TCP.                                                                |
| database                                            |               | Default database (schema) to connect to.                                                                                                   |
| user                                                |               | User to authenticate with.                                                                                                                 |
| password                                            |               | Password to authenticate with.                                                                                                             |
| defaults-file                                       |               | my.cnf-style file whose [client] section supplies credentials, so passwords can be kept out of nsclient.ini.                               |
| plugin-dir                                          |               | Directory the connector loads client auth plugins from (needed for MySQL 8's caching_sha2_password when the connector's default is wrong). |
| [tls](#check_mysql_query_tls)                       | false         | Require TLS on the connection.                                                                                                             |
| [timeout](#check_mysql_query_timeout)               | 10            | Connection timeout in seconds.                                                                                                             |
| [query-timeout](#check_mysql_query_query-timeout)   | 30            | Query (read/write) timeout in seconds.                                                                                                     |



<h5 id="check_mysql_query_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_mysql_query_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_mysql_query_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_mysql_query_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_mysql_query_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `false`

<h5 id="check_mysql_query_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `false`

<h5 id="check_mysql_query_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_mysql_query_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_mysql_query_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `false`

<h5 id="check_mysql_query_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_mysql_query_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${list}`

<h5 id="check_mysql_query_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_mysql_query_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_mysql_query_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `%(line)`

<h5 id="check_mysql_query_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.


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


<a id="check_mysql_query_filter_keys"></a>
#### Filter keywords

**Common options for all checks:**

| Option        | Description                                                                                                                                                                                                                                                           |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                                                                                                                                                                                                                  |
| crit_count    | Number of items matched the critical criteria.                                                                                                                                                                                                                        |
| crit_list     | A list of all items which matched the critical criteria.                                                                                                                                                                                                              |
| detail_list   | A special list with critical, then warning and finally ok.                                                                                                                                                                                                            |
| list          | A list of all items which matched the filter.                                                                                                                                                                                                                         |
| ok_count      | Number of items matched the ok criteria.                                                                                                                                                                                                                              |
| ok_list       | A list of all items which matched the ok criteria.                                                                                                                                                                                                                    |
| problem_count | Number of items matched either warning or critical criteria.                                                                                                                                                                                                          |
| problem_list  | A list of all items which matched either the critical or the warning criteria.                                                                                                                                                                                        |
| sep           | The decoded list-separator, for use in the top-syntax: templates are never escape-decoded (a literal C:\temp must stay a literal C:\temp), so reference %(sep) to break the line before the first list item, e.g. top-syntax=%(status): %(count) items:%(sep)%(list). |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                                                                                                                                                                                                           |
| total         | Total number of items.                                                                                                                                                                                                                                                |
| warn_count    | Number of items matched the warning criteria.                                                                                                                                                                                                                         |
| warn_list     | A list of all items which matched the warning criteria.                                                                                                                                                                                                               |

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
