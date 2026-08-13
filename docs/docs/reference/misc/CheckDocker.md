# CheckDocker

Check docker containers (state, health) and the docker daemon itself.

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

| Command                                         | Description                                                                                   |
|-------------------------------------------------|-----------------------------------------------------------------------------------------------|
| [check_docker](#check_docker)                   | Check the state of docker containers, optionally requiring specific containers to be running. |
| [check_docker_df](#check_docker_df)             | Check docker disk usage: images, containers, volumes, build cache and reclaimable space.      |
| [check_docker_info](#check_docker_info)         | Check that the docker daemon is healthy: version plus container and image counts.             |
| [check_docker_restarts](#check_docker_restarts) | Detect container restart loops and out-of-memory kills.                                       |
| [check_docker_stats](#check_docker_stats)       | Check per-container resource usage: CPU percent and memory versus limit.                      |

### check_docker

Check the state of docker containers, optionally requiring specific containers to be running.

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

=== "Windows"

    <a id="check_docker_warn"></a>
    <a id="check_docker_crit"></a>
    <a id="check_docker_help"></a>
    <a id="check_docker_help-pb"></a>
    <a id="check_docker_show-default"></a>
    <a id="check_docker_help-short"></a>
    <a id="check_docker_container"></a>

    | Option                                         | Default Value                | Description                                                                                                                    |
    |------------------------------------------------|------------------------------|--------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_filter)                 |                              | Filter which marks interesting items.                                                                                          |
    | [warning](#check_docker_warning)               |                              | Filter which marks items which generates a warning state.                                                                      |
    | warn                                           |                              | Short alias for warning                                                                                                        |
    | [critical](#check_docker_critical)             | container_state != 'running' | Filter which marks items which generates a critical state.                                                                     |
    | crit                                           |                              | Short alias for critical.                                                                                                      |
    | [ok](#check_docker_ok)                         |                              | Filter which marks items which generates an ok state.                                                                          |
    | [debug](#check_docker_debug)                   | 1)] (=0                      | Show debugging information in the log                                                                                          |
    | [show-all](#check_docker_show-all)             | 1)] (=0                      | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).               |
    | [empty-state](#check_docker_empty-state)       | warning                      | Return status to use when nothing matched filter.                                                                              |
    | [perf-config](#check_docker_perf-config)       |                              | Performance data generation configuration                                                                                      |
    | [escape-html](#check_docker_escape-html)       | 1)] (=0                      | Escape any < and > characters to prevent HTML encoding                                                                         |
    | [list-separator](#check_docker_list-separator) | ,                            | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).      |
    | help                                           | N/A                          | Show help screen (this screen)                                                                                                 |
    | help-pb                                        | N/A                          | Show help screen as a protocol buffer payload                                                                                  |
    | show-default                                   | N/A                          | Show default values for a given command                                                                                        |
    | help-short                                     | N/A                          | Show help screen (short format).                                                                                               |
    | [top-syntax](#check_docker_top-syntax)         | ${status}: ${list}           | Top level syntax.                                                                                                              |
    | [ok-syntax](#check_docker_ok-syntax)           |                              | ok syntax.                                                                                                                     |
    | [empty-syntax](#check_docker_empty-syntax)     | No containers found          | Empty syntax.                                                                                                                  |
    | [detail-syntax](#check_docker_detail-syntax)   | ${names}=${container_state}  | Detail level syntax.                                                                                                           |
    | [perf-syntax](#check_docker_perf-syntax)       | ${names}                     | Performance alias syntax.                                                                                                      |
    | [host](#check_docker_host)                     | \\.\pipe\docker_engine       | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                 |
    | [timeout](#check_docker_timeout)               | 10                           | Timeout for talking to the daemon, in seconds.                                                                                 |
    | [all](#check_docker_all)                       | 1)] (=0                      | Include stopped containers (docker ps -a); by default only running containers are listed.                                      |
    | container                                      |                              | Name of a container that must exist (repeatable). Implies all; a name the daemon does not know gets container_state 'missing'. |



    <h5 id="check_docker_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



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

    *Default Value:* `No containers found`

    <h5 id="check_docker_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${names}=${container_state}`

    <h5 id="check_docker_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${names}`

    <h5 id="check_docker_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

    <h5 id="check_docker_all">all:</h5>

    Include stopped containers (docker ps -a); by default only running containers are listed.

    *Default Value:* `1)] (=0`

=== "Linux"

    <a id="check_docker_warn"></a>
    <a id="check_docker_crit"></a>
    <a id="check_docker_help"></a>
    <a id="check_docker_help-pb"></a>
    <a id="check_docker_show-default"></a>
    <a id="check_docker_help-short"></a>
    <a id="check_docker_container"></a>

    | Option                                         | Default Value                | Description                                                                                                                    |
    |------------------------------------------------|------------------------------|--------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_filter)                 |                              | Filter which marks interesting items.                                                                                          |
    | [warning](#check_docker_warning)               |                              | Filter which marks items which generates a warning state.                                                                      |
    | warn                                           |                              | Short alias for warning                                                                                                        |
    | [critical](#check_docker_critical)             | container_state != 'running' | Filter which marks items which generates a critical state.                                                                     |
    | crit                                           |                              | Short alias for critical.                                                                                                      |
    | [ok](#check_docker_ok)                         |                              | Filter which marks items which generates an ok state.                                                                          |
    | [debug](#check_docker_debug)                   | 1)] (=0                      | Show debugging information in the log                                                                                          |
    | [show-all](#check_docker_show-all)             | 1)] (=0                      | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).               |
    | [empty-state](#check_docker_empty-state)       | warning                      | Return status to use when nothing matched filter.                                                                              |
    | [perf-config](#check_docker_perf-config)       |                              | Performance data generation configuration                                                                                      |
    | [escape-html](#check_docker_escape-html)       | 1)] (=0                      | Escape any < and > characters to prevent HTML encoding                                                                         |
    | [list-separator](#check_docker_list-separator) | ,                            | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).      |
    | help                                           | N/A                          | Show help screen (this screen)                                                                                                 |
    | help-pb                                        | N/A                          | Show help screen as a protocol buffer payload                                                                                  |
    | show-default                                   | N/A                          | Show default values for a given command                                                                                        |
    | help-short                                     | N/A                          | Show help screen (short format).                                                                                               |
    | [top-syntax](#check_docker_top-syntax)         | ${status}: ${list}           | Top level syntax.                                                                                                              |
    | [ok-syntax](#check_docker_ok-syntax)           |                              | ok syntax.                                                                                                                     |
    | [empty-syntax](#check_docker_empty-syntax)     | No containers found          | Empty syntax.                                                                                                                  |
    | [detail-syntax](#check_docker_detail-syntax)   | ${names}=${container_state}  | Detail level syntax.                                                                                                           |
    | [perf-syntax](#check_docker_perf-syntax)       | ${names}                     | Performance alias syntax.                                                                                                      |
    | [host](#check_docker_host)                     | /var/run/docker.sock         | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                 |
    | [timeout](#check_docker_timeout)               | 10                           | Timeout for talking to the daemon, in seconds.                                                                                 |
    | [all](#check_docker_all)                       | 1)] (=0                      | Include stopped containers (docker ps -a); by default only running containers are listed.                                      |
    | container                                      |                              | Name of a container that must exist (repeatable). Implies all; a name the daemon does not know gets container_state 'missing'. |



    <h5 id="check_docker_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



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

    *Default Value:* `No containers found`

    <h5 id="check_docker_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${names}=${container_state}`

    <h5 id="check_docker_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${names}`

    <h5 id="check_docker_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

    <h5 id="check_docker_all">all:</h5>

    Include stopped containers (docker ps -a); by default only running containers are listed.

    *Default Value:* `1)] (=0`


<a id="check_docker_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                                                                     |
|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| command          | Command the container runs                                                                                                                      |
| container_state  | Container state: created, restarting, running, removing, paused, exited, dead or missing (a requested container the daemon does not know about) |
| created          | When the container was created                                                                                                                  |
| has_health_check | 1 when the container defines a health check, else 0                                                                                             |
| health           | Health-check state: healthy, unhealthy, starting or empty when the container has no health check                                                |
| id               | Container id                                                                                                                                    |
| image            | Image the container was created from                                                                                                            |
| image_id         | Id of the image the container was created from                                                                                                  |
| ip               | First IP address on any network the container is attached to                                                                                    |
| labels           | Container labels as key=value, comma separated                                                                                                  |
| names            | Container name(s), comma separated                                                                                                              |
| ports            | Published/exposed ports, e.g. 0.0.0.0:8080->80/tcp                                                                                              |

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

### check_docker_df

Check docker disk usage: images, containers, volumes, build cache and reclaimable space.

#### About `check_docker_df`

`check_docker_df` reports docker's disk usage the way `docker system df`
does — images, container writable layers, volumes and the build cache — plus
what a prune would reclaim: unused images (their non-shared size), stopped
containers, unreferenced volumes and idle build cache.

The daemon computes this by walking every image layer, container filesystem
and volume, so **the request is slow on large hosts** (seconds to minutes).
The check therefore defaults to six times the module's normal timeout; don't
schedule it more often than you need it.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword                   | Description                                                  |
|---------------------------|--------------------------------------------------------------|
| `images` / `unused_images`| Number of images / images no container uses                  |
| `images_size`             | Disk used by images (unique layer sizes)                     |
| `images_reclaimable`      | Freed by pruning unused images                               |
| `containers`              | Number of containers (running and stopped)                   |
| `containers_size`         | Disk used by container writable layers                       |
| `containers_reclaimable`  | Freed by pruning stopped containers                          |
| `volumes` / `unused_volumes` | Number of volumes / volumes no container references       |
| `volumes_size`            | Disk used by volumes                                         |
| `volumes_reclaimable`     | Freed by pruning unused volumes                              |
| `build_cache_size`        | Disk used by the build cache                                 |
| `build_cache_reclaimable` | Freed by pruning the idle build cache                        |
| `total_size`              | Everything above combined                                    |
| `total_reclaimable`       | Everything a full prune would free                           |
| `message`                 | The human readable summary line                              |

All `*_size` / `*_reclaimable` keywords are byte-sized: thresholds take a unit
(`500M`, `10G`, `1T`); a bare number is rejected. No default thresholds — a
responding daemon is OK until you add limits.

**Jump to section:**

* [Sample Commands](#check_docker_df_samples)
* [Command-line Arguments](#check_docker_df_options)
* [Filter keywords](#check_docker_df_filter_keys)


<a id="check_docker_df_samples"></a>
#### Sample Commands

**Show docker disk usage (like `docker system df`):**

```
check_docker_df
OK: images 71 (8.216GB), containers 19 (388KB), volumes 6 (4.517GB), build cache 12.808GB, reclaimable 21.479GB
```

**Alert when a prune would free a lot of space (size thresholds take a unit):**

```
check_docker_df "warning=total_reclaimable > 10G" "critical=total_reclaimable > 50G"
WARNING: images 71 (8.216GB), containers 19 (388KB), volumes 6 (4.517GB), build cache 12.808GB, reclaimable 21.479GB|'docker reclaimable'=23063090871B;10737418240;53687091200
```

**Alert on unused images piling up:**

```
check_docker_df "warning=unused_images > 20"
OK: images 71 (8.216GB), containers 19 (388KB), volumes 6 (4.517GB), build cache 12.808GB, reclaimable 21.479GB|'docker unused images'=63;20;0
```

**Watch a specific category, e.g. the build cache:**

```
check_docker_df "warning=build_cache_size > 20G" "detail-syntax=build cache %(build_cache_size) (reclaimable %(build_cache_reclaimable))"
OK: build cache 13752766549 (reclaimable 13752766549)|'docker build cache'=13752766549B;21474836480;0
```



<a id="check_docker_df_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_docker_df_warn"></a>
    <a id="check_docker_df_crit"></a>
    <a id="check_docker_df_help"></a>
    <a id="check_docker_df_help-pb"></a>
    <a id="check_docker_df_show-default"></a>
    <a id="check_docker_df_help-short"></a>

    | Option                                            | Default Value                                 | Description                                                                                                               |
    |---------------------------------------------------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_df_filter)                 |                                               | Filter which marks interesting items.                                                                                     |
    | [warning](#check_docker_df_warning)               |                                               | Filter which marks items which generates a warning state.                                                                 |
    | warn                                              |                                               | Short alias for warning                                                                                                   |
    | [critical](#check_docker_df_critical)             |                                               | Filter which marks items which generates a critical state.                                                                |
    | crit                                              |                                               | Short alias for critical.                                                                                                 |
    | [ok](#check_docker_df_ok)                         |                                               | Filter which marks items which generates an ok state.                                                                     |
    | [debug](#check_docker_df_debug)                   | 1)] (=0                                       | Show debugging information in the log                                                                                     |
    | [show-all](#check_docker_df_show-all)             | 1)] (=0                                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
    | [empty-state](#check_docker_df_empty-state)       | unknown                                       | Return status to use when nothing matched filter.                                                                         |
    | [perf-config](#check_docker_df_perf-config)       |                                               | Performance data generation configuration                                                                                 |
    | [escape-html](#check_docker_df_escape-html)       | 1)] (=0                                       | Escape any < and > characters to prevent HTML encoding                                                                    |
    | [list-separator](#check_docker_df_list-separator) | ,                                             | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
    | help                                              | N/A                                           | Show help screen (this screen)                                                                                            |
    | help-pb                                           | N/A                                           | Show help screen as a protocol buffer payload                                                                             |
    | show-default                                      | N/A                                           | Show default values for a given command                                                                                   |
    | help-short                                        | N/A                                           | Show help screen (short format).                                                                                          |
    | [top-syntax](#check_docker_df_top-syntax)         | ${status}: ${list}                            | Top level syntax.                                                                                                         |
    | [ok-syntax](#check_docker_df_ok-syntax)           |                                               | ok syntax.                                                                                                                |
    | [empty-syntax](#check_docker_df_empty-syntax)     | %(status): No disk usage information returned | Empty syntax.                                                                                                             |
    | [detail-syntax](#check_docker_df_detail-syntax)   | ${message}                                    | Detail level syntax.                                                                                                      |
    | [perf-syntax](#check_docker_df_perf-syntax)       | docker                                        | Performance alias syntax.                                                                                                 |
    | [host](#check_docker_df_host)                     | \\.\pipe\docker_engine                        | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                            |
    | [timeout](#check_docker_df_timeout)               | 60                                            | Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).                                     |



    <h5 id="check_docker_df_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_df_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_docker_df_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_docker_df_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_df_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_df_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_df_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_docker_df_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_df_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_df_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_df_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_df_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_df_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `%(status): No disk usage information returned`

    <h5 id="check_docker_df_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${message}`

    <h5 id="check_docker_df_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `docker`

    <h5 id="check_docker_df_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_df_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).

    *Default Value:* `60`

=== "Linux"

    <a id="check_docker_df_warn"></a>
    <a id="check_docker_df_crit"></a>
    <a id="check_docker_df_help"></a>
    <a id="check_docker_df_help-pb"></a>
    <a id="check_docker_df_show-default"></a>
    <a id="check_docker_df_help-short"></a>

    | Option                                            | Default Value                                 | Description                                                                                                               |
    |---------------------------------------------------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_df_filter)                 |                                               | Filter which marks interesting items.                                                                                     |
    | [warning](#check_docker_df_warning)               |                                               | Filter which marks items which generates a warning state.                                                                 |
    | warn                                              |                                               | Short alias for warning                                                                                                   |
    | [critical](#check_docker_df_critical)             |                                               | Filter which marks items which generates a critical state.                                                                |
    | crit                                              |                                               | Short alias for critical.                                                                                                 |
    | [ok](#check_docker_df_ok)                         |                                               | Filter which marks items which generates an ok state.                                                                     |
    | [debug](#check_docker_df_debug)                   | 1)] (=0                                       | Show debugging information in the log                                                                                     |
    | [show-all](#check_docker_df_show-all)             | 1)] (=0                                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
    | [empty-state](#check_docker_df_empty-state)       | unknown                                       | Return status to use when nothing matched filter.                                                                         |
    | [perf-config](#check_docker_df_perf-config)       |                                               | Performance data generation configuration                                                                                 |
    | [escape-html](#check_docker_df_escape-html)       | 1)] (=0                                       | Escape any < and > characters to prevent HTML encoding                                                                    |
    | [list-separator](#check_docker_df_list-separator) | ,                                             | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
    | help                                              | N/A                                           | Show help screen (this screen)                                                                                            |
    | help-pb                                           | N/A                                           | Show help screen as a protocol buffer payload                                                                             |
    | show-default                                      | N/A                                           | Show default values for a given command                                                                                   |
    | help-short                                        | N/A                                           | Show help screen (short format).                                                                                          |
    | [top-syntax](#check_docker_df_top-syntax)         | ${status}: ${list}                            | Top level syntax.                                                                                                         |
    | [ok-syntax](#check_docker_df_ok-syntax)           |                                               | ok syntax.                                                                                                                |
    | [empty-syntax](#check_docker_df_empty-syntax)     | %(status): No disk usage information returned | Empty syntax.                                                                                                             |
    | [detail-syntax](#check_docker_df_detail-syntax)   | ${message}                                    | Detail level syntax.                                                                                                      |
    | [perf-syntax](#check_docker_df_perf-syntax)       | docker                                        | Performance alias syntax.                                                                                                 |
    | [host](#check_docker_df_host)                     | /var/run/docker.sock                          | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                            |
    | [timeout](#check_docker_df_timeout)               | 60                                            | Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).                                     |



    <h5 id="check_docker_df_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_df_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_docker_df_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_docker_df_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_df_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_df_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_df_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_docker_df_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_df_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_df_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_df_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_df_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_df_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `%(status): No disk usage information returned`

    <h5 id="check_docker_df_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${message}`

    <h5 id="check_docker_df_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `docker`

    <h5 id="check_docker_df_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_df_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).

    *Default Value:* `60`


<a id="check_docker_df_filter_keys"></a>
#### Filter keywords

| Option                  | Description                                                         |
|-------------------------|---------------------------------------------------------------------|
| build_cache_reclaimable | Disk freed by pruning the idle build cache (bytes)                  |
| build_cache_size        | Disk used by the build cache (bytes)                                |
| containers              | Number of containers (running and stopped)                          |
| containers_reclaimable  | Disk freed by pruning stopped containers (bytes)                    |
| containers_size         | Disk used by container writable layers (bytes)                      |
| images                  | Number of images                                                    |
| images_reclaimable      | Disk freed by pruning unused images (bytes)                         |
| images_size             | Disk used by images (bytes; supports units, e.g. images_size > 10G) |
| message                 | Human readable disk-usage summary                                   |
| total_reclaimable       | Total disk a full prune would free (bytes)                          |
| total_size              | Total disk used by docker (bytes)                                   |
| unused_images           | Number of images not used by any container                          |
| unused_volumes          | Number of volumes not referenced by any container                   |
| volumes                 | Number of volumes                                                   |
| volumes_reclaimable     | Disk freed by pruning unused volumes (bytes)                        |
| volumes_size            | Disk used by volumes (bytes)                                        |

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

### check_docker_info

Check that the docker daemon is healthy: version plus container and image counts.

#### About `check_docker_info`

`check_docker_info` checks the docker daemon itself: it reads `/info` from the
local daemon socket and reports the server version, host name and the
container/image counts. A responding daemon is **OK** unless you threshold the
counts; an unreachable daemon is **UNKNOWN** with the transport error — which
makes this the natural "is docker itself healthy" companion to per-container
`check_docker` checks.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword      | Description                            |
|--------------|----------------------------------------|
| `version`    | Docker server version                  |
| `name`       | Daemon host name                       |
| `os`         | Operating system the daemon runs on    |
| `containers` | Total number of containers             |
| `running`    | Number of running containers           |
| `paused`     | Number of paused containers            |
| `stopped`    | Number of stopped containers           |
| `images`     | Number of images                       |

Count keywords used in thresholds are emitted as performance data.

**Jump to section:**

* [Sample Commands](#check_docker_info_samples)
* [Command-line Arguments](#check_docker_info_options)
* [Filter keywords](#check_docker_info_filter_keys)


<a id="check_docker_info_samples"></a>
#### Sample Commands

**Check that the docker daemon itself is up and responding:**

```
check_docker_info
OK: docker 29.5.3 on docker-host: 16 running, 0 paused, 4 stopped containers, 231 images
```

**Alert when nothing is running (or too much is stopped), with perf data:**

```
check_docker_info "warning=running < 1" "critical=stopped > 100"
OK: docker 29.5.3 on docker-host: 16 running, 0 paused, 4 stopped containers, 231 images|'docker-host running'=16;1;0 'docker-host stopped'=4;0;100
```

**A daemon that is down is clearly reported (UNKNOWN):**

```
check_docker_info host=/var/run/missing.sock
Failed to connect to docker daemon at '/var/run/missing.sock': Failed to connect to /var/run/missing.sock: No such file or directory
```



<a id="check_docker_info_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_docker_info_warn"></a>
    <a id="check_docker_info_crit"></a>
    <a id="check_docker_info_help"></a>
    <a id="check_docker_info_help-pb"></a>
    <a id="check_docker_info_show-default"></a>
    <a id="check_docker_info_help-short"></a>

    | Option                                              | Default Value                                                                                                       | Description                                                                                                               |
    |-----------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_info_filter)                 |                                                                                                                     | Filter which marks interesting items.                                                                                     |
    | [warning](#check_docker_info_warning)               |                                                                                                                     | Filter which marks items which generates a warning state.                                                                 |
    | warn                                                |                                                                                                                     | Short alias for warning                                                                                                   |
    | [critical](#check_docker_info_critical)             |                                                                                                                     | Filter which marks items which generates a critical state.                                                                |
    | crit                                                |                                                                                                                     | Short alias for critical.                                                                                                 |
    | [ok](#check_docker_info_ok)                         |                                                                                                                     | Filter which marks items which generates an ok state.                                                                     |
    | [debug](#check_docker_info_debug)                   | 1)] (=0                                                                                                             | Show debugging information in the log                                                                                     |
    | [show-all](#check_docker_info_show-all)             | 1)] (=0                                                                                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
    | [empty-state](#check_docker_info_empty-state)       | unknown                                                                                                             | Return status to use when nothing matched filter.                                                                         |
    | [perf-config](#check_docker_info_perf-config)       |                                                                                                                     | Performance data generation configuration                                                                                 |
    | [escape-html](#check_docker_info_escape-html)       | 1)] (=0                                                                                                             | Escape any < and > characters to prevent HTML encoding                                                                    |
    | [list-separator](#check_docker_info_list-separator) | ,                                                                                                                   | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
    | help                                                | N/A                                                                                                                 | Show help screen (this screen)                                                                                            |
    | help-pb                                             | N/A                                                                                                                 | Show help screen as a protocol buffer payload                                                                             |
    | show-default                                        | N/A                                                                                                                 | Show default values for a given command                                                                                   |
    | help-short                                          | N/A                                                                                                                 | Show help screen (short format).                                                                                          |
    | [top-syntax](#check_docker_info_top-syntax)         | ${status}: ${list}                                                                                                  | Top level syntax.                                                                                                         |
    | [ok-syntax](#check_docker_info_ok-syntax)           |                                                                                                                     | ok syntax.                                                                                                                |
    | [empty-syntax](#check_docker_info_empty-syntax)     | %(status): No daemon information returned                                                                           | Empty syntax.                                                                                                             |
    | [detail-syntax](#check_docker_info_detail-syntax)   | docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images | Detail level syntax.                                                                                                      |
    | [perf-syntax](#check_docker_info_perf-syntax)       | ${name}                                                                                                             | Performance alias syntax.                                                                                                 |
    | [host](#check_docker_info_host)                     | \\.\pipe\docker_engine                                                                                              | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                            |
    | [timeout](#check_docker_info_timeout)               | 10                                                                                                                  | Timeout for talking to the daemon, in seconds.                                                                            |



    <h5 id="check_docker_info_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_info_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_docker_info_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_docker_info_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_info_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_info_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_info_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_docker_info_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_info_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_info_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_info_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_info_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_info_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `%(status): No daemon information returned`

    <h5 id="check_docker_info_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images`

    <h5 id="check_docker_info_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

    <h5 id="check_docker_info_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_info_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

=== "Linux"

    <a id="check_docker_info_warn"></a>
    <a id="check_docker_info_crit"></a>
    <a id="check_docker_info_help"></a>
    <a id="check_docker_info_help-pb"></a>
    <a id="check_docker_info_show-default"></a>
    <a id="check_docker_info_help-short"></a>

    | Option                                              | Default Value                                                                                                       | Description                                                                                                               |
    |-----------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_info_filter)                 |                                                                                                                     | Filter which marks interesting items.                                                                                     |
    | [warning](#check_docker_info_warning)               |                                                                                                                     | Filter which marks items which generates a warning state.                                                                 |
    | warn                                                |                                                                                                                     | Short alias for warning                                                                                                   |
    | [critical](#check_docker_info_critical)             |                                                                                                                     | Filter which marks items which generates a critical state.                                                                |
    | crit                                                |                                                                                                                     | Short alias for critical.                                                                                                 |
    | [ok](#check_docker_info_ok)                         |                                                                                                                     | Filter which marks items which generates an ok state.                                                                     |
    | [debug](#check_docker_info_debug)                   | 1)] (=0                                                                                                             | Show debugging information in the log                                                                                     |
    | [show-all](#check_docker_info_show-all)             | 1)] (=0                                                                                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
    | [empty-state](#check_docker_info_empty-state)       | unknown                                                                                                             | Return status to use when nothing matched filter.                                                                         |
    | [perf-config](#check_docker_info_perf-config)       |                                                                                                                     | Performance data generation configuration                                                                                 |
    | [escape-html](#check_docker_info_escape-html)       | 1)] (=0                                                                                                             | Escape any < and > characters to prevent HTML encoding                                                                    |
    | [list-separator](#check_docker_info_list-separator) | ,                                                                                                                   | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
    | help                                                | N/A                                                                                                                 | Show help screen (this screen)                                                                                            |
    | help-pb                                             | N/A                                                                                                                 | Show help screen as a protocol buffer payload                                                                             |
    | show-default                                        | N/A                                                                                                                 | Show default values for a given command                                                                                   |
    | help-short                                          | N/A                                                                                                                 | Show help screen (short format).                                                                                          |
    | [top-syntax](#check_docker_info_top-syntax)         | ${status}: ${list}                                                                                                  | Top level syntax.                                                                                                         |
    | [ok-syntax](#check_docker_info_ok-syntax)           |                                                                                                                     | ok syntax.                                                                                                                |
    | [empty-syntax](#check_docker_info_empty-syntax)     | %(status): No daemon information returned                                                                           | Empty syntax.                                                                                                             |
    | [detail-syntax](#check_docker_info_detail-syntax)   | docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images | Detail level syntax.                                                                                                      |
    | [perf-syntax](#check_docker_info_perf-syntax)       | ${name}                                                                                                             | Performance alias syntax.                                                                                                 |
    | [host](#check_docker_info_host)                     | /var/run/docker.sock                                                                                                | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                            |
    | [timeout](#check_docker_info_timeout)               | 10                                                                                                                  | Timeout for talking to the daemon, in seconds.                                                                            |



    <h5 id="check_docker_info_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_info_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_docker_info_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_docker_info_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_info_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_info_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_info_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `unknown`

    <h5 id="check_docker_info_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_info_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_info_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_info_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_info_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_info_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `%(status): No daemon information returned`

    <h5 id="check_docker_info_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images`

    <h5 id="check_docker_info_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

    <h5 id="check_docker_info_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_info_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


<a id="check_docker_info_filter_keys"></a>
#### Filter keywords

| Option     | Description                         |
|------------|-------------------------------------|
| containers | Total number of containers          |
| images     | Number of images                    |
| name       | Daemon host name                    |
| os         | Operating system the daemon runs on |
| paused     | Number of paused containers         |
| running    | Number of running containers        |
| stopped    | Number of stopped containers        |
| version    | Docker server version               |

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

### check_docker_restarts

Detect container restart loops and out-of-memory kills.

#### About `check_docker_restarts`

`check_docker_restarts` detects crash-looping and OOM-killed containers via
the inspect endpoint (`RestartCount`, `State.OOMKilled`, `State.StartedAt`,
`State.ExitCode`). It always includes stopped containers: a crash-looping
container spends most of its time not-running, and a final crash leaves it
exited — both must stay visible.

Default thresholds encode the crash-loop signature: **warning** when
`restart_count > 3 and started < 15m and started >= 0` (many restarts *and* a
recent start — a container that has been up for a month is fine no matter how
bumpy its past), **critical** when `oom_killed = 1` (the last exit was an
out-of-memory kill).

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword           | Description                                                           |
|-------------------|-----------------------------------------------------------------------|
| `names`           | Container name(s), comma separated                                    |
| `image`           | Image the container was created from                                  |
| `container_state` | `created`, `restarting`, `running`, `removing`, `paused`, `exited`, `dead` |
| `restart_count`   | Restarts since the container was created                              |
| `started`         | Seconds since the last start, `-1` if it never started; supports units (`started < 10m`) |
| `exit_code`       | Exit code of the last exit (0 while running fine)                     |
| `oom_killed`      | `1` when the last exit was an out-of-memory kill, else `0`            |

`container=<name>` (repeatable) scopes the check to specific containers.
Note that `restart_count` is cumulative for the container's lifetime; that is
why the default warning also requires a recent start before it fires.

**Jump to section:**

* [Sample Commands](#check_docker_restarts_samples)
* [Command-line Arguments](#check_docker_restarts_options)
* [Filter keywords](#check_docker_restarts_filter_keys)


<a id="check_docker_restarts_samples"></a>
#### Sample Commands

**Detect restart loops (default: WARNING when a container restarted more than 3 times and last started within 15 minutes; OOM kills are CRITICAL):**

```
check_docker_restarts
WARNING: crashy-app: 8 restarts, restarting
```

A stable container is OK no matter how bumpy its distant past:

```
check_docker_restarts container=app-backend
OK: app-backend: 0 restarts, running
```

**An out-of-memory kill is CRITICAL by default:**

```
check_docker_restarts container=greedy-app
CRITICAL: greedy-app: 2 restarts, exited
```

**Custom rules using the keywords (e.g. any restart of a specific container within the last hour):**

```
check_docker_restarts container=app-backend "warning=restart_count > 0 and started < 1h" "detail-syntax=%(names): %(restart_count) restarts, up %(started)s, exit=%(exit_code) oom=%(oom_killed)"
OK: app-backend: 0 restarts, up 236s, exit=0 oom=0|'app-backend restarts'=0;0;0
```



<a id="check_docker_restarts_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_docker_restarts_warn"></a>
    <a id="check_docker_restarts_crit"></a>
    <a id="check_docker_restarts_help"></a>
    <a id="check_docker_restarts_help-pb"></a>
    <a id="check_docker_restarts_show-default"></a>
    <a id="check_docker_restarts_help-short"></a>
    <a id="check_docker_restarts_container"></a>

    | Option                                                  | Default Value                                           | Description                                                                                                               |
    |---------------------------------------------------------|---------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_restarts_filter)                 |                                                         | Filter which marks interesting items.                                                                                     |
    | [warning](#check_docker_restarts_warning)               | restart_count > 3 and started < 15m and started >= 0    | Filter which marks items which generates a warning state.                                                                 |
    | warn                                                    |                                                         | Short alias for warning                                                                                                   |
    | [critical](#check_docker_restarts_critical)             | oom_killed = 1                                          | Filter which marks items which generates a critical state.                                                                |
    | crit                                                    |                                                         | Short alias for critical.                                                                                                 |
    | [ok](#check_docker_restarts_ok)                         |                                                         | Filter which marks items which generates an ok state.                                                                     |
    | [debug](#check_docker_restarts_debug)                   | 1)] (=0                                                 | Show debugging information in the log                                                                                     |
    | [show-all](#check_docker_restarts_show-all)             | 1)] (=0                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
    | [empty-state](#check_docker_restarts_empty-state)       | ok                                                      | Return status to use when nothing matched filter.                                                                         |
    | [perf-config](#check_docker_restarts_perf-config)       |                                                         | Performance data generation configuration                                                                                 |
    | [escape-html](#check_docker_restarts_escape-html)       | 1)] (=0                                                 | Escape any < and > characters to prevent HTML encoding                                                                    |
    | [list-separator](#check_docker_restarts_list-separator) | ,                                                       | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
    | help                                                    | N/A                                                     | Show help screen (this screen)                                                                                            |
    | help-pb                                                 | N/A                                                     | Show help screen as a protocol buffer payload                                                                             |
    | show-default                                            | N/A                                                     | Show default values for a given command                                                                                   |
    | help-short                                              | N/A                                                     | Show help screen (short format).                                                                                          |
    | [top-syntax](#check_docker_restarts_top-syntax)         | ${status}: ${list}                                      | Top level syntax.                                                                                                         |
    | [ok-syntax](#check_docker_restarts_ok-syntax)           |                                                         | ok syntax.                                                                                                                |
    | [empty-syntax](#check_docker_restarts_empty-syntax)     | No containers found                                     | Empty syntax.                                                                                                             |
    | [detail-syntax](#check_docker_restarts_detail-syntax)   | ${names}: ${restart_count} restarts, ${container_state} | Detail level syntax.                                                                                                      |
    | [perf-syntax](#check_docker_restarts_perf-syntax)       | ${names}                                                | Performance alias syntax.                                                                                                 |
    | [host](#check_docker_restarts_host)                     | \\.\pipe\docker_engine                                  | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                            |
    | [timeout](#check_docker_restarts_timeout)               | 10                                                      | Timeout for talking to the daemon, in seconds.                                                                            |
    | container                                               |                                                         | Only inspect the named container (repeatable).                                                                            |



    <h5 id="check_docker_restarts_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_restarts_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `restart_count > 3 and started < 15m and started >= 0`

    <h5 id="check_docker_restarts_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `oom_killed = 1`

    <h5 id="check_docker_restarts_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_restarts_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_restarts_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_restarts_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `ok`

    <h5 id="check_docker_restarts_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_restarts_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_restarts_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_restarts_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_restarts_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_restarts_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No containers found`

    <h5 id="check_docker_restarts_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${names}: ${restart_count} restarts, ${container_state}`

    <h5 id="check_docker_restarts_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${names}`

    <h5 id="check_docker_restarts_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_restarts_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

=== "Linux"

    <a id="check_docker_restarts_warn"></a>
    <a id="check_docker_restarts_crit"></a>
    <a id="check_docker_restarts_help"></a>
    <a id="check_docker_restarts_help-pb"></a>
    <a id="check_docker_restarts_show-default"></a>
    <a id="check_docker_restarts_help-short"></a>
    <a id="check_docker_restarts_container"></a>

    | Option                                                  | Default Value                                           | Description                                                                                                               |
    |---------------------------------------------------------|---------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_restarts_filter)                 |                                                         | Filter which marks interesting items.                                                                                     |
    | [warning](#check_docker_restarts_warning)               | restart_count > 3 and started < 15m and started >= 0    | Filter which marks items which generates a warning state.                                                                 |
    | warn                                                    |                                                         | Short alias for warning                                                                                                   |
    | [critical](#check_docker_restarts_critical)             | oom_killed = 1                                          | Filter which marks items which generates a critical state.                                                                |
    | crit                                                    |                                                         | Short alias for critical.                                                                                                 |
    | [ok](#check_docker_restarts_ok)                         |                                                         | Filter which marks items which generates an ok state.                                                                     |
    | [debug](#check_docker_restarts_debug)                   | 1)] (=0                                                 | Show debugging information in the log                                                                                     |
    | [show-all](#check_docker_restarts_show-all)             | 1)] (=0                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
    | [empty-state](#check_docker_restarts_empty-state)       | ok                                                      | Return status to use when nothing matched filter.                                                                         |
    | [perf-config](#check_docker_restarts_perf-config)       |                                                         | Performance data generation configuration                                                                                 |
    | [escape-html](#check_docker_restarts_escape-html)       | 1)] (=0                                                 | Escape any < and > characters to prevent HTML encoding                                                                    |
    | [list-separator](#check_docker_restarts_list-separator) | ,                                                       | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
    | help                                                    | N/A                                                     | Show help screen (this screen)                                                                                            |
    | help-pb                                                 | N/A                                                     | Show help screen as a protocol buffer payload                                                                             |
    | show-default                                            | N/A                                                     | Show default values for a given command                                                                                   |
    | help-short                                              | N/A                                                     | Show help screen (short format).                                                                                          |
    | [top-syntax](#check_docker_restarts_top-syntax)         | ${status}: ${list}                                      | Top level syntax.                                                                                                         |
    | [ok-syntax](#check_docker_restarts_ok-syntax)           |                                                         | ok syntax.                                                                                                                |
    | [empty-syntax](#check_docker_restarts_empty-syntax)     | No containers found                                     | Empty syntax.                                                                                                             |
    | [detail-syntax](#check_docker_restarts_detail-syntax)   | ${names}: ${restart_count} restarts, ${container_state} | Detail level syntax.                                                                                                      |
    | [perf-syntax](#check_docker_restarts_perf-syntax)       | ${names}                                                | Performance alias syntax.                                                                                                 |
    | [host](#check_docker_restarts_host)                     | /var/run/docker.sock                                    | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                            |
    | [timeout](#check_docker_restarts_timeout)               | 10                                                      | Timeout for talking to the daemon, in seconds.                                                                            |
    | container                                               |                                                         | Only inspect the named container (repeatable).                                                                            |



    <h5 id="check_docker_restarts_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_restarts_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.


    *Default Value:* `restart_count > 3 and started < 15m and started >= 0`

    <h5 id="check_docker_restarts_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `oom_killed = 1`

    <h5 id="check_docker_restarts_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_restarts_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_restarts_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_restarts_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `ok`

    <h5 id="check_docker_restarts_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_restarts_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_restarts_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_restarts_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_restarts_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_restarts_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No containers found`

    <h5 id="check_docker_restarts_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${names}: ${restart_count} restarts, ${container_state}`

    <h5 id="check_docker_restarts_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${names}`

    <h5 id="check_docker_restarts_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_restarts_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


<a id="check_docker_restarts_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                             |
|-----------------|---------------------------------------------------------------------------------------------------------|
| container_state | Container state: created, restarting, running, removing, paused, exited or dead                         |
| exit_code       | Exit code of the last exit (0 while running fine)                                                       |
| image           | Image the container was created from                                                                    |
| names           | Container name(s), comma separated                                                                      |
| oom_killed      | 1 when the last exit was an out-of-memory kill, else 0                                                  |
| restart_count   | How many times the container has been restarted (since creation)                                        |
| started         | Seconds since the container last started, -1 when it never started (supports units, e.g. started < 10m) |

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

### check_docker_stats

Check per-container resource usage: CPU percent and memory versus limit.

#### About `check_docker_stats`

`check_docker_stats` samples per-container resource usage the way
`docker stats` does: CPU as a percentage of the host (delta of the container's
CPU time over the host's, scaled by online CPUs) and memory usage with the
page cache excluded, against the container's memory limit.

Sampling uses the daemon's `stream=false` stats mode, which takes two readings
about a second apart so the CPU delta is meaningful — expect roughly **one
second per sampled container**. On hosts with many containers, scope the check
with `container=<name>` (repeatable) or accept the added latency.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword        | Description                                                        |
|----------------|--------------------------------------------------------------------|
| `names`        | Container name(s), comma separated                                 |
| `image`        | Image the container was created from                               |
| `cpu_pct`      | CPU usage in percent of the host (like `docker stats`)             |
| `memory_used`  | Memory used in bytes, page cache excluded; size units in thresholds (`memory_used > 200M`) |
| `memory_limit` | Memory limit in bytes (the host's total memory when unlimited)     |
| `memory_pct`   | Memory usage in percent of the limit                               |
| `memory`       | Human readable usage, e.g. `45.2MB of 256MB` (display only)        |

Size-typed thresholds need a unit (`1b`, `64k`, `200M`, `1G`); a bare number
is rejected. No default thresholds: an idle container is OK until you say
otherwise.

**Jump to section:**

* [Sample Commands](#check_docker_stats_samples)
* [Command-line Arguments](#check_docker_stats_options)
* [Filter keywords](#check_docker_stats_filter_keys)


<a id="check_docker_stats_samples"></a>
#### Sample Commands

**Check resource usage of a specific container:**

```
check_docker_stats container=app-backend
OK: app-backend: cpu 2%, memory 45.2MB of 256MB (17%)
```

**Alert on memory pressure or CPU saturation (thresholded keywords become perf data):**

```
check_docker_stats container=app-backend "warning=memory_pct > 80" "critical=memory_pct > 95"
OK: app-backend: cpu 2%, memory 45.2MB of 256MB (17%)|'app-backend memory %'=17%;80;95
```

**Absolute memory thresholds take byte units (`1b`, `64k`, `200M`, `1G`, ...):**

```
check_docker_stats container=app-backend "critical=memory_used > 200M"
OK: app-backend: cpu 2%, memory 45.2MB of 256MB (17%)|'app-backend memory'=47401984B;0;209715200
```

**Sample every running container (about a second per container, so scope on busy hosts):**

```
check_docker_stats "warning=cpu_pct > 80"
OK: web-frontend: cpu 1%, memory 12.4MB of 15.35GB (0%), app-backend: cpu 2%, memory 45.2MB of 256MB (17%)
```



<a id="check_docker_stats_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_docker_stats_warn"></a>
    <a id="check_docker_stats_crit"></a>
    <a id="check_docker_stats_help"></a>
    <a id="check_docker_stats_help-pb"></a>
    <a id="check_docker_stats_show-default"></a>
    <a id="check_docker_stats_help-short"></a>
    <a id="check_docker_stats_container"></a>

    | Option                                               | Default Value                                                | Description                                                                                                                   |
    |------------------------------------------------------|--------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_stats_filter)                 |                                                              | Filter which marks interesting items.                                                                                         |
    | [warning](#check_docker_stats_warning)               |                                                              | Filter which marks items which generates a warning state.                                                                     |
    | warn                                                 |                                                              | Short alias for warning                                                                                                       |
    | [critical](#check_docker_stats_critical)             |                                                              | Filter which marks items which generates a critical state.                                                                    |
    | crit                                                 |                                                              | Short alias for critical.                                                                                                     |
    | [ok](#check_docker_stats_ok)                         |                                                              | Filter which marks items which generates an ok state.                                                                         |
    | [debug](#check_docker_stats_debug)                   | 1)] (=0                                                      | Show debugging information in the log                                                                                         |
    | [show-all](#check_docker_stats_show-all)             | 1)] (=0                                                      | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).              |
    | [empty-state](#check_docker_stats_empty-state)       | ok                                                           | Return status to use when nothing matched filter.                                                                             |
    | [perf-config](#check_docker_stats_perf-config)       |                                                              | Performance data generation configuration                                                                                     |
    | [escape-html](#check_docker_stats_escape-html)       | 1)] (=0                                                      | Escape any < and > characters to prevent HTML encoding                                                                        |
    | [list-separator](#check_docker_stats_list-separator) | ,                                                            | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).     |
    | help                                                 | N/A                                                          | Show help screen (this screen)                                                                                                |
    | help-pb                                              | N/A                                                          | Show help screen as a protocol buffer payload                                                                                 |
    | show-default                                         | N/A                                                          | Show default values for a given command                                                                                       |
    | help-short                                           | N/A                                                          | Show help screen (short format).                                                                                              |
    | [top-syntax](#check_docker_stats_top-syntax)         | ${status}: ${list}                                           | Top level syntax.                                                                                                             |
    | [ok-syntax](#check_docker_stats_ok-syntax)           |                                                              | ok syntax.                                                                                                                    |
    | [empty-syntax](#check_docker_stats_empty-syntax)     | No running containers                                        | Empty syntax.                                                                                                                 |
    | [detail-syntax](#check_docker_stats_detail-syntax)   | ${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%) | Detail level syntax.                                                                                                          |
    | [perf-syntax](#check_docker_stats_perf-syntax)       | ${names}                                                     | Performance alias syntax.                                                                                                     |
    | [host](#check_docker_stats_host)                     | \\.\pipe\docker_engine                                       | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                |
    | [timeout](#check_docker_stats_timeout)               | 10                                                           | Timeout for talking to the daemon, in seconds.                                                                                |
    | container                                            |                                                              | Only sample the named container (repeatable). Sampling takes about a second per container, so scope this check on busy hosts. |



    <h5 id="check_docker_stats_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_stats_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_docker_stats_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_docker_stats_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_stats_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_stats_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_stats_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `ok`

    <h5 id="check_docker_stats_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_stats_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_stats_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_stats_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_stats_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_stats_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No running containers`

    <h5 id="check_docker_stats_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%)`

    <h5 id="check_docker_stats_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${names}`

    <h5 id="check_docker_stats_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_stats_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

=== "Linux"

    <a id="check_docker_stats_warn"></a>
    <a id="check_docker_stats_crit"></a>
    <a id="check_docker_stats_help"></a>
    <a id="check_docker_stats_help-pb"></a>
    <a id="check_docker_stats_show-default"></a>
    <a id="check_docker_stats_help-short"></a>
    <a id="check_docker_stats_container"></a>

    | Option                                               | Default Value                                                | Description                                                                                                                   |
    |------------------------------------------------------|--------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_docker_stats_filter)                 |                                                              | Filter which marks interesting items.                                                                                         |
    | [warning](#check_docker_stats_warning)               |                                                              | Filter which marks items which generates a warning state.                                                                     |
    | warn                                                 |                                                              | Short alias for warning                                                                                                       |
    | [critical](#check_docker_stats_critical)             |                                                              | Filter which marks items which generates a critical state.                                                                    |
    | crit                                                 |                                                              | Short alias for critical.                                                                                                     |
    | [ok](#check_docker_stats_ok)                         |                                                              | Filter which marks items which generates an ok state.                                                                         |
    | [debug](#check_docker_stats_debug)                   | 1)] (=0                                                      | Show debugging information in the log                                                                                         |
    | [show-all](#check_docker_stats_show-all)             | 1)] (=0                                                      | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).              |
    | [empty-state](#check_docker_stats_empty-state)       | ok                                                           | Return status to use when nothing matched filter.                                                                             |
    | [perf-config](#check_docker_stats_perf-config)       |                                                              | Performance data generation configuration                                                                                     |
    | [escape-html](#check_docker_stats_escape-html)       | 1)] (=0                                                      | Escape any < and > characters to prevent HTML encoding                                                                        |
    | [list-separator](#check_docker_stats_list-separator) | ,                                                            | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).     |
    | help                                                 | N/A                                                          | Show help screen (this screen)                                                                                                |
    | help-pb                                              | N/A                                                          | Show help screen as a protocol buffer payload                                                                                 |
    | show-default                                         | N/A                                                          | Show default values for a given command                                                                                       |
    | help-short                                           | N/A                                                          | Show help screen (short format).                                                                                              |
    | [top-syntax](#check_docker_stats_top-syntax)         | ${status}: ${list}                                           | Top level syntax.                                                                                                             |
    | [ok-syntax](#check_docker_stats_ok-syntax)           |                                                              | ok syntax.                                                                                                                    |
    | [empty-syntax](#check_docker_stats_empty-syntax)     | No running containers                                        | Empty syntax.                                                                                                                 |
    | [detail-syntax](#check_docker_stats_detail-syntax)   | ${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%) | Detail level syntax.                                                                                                          |
    | [perf-syntax](#check_docker_stats_perf-syntax)       | ${names}                                                     | Performance alias syntax.                                                                                                     |
    | [host](#check_docker_stats_host)                     | /var/run/docker.sock                                         | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                |
    | [timeout](#check_docker_stats_timeout)               | 10                                                           | Timeout for talking to the daemon, in seconds.                                                                                |
    | container                                            |                                                              | Only sample the named container (repeatable). Sampling takes about a second per container, so scope this check on busy hosts. |



    <h5 id="check_docker_stats_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


    <h5 id="check_docker_stats_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_docker_stats_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.



    <h5 id="check_docker_stats_ok">ok:</h5>

    Filter which marks items which generates an ok state.
    If anything matches this any previous state for this item will be reset to ok.


    <h5 id="check_docker_stats_debug">debug:</h5>

    Show debugging information in the log

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_stats_show-all">show-all:</h5>

    Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_stats_empty-state">empty-state:</h5>

    Return status to use when nothing matched filter.
    If no filter is specified this will never happen unless the file is empty.

    *Default Value:* `ok`

    <h5 id="check_docker_stats_perf-config">perf-config:</h5>

    Performance data generation configuration
    TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


    <h5 id="check_docker_stats_escape-html">escape-html:</h5>

    Escape any < and > characters to prevent HTML encoding

    *Default Value:* `1)] (=0`

    <h5 id="check_docker_stats_list-separator">list-separator:</h5>

    String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
    Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
    Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
    The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

    *Default Value:* `, `

    <h5 id="check_docker_stats_top-syntax">top-syntax:</h5>

    Top level syntax.
    Used to format the message to return can include text as well as special keywords which will include information from the checks.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${status}: ${list}`

    <h5 id="check_docker_stats_ok-syntax">ok-syntax:</h5>

    ok syntax.
    DEPRECATED! This is the syntax for when an ok result is returned.
    This value will not be used if your syntax contains %(list) or %(count).


    <h5 id="check_docker_stats_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.

    *Default Value:* `No running containers`

    <h5 id="check_docker_stats_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%)`

    <h5 id="check_docker_stats_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${names}`

    <h5 id="check_docker_stats_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_stats_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


<a id="check_docker_stats_filter_keys"></a>
#### Filter keywords

| Option       | Description                                                                     |
|--------------|---------------------------------------------------------------------------------|
| cpu_pct      | CPU usage in percent of the host (like docker stats)                            |
| image        | Image the container was created from                                            |
| memory       | Memory usage as human readable text, e.g. 45.2M of 512M                         |
| memory_limit | Memory limit in bytes (the host's total memory when the container is unlimited) |
| memory_pct   | Memory usage in percent of the container's limit                                |
| memory_used  | Memory used in bytes (page cache excluded)                                      |
| names        | Container name(s), comma separated                                              |

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

| Path / Section                        | Description |
|---------------------------------------|-------------|
| [/settings/docker](#/settings/docker) |             |


### /settings/docker <a id="/settings/docker"></a>



| Key                          | Default Value          | Description     |
|------------------------------|------------------------|-----------------|
| [endpoint](#docker-endpoint) | \\.\pipe\docker_engine | DOCKER ENDPOINT |
| [timeout](#timeout)          | 10                     | TIMEOUT         |


```ini
# 
[/settings/docker]
endpoint=\\.\pipe\docker_engine
timeout=10
```

=== "Windows"

    #### DOCKER ENDPOINT <a id="/settings/docker/endpoint"></a>

    The local docker daemon socket: a named pipe (\\\\.\\pipe\\docker_engine) on Windows, a unix socket (/var/run/docker.sock) elsewhere.


    | Key            | Description                           |
    |----------------|---------------------------------------|
    | Path:          | [/settings/docker](#/settings/docker) |
    | Key:           | endpoint                              |
    | Default value: | `\\.\pipe\docker_engine`              |


    **Sample:**

    ```
    [/settings/docker]
    # DOCKER ENDPOINT
    endpoint=\\.\pipe\docker_engine
    ```

=== "Linux"

    #### DOCKER ENDPOINT <a id="/settings/docker/endpoint"></a>

    The local docker daemon socket: a named pipe (\\\\.\\pipe\\docker_engine) on Windows, a unix socket (/var/run/docker.sock) elsewhere.


    | Key            | Description                           |
    |----------------|---------------------------------------|
    | Path:          | [/settings/docker](#/settings/docker) |
    | Key:           | endpoint                              |
    | Default value: | `/var/run/docker.sock`                |


    **Sample:**

    ```
    [/settings/docker]
    # DOCKER ENDPOINT
    endpoint=/var/run/docker.sock
    ```

#### TIMEOUT <a id="/settings/docker/timeout"></a>

Timeout for talking to the daemon, in seconds.


| Key            | Description                           |
|----------------|---------------------------------------|
| Path:          | [/settings/docker](#/settings/docker) |
| Key:           | timeout                               |
| Advanced:      | Yes (means it is not commonly used)   |
| Default value: | `10`                                  |


**Sample:**

```
[/settings/docker]
# TIMEOUT
timeout=10
```
