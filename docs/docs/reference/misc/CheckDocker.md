# CheckDocker

Use this module to detect and monitor docker containers

## Enable module

To enable this module and and allow using the commands you need to ass `CheckDocker = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckDocker = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckDocker module.

**List of commands:**

A list of all available queries (check commands)

| Command                       | Description                                      |
|-------------------------------|--------------------------------------------------|
| [check_docker](#check_docker) | Checks that a given docker container is running. |

### check_docker

Checks that a given docker container is running.

#### About `check_docker`

`check_docker` checks the state of docker containers via the local daemon
socket (`/var/run/docker.sock` on Linux, the `\\.\pipe\docker_engine` named
pipe on Windows; configurable via the `endpoint` setting or `host=`). It works
against any daemon speaking the docker API, including podman's compatibility
socket.

Three ways to use it:

* `container=<name>` (repeatable) — the "is my container running" mode: only
  the named containers are checked, and a name the daemon does not know gets
  the synthetic state `missing`, so it trips the default critical instead of
  silently disappearing from the listing.
* No arguments — lists running containers; nothing trips the default
  thresholds, so this is an inventory-style check.
* `all=true` — also includes stopped containers (like `docker ps -a`); any
  non-running container then trips the default critical
  (`container_state != 'running'`), so combine it with `filter=` on hosts
  where exited one-shot containers are expected.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword           | Description                                                                  |
|-------------------|------------------------------------------------------------------------------|
| `names`           | Container name(s), comma separated                                           |
| `container_state` | `created`, `restarting`, `running`, `removing`, `paused`, `exited`, `dead` or `missing` |
| `status`          | Human readable status, e.g. `Up 3 hours (healthy)`                           |
| `health`          | Health-check state: `healthy`, `unhealthy`, `starting`, or empty without a health check |
| `image`           | Image the container was created from                                         |
| `image_id`        | Id of that image                                                             |
| `id`              | Container id                                                                 |
| `command`         | Command the container runs                                                   |
| `ip`              | First IP address on any attached network                                     |
| `ports`           | Published/exposed ports, e.g. `0.0.0.0:8080->80/tcp`                         |
| `labels`          | Container labels as `key=value`, comma separated                             |
| `created`         | When the container was created (date)                                        |

The daemon endpoint is restricted to a local named pipe / absolute socket path:
a UNC path in `host=` would make a Windows host authenticate to a remote SMB
server with the service account, so anything non-local is refused outright.

**Jump to section:**

* [Sample Commands](#check_docker_samples)
* [Command-line Arguments](#check_docker_options)
* [Filter keywords](#check_docker_filter_keys)


<a id="check_docker_samples"></a>
#### Sample Commands

**Require that specific containers are running (a missing or stopped container is CRITICAL):**

```
check_docker container=web-frontend
OK: web-frontend=running
```

```
check_docker container=web-frontend container=backup-agent
CRITICAL: backup-agent=missing
```

**List all running containers (inventory-style):**

```
check_docker
OK: web-frontend=running, database=running
```

**Include stopped containers (`docker ps -a`) — any non-running container trips the default critical:**

```
check_docker all=true
CRITICAL: old-job=exited
```

**Use the container keywords in the output:**

```
check_docker container=web-frontend "detail-syntax=%(names): %(image) %(status) ports=%(ports)" "top-syntax=${status}: ${list}"
OK: web-frontend: nginx:alpine Up 2 hours ports=0.0.0.0:18080->80/tcp,:::18080->80/tcp
```

**Alert on failing container health checks instead of state:**

```
check_docker "filter=has_health_check = 1" "warning=health = 'starting'" "critical=health = 'unhealthy'" "detail-syntax=%(names)=%(health)"
OK: web-frontend=healthy
```

**A daemon that is down is clearly reported (UNKNOWN):**

```
check_docker host=/var/run/missing.sock
Failed to connect to docker daemon at '/var/run/missing.sock': Failed to connect to /var/run/missing.sock: No such file or directory
```



<a id="check_docker_options"></a>
#### Command-line Arguments

<a id="check_docker_warn"></a>
<a id="check_docker_crit"></a>
<a id="check_docker_help"></a>
<a id="check_docker_help-pb"></a>
<a id="check_docker_show-default"></a>
<a id="check_docker_help-short"></a>
<a id="check_docker_host"></a>

| Option                                         | Default Value                | Description                                                                                                               |
|------------------------------------------------|------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_docker_filter)                 |                              | Filter which marks interesting items.                                                                                     |
| [warning](#check_docker_warning)               | container_state != 'running' | Filter which marks items which generates a warning state.                                                                 |
| warn                                           |                              | Short alias for warning                                                                                                   |
| [critical](#check_docker_critical)             | container_state != 'running' | Filter which marks items which generates a critical state.                                                                |
| crit                                           |                              | Short alias for critical.                                                                                                 |
| [ok](#check_docker_ok)                         |                              | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_docker_debug)                   | 1)] (=0                      | Show debugging information in the log                                                                                     |
| [show-all](#check_docker_show-all)             | 1)] (=0                      | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_docker_empty-state)       | warning                      | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_docker_perf-config)       |                              | Performance data generation configuration                                                                                 |
| [escape-html](#check_docker_escape-html)       | 1)] (=0                      | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_docker_list-separator) | ,                            | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                           | N/A                          | Show help screen (this screen)                                                                                            |
| help-pb                                        | N/A                          | Show help screen as a protocol buffer payload                                                                             |
| show-default                                   | N/A                          | Show default values for a given command                                                                                   |
| help-short                                     | N/A                          | Show help screen (short format).                                                                                          |
| [top-syntax](#check_docker_top-syntax)         | ${status}: ${list}           | Top level syntax.                                                                                                         |
| [ok-syntax](#check_docker_ok-syntax)           |                              | ok syntax.                                                                                                                |
| [empty-syntax](#check_docker_empty-syntax)     |                              | Empty syntax.                                                                                                             |
| [detail-syntax](#check_docker_detail-syntax)   | ${names}=${container_state}  | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_docker_perf-syntax)       | ${id}                        | Performance alias syntax.                                                                                                 |
| host                                           |                              | The host or socket of the docker daemon                                                                                   |



<h5 id="check_docker_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_docker_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `container_state != 'running'`

<h5 id="check_docker_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `container_state != 'running'`

<h5 id="check_docker_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_docker_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_docker_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_docker_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `warning`

<h5 id="check_docker_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_docker_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_docker_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_docker_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_docker_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_docker_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_docker_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${names}=${container_state}`

<h5 id="check_docker_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${id}`


<a id="check_docker_filter_keys"></a>
#### Filter keywords

| Option          | Description        |
|-----------------|--------------------|
| command         | Command            |
| container_state | Container image    |
| id              | Container id       |
| image           | Container image    |
| image_id        | Container image id |
| ip              | IP of container    |
| names           | Container image    |

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

