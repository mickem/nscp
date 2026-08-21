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

**Jump to section:**

* [Command-line Arguments](#check_iis_app_pools_options)
* [Filter keywords](#check_iis_app_pools_filter_keys)



<a id="check_iis_app_pools_options"></a>
#### Command-line Arguments

<a id="check_iis_app_pools_warn"></a>
<a id="check_iis_app_pools_crit"></a>
<a id="check_iis_app_pools_help"></a>
<a id="check_iis_app_pools_help-pb"></a>
<a id="check_iis_app_pools_show-default"></a>
<a id="check_iis_app_pools_help-short"></a>

| Option                                                | Default Value                                              | Description                                                                                                               |
|-------------------------------------------------------|------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_iis_app_pools_filter)                 |                                                            | Filter which marks interesting items.                                                                                     |
| [warning](#check_iis_app_pools_warning)               |                                                            | Filter which marks items which generates a warning state.                                                                 |
| warn                                                  |                                                            | Short alias for warning                                                                                                   |
| [critical](#check_iis_app_pools_critical)             | state != 'running' and auto_start != 0                     | Filter which marks items which generates a critical state.                                                                |
| crit                                                  |                                                            | Short alias for critical.                                                                                                 |
| [ok](#check_iis_app_pools_ok)                         |                                                            | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_iis_app_pools_debug)                   | 1)] (=0                                                    | Show debugging information in the log                                                                                     |
| [show-all](#check_iis_app_pools_show-all)             | 1)] (=0                                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_iis_app_pools_empty-state)       | unknown                                                    | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_iis_app_pools_perf-config)       |                                                            | Performance data generation configuration                                                                                 |
| [escape-html](#check_iis_app_pools_escape-html)       | 1)] (=0                                                    | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_iis_app_pools_list-separator) | ,                                                          | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                  | N/A                                                        | Show help screen (this screen)                                                                                            |
| help-pb                                               | N/A                                                        | Show help screen as a protocol buffer payload                                                                             |
| show-default                                          | N/A                                                        | Show default values for a given command                                                                                   |
| help-short                                            | N/A                                                        | Show help screen (short format).                                                                                          |
| [top-syntax](#check_iis_app_pools_top-syntax)         | ${status}: ${list}                                         | Top level syntax.                                                                                                         |
| [ok-syntax](#check_iis_app_pools_ok-syntax)           |                                                            | ok syntax.                                                                                                                |
| [empty-syntax](#check_iis_app_pools_empty-syntax)     | No application pools found                                 | Empty syntax.                                                                                                             |
| [detail-syntax](#check_iis_app_pools_detail-syntax)   | ${pool}: ${state}, uptime ${uptime}s, ${recycles} recycles | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_iis_app_pools_perf-syntax)       | ${pool}                                                    | Performance alias syntax.                                                                                                 |



<h5 id="check_iis_app_pools_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_iis_app_pools_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_iis_app_pools_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `state != 'running' and auto_start != 0`

<h5 id="check_iis_app_pools_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_iis_app_pools_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_iis_app_pools_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_iis_app_pools_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_iis_app_pools_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_iis_app_pools_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_iis_app_pools_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_iis_app_pools_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_iis_app_pools_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_iis_app_pools_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No application pools found`

<h5 id="check_iis_app_pools_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${pool}: ${state}, uptime ${uptime}s, ${recycles} recycles`

<h5 id="check_iis_app_pools_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${pool}`


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

**Jump to section:**

* [Command-line Arguments](#check_iis_request_queues_options)
* [Filter keywords](#check_iis_request_queues_filter_keys)



<a id="check_iis_request_queues_options"></a>
#### Command-line Arguments

<a id="check_iis_request_queues_warn"></a>
<a id="check_iis_request_queues_crit"></a>
<a id="check_iis_request_queues_help"></a>
<a id="check_iis_request_queues_help-pb"></a>
<a id="check_iis_request_queues_show-default"></a>
<a id="check_iis_request_queues_help-short"></a>

| Option                                                     | Default Value                                          | Description                                                                                                               |
|------------------------------------------------------------|--------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_iis_request_queues_filter)                 |                                                        | Filter which marks interesting items.                                                                                     |
| [warning](#check_iis_request_queues_warning)               | queue_length > 800                                     | Filter which marks items which generates a warning state.                                                                 |
| warn                                                       |                                                        | Short alias for warning                                                                                                   |
| [critical](#check_iis_request_queues_critical)             | queue_length > 1000                                    | Filter which marks items which generates a critical state.                                                                |
| crit                                                       |                                                        | Short alias for critical.                                                                                                 |
| [ok](#check_iis_request_queues_ok)                         |                                                        | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_iis_request_queues_debug)                   | 1)] (=0                                                | Show debugging information in the log                                                                                     |
| [show-all](#check_iis_request_queues_show-all)             | 1)] (=0                                                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_iis_request_queues_empty-state)       | ok                                                     | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_iis_request_queues_perf-config)       |                                                        | Performance data generation configuration                                                                                 |
| [escape-html](#check_iis_request_queues_escape-html)       | 1)] (=0                                                | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_iis_request_queues_list-separator) | ,                                                      | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                       | N/A                                                    | Show help screen (this screen)                                                                                            |
| help-pb                                                    | N/A                                                    | Show help screen as a protocol buffer payload                                                                             |
| show-default                                               | N/A                                                    | Show default values for a given command                                                                                   |
| help-short                                                 | N/A                                                    | Show help screen (short format).                                                                                          |
| [top-syntax](#check_iis_request_queues_top-syntax)         | ${status}: ${list}                                     | Top level syntax.                                                                                                         |
| [ok-syntax](#check_iis_request_queues_ok-syntax)           |                                                        | ok syntax.                                                                                                                |
| [empty-syntax](#check_iis_request_queues_empty-syntax)     | No HTTP.sys request queues found                       | Empty syntax.                                                                                                             |
| [detail-syntax](#check_iis_request_queues_detail-syntax)   | ${queue}: ${queue_length} queued, ${rejected} rejected | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_iis_request_queues_perf-syntax)       | ${queue}                                               | Performance alias syntax.                                                                                                 |



<h5 id="check_iis_request_queues_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_iis_request_queues_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `queue_length > 800`

<h5 id="check_iis_request_queues_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `queue_length > 1000`

<h5 id="check_iis_request_queues_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_iis_request_queues_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_iis_request_queues_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_iis_request_queues_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_iis_request_queues_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_iis_request_queues_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_iis_request_queues_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_iis_request_queues_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_iis_request_queues_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_iis_request_queues_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No HTTP.sys request queues found`

<h5 id="check_iis_request_queues_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${queue}: ${queue_length} queued, ${rejected} rejected`

<h5 id="check_iis_request_queues_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${queue}`


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

**Jump to section:**

* [Command-line Arguments](#check_iis_sites_options)
* [Filter keywords](#check_iis_sites_filter_keys)



<a id="check_iis_sites_options"></a>
#### Command-line Arguments

<a id="check_iis_sites_warn"></a>
<a id="check_iis_sites_crit"></a>
<a id="check_iis_sites_help"></a>
<a id="check_iis_sites_help-pb"></a>
<a id="check_iis_sites_show-default"></a>
<a id="check_iis_sites_help-short"></a>

| Option                                            | Default Value                                                    | Description                                                                                                               |
|---------------------------------------------------|------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_iis_sites_filter)                 |                                                                  | Filter which marks interesting items.                                                                                     |
| [warning](#check_iis_sites_warning)               |                                                                  | Filter which marks items which generates a warning state.                                                                 |
| warn                                              |                                                                  | Short alias for warning                                                                                                   |
| [critical](#check_iis_sites_critical)             | state = 'stopped' and auto_start != 0                            | Filter which marks items which generates a critical state.                                                                |
| crit                                              |                                                                  | Short alias for critical.                                                                                                 |
| [ok](#check_iis_sites_ok)                         |                                                                  | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_iis_sites_debug)                   | 1)] (=0                                                          | Show debugging information in the log                                                                                     |
| [show-all](#check_iis_sites_show-all)             | 1)] (=0                                                          | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_iis_sites_empty-state)       | unknown                                                          | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_iis_sites_perf-config)       |                                                                  | Performance data generation configuration                                                                                 |
| [escape-html](#check_iis_sites_escape-html)       | 1)] (=0                                                          | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_iis_sites_list-separator) | ,                                                                | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                              | N/A                                                              | Show help screen (this screen)                                                                                            |
| help-pb                                           | N/A                                                              | Show help screen as a protocol buffer payload                                                                             |
| show-default                                      | N/A                                                              | Show default values for a given command                                                                                   |
| help-short                                        | N/A                                                              | Show help screen (short format).                                                                                          |
| [top-syntax](#check_iis_sites_top-syntax)         | ${status}: ${list}                                               | Top level syntax.                                                                                                         |
| [ok-syntax](#check_iis_sites_ok-syntax)           |                                                                  | ok syntax.                                                                                                                |
| [empty-syntax](#check_iis_sites_empty-syntax)     | No web sites found                                               | Empty syntax.                                                                                                             |
| [detail-syntax](#check_iis_sites_detail-syntax)   | ${site}: ${state}, ${connections} connections, uptime ${uptime}s | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_iis_sites_perf-syntax)       | ${site}                                                          | Performance alias syntax.                                                                                                 |
| [averages](#check_iis_sites_averages)             | 1)] (=0                                                          | Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values.        |



<h5 id="check_iis_sites_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_iis_sites_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_iis_sites_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `state = 'stopped' and auto_start != 0`

<h5 id="check_iis_sites_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_iis_sites_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_iis_sites_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_iis_sites_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_iis_sites_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_iis_sites_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_iis_sites_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_iis_sites_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_iis_sites_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_iis_sites_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No web sites found`

<h5 id="check_iis_sites_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${site}: ${state}, ${connections} connections, uptime ${uptime}s`

<h5 id="check_iis_sites_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${site}`

<h5 id="check_iis_sites_averages">averages:</h5>

Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values.

*Default Value:* `1)] (=0`


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

**Jump to section:**

* [Command-line Arguments](#check_iis_worker_processes_options)
* [Filter keywords](#check_iis_worker_processes_filter_keys)



<a id="check_iis_worker_processes_options"></a>
#### Command-line Arguments

<a id="check_iis_worker_processes_warn"></a>
<a id="check_iis_worker_processes_crit"></a>
<a id="check_iis_worker_processes_help"></a>
<a id="check_iis_worker_processes_help-pb"></a>
<a id="check_iis_worker_processes_show-default"></a>
<a id="check_iis_worker_processes_help-short"></a>

| Option                                                       | Default Value                                            | Description                                                                                                               |
|--------------------------------------------------------------|----------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_iis_worker_processes_filter)                 |                                                          | Filter which marks interesting items.                                                                                     |
| [warning](#check_iis_worker_processes_warning)               |                                                          | Filter which marks items which generates a warning state.                                                                 |
| warn                                                         |                                                          | Short alias for warning                                                                                                   |
| [critical](#check_iis_worker_processes_critical)             |                                                          | Filter which marks items which generates a critical state.                                                                |
| crit                                                         |                                                          | Short alias for critical.                                                                                                 |
| [ok](#check_iis_worker_processes_ok)                         |                                                          | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_iis_worker_processes_debug)                   | 1)] (=0                                                  | Show debugging information in the log                                                                                     |
| [show-all](#check_iis_worker_processes_show-all)             | 1)] (=0                                                  | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_iis_worker_processes_empty-state)       | ok                                                       | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_iis_worker_processes_perf-config)       |                                                          | Performance data generation configuration                                                                                 |
| [escape-html](#check_iis_worker_processes_escape-html)       | 1)] (=0                                                  | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_iis_worker_processes_list-separator) | ,                                                        | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                         | N/A                                                      | Show help screen (this screen)                                                                                            |
| help-pb                                                      | N/A                                                      | Show help screen as a protocol buffer payload                                                                             |
| show-default                                                 | N/A                                                      | Show default values for a given command                                                                                   |
| help-short                                                   | N/A                                                      | Show help screen (short format).                                                                                          |
| [top-syntax](#check_iis_worker_processes_top-syntax)         | ${status}: ${list}                                       | Top level syntax.                                                                                                         |
| [ok-syntax](#check_iis_worker_processes_ok-syntax)           |                                                          | ok syntax.                                                                                                                |
| [empty-syntax](#check_iis_worker_processes_empty-syntax)     | No IIS worker processes running                          | Empty syntax.                                                                                                             |
| [detail-syntax](#check_iis_worker_processes_detail-syntax)   | ${pool} (pid ${pid}): ${active_requests} active requests | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_iis_worker_processes_perf-syntax)       | ${pool}_${pid}                                           | Performance alias syntax.                                                                                                 |



<h5 id="check_iis_worker_processes_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_iis_worker_processes_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_iis_worker_processes_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_iis_worker_processes_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_iis_worker_processes_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_iis_worker_processes_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_iis_worker_processes_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_iis_worker_processes_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_iis_worker_processes_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_iis_worker_processes_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_iis_worker_processes_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_iis_worker_processes_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_iis_worker_processes_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No IIS worker processes running`

<h5 id="check_iis_worker_processes_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${pool} (pid ${pid}): ${active_requests} active requests`

<h5 id="check_iis_worker_processes_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${pool}_${pid}`


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

**Jump to section:**

* [Command-line Arguments](#check_rds_broker_options)
* [Filter keywords](#check_rds_broker_filter_keys)



<a id="check_rds_broker_options"></a>
#### Command-line Arguments

<a id="check_rds_broker_warn"></a>
<a id="check_rds_broker_crit"></a>
<a id="check_rds_broker_help"></a>
<a id="check_rds_broker_help-pb"></a>
<a id="check_rds_broker_show-default"></a>
<a id="check_rds_broker_help-short"></a>

| Option                                             | Default Value                       | Description                                                                                                               |
|----------------------------------------------------|-------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_rds_broker_filter)                 |                                     | Filter which marks interesting items.                                                                                     |
| [warning](#check_rds_broker_warning)               |                                     | Filter which marks items which generates a warning state.                                                                 |
| warn                                               |                                     | Short alias for warning                                                                                                   |
| [critical](#check_rds_broker_critical)             |                                     | Filter which marks items which generates a critical state.                                                                |
| crit                                               |                                     | Short alias for critical.                                                                                                 |
| [ok](#check_rds_broker_ok)                         |                                     | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_rds_broker_debug)                   | 1)] (=0                             | Show debugging information in the log                                                                                     |
| [show-all](#check_rds_broker_show-all)             | 1)] (=0                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_rds_broker_empty-state)       | unknown                             | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_rds_broker_perf-config)       |                                     | Performance data generation configuration                                                                                 |
| [escape-html](#check_rds_broker_escape-html)       | 1)] (=0                             | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_rds_broker_list-separator) | ,                                   | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                               | N/A                                 | Show help screen (this screen)                                                                                            |
| help-pb                                            | N/A                                 | Show help screen as a protocol buffer payload                                                                             |
| show-default                                       | N/A                                 | Show default values for a given command                                                                                   |
| help-short                                         | N/A                                 | Show help screen (short format).                                                                                          |
| [top-syntax](#check_rds_broker_top-syntax)         | ${status}: ${list}                  | Top level syntax.                                                                                                         |
| [ok-syntax](#check_rds_broker_ok-syntax)           |                                     | ok syntax.                                                                                                                |
| [empty-syntax](#check_rds_broker_empty-syntax)     | No Connection Broker counters found | Empty syntax.                                                                                                             |
| [detail-syntax](#check_rds_broker_detail-syntax)   | ${label} = ${value}                 | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_rds_broker_perf-syntax)       | ${label}                            | Performance alias syntax.                                                                                                 |
| [averages](#check_rds_broker_averages)             | 1)] (=0                             | Collect a second sample after one second so rate counters carry real values.                                              |



<h5 id="check_rds_broker_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_rds_broker_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_rds_broker_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_rds_broker_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_rds_broker_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_rds_broker_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_rds_broker_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_rds_broker_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_rds_broker_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_rds_broker_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_rds_broker_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_rds_broker_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_rds_broker_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No Connection Broker counters found`

<h5 id="check_rds_broker_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${label} = ${value}`

<h5 id="check_rds_broker_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${label}`

<h5 id="check_rds_broker_averages">averages:</h5>

Collect a second sample after one second so rate counters carry real values.

*Default Value:* `1)] (=0`


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

**Jump to section:**

* [Command-line Arguments](#check_rds_licenses_options)
* [Filter keywords](#check_rds_licenses_filter_keys)



<a id="check_rds_licenses_options"></a>
#### Command-line Arguments

<a id="check_rds_licenses_warn"></a>
<a id="check_rds_licenses_crit"></a>
<a id="check_rds_licenses_help"></a>
<a id="check_rds_licenses_help-pb"></a>
<a id="check_rds_licenses_show-default"></a>
<a id="check_rds_licenses_help-short"></a>

| Option                                               | Default Value                                                     | Description                                                                                                               |
|------------------------------------------------------|-------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_rds_licenses_filter)                 |                                                                   | Filter which marks interesting items.                                                                                     |
| [warning](#check_rds_licenses_warning)               | available < 10 and total > 0                                      | Filter which marks items which generates a warning state.                                                                 |
| warn                                                 |                                                                   | Short alias for warning                                                                                                   |
| [critical](#check_rds_licenses_critical)             | available = 0 and total > 0                                       | Filter which marks items which generates a critical state.                                                                |
| crit                                                 |                                                                   | Short alias for critical.                                                                                                 |
| [ok](#check_rds_licenses_ok)                         |                                                                   | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_rds_licenses_debug)                   | 1)] (=0                                                           | Show debugging information in the log                                                                                     |
| [show-all](#check_rds_licenses_show-all)             | 1)] (=0                                                           | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_rds_licenses_empty-state)       | unknown                                                           | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_rds_licenses_perf-config)       |                                                                   | Performance data generation configuration                                                                                 |
| [escape-html](#check_rds_licenses_escape-html)       | 1)] (=0                                                           | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_rds_licenses_list-separator) | ,                                                                 | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                 | N/A                                                               | Show help screen (this screen)                                                                                            |
| help-pb                                              | N/A                                                               | Show help screen as a protocol buffer payload                                                                             |
| show-default                                         | N/A                                                               | Show default values for a given command                                                                                   |
| help-short                                           | N/A                                                               | Show help screen (short format).                                                                                          |
| [top-syntax](#check_rds_licenses_top-syntax)         | ${status}: ${list}                                                | Top level syntax.                                                                                                         |
| [ok-syntax](#check_rds_licenses_ok-syntax)           |                                                                   | ok syntax.                                                                                                                |
| [empty-syntax](#check_rds_licenses_empty-syntax)     | No license key packs found                                        | Empty syntax.                                                                                                             |
| [detail-syntax](#check_rds_licenses_detail-syntax)   | ${description}: ${issued}/${total} issued, ${available} available | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_rds_licenses_perf-syntax)       | ${description}                                                    | Performance alias syntax.                                                                                                 |



<h5 id="check_rds_licenses_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_rds_licenses_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `available < 10 and total > 0`

<h5 id="check_rds_licenses_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `available = 0 and total > 0`

<h5 id="check_rds_licenses_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_rds_licenses_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_rds_licenses_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_rds_licenses_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_rds_licenses_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_rds_licenses_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_rds_licenses_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_rds_licenses_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_rds_licenses_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_rds_licenses_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No license key packs found`

<h5 id="check_rds_licenses_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${description}: ${issued}/${total} issued, ${available} available`

<h5 id="check_rds_licenses_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${description}`


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
| type            | Key pack type: unknown, retail, volume, concurrent, temporary, open or built-in |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_session_load

Check per-session resource usage (CPU, working set, protocol bytes).

**Jump to section:**

* [Command-line Arguments](#check_rds_session_load_options)
* [Filter keywords](#check_rds_session_load_filter_keys)



<a id="check_rds_session_load_options"></a>
#### Command-line Arguments

<a id="check_rds_session_load_warn"></a>
<a id="check_rds_session_load_crit"></a>
<a id="check_rds_session_load_help"></a>
<a id="check_rds_session_load_help-pb"></a>
<a id="check_rds_session_load_show-default"></a>
<a id="check_rds_session_load_help-short"></a>

| Option                                                   | Default Value                                        | Description                                                                                                               |
|----------------------------------------------------------|------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_rds_session_load_filter)                 |                                                      | Filter which marks interesting items.                                                                                     |
| [warning](#check_rds_session_load_warning)               |                                                      | Filter which marks items which generates a warning state.                                                                 |
| warn                                                     |                                                      | Short alias for warning                                                                                                   |
| [critical](#check_rds_session_load_critical)             |                                                      | Filter which marks items which generates a critical state.                                                                |
| crit                                                     |                                                      | Short alias for critical.                                                                                                 |
| [ok](#check_rds_session_load_ok)                         |                                                      | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_rds_session_load_debug)                   | 1)] (=0                                              | Show debugging information in the log                                                                                     |
| [show-all](#check_rds_session_load_show-all)             | 1)] (=0                                              | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_rds_session_load_empty-state)       | ok                                                   | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_rds_session_load_perf-config)       |                                                      | Performance data generation configuration                                                                                 |
| [escape-html](#check_rds_session_load_escape-html)       | 1)] (=0                                              | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_rds_session_load_list-separator) | ,                                                    | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                     | N/A                                                  | Show help screen (this screen)                                                                                            |
| help-pb                                                  | N/A                                                  | Show help screen as a protocol buffer payload                                                                             |
| show-default                                             | N/A                                                  | Show default values for a given command                                                                                   |
| help-short                                               | N/A                                                  | Show help screen (short format).                                                                                          |
| [top-syntax](#check_rds_session_load_top-syntax)         | ${status}: ${list}                                   | Top level syntax.                                                                                                         |
| [ok-syntax](#check_rds_session_load_ok-syntax)           |                                                      | ok syntax.                                                                                                                |
| [empty-syntax](#check_rds_session_load_empty-syntax)     | No sessions found                                    | Empty syntax.                                                                                                             |
| [detail-syntax](#check_rds_session_load_detail-syntax)   | ${session}: ${cpu}% cpu, ${working_set}B working set | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_rds_session_load_perf-syntax)       | ${session}                                           | Performance alias syntax.                                                                                                 |
| [averages](#check_rds_session_load_averages)             | 1)] (=0                                              | Collect a second sample after one second so the cpu keyword carries a real value.                                         |
| [sessions-only](#check_rds_session_load_sessions-only)   | 1)] (=0                                              | Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions.               |



<h5 id="check_rds_session_load_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_rds_session_load_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_rds_session_load_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_rds_session_load_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_rds_session_load_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_rds_session_load_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_rds_session_load_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_rds_session_load_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_rds_session_load_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_rds_session_load_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_rds_session_load_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_rds_session_load_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_rds_session_load_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No sessions found`

<h5 id="check_rds_session_load_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${session}: ${cpu}% cpu, ${working_set}B working set`

<h5 id="check_rds_session_load_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${session}`

<h5 id="check_rds_session_load_averages">averages:</h5>

Collect a second sample after one second so the cpu keyword carries a real value.

*Default Value:* `1)] (=0`

<h5 id="check_rds_session_load_sessions-only">sessions-only:</h5>

Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions.

*Default Value:* `1)] (=0`


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

**Jump to section:**

* [Command-line Arguments](#check_rds_sessions_options)
* [Filter keywords](#check_rds_sessions_filter_keys)



<a id="check_rds_sessions_options"></a>
#### Command-line Arguments

<a id="check_rds_sessions_warn"></a>
<a id="check_rds_sessions_crit"></a>
<a id="check_rds_sessions_help"></a>
<a id="check_rds_sessions_help-pb"></a>
<a id="check_rds_sessions_show-default"></a>
<a id="check_rds_sessions_help-short"></a>

| Option                                               | Default Value                                           | Description                                                                                                               |
|------------------------------------------------------|---------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_rds_sessions_filter)                 |                                                         | Filter which marks interesting items.                                                                                     |
| [warning](#check_rds_sessions_warning)               |                                                         | Filter which marks items which generates a warning state.                                                                 |
| warn                                                 |                                                         | Short alias for warning                                                                                                   |
| [critical](#check_rds_sessions_critical)             |                                                         | Filter which marks items which generates a critical state.                                                                |
| crit                                                 |                                                         | Short alias for critical.                                                                                                 |
| [ok](#check_rds_sessions_ok)                         |                                                         | Filter which marks items which generates an ok state.                                                                     |
| [debug](#check_rds_sessions_debug)                   | 1)] (=0                                                 | Show debugging information in the log                                                                                     |
| [show-all](#check_rds_sessions_show-all)             | 1)] (=0                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).          |
| [empty-state](#check_rds_sessions_empty-state)       | unknown                                                 | Return status to use when nothing matched filter.                                                                         |
| [perf-config](#check_rds_sessions_perf-config)       |                                                         | Performance data generation configuration                                                                                 |
| [escape-html](#check_rds_sessions_escape-html)       | 1)] (=0                                                 | Escape any < and > characters to prevent HTML encoding                                                                    |
| [list-separator](#check_rds_sessions_list-separator) | ,                                                       | String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list). |
| help                                                 | N/A                                                     | Show help screen (this screen)                                                                                            |
| help-pb                                              | N/A                                                     | Show help screen as a protocol buffer payload                                                                             |
| show-default                                         | N/A                                                     | Show default values for a given command                                                                                   |
| help-short                                           | N/A                                                     | Show help screen (short format).                                                                                          |
| [top-syntax](#check_rds_sessions_top-syntax)         | ${status}: ${list}                                      | Top level syntax.                                                                                                         |
| [ok-syntax](#check_rds_sessions_ok-syntax)           |                                                         | ok syntax.                                                                                                                |
| [empty-syntax](#check_rds_sessions_empty-syntax)     | No session counters found                               | Empty syntax.                                                                                                             |
| [detail-syntax](#check_rds_sessions_detail-syntax)   | ${active} active, ${inactive} inactive (${total} total) | Detail level syntax.                                                                                                      |
| [perf-syntax](#check_rds_sessions_perf-syntax)       | sessions                                                | Performance alias syntax.                                                                                                 |



<h5 id="check_rds_sessions_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_rds_sessions_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_rds_sessions_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_rds_sessions_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_rds_sessions_debug">debug:</h5>

Show debugging information in the log

*Default Value:* `1)] (=0`

<h5 id="check_rds_sessions_show-all">show-all:</h5>

Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

*Default Value:* `1)] (=0`

<h5 id="check_rds_sessions_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_rds_sessions_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_rds_sessions_escape-html">escape-html:</h5>

Escape any < and > characters to prevent HTML encoding

*Default Value:* `1)] (=0`

<h5 id="check_rds_sessions_list-separator">list-separator:</h5>

String used to separate the items of %(list), %(ok_list), %(warn_list), %(crit_list), %(problem_list) and %(detail_list).
Accepts the escapes \n, \r, \t and \\ (a configuration file value is a single line, so a real newline cannot be written).
Set to \n to render one item per line, which most Nagios compatible frontends show as long output below the summary line.
The top-syntax decides what precedes the first item; templates are never escape-decoded, so reference the decoded separator as %(sep) to break before it too: --top-syntax "%(status): %(count) items:%(sep)%(list)".

*Default Value:* `, `

<h5 id="check_rds_sessions_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_rds_sessions_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_rds_sessions_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No session counters found`

<h5 id="check_rds_sessions_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formatted by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${active} active, ${inactive} inactive (${total} total)`

<h5 id="check_rds_sessions_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `sessions`


<a id="check_rds_sessions_filter_keys"></a>
#### Filter keywords

| Option   | Description                                          |
|----------|------------------------------------------------------|
| active   | Sessions with a connected user                       |
| inactive | Disconnected (idle) sessions still holding resources |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

