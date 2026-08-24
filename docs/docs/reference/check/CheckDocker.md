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
check_docker container=web-frontend "detail-syntax=%(names): %(image) %(container_status) ports=%(ports)" "top-syntax=${status}: ${list}"
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

    <a id="check_docker_container"></a>

    | Option                           | Default Value          | Description                                                                                                                    |
    |----------------------------------|------------------------|--------------------------------------------------------------------------------------------------------------------------------|
    | [host](#check_docker_host)       | \\.\pipe\docker_engine | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                 |
    | [timeout](#check_docker_timeout) | 10                     | Timeout for talking to the daemon, in seconds.                                                                                 |
    | [all](#check_docker_all)         | false                  | Include stopped containers (docker ps -a); by default only running containers are listed.                                      |
    | container                        |                        | Name of a container that must exist (repeatable). Implies all; a name the daemon does not know gets container_state 'missing'. |



    <h5 id="check_docker_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

    <h5 id="check_docker_all">all:</h5>

    Include stopped containers (docker ps -a); by default only running containers are listed.

    *Default Value:* `false`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                       | Default Value                |
    |--------------------------------------------------------------------------------------------------------------|------------------------------|
    | <a id="check_docker_filter"></a>[filter](../common-options.md#filter)                                        |                              |
    | <a id="check_docker_warning"></a>[warning](../common-options.md#warning)                                     |                              |
    | <a id="check_docker_warn"></a>[warn](../common-options.md#warn)                                              |                              |
    | <a id="check_docker_critical"></a>[critical](../common-options.md#critical)                                  | container_state != 'running' |
    | <a id="check_docker_crit"></a>[crit](../common-options.md#crit)                                              |                              |
    | <a id="check_docker_ok"></a>[ok](../common-options.md#ok)                                                    |                              |
    | <a id="check_docker_debug"></a>[debug](../common-options.md#debug)                                           | false                        |
    | <a id="check_docker_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                        |
    | <a id="check_docker_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | warning                      |
    | <a id="check_docker_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                              |
    | <a id="check_docker_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                        |
    | <a id="check_docker_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                            |
    | <a id="check_docker_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}           |
    | <a id="check_docker_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                              |
    | <a id="check_docker_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No containers found          |
    | <a id="check_docker_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${names}=${container_state}  |
    | <a id="check_docker_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${names}                     |
    | <a id="check_docker_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                              |
    | <a id="check_docker_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                              |
    | <a id="check_docker_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                           |
    | <a id="check_docker_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                              |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_docker_container"></a>

    | Option                           | Default Value        | Description                                                                                                                    |
    |----------------------------------|----------------------|--------------------------------------------------------------------------------------------------------------------------------|
    | [host](#check_docker_host)       | /var/run/docker.sock | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                 |
    | [timeout](#check_docker_timeout) | 10                   | Timeout for talking to the daemon, in seconds.                                                                                 |
    | [all](#check_docker_all)         | false                | Include stopped containers (docker ps -a); by default only running containers are listed.                                      |
    | container                        |                      | Name of a container that must exist (repeatable). Implies all; a name the daemon does not know gets container_state 'missing'. |



    <h5 id="check_docker_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`

    <h5 id="check_docker_all">all:</h5>

    Include stopped containers (docker ps -a); by default only running containers are listed.

    *Default Value:* `false`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                       | Default Value                |
    |--------------------------------------------------------------------------------------------------------------|------------------------------|
    | <a id="check_docker_filter"></a>[filter](../common-options.md#filter)                                        |                              |
    | <a id="check_docker_warning"></a>[warning](../common-options.md#warning)                                     |                              |
    | <a id="check_docker_warn"></a>[warn](../common-options.md#warn)                                              |                              |
    | <a id="check_docker_critical"></a>[critical](../common-options.md#critical)                                  | container_state != 'running' |
    | <a id="check_docker_crit"></a>[crit](../common-options.md#crit)                                              |                              |
    | <a id="check_docker_ok"></a>[ok](../common-options.md#ok)                                                    |                              |
    | <a id="check_docker_debug"></a>[debug](../common-options.md#debug)                                           | false                        |
    | <a id="check_docker_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                        |
    | <a id="check_docker_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | warning                      |
    | <a id="check_docker_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                              |
    | <a id="check_docker_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                        |
    | <a id="check_docker_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                            |
    | <a id="check_docker_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}           |
    | <a id="check_docker_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                              |
    | <a id="check_docker_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No containers found          |
    | <a id="check_docker_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${names}=${container_state}  |
    | <a id="check_docker_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${names}                     |
    | <a id="check_docker_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                              |
    | <a id="check_docker_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                              |
    | <a id="check_docker_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                           |
    | <a id="check_docker_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                              |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_docker_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                                                                     |
|------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| command          | Command the container runs                                                                                                                      |
| container_state  | Container state: created, restarting, running, removing, paused, exited, dead or missing (a requested container the daemon does not know about) |
| container_status | Human readable status, e.g. 'Up 3 hours (healthy)'                                                                                              |
| created          | When the container was created (date)                                                                                                           |
| has_health_check | 1 when the container defines a health check, else 0                                                                                             |
| health           | Health-check state: healthy, unhealthy, starting or empty when the container has no health check                                                |
| id               | Container id                                                                                                                                    |
| image            | Image the container was created from                                                                                                            |
| image_id         | Id of the image the container was created from                                                                                                  |
| ip               | First IP address on any network the container is attached to                                                                                    |
| labels           | Container labels as key=value, comma separated                                                                                                  |
| names            | Container name(s), comma separated                                                                                                              |
| ports            | Published/exposed ports, e.g. 0.0.0.0:8080->80/tcp                                                                                              |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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

    | Option                              | Default Value          | Description                                                                           |
    |-------------------------------------|------------------------|---------------------------------------------------------------------------------------|
    | [host](#check_docker_df_host)       | \\.\pipe\docker_engine | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).        |
    | [timeout](#check_docker_df_timeout) | 60                     | Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts). |



    <h5 id="check_docker_df_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_df_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).

    *Default Value:* `60`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                          | Default Value                                 |
    |-----------------------------------------------------------------------------------------------------------------|-----------------------------------------------|
    | <a id="check_docker_df_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
    | <a id="check_docker_df_warning"></a>[warning](../common-options.md#warning)                                     |                                               |
    | <a id="check_docker_df_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
    | <a id="check_docker_df_critical"></a>[critical](../common-options.md#critical)                                  |                                               |
    | <a id="check_docker_df_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
    | <a id="check_docker_df_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
    | <a id="check_docker_df_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
    | <a id="check_docker_df_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
    | <a id="check_docker_df_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                       |
    | <a id="check_docker_df_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
    | <a id="check_docker_df_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
    | <a id="check_docker_df_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
    | <a id="check_docker_df_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
    | <a id="check_docker_df_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                               |
    | <a id="check_docker_df_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No disk usage information returned |
    | <a id="check_docker_df_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${message}                                    |
    | <a id="check_docker_df_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | docker                                        |
    | <a id="check_docker_df_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
    | <a id="check_docker_df_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
    | <a id="check_docker_df_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
    | <a id="check_docker_df_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    | Option                              | Default Value        | Description                                                                           |
    |-------------------------------------|----------------------|---------------------------------------------------------------------------------------|
    | [host](#check_docker_df_host)       | /var/run/docker.sock | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).        |
    | [timeout](#check_docker_df_timeout) | 60                   | Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts). |



    <h5 id="check_docker_df_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_df_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds (this endpoint is slow on large hosts).

    *Default Value:* `60`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                          | Default Value                                 |
    |-----------------------------------------------------------------------------------------------------------------|-----------------------------------------------|
    | <a id="check_docker_df_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
    | <a id="check_docker_df_warning"></a>[warning](../common-options.md#warning)                                     |                                               |
    | <a id="check_docker_df_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
    | <a id="check_docker_df_critical"></a>[critical](../common-options.md#critical)                                  |                                               |
    | <a id="check_docker_df_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
    | <a id="check_docker_df_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
    | <a id="check_docker_df_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
    | <a id="check_docker_df_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
    | <a id="check_docker_df_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                       |
    | <a id="check_docker_df_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
    | <a id="check_docker_df_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
    | <a id="check_docker_df_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
    | <a id="check_docker_df_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
    | <a id="check_docker_df_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                               |
    | <a id="check_docker_df_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No disk usage information returned |
    | <a id="check_docker_df_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${message}                                    |
    | <a id="check_docker_df_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | docker                                        |
    | <a id="check_docker_df_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
    | <a id="check_docker_df_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
    | <a id="check_docker_df_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
    | <a id="check_docker_df_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_docker_df_filter_keys"></a>
#### Filter keywords

| Option                  | Description                                                                                                        |
|-------------------------|--------------------------------------------------------------------------------------------------------------------|
| build_cache_reclaimable | Disk freed by pruning the idle build cache (bytes)                                                                 |
| build_cache_size        | Disk used by the build cache (bytes)                                                                               |
| containers              | Number of containers (running and stopped)                                                                         |
| containers_reclaimable  | Disk freed by pruning stopped containers (bytes)                                                                   |
| containers_size         | Disk used by container writable layers (bytes)                                                                     |
| images                  | Number of images                                                                                                   |
| images_reclaimable      | Disk freed by pruning unused images (bytes)                                                                        |
| images_size             | Disk used by all image layers, deduplicated as in docker system df (bytes; supports units, e.g. images_size > 10G) |
| message                 | Human readable disk-usage summary                                                                                  |
| total_reclaimable       | Total disk a full prune would free (bytes)                                                                         |
| total_size              | Total disk used by docker (bytes)                                                                                  |
| unused_images           | Number of images not used by any container                                                                         |
| unused_volumes          | Number of volumes not referenced by any container                                                                  |
| volumes                 | Number of volumes                                                                                                  |
| volumes_reclaimable     | Disk freed by pruning unused volumes (bytes)                                                                       |
| volumes_size            | Disk used by volumes (bytes)                                                                                       |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_docker_info

Check that the docker daemon is healthy: version plus container and image counts.

#### About `check_docker_info`

`check_docker_info` checks the docker daemon itself: it reads `/info` from the
local daemon socket and reports the server version, host name and the
container/image counts. A responding daemon is **OK** unless you threshold the
counts; an unreachable daemon is **UNKNOWN** with the transport error — which
makes this the natural "is docker itself healthy" companion to per-container
`check_docker` checks.

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

    | Option                                | Default Value          | Description                                                                    |
    |---------------------------------------|------------------------|--------------------------------------------------------------------------------|
    | [host](#check_docker_info_host)       | \\.\pipe\docker_engine | The local docker daemon socket (named pipe on Windows, unix socket elsewhere). |
    | [timeout](#check_docker_info_timeout) | 10                     | Timeout for talking to the daemon, in seconds.                                 |



    <h5 id="check_docker_info_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_info_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                            | Default Value                                                                                                       |
    |-------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
    | <a id="check_docker_info_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                     |
    | <a id="check_docker_info_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                     |
    | <a id="check_docker_info_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                     |
    | <a id="check_docker_info_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                                     |
    | <a id="check_docker_info_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                     |
    | <a id="check_docker_info_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                     |
    | <a id="check_docker_info_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                               |
    | <a id="check_docker_info_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                               |
    | <a id="check_docker_info_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                             |
    | <a id="check_docker_info_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                     |
    | <a id="check_docker_info_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                               |
    | <a id="check_docker_info_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                   |
    | <a id="check_docker_info_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                  |
    | <a id="check_docker_info_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                     |
    | <a id="check_docker_info_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No daemon information returned                                                                           |
    | <a id="check_docker_info_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images |
    | <a id="check_docker_info_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                                                             |
    | <a id="check_docker_info_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                     |
    | <a id="check_docker_info_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                     |
    | <a id="check_docker_info_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                  |
    | <a id="check_docker_info_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                     |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    | Option                                | Default Value        | Description                                                                    |
    |---------------------------------------|----------------------|--------------------------------------------------------------------------------|
    | [host](#check_docker_info_host)       | /var/run/docker.sock | The local docker daemon socket (named pipe on Windows, unix socket elsewhere). |
    | [timeout](#check_docker_info_timeout) | 10                   | Timeout for talking to the daemon, in seconds.                                 |



    <h5 id="check_docker_info_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_info_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                            | Default Value                                                                                                       |
    |-------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
    | <a id="check_docker_info_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                     |
    | <a id="check_docker_info_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                     |
    | <a id="check_docker_info_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                     |
    | <a id="check_docker_info_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                                     |
    | <a id="check_docker_info_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                     |
    | <a id="check_docker_info_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                     |
    | <a id="check_docker_info_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                               |
    | <a id="check_docker_info_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                               |
    | <a id="check_docker_info_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                             |
    | <a id="check_docker_info_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                     |
    | <a id="check_docker_info_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                               |
    | <a id="check_docker_info_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                   |
    | <a id="check_docker_info_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                  |
    | <a id="check_docker_info_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                     |
    | <a id="check_docker_info_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No daemon information returned                                                                           |
    | <a id="check_docker_info_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | docker ${version} on ${name}: ${running} running, ${paused} paused, ${stopped} stopped containers, ${images} images |
    | <a id="check_docker_info_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                                                             |
    | <a id="check_docker_info_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                     |
    | <a id="check_docker_info_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                     |
    | <a id="check_docker_info_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                  |
    | <a id="check_docker_info_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                     |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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

    <a id="check_docker_restarts_container"></a>

    | Option                                    | Default Value          | Description                                                                    |
    |-------------------------------------------|------------------------|--------------------------------------------------------------------------------|
    | [host](#check_docker_restarts_host)       | \\.\pipe\docker_engine | The local docker daemon socket (named pipe on Windows, unix socket elsewhere). |
    | [timeout](#check_docker_restarts_timeout) | 10                     | Timeout for talking to the daemon, in seconds.                                 |
    | container                                 |                        | Only inspect the named container (repeatable).                                 |



    <h5 id="check_docker_restarts_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_restarts_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                                | Default Value                                           |
    |-----------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------|
    | <a id="check_docker_restarts_filter"></a>[filter](../common-options.md#filter)                                        |                                                         |
    | <a id="check_docker_restarts_warning"></a>[warning](../common-options.md#warning)                                     | restart_count > 3 and started < 15m and started >= 0    |
    | <a id="check_docker_restarts_warn"></a>[warn](../common-options.md#warn)                                              |                                                         |
    | <a id="check_docker_restarts_critical"></a>[critical](../common-options.md#critical)                                  | oom_killed = 1                                          |
    | <a id="check_docker_restarts_crit"></a>[crit](../common-options.md#crit)                                              |                                                         |
    | <a id="check_docker_restarts_ok"></a>[ok](../common-options.md#ok)                                                    |                                                         |
    | <a id="check_docker_restarts_debug"></a>[debug](../common-options.md#debug)                                           | false                                                   |
    | <a id="check_docker_restarts_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                   |
    | <a id="check_docker_restarts_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                      |
    | <a id="check_docker_restarts_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                         |
    | <a id="check_docker_restarts_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                   |
    | <a id="check_docker_restarts_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                       |
    | <a id="check_docker_restarts_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                      |
    | <a id="check_docker_restarts_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                         |
    | <a id="check_docker_restarts_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No containers found                                     |
    | <a id="check_docker_restarts_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${names}: ${restart_count} restarts, ${container_state} |
    | <a id="check_docker_restarts_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${names}                                                |
    | <a id="check_docker_restarts_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                         |
    | <a id="check_docker_restarts_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                         |
    | <a id="check_docker_restarts_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                      |
    | <a id="check_docker_restarts_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                         |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_docker_restarts_container"></a>

    | Option                                    | Default Value        | Description                                                                    |
    |-------------------------------------------|----------------------|--------------------------------------------------------------------------------|
    | [host](#check_docker_restarts_host)       | /var/run/docker.sock | The local docker daemon socket (named pipe on Windows, unix socket elsewhere). |
    | [timeout](#check_docker_restarts_timeout) | 10                   | Timeout for talking to the daemon, in seconds.                                 |
    | container                                 |                      | Only inspect the named container (repeatable).                                 |



    <h5 id="check_docker_restarts_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_restarts_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                                | Default Value                                           |
    |-----------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------|
    | <a id="check_docker_restarts_filter"></a>[filter](../common-options.md#filter)                                        |                                                         |
    | <a id="check_docker_restarts_warning"></a>[warning](../common-options.md#warning)                                     | restart_count > 3 and started < 15m and started >= 0    |
    | <a id="check_docker_restarts_warn"></a>[warn](../common-options.md#warn)                                              |                                                         |
    | <a id="check_docker_restarts_critical"></a>[critical](../common-options.md#critical)                                  | oom_killed = 1                                          |
    | <a id="check_docker_restarts_crit"></a>[crit](../common-options.md#crit)                                              |                                                         |
    | <a id="check_docker_restarts_ok"></a>[ok](../common-options.md#ok)                                                    |                                                         |
    | <a id="check_docker_restarts_debug"></a>[debug](../common-options.md#debug)                                           | false                                                   |
    | <a id="check_docker_restarts_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                   |
    | <a id="check_docker_restarts_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                      |
    | <a id="check_docker_restarts_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                         |
    | <a id="check_docker_restarts_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                   |
    | <a id="check_docker_restarts_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                       |
    | <a id="check_docker_restarts_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                      |
    | <a id="check_docker_restarts_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                         |
    | <a id="check_docker_restarts_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No containers found                                     |
    | <a id="check_docker_restarts_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${names}: ${restart_count} restarts, ${container_state} |
    | <a id="check_docker_restarts_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${names}                                                |
    | <a id="check_docker_restarts_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                         |
    | <a id="check_docker_restarts_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                         |
    | <a id="check_docker_restarts_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                      |
    | <a id="check_docker_restarts_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                         |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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

    <a id="check_docker_stats_container"></a>

    | Option                                 | Default Value          | Description                                                                                                                   |
    |----------------------------------------|------------------------|-------------------------------------------------------------------------------------------------------------------------------|
    | [host](#check_docker_stats_host)       | \\.\pipe\docker_engine | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                |
    | [timeout](#check_docker_stats_timeout) | 10                     | Timeout for talking to the daemon, in seconds.                                                                                |
    | container                              |                        | Only sample the named container (repeatable). Sampling takes about a second per container, so scope this check on busy hosts. |



    <h5 id="check_docker_stats_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `\\.\pipe\docker_engine`

    <h5 id="check_docker_stats_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                             | Default Value                                                |
    |--------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------|
    | <a id="check_docker_stats_filter"></a>[filter](../common-options.md#filter)                                        |                                                              |
    | <a id="check_docker_stats_warning"></a>[warning](../common-options.md#warning)                                     |                                                              |
    | <a id="check_docker_stats_warn"></a>[warn](../common-options.md#warn)                                              |                                                              |
    | <a id="check_docker_stats_critical"></a>[critical](../common-options.md#critical)                                  |                                                              |
    | <a id="check_docker_stats_crit"></a>[crit](../common-options.md#crit)                                              |                                                              |
    | <a id="check_docker_stats_ok"></a>[ok](../common-options.md#ok)                                                    |                                                              |
    | <a id="check_docker_stats_debug"></a>[debug](../common-options.md#debug)                                           | false                                                        |
    | <a id="check_docker_stats_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                        |
    | <a id="check_docker_stats_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                           |
    | <a id="check_docker_stats_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                              |
    | <a id="check_docker_stats_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                        |
    | <a id="check_docker_stats_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                            |
    | <a id="check_docker_stats_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                           |
    | <a id="check_docker_stats_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                              |
    | <a id="check_docker_stats_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No running containers                                        |
    | <a id="check_docker_stats_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%) |
    | <a id="check_docker_stats_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${names}                                                     |
    | <a id="check_docker_stats_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                              |
    | <a id="check_docker_stats_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                              |
    | <a id="check_docker_stats_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                           |
    | <a id="check_docker_stats_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                              |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_docker_stats_container"></a>

    | Option                                 | Default Value        | Description                                                                                                                   |
    |----------------------------------------|----------------------|-------------------------------------------------------------------------------------------------------------------------------|
    | [host](#check_docker_stats_host)       | /var/run/docker.sock | The local docker daemon socket (named pipe on Windows, unix socket elsewhere).                                                |
    | [timeout](#check_docker_stats_timeout) | 10                   | Timeout for talking to the daemon, in seconds.                                                                                |
    | container                              |                      | Only sample the named container (repeatable). Sampling takes about a second per container, so scope this check on busy hosts. |



    <h5 id="check_docker_stats_host">host:</h5>

    The local docker daemon socket (named pipe on Windows, unix socket elsewhere).

    *Default Value:* `/var/run/docker.sock`

    <h5 id="check_docker_stats_timeout">timeout:</h5>

    Timeout for talking to the daemon, in seconds.

    *Default Value:* `10`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                             | Default Value                                                |
    |--------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------|
    | <a id="check_docker_stats_filter"></a>[filter](../common-options.md#filter)                                        |                                                              |
    | <a id="check_docker_stats_warning"></a>[warning](../common-options.md#warning)                                     |                                                              |
    | <a id="check_docker_stats_warn"></a>[warn](../common-options.md#warn)                                              |                                                              |
    | <a id="check_docker_stats_critical"></a>[critical](../common-options.md#critical)                                  |                                                              |
    | <a id="check_docker_stats_crit"></a>[crit](../common-options.md#crit)                                              |                                                              |
    | <a id="check_docker_stats_ok"></a>[ok](../common-options.md#ok)                                                    |                                                              |
    | <a id="check_docker_stats_debug"></a>[debug](../common-options.md#debug)                                           | false                                                        |
    | <a id="check_docker_stats_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                        |
    | <a id="check_docker_stats_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                           |
    | <a id="check_docker_stats_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                              |
    | <a id="check_docker_stats_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                        |
    | <a id="check_docker_stats_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                            |
    | <a id="check_docker_stats_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                           |
    | <a id="check_docker_stats_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                              |
    | <a id="check_docker_stats_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No running containers                                        |
    | <a id="check_docker_stats_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${names}: cpu ${cpu_pct}%, memory ${memory} (${memory_pct}%) |
    | <a id="check_docker_stats_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${names}                                                     |
    | <a id="check_docker_stats_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                              |
    | <a id="check_docker_stats_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                              |
    | <a id="check_docker_stats_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                           |
    | <a id="check_docker_stats_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                              |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_docker_stats_filter_keys"></a>
#### Filter keywords

| Option       | Description                                                                                                    |
|--------------|----------------------------------------------------------------------------------------------------------------|
| cpu_pct      | CPU usage in percent of the host (like docker stats)                                                           |
| image        | Image the container was created from                                                                           |
| memory       | Memory usage as human readable text, e.g. 45.2M of 512M (display only; threshold on memory_used or memory_pct) |
| memory_limit | Memory limit in bytes (the host's total memory when the container is unlimited)                                |
| memory_pct   | Memory usage in percent of the container's limit                                                               |
| memory_used  | Memory used in bytes, page cache excluded (supports size units, e.g. memory_used > 200M)                       |
| names        | Container name(s), comma separated                                                                             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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
