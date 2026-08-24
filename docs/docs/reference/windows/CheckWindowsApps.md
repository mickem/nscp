# CheckWindowsApps

*Available on Windows only.*

Checks for applications and server roles on Windows: IIS (web sites, application pools, worker processes, HTTP.sys request queues) and Remote Desktop Services (CAL licensing, session host load, Connection Broker).

## Enable module

To enable this module and and allow using the commands you need to ass `CheckWindowsApps = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckWindowsApps = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckWindowsApps module.

**List of commands:**

A list of all available queries (check commands)

| Command                                                   | Description                                                                         |
|-----------------------------------------------------------|-------------------------------------------------------------------------------------|
| [check_iis_app_pools](#check_iis_app_pools)               | Check IIS application pools (state, uptime, recycles).                              |
| [check_iis_request_queues](#check_iis_request_queues)     | Check HTTP.sys request queues (length, rejections, age).                            |
| [check_iis_sites](#check_iis_sites)                       | Check IIS web sites (state, connections, traffic).                                  |
| [check_iis_worker_processes](#check_iis_worker_processes) | Check IIS worker processes (active and served requests per w3wp).                   |
| [check_rds_broker](#check_rds_broker)                     | Check the Remote Desktop Connection Broker counterset (failed/pending connections). |
| [check_rds_licenses](#check_rds_licenses)                 | Check Remote Desktop licensing (CAL key packs: issued versus available licenses).   |
| [check_rds_session_load](#check_rds_session_load)         | Check per-session resource usage (CPU, working set, protocol bytes).                |
| [check_rds_sessions](#check_rds_sessions)                 | Check session counts on a session host (active, inactive, total).                   |

### check_iis_app_pools

Check IIS application pools (state, uptime, recycles).

#### About `check_iis_app_pools`

`check_iis_app_pools` reports one record per IIS application pool from the
`APP_POOL_WAS` performance counters (state, uptime, recycles). When the IIS
WMI provider ("IIS Management Scripts and Tools") is installed, the records
are enriched from `root\WebAdministration`: each pool gains its `auto_start`
configuration flag, and pools that WAS has no counter instance for yet (never
started since boot) are added with state `unknown` so they cannot hide.

The default **critical** expression is
`state != 'running' and auto_start != 0`: an auto-start pool that is not
running alerts, a pool an administrator stopped on purpose (`auto_start` = 0)
stays quiet. Without the WMI provider `auto_start` is `-1` for every pool, so
every non-running pool alerts.

`recycles` counts since WAS started, so a recycle *storm* shows as a high and
climbing value combined with a low `uptime`; `warning=recycles > 10 and
uptime < 600` is a useful storm signature.

**Jump to section:**

* [Sample Commands](#check_iis_app_pools_samples)
* [Command-line Arguments](#check_iis_app_pools_options)
* [Filter keywords](#check_iis_app_pools_filter_keys)


<a id="check_iis_app_pools_samples"></a>
#### Sample Commands

**Check all IIS application pools (critical when an auto-start pool is not running):**

```
check_iis_app_pools
OK: DefaultAppPool (running)|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0
```

**A stopped pool that is configured to auto-start goes critical:**

```
check_iis_app_pools
CRITICAL: MyAppPool (disabled)|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0 'MyAppPool_uptime'=0s;0;0 'MyAppPool_recycles'=5c;0;0
```

**Detect recycle storms (a pool that keeps recycling):**

```
check_iis_app_pools "warning=recycles > 10" "critical=recycles > 50"
WARNING: MyAppPool (running)|'MyAppPool_recycles'=17c;10;50 'MyAppPool_uptime'=42s;0;0 ...
```

**Scope to one pool and show everything:**

```
check_iis_app_pools "filter=pool = 'DefaultAppPool'" show-all
OK: DefaultAppPool: running, uptime 86400s, 2 recycles|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_app_pools
IIS performance counters (APP_POOL_WAS) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_app_pools_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                                              |
|---------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------|
| <a id="check_iis_app_pools_filter"></a>[filter](../common-options.md#filter)                                        |                                                            |
| <a id="check_iis_app_pools_warning"></a>[warning](../common-options.md#warning)                                     |                                                            |
| <a id="check_iis_app_pools_warn"></a>[warn](../common-options.md#warn)                                              |                                                            |
| <a id="check_iis_app_pools_critical"></a>[critical](../common-options.md#critical)                                  | state != 'running' and auto_start != 0                     |
| <a id="check_iis_app_pools_crit"></a>[crit](../common-options.md#crit)                                              |                                                            |
| <a id="check_iis_app_pools_ok"></a>[ok](../common-options.md#ok)                                                    |                                                            |
| <a id="check_iis_app_pools_debug"></a>[debug](../common-options.md#debug)                                           | false                                                      |
| <a id="check_iis_app_pools_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                      |
| <a id="check_iis_app_pools_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                    |
| <a id="check_iis_app_pools_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                            |
| <a id="check_iis_app_pools_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                      |
| <a id="check_iis_app_pools_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                          |
| <a id="check_iis_app_pools_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                         |
| <a id="check_iis_app_pools_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                            |
| <a id="check_iis_app_pools_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No application pools found                                 |
| <a id="check_iis_app_pools_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${pool}: ${state}, uptime ${uptime}s, ${recycles} recycles |
| <a id="check_iis_app_pools_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${pool}                                                    |
| <a id="check_iis_app_pools_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                            |
| <a id="check_iis_app_pools_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                            |
| <a id="check_iis_app_pools_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                         |
| <a id="check_iis_app_pools_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_app_pools_filter_keys"></a>
#### Filter keywords

| Option     | Description                                                                                                       |
|------------|-------------------------------------------------------------------------------------------------------------------|
| auto_start | 1 when the pool is set to start automatically, 0 when not, -1 when the IIS WMI provider is unavailable            |
| pool       | Name of the application pool                                                                                      |
| recycles   | Pool recycles since WAS started                                                                                   |
| state      | Pool state: running, disabled, disabling, shutdown_pending, delete_pending, initialized, uninitialized or unknown |
| state_id   | Raw APP_POOL_WAS state value (3 = running)                                                                        |
| uptime     | Seconds since the pool last started                                                                               |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_request_queues

Check HTTP.sys request queues (length, rejections, age).

#### About `check_iis_request_queues`

`check_iis_request_queues` reports one record per HTTP.sys request queue from
the "HTTP Service Request Queues" performance counters. Queues are usually
named after the application pool they feed; requests wait here when no worker
is available, and once a queue hits its limit HTTP.sys rejects new requests
with 503 without the application ever seeing them.

The defaults track HTTP.sys' default per-queue limit of 1000: **warning** at
`queue_length > 800`, **critical** at `queue_length > 1000` (adjust when
`queueLength` is raised in the pool configuration).

`rejected` is cumulative, so alert on it going non-zero (`warning=rejected >
0`) after a deploy, or graph its rate; a growing `max_age` with a modest
`queue_length` points at a hung worker rather than overload.

**Jump to section:**

* [Sample Commands](#check_iis_request_queues_samples)
* [Command-line Arguments](#check_iis_request_queues_options)
* [Filter keywords](#check_iis_request_queues_filter_keys)


<a id="check_iis_request_queues_samples"></a>
#### Sample Commands

**Check the HTTP.sys request queues (defaults: warn above 800 queued, critical above 1000):**

```
check_iis_request_queues
OK: DefaultAppPool (0 queued)|'DefaultAppPool_rejected'=0c;0;0
```

**A backed-up queue trips the defaults (1000 is HTTP.sys' default queue limit):**

```
check_iis_request_queues
WARNING: DefaultAppPool (912 queued)|'DefaultAppPool_queue_length'=912;800;1000 'DefaultAppPool_rejected'=0c;0;0
```

**Alert on rejected requests instead (503s served straight from HTTP.sys):**

```
check_iis_request_queues "warning=rejected > 0" "critical=queue_length > 1000"
WARNING: DefaultAppPool (14 queued)|'DefaultAppPool_rejected'=27c;0;0 'DefaultAppPool_queue_length'=14;0;1000
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_request_queues
IIS performance counters (HTTP Service Request Queues) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_request_queues_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                   | Default Value                                          |
|--------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| <a id="check_iis_request_queues_filter"></a>[filter](../common-options.md#filter)                                        |                                                        |
| <a id="check_iis_request_queues_warning"></a>[warning](../common-options.md#warning)                                     | queue_length > 800                                     |
| <a id="check_iis_request_queues_warn"></a>[warn](../common-options.md#warn)                                              |                                                        |
| <a id="check_iis_request_queues_critical"></a>[critical](../common-options.md#critical)                                  | queue_length > 1000                                    |
| <a id="check_iis_request_queues_crit"></a>[crit](../common-options.md#crit)                                              |                                                        |
| <a id="check_iis_request_queues_ok"></a>[ok](../common-options.md#ok)                                                    |                                                        |
| <a id="check_iis_request_queues_debug"></a>[debug](../common-options.md#debug)                                           | false                                                  |
| <a id="check_iis_request_queues_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                  |
| <a id="check_iis_request_queues_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                     |
| <a id="check_iis_request_queues_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                        |
| <a id="check_iis_request_queues_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                  |
| <a id="check_iis_request_queues_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                      |
| <a id="check_iis_request_queues_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                     |
| <a id="check_iis_request_queues_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                        |
| <a id="check_iis_request_queues_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No HTTP.sys request queues found                       |
| <a id="check_iis_request_queues_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${queue}: ${queue_length} queued, ${rejected} rejected |
| <a id="check_iis_request_queues_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${queue}                                               |
| <a id="check_iis_request_queues_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                        |
| <a id="check_iis_request_queues_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                        |
| <a id="check_iis_request_queues_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                     |
| <a id="check_iis_request_queues_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_request_queues_filter_keys"></a>
#### Filter keywords

| Option       | Description                                                       |
|--------------|-------------------------------------------------------------------|
| max_age      | Age of the oldest request in the queue                            |
| queue        | Name of the HTTP.sys request queue (usually the application pool) |
| queue_length | Requests currently waiting in the queue                           |
| rejected     | Requests rejected from the queue since it was created             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_sites

Check IIS web sites (state, connections, traffic).

#### About `check_iis_sites`

`check_iis_sites` reports one record per IIS web site from the "Web Service"
performance counters (current connections, uptime, request/byte rates). When
the IIS WMI provider is installed, records are enriched from
`root\WebAdministration`: sites gain their `ServerAutoStart` flag
(`auto_start`) and sites with no counter instance (stopped) are still listed.

A site is considered `stopped` when it has no Web Service counter instance
(the counters only exist for started sites, so stopped sites are surfaced via
the WMI enrichment). The default **critical** expression is
`state = 'stopped' and auto_start != 0` — an auto-start site that is not
serving alerts, an intentionally stopped one (`auto_start` = 0) stays quiet.
Without the WMI provider `auto_start` is `-1`, so every stopped site alerts.

The rate keywords need two counter samples: pass `averages=true` to collect a
second sample after one second (the check then takes a second longer);
without it `requests_per_sec`/`bytes_per_sec` read 0.

**Jump to section:**

* [Sample Commands](#check_iis_sites_samples)
* [Command-line Arguments](#check_iis_sites_options)
* [Filter keywords](#check_iis_sites_filter_keys)


<a id="check_iis_sites_samples"></a>
#### Sample Commands

**Check all IIS web sites (critical when an auto-start site is stopped):**

```
check_iis_sites
OK: Default Web Site (running)|'Default Web Site_connections'=12;0;0
```

**A stopped site that is configured to auto-start goes critical:**

```
check_iis_sites
CRITICAL: MySite (stopped)|'Default Web Site_connections'=12;0;0 'MySite_connections'=0;0;0
```

**Alert on connection count per site:**

```
check_iis_sites "warning=connections > 500" "critical=connections > 1000"
OK: Default Web Site (running)|'Default Web Site_connections'=12;500;1000
```

**Measure request/byte rates (takes one extra second for the second sample):**

```
check_iis_sites averages=true "warning=requests_per_sec > 200" "detail-syntax=${site}: ${requests_per_sec} req/s, ${bytes_per_sec} B/s" show-all
OK: Default Web Site: 3.5 req/s, 1024.5 B/s|'Default Web Site_requests_per_sec'=3.5;200;0 'Default Web Site_connections'=12;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_sites
IIS performance counters (Web Service) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_sites_options"></a>
#### Command-line Arguments

| Option                                | Default Value | Description                                                                                                        |
|---------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------|
| [averages](#check_iis_sites_averages) | false         | Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values. |



<h5 id="check_iis_sites_averages">averages:</h5>

Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                          | Default Value                                                    |
|-----------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| <a id="check_iis_sites_filter"></a>[filter](../common-options.md#filter)                                        |                                                                  |
| <a id="check_iis_sites_warning"></a>[warning](../common-options.md#warning)                                     |                                                                  |
| <a id="check_iis_sites_warn"></a>[warn](../common-options.md#warn)                                              |                                                                  |
| <a id="check_iis_sites_critical"></a>[critical](../common-options.md#critical)                                  | state = 'stopped' and auto_start != 0                            |
| <a id="check_iis_sites_crit"></a>[crit](../common-options.md#crit)                                              |                                                                  |
| <a id="check_iis_sites_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                  |
| <a id="check_iis_sites_debug"></a>[debug](../common-options.md#debug)                                           | false                                                            |
| <a id="check_iis_sites_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                            |
| <a id="check_iis_sites_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                          |
| <a id="check_iis_sites_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                  |
| <a id="check_iis_sites_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                            |
| <a id="check_iis_sites_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                |
| <a id="check_iis_sites_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                               |
| <a id="check_iis_sites_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                  |
| <a id="check_iis_sites_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No web sites found                                               |
| <a id="check_iis_sites_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${site}: ${state}, ${connections} connections, uptime ${uptime}s |
| <a id="check_iis_sites_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${site}                                                          |
| <a id="check_iis_sites_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                  |
| <a id="check_iis_sites_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                  |
| <a id="check_iis_sites_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                               |
| <a id="check_iis_sites_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                  |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_sites_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                            |
|------------------|--------------------------------------------------------------------------------------------------------|
| auto_start       | 1 when the site is set to start automatically, 0 when not, -1 when the IIS WMI provider is unavailable |
| bytes_per_sec    | Bytes sent+received per second (needs averages=true, otherwise 0)                                      |
| connections      | Current connections to the site                                                                        |
| requests_per_sec | Requests per second (needs averages=true, otherwise 0)                                                 |
| site             | Name of the web site                                                                                   |
| state            | running or stopped (stopped = the site has no Web Service counter instance)                            |
| uptime           | Seconds the site has been up (0 when stopped)                                                          |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_worker_processes

Check IIS worker processes (active and served requests per w3wp).

#### About `check_iis_worker_processes`

`check_iis_worker_processes` reports one record per running IIS worker process
(w3wp.exe) from the `W3SVC_W3WP` performance counters. Counter instances are
named `<pid>_<pool>`; the check splits that into the `pid` and `pool`
keywords.

An *empty* result is OK by default: idle application pools spin their workers
down, so "no workers" is a normal state, not a failure (`empty-state=critical`
turns it into an alert for pools that must always be warm). The check goes
UNKNOWN with a clear message when the IIS role (and with it the counter set)
is missing.

There are no default thresholds — a sensible starting point is
`warning=active_requests > 50` scaled to your pools' concurrency.

**Jump to section:**

* [Sample Commands](#check_iis_worker_processes_samples)
* [Command-line Arguments](#check_iis_worker_processes_options)
* [Filter keywords](#check_iis_worker_processes_filter_keys)


<a id="check_iis_worker_processes_samples"></a>
#### Sample Commands

**Check the running IIS worker processes (w3wp):**

```
check_iis_worker_processes
OK: DefaultAppPool (pid 4711, 2 active)|'DefaultAppPool_4711_active_requests'=2;0;0
```

**No workers is a normal state (idle pools spin down), so the empty set is OK:**

```
check_iis_worker_processes
OK: No IIS worker processes running
```

**Alert when requests pile up inside a worker:**

```
check_iis_worker_processes "warning=active_requests > 50" "critical=active_requests > 200"
WARNING: DefaultAppPool (pid 4711, 73 active)|'DefaultAppPool_4711_active_requests'=73;50;200
```

**Scope to one pool's workers:**

```
check_iis_worker_processes "filter=pool = 'DefaultAppPool'" show-all
OK: DefaultAppPool (pid 4711): 2 active requests|'DefaultAppPool_4711_active_requests'=2;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_worker_processes
IIS performance counters (W3SVC_W3WP) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_worker_processes_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                     | Default Value                                            |
|----------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------|
| <a id="check_iis_worker_processes_filter"></a>[filter](../common-options.md#filter)                                        |                                                          |
| <a id="check_iis_worker_processes_warning"></a>[warning](../common-options.md#warning)                                     |                                                          |
| <a id="check_iis_worker_processes_warn"></a>[warn](../common-options.md#warn)                                              |                                                          |
| <a id="check_iis_worker_processes_critical"></a>[critical](../common-options.md#critical)                                  |                                                          |
| <a id="check_iis_worker_processes_crit"></a>[crit](../common-options.md#crit)                                              |                                                          |
| <a id="check_iis_worker_processes_ok"></a>[ok](../common-options.md#ok)                                                    |                                                          |
| <a id="check_iis_worker_processes_debug"></a>[debug](../common-options.md#debug)                                           | false                                                    |
| <a id="check_iis_worker_processes_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                    |
| <a id="check_iis_worker_processes_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                       |
| <a id="check_iis_worker_processes_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                          |
| <a id="check_iis_worker_processes_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                    |
| <a id="check_iis_worker_processes_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                        |
| <a id="check_iis_worker_processes_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                       |
| <a id="check_iis_worker_processes_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                          |
| <a id="check_iis_worker_processes_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No IIS worker processes running                          |
| <a id="check_iis_worker_processes_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${pool} (pid ${pid}): ${active_requests} active requests |
| <a id="check_iis_worker_processes_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${pool}_${pid}                                           |
| <a id="check_iis_worker_processes_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                          |
| <a id="check_iis_worker_processes_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                          |
| <a id="check_iis_worker_processes_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                       |
| <a id="check_iis_worker_processes_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_worker_processes_filter_keys"></a>
#### Filter keywords

| Option          | Description                                   |
|-----------------|-----------------------------------------------|
| active_requests | Requests currently executing in the worker    |
| instance        | Raw counter instance name (<pid>_<pool>)      |
| pid             | Process id of the w3wp worker                 |
| pool            | Application pool the worker serves            |
| total_requests  | HTTP requests served since the worker started |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_broker

Check the Remote Desktop Connection Broker counterset (failed/pending connections).

#### About `check_rds_broker`

`check_rds_broker` reads the "Remote Desktop Connection Broker Counterset"
performance object on an RD Connection Broker and reports one record per
counter (per instance where the counterset has instances). The broker's
counter names vary between Windows Server versions, so the check
**enumerates** whatever this broker exposes instead of hard-coding names —
run it once with `show-all` to see your version's counters, then select with
the `counter` keyword.

Run it on the Connection Broker itself; on any other host the counterset does
not exist and the check returns UNKNOWN with a clear "is this host an RD
Connection Broker?" message.

Pass `averages=true` to collect a second sample after one second so rate
counters carry real values. Typical alerts target the failed/pending request
counters, e.g. `critical=counter like 'Failed' and value > 0`.

**Jump to section:**

* [Sample Commands](#check_rds_broker_samples)
* [Command-line Arguments](#check_rds_broker_options)
* [Filter keywords](#check_rds_broker_filter_keys)


<a id="check_rds_broker_samples"></a>
#### Sample Commands

**List the Connection Broker counters (one record per counter, whatever this broker exposes):**

```
check_rds_broker show-all
OK: RDCB Connection Requests Failed = 0, RDCB Connection Requests Pending = 1, RDCB Connection Requests Total = 4211|'RDCB Connection Requests Failed'=0;0;0 'RDCB Connection Requests Pending'=1;0;0 'RDCB Connection Requests Total'=4211;0;0
```

**Alert on failed or piling-up connection requests:**

```
check_rds_broker "warning=counter like 'Pending' and value > 20" "critical=counter like 'Failed' and value > 0"
CRITICAL: RDCB Connection Requests Failed = 17|'RDCB Connection Requests Failed'=17;0;0 ...
```

**Sample rate counters over a second:**

```
check_rds_broker averages=true "filter=counter like 'Requests'"
OK: RDCB Connection Requests Total = 4211|'RDCB Connection Requests Total'=4211;0;0 ...
```

**On a host that is not a Connection Broker the check reports UNKNOWN with a clear message:**

```
check_rds_broker
Connection Broker counters (Remote Desktop Connection Broker Counterset) not available - is this host an RD Connection Broker? (Failed to enumerate object: Remote Desktop Connection Broker Counterset)
```



<a id="check_rds_broker_options"></a>
#### Command-line Arguments

| Option                                 | Default Value | Description                                                                  |
|----------------------------------------|---------------|------------------------------------------------------------------------------|
| [averages](#check_rds_broker_averages) | false         | Collect a second sample after one second so rate counters carry real values. |



<h5 id="check_rds_broker_averages">averages:</h5>

Collect a second sample after one second so rate counters carry real values.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                       |
|------------------------------------------------------------------------------------------------------------------|-------------------------------------|
| <a id="check_rds_broker_filter"></a>[filter](../common-options.md#filter)                                        |                                     |
| <a id="check_rds_broker_warning"></a>[warning](../common-options.md#warning)                                     |                                     |
| <a id="check_rds_broker_warn"></a>[warn](../common-options.md#warn)                                              |                                     |
| <a id="check_rds_broker_critical"></a>[critical](../common-options.md#critical)                                  |                                     |
| <a id="check_rds_broker_crit"></a>[crit](../common-options.md#crit)                                              |                                     |
| <a id="check_rds_broker_ok"></a>[ok](../common-options.md#ok)                                                    |                                     |
| <a id="check_rds_broker_debug"></a>[debug](../common-options.md#debug)                                           | false                               |
| <a id="check_rds_broker_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                               |
| <a id="check_rds_broker_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                             |
| <a id="check_rds_broker_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                     |
| <a id="check_rds_broker_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                               |
| <a id="check_rds_broker_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                   |
| <a id="check_rds_broker_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                  |
| <a id="check_rds_broker_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                     |
| <a id="check_rds_broker_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No Connection Broker counters found |
| <a id="check_rds_broker_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${label} = ${value}                 |
| <a id="check_rds_broker_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${label}                            |
| <a id="check_rds_broker_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                     |
| <a id="check_rds_broker_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                     |
| <a id="check_rds_broker_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                  |
| <a id="check_rds_broker_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_broker_filter_keys"></a>
#### Filter keywords

| Option   | Description                                                                                            |
|----------|--------------------------------------------------------------------------------------------------------|
| counter  | Name of the broker counter                                                                             |
| instance | Counter instance (empty for the single-instance counters)                                              |
| label    | Counter name suffixed with the instance name when the counterset is multi-instance (unique per record) |
| value    | Value of the counter                                                                                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_licenses

Check Remote Desktop licensing (CAL key packs: issued versus available licenses).

#### About `check_rds_licenses`

`check_rds_licenses` reads the Remote Desktop licensing key packs from WMI
(`Win32_TSLicenseKeyPack` in `root\cimv2`) and reports one record per key pack
with the issued versus available CAL counts. Run it on the RD **licensing**
server (the host holding the "Remote Desktop Licensing" role) — on any other
host the class does not exist and the check returns UNKNOWN with a clear
"role is not installed" message.

CAL exhaustion locks new users out of an RDS farm, so the default thresholds
alert on the available count: **warning** when `available < 10 and total_licenses > 0`,
**critical** when `available = 0 and total_licenses > 0`. The `total_licenses > 0`
guard keeps
the built-in/unlimited key packs (which report no meaningful counts) from
tripping the thresholds.

Per-user CALs are not enforced by the session host, so `issued` growing past
`total_licenses` is possible in per-user mode; alert on `available` (as the defaults
do) or on `issued` explicitly if you track compliance.

**Jump to section:**

* [Sample Commands](#check_rds_licenses_samples)
* [Command-line Arguments](#check_rds_licenses_options)
* [Filter keywords](#check_rds_licenses_filter_keys)


<a id="check_rds_licenses_samples"></a>
#### Sample Commands

**Check CAL key packs on an RD licensing server (defaults: warn below 10 available, critical when exhausted):**

```
check_rds_licenses
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=25;0;0 'RDS Per User CAL_available'=25;10;0
```

**A key pack running out of licenses trips the default thresholds:**

```
check_rds_licenses
CRITICAL: RDS Per User CAL: 50/50 issued, 0 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=50;0;0 'RDS Per User CAL_available'=0;10;0
```

**Custom thresholds, e.g. warn when more than 80% of a pack is issued:**

```
check_rds_licenses "warning=issued > 40 and total_licenses > 0" "critical=available = 0 and total_licenses > 0"
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_issued'=25;40;0 'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_available'=25;0;0
```

**Scope the check to a product version or exclude the built-in pack:**

```
check_rds_licenses "filter=product_version like '2022' and type != 'built-in'"
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=25;0;0 'RDS Per User CAL_available'=25;10;0
```

**On a host without the RD Licensing role the check reports UNKNOWN with a clear message:**

```
check_rds_licenses
Remote Desktop licensing information not available: the Remote Desktop licensing role is not installed (Win32_TSLicenseKeyPack missing)
```



<a id="check_rds_licenses_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                              |
|--------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|
| <a id="check_rds_licenses_filter"></a>[filter](../common-options.md#filter)                                        |                                                                            |
| <a id="check_rds_licenses_warning"></a>[warning](../common-options.md#warning)                                     | available < 10 and total_licenses > 0                                      |
| <a id="check_rds_licenses_warn"></a>[warn](../common-options.md#warn)                                              |                                                                            |
| <a id="check_rds_licenses_critical"></a>[critical](../common-options.md#critical)                                  | available = 0 and total_licenses > 0                                       |
| <a id="check_rds_licenses_crit"></a>[crit](../common-options.md#crit)                                              |                                                                            |
| <a id="check_rds_licenses_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                            |
| <a id="check_rds_licenses_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                      |
| <a id="check_rds_licenses_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                      |
| <a id="check_rds_licenses_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                    |
| <a id="check_rds_licenses_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                            |
| <a id="check_rds_licenses_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                      |
| <a id="check_rds_licenses_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                          |
| <a id="check_rds_licenses_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                         |
| <a id="check_rds_licenses_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                            |
| <a id="check_rds_licenses_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No license key packs found                                                 |
| <a id="check_rds_licenses_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${description}: ${issued}/${total_licenses} issued, ${available} available |
| <a id="check_rds_licenses_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${description}                                                             |
| <a id="check_rds_licenses_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                            |
| <a id="check_rds_licenses_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                            |
| <a id="check_rds_licenses_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                         |
| <a id="check_rds_licenses_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_licenses_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                     |
|-----------------|---------------------------------------------------------------------------------|
| available       | Licenses still available                                                        |
| description     | License type and model (e.g. 'RDS Per User CAL')                                |
| id              | Key pack id                                                                     |
| issued          | Licenses issued to clients                                                      |
| keypack_type    | Raw KeyPackType value                                                           |
| product_version | Product version the key pack applies to                                         |
| total_licenses  | Total licenses in the key pack                                                  |
| type            | Key pack type: unknown, retail, volume, concurrent, temporary, open or built-in |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_session_load

Check per-session resource usage (CPU, working set, protocol bytes).

#### About `check_rds_session_load`

`check_rds_session_load` reports one record per "Terminal Services Session"
counter instance: the console, the session-0 `Services` aggregate, and one
`RDP-Tcp <n>` instance per remote session. It answers "which session is
eating the host?" — the per-session CPU and working set that plain
`check_process` cannot attribute to a user.

Pass `sessions-only=true` to skip the `Services` aggregate (system processes,
not a user session), and `averages=true` to sample CPU over a full second
(the check then takes about a second longer; without it the first PDH sample
is used, which can be noisy).

Counter instances are named after the connection, not the user; correlate the
`RDP-Tcp <n>` number with `quser`/`qwinsta` output to find who it is. The
protocol counters (`total_bytes`) only exist on hosts with the RD Session Host
role; on other machines the keyword reads 0.

**Jump to section:**

* [Sample Commands](#check_rds_session_load_samples)
* [Command-line Arguments](#check_rds_session_load_options)
* [Filter keywords](#check_rds_session_load_filter_keys)


<a id="check_rds_session_load_samples"></a>
#### Sample Commands

**Show per-session resource usage (one record per counter instance):**

```
check_rds_session_load
OK: Console: 6.064% cpu, 18833346560B working set, Services: 1.68039% cpu, 6785593344B working set|'Console_working_set'=18833346560B;0;0 'Services_working_set'=6785593344B;0;0
```

**Only real sessions (skip the session-0 'Services' aggregate) and sample CPU over a second:**

```
check_rds_session_load sessions-only=true averages=true
OK: Console: 7.88642% cpu, 18829905920B working set|'Console_working_set'=18829905920B;0;0
```

**Find the runaway session eating the host:**

```
check_rds_session_load sessions-only=true averages=true "warning=cpu > 50" "critical=cpu > 80"
WARNING: RDP-Tcp 55: 63.2% cpu, 4831838208B working set|'RDP-Tcp 55_cpu'=63.2%;50;80 'RDP-Tcp 55_working_set'=4831838208B;0;0 ...
```

**Alert on per-session memory:**

```
check_rds_session_load sessions-only=true "warning=working_set > 8000000000"
OK: Console: 6.1% cpu, 4833346560B working set|'Console_working_set'=4833346560B;8000000000;0
```

**On a host without the counters the check reports UNKNOWN with a clear message:**

```
check_rds_session_load
Remote Desktop Services counters (Terminal Services Session) not available - is the role installed on this host? (...)
```



<a id="check_rds_session_load_options"></a>
#### Command-line Arguments

| Option                                                 | Default Value | Description                                                                                                 |
|--------------------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------------|
| [averages](#check_rds_session_load_averages)           | false         | Collect a second sample after one second so the cpu keyword carries a real value.                           |
| [sessions-only](#check_rds_session_load_sessions-only) | false         | Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions. |



<h5 id="check_rds_session_load_averages">averages:</h5>

Collect a second sample after one second so the cpu keyword carries a real value.

*Default Value:* `false`

<h5 id="check_rds_session_load_sessions-only">sessions-only:</h5>

Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                 | Default Value                                        |
|------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------|
| <a id="check_rds_session_load_filter"></a>[filter](../common-options.md#filter)                                        |                                                      |
| <a id="check_rds_session_load_warning"></a>[warning](../common-options.md#warning)                                     |                                                      |
| <a id="check_rds_session_load_warn"></a>[warn](../common-options.md#warn)                                              |                                                      |
| <a id="check_rds_session_load_critical"></a>[critical](../common-options.md#critical)                                  |                                                      |
| <a id="check_rds_session_load_crit"></a>[crit](../common-options.md#crit)                                              |                                                      |
| <a id="check_rds_session_load_ok"></a>[ok](../common-options.md#ok)                                                    |                                                      |
| <a id="check_rds_session_load_debug"></a>[debug](../common-options.md#debug)                                           | false                                                |
| <a id="check_rds_session_load_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                |
| <a id="check_rds_session_load_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                   |
| <a id="check_rds_session_load_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                      |
| <a id="check_rds_session_load_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                |
| <a id="check_rds_session_load_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                    |
| <a id="check_rds_session_load_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                   |
| <a id="check_rds_session_load_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                      |
| <a id="check_rds_session_load_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No sessions found                                    |
| <a id="check_rds_session_load_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${session}: ${cpu}% cpu, ${working_set}B working set |
| <a id="check_rds_session_load_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${session}                                           |
| <a id="check_rds_session_load_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                      |
| <a id="check_rds_session_load_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                      |
| <a id="check_rds_session_load_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                   |
| <a id="check_rds_session_load_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_session_load_filter_keys"></a>
#### Filter keywords

| Option      | Description                                                        |
|-------------|--------------------------------------------------------------------|
| cpu         | % processor time of the session (needs averages=true, otherwise 0) |
| session     | Counter instance name (Console, Services, RDP-Tcp <n>, ...)        |
| total_bytes | Protocol bytes in+out since the session connected                  |
| working_set | Working set of the session in bytes                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_sessions

Check session counts on a session host (active, inactive, total).

#### About `check_rds_sessions`

`check_rds_sessions` reads the "Terminal Services" performance counters
(Active/Inactive/Total Sessions) and reports the session-count picture of a
Remote Desktop session host in one record. The counter object exists on every
Windows SKU (the console counts as a session), so the check also works on
plain servers — the numbers only become interesting on session hosts.

There are no default thresholds; all three values are emitted as perfdata so
capacity can be graphed even on an all-OK check.

Disconnected (`inactive`) sessions still hold memory, licenses and (for
per-device CALs) a seat, so `warning=inactive > <n>` is a useful signal that
idle-session limits are not configured or not working. For per-session
resource usage see `check_rds_session_load`; for CAL exhaustion see
`check_rds_licenses`.

**Jump to section:**

* [Sample Commands](#check_rds_sessions_samples)
* [Command-line Arguments](#check_rds_sessions_options)
* [Filter keywords](#check_rds_sessions_filter_keys)


<a id="check_rds_sessions_samples"></a>
#### Sample Commands

**Check session counts on a session host:**

```
check_rds_sessions
OK: 1 active, 1 inactive (2 total)|'sessions_active'=1;0;0 'sessions_inactive'=1;0;0 'sessions_total'=2;0;0
```

**Alert when the host approaches its session capacity:**

```
check_rds_sessions "warning=active > 40" "critical=active > 50"
OK: 32 active, 5 inactive (37 total)|'sessions_active'=32;40;50 'sessions_inactive'=5;0;0 'sessions_total'=37;0;0
```

**Alert on disconnected sessions piling up (they still hold memory and CALs):**

```
check_rds_sessions "warning=inactive > 10"
WARNING: 12 active, 14 inactive (26 total)|'sessions_inactive'=14;10;0 'sessions_active'=12;0;0 'sessions_total'=26;0;0
```

**On a host without the counters the check reports UNKNOWN with a clear message:**

```
check_rds_sessions
Remote Desktop Services counters (Terminal Services) not available - is the role installed on this host? (...)
```



<a id="check_rds_sessions_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                    |
|--------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| <a id="check_rds_sessions_filter"></a>[filter](../common-options.md#filter)                                        |                                                                  |
| <a id="check_rds_sessions_warning"></a>[warning](../common-options.md#warning)                                     |                                                                  |
| <a id="check_rds_sessions_warn"></a>[warn](../common-options.md#warn)                                              |                                                                  |
| <a id="check_rds_sessions_critical"></a>[critical](../common-options.md#critical)                                  |                                                                  |
| <a id="check_rds_sessions_crit"></a>[crit](../common-options.md#crit)                                              |                                                                  |
| <a id="check_rds_sessions_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                  |
| <a id="check_rds_sessions_debug"></a>[debug](../common-options.md#debug)                                           | false                                                            |
| <a id="check_rds_sessions_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                            |
| <a id="check_rds_sessions_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                          |
| <a id="check_rds_sessions_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                  |
| <a id="check_rds_sessions_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                            |
| <a id="check_rds_sessions_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                |
| <a id="check_rds_sessions_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                               |
| <a id="check_rds_sessions_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                  |
| <a id="check_rds_sessions_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No session counters found                                        |
| <a id="check_rds_sessions_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${active} active, ${inactive} inactive (${total_sessions} total) |
| <a id="check_rds_sessions_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | sessions                                                         |
| <a id="check_rds_sessions_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                  |
| <a id="check_rds_sessions_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                  |
| <a id="check_rds_sessions_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                               |
| <a id="check_rds_sessions_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                  |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_sessions_filter_keys"></a>
#### Filter keywords

| Option         | Description                                          |
|----------------|------------------------------------------------------|
| active         | Sessions with a connected user                       |
| inactive       | Disconnected (idle) sessions still holding resources |
| total_sessions | Total sessions on the host                           |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

