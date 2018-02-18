# CheckSystem

Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.




## Queries

A quick reference for all available queries (check commands) in the CheckSystem module.

**List of commands:**

A list of all available queries (check commands)

| Command                                 | Description                                                                   |
|-----------------------------------------|-------------------------------------------------------------------------------|
| [check_cpu](#check_cpu)                 | Check that the load of the CPU(s) are within bounds.                          |
| [check_memory](#check_memory)           | Check free/used memory on the system.                                         |
| [check_network](#check_network)         | Check network interface status.                                               |
| [check_os_version](#check_os_version)   | Check the version of the underlaying OS.                                      |
| [check_pagefile](#check_pagefile)       | Check the size of the system pagefile(s).                                     |
| [check_pdh](#check_pdh)                 | Check the value of a performance (PDH) counter on the local or remote system. |
| [check_process](#check_process)         | Check state/metrics of one or more of the processes running on the computer.  |
| [check_service](#check_service)         | Check the state of one or more of the computer services.                      |
| [check_uptime](#check_uptime)           | Check time since last server re-boot.                                         |
| [checkcounter](#checkcounter)           | Legacy version of check_pdh                                                   |
| [checkcpu](#checkcpu)                   | Legacy version of check_cpu                                                   |
| [checkmem](#checkmem)                   | Legacy version of check_mem                                                   |
| [checkprocstate](#checkprocstate)       | Legacy version of check_process                                               |
| [checkservicestate](#checkservicestate) | Legacy version of check_service                                               |
| [checkuptime](#checkuptime)             | Legacy version of check_uptime                                                |


**List of command aliases:**

A list of all short hand aliases for queries (check commands)


| Command       | Description                   |
|---------------|-------------------------------|
| check_counter | Alias for: :query:`check_pdh` |


### check_cpu

Check that the load of the CPU(s) are within bounds.

The check_cpu command is a query based command which means it has a filter where you can use a filter expression with filter keywords to define which rows are relevant to the check.
The filter is written using the filter query language and in it you can use various filter keywords to define the filtering logic.
The filter keywords can also be used to create the bound expressions for the warning and critical which defines when a check returns warning or critical.

* [Samples](#check_cpu_samples)

* [Command-line Arguments](#check_cpu_options)
* [Filter keywords](#check_cpu_filter_keys)


<a name="check_cpu_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_cpu_samples.md)_

**Default check:**

```
check_cpu
CPU Load ok
'total 5m load'=0%;80;90 'total 1m load'=0%;80;90 'total 5s load'=7%;80;90
```

Checking **all cores** by adding filter=none (disabling the filter):

```
check_cpu filter=none "warn=load > 80" "crit=load > 90"
CPU Load ok
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

Adding kernel times to the check::

```
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90" "top-syntax=${list}"
core 0 > 3, core 1 > 0, core 2 > 0, core  ... , core 7 > 15, total > 7
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

Default check **via NRPE**::

```
check_nscp --host 192.168.56.103 --command check_cpu
CPU Load ok|'total 5m'=16%;80;90 'total 1m'=13%;80;90 'total 5s'=13%;80;90
```



<a name="check_cpu_warn"/>
<a name="check_cpu_crit"/>
<a name="check_cpu_debug"/>
<a name="check_cpu_show-all"/>
<a name="check_cpu_escape-html"/>
<a name="check_cpu_help"/>
<a name="check_cpu_help-pb"/>
<a name="check_cpu_show-default"/>
<a name="check_cpu_help-short"/>
<a name="check_cpu_time"/>
<a name="check_cpu_options"/>
#### Command-line Arguments


| Option                                    | Default Value              | Description                                                                                                      |
|-------------------------------------------|----------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_cpu_filter)               | core = 'total'             | Filter which marks interesting items.                                                                            |
| [warning](#check_cpu_warning)             | load > 80                  | Filter which marks items which generates a warning state.                                                        |
| warn                                      |                            | Short alias for warning                                                                                          |
| [critical](#check_cpu_critical)           | load > 90                  | Filter which marks items which generates a critical state.                                                       |
| crit                                      |                            | Short alias for critical.                                                                                        |
| [ok](#check_cpu_ok)                       |                            | Filter which marks items which generates an ok state.                                                            |
| debug                                     | N/A                        | Show debugging information in the log                                                                            |
| show-all                                  | N/A                        | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_cpu_empty-state)     | ignored                    | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_cpu_perf-config)     |                            | Performance data generation configuration                                                                        |
| escape-html                               | N/A                        | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                      | N/A                        | Show help screen (this screen)                                                                                   |
| help-pb                                   | N/A                        | Show help screen as a protocol buffer payload                                                                    |
| show-default                              | N/A                        | Show default values for a given command                                                                          |
| help-short                                | N/A                        | Show help screen (short format).                                                                                 |
| [top-syntax](#check_cpu_top-syntax)       | ${status}: ${problem_list} | Top level syntax.                                                                                                |
| [ok-syntax](#check_cpu_ok-syntax)         | %(status): CPU load is ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_cpu_empty-syntax)   |                            | Empty syntax.                                                                                                    |
| [detail-syntax](#check_cpu_detail-syntax) | ${time}: ${load}%          | Detail level syntax.                                                                                             |
| [perf-syntax](#check_cpu_perf-syntax)     | ${core} ${time}            | Performance alias syntax.                                                                                        |
| time                                      |                            | The time to check                                                                                                |



<h5 id="check_cpu_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

*Default Value:* `core = 'total'`

<h5 id="check_cpu_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `load > 80`

<h5 id="check_cpu_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `load > 90`

<h5 id="check_cpu_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_cpu_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_cpu_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_cpu_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_cpu_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): CPU load is ok.`

<h5 id="check_cpu_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_cpu_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${time}: ${load}%`

<h5 id="check_cpu_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${core} ${time}`


<a name="check_cpu_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| core          | The core to check (total or core ##)                                                                         |
| core_id       | The core to check (total or core_##)                                                                         |
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| idle          | The current idle load for a given core                                                                       |
| kernel        | The current kernel load for a given core                                                                     |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| load          | The current load for a given core                                                                            |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| time          | The time frame to check                                                                                      |
| total         | Total number of items. Common option for all checks.                                                         |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_memory

Check free/used memory on the system.

* [Samples](#check_memory_samples)

* [Command-line Arguments](#check_memory_options)
* [Filter keywords](#check_memory_filter_keys)


<a name="check_memory_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_memory_samples.md)_

**Default check:**

```
check_memory
OK memory within bounds.
'page used'=8G;19;21 'page used %'=33%;79;89 'physical used'=7G;9;10 'physical used %'=65%;79;89
```

Using --show-all **to show the result**:


```
check_memory "warn=free < 20%" "crit=free < 10G" --show-all
page = 8.05G, physical = 7.85G
'page free'=15G;4;2 'page free %'=66%;19;9 'physical free'=4G;2;1 'physical free %'=34%;19;9
```

Changing the return syntax to include more information::

```
check_memory "top-syntax=${list}" "detail-syntax=${type} free: ${free} used: ${used} size: ${size}"
page free: 16G used: 7.98G size: 24G, physical free: 4.18G used: 7.8G size: 12G
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_memory
OK memory within bounds.|'page'=531G;3;3;0;3 'page %'=12%;79;89;0;100 'physical'=530G;1;1;0;1 'physical %'=25%;79;89;0;100
```
**Overriding the unit:**

Most "byte" checks such as memory have an auto scaling feature which means values wqill go from 800M to 1.2G between checks.
Some graphing systems does not honor the units in performance data in which case you can get unexpected large values (such as 800G).
To remedy this you can lock the unit by adding `perf-config=*(unit:G)`

```
check_memory perf-config=*(unit:G)
page = 8.05G, physical = 7.85G
'page free'=15G;4;2 'page free %'=66%;19;9 'physical free'=4G;2;1 'physical free %'=34%;19;9
```



<a name="check_memory_warn"/>
<a name="check_memory_crit"/>
<a name="check_memory_debug"/>
<a name="check_memory_show-all"/>
<a name="check_memory_escape-html"/>
<a name="check_memory_help"/>
<a name="check_memory_help-pb"/>
<a name="check_memory_show-default"/>
<a name="check_memory_help-short"/>
<a name="check_memory_type"/>
<a name="check_memory_options"/>
#### Command-line Arguments


| Option                                       | Default Value      | Description                                                                                                      |
|----------------------------------------------|--------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_memory_filter)               |                    | Filter which marks interesting items.                                                                            |
| [warning](#check_memory_warning)             | used > 80%         | Filter which marks items which generates a warning state.                                                        |
| warn                                         |                    | Short alias for warning                                                                                          |
| [critical](#check_memory_critical)           | used > 90%         | Filter which marks items which generates a critical state.                                                       |
| crit                                         |                    | Short alias for critical.                                                                                        |
| [ok](#check_memory_ok)                       |                    | Filter which marks items which generates an ok state.                                                            |
| debug                                        | N/A                | Show debugging information in the log                                                                            |
| show-all                                     | N/A                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_memory_empty-state)     | ignored            | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_memory_perf-config)     |                    | Performance data generation configuration                                                                        |
| escape-html                                  | N/A                | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                         | N/A                | Show help screen (this screen)                                                                                   |
| help-pb                                      | N/A                | Show help screen as a protocol buffer payload                                                                    |
| show-default                                 | N/A                | Show default values for a given command                                                                          |
| help-short                                   | N/A                | Show help screen (short format).                                                                                 |
| [top-syntax](#check_memory_top-syntax)       | ${status}: ${list} | Top level syntax.                                                                                                |
| [ok-syntax](#check_memory_ok-syntax)         |                    | ok syntax.                                                                                                       |
| [empty-syntax](#check_memory_empty-syntax)   |                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_memory_detail-syntax) | ${type} = ${used}  | Detail level syntax.                                                                                             |
| [perf-syntax](#check_memory_perf-syntax)     | ${type}            | Performance alias syntax.                                                                                        |
| type                                         |                    | The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)               |



<h5 id="check_memory_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_memory_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `used > 80%`

<h5 id="check_memory_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `used > 90%`

<h5 id="check_memory_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_memory_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_memory_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_memory_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_memory_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_memory_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_memory_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${type} = ${used}`

<h5 id="check_memory_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${type}`


<a name="check_memory_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                              |
| free_pct      | % free memory                                                                                                |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| size          | Total size of memory                                                                                         |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| type          | The type of memory to check                                                                                  |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                              |
| used_pct      | % used memory                                                                                                |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_network

Check network interface status.


* [Command-line Arguments](#check_network_options)
* [Filter keywords](#check_network_filter_keys)





<a name="check_network_warn"/>
<a name="check_network_crit"/>
<a name="check_network_debug"/>
<a name="check_network_show-all"/>
<a name="check_network_escape-html"/>
<a name="check_network_help"/>
<a name="check_network_help-pb"/>
<a name="check_network_show-default"/>
<a name="check_network_help-short"/>
<a name="check_network_options"/>
#### Command-line Arguments


| Option                                        | Default Value                          | Description                                                                                                      |
|-----------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_network_filter)               |                                        | Filter which marks interesting items.                                                                            |
| [warning](#check_network_warning)             | total > 10000                          | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                        | Short alias for warning                                                                                          |
| [critical](#check_network_critical)           | total > 100000                         | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                        | Short alias for critical.                                                                                        |
| [ok](#check_network_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                    | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_network_empty-state)     | critical                               | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_network_perf-config)     |                                        | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                    | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                    | Show default values for a given command                                                                          |
| help-short                                    | N/A                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#check_network_top-syntax)       | ${status}: ${list}                     | Top level syntax.                                                                                                |
| [ok-syntax](#check_network_ok-syntax)         | %(status): Network interfaces seem ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_network_empty-syntax)   |                                        | Empty syntax.                                                                                                    |
| [detail-syntax](#check_network_detail-syntax) | ${name} >${sent} <${received} bps      | Detail level syntax.                                                                                             |
| [perf-syntax](#check_network_perf-syntax)     | ${name}                                | Performance alias syntax.                                                                                        |



<h5 id="check_network_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_network_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `total > 10000`

<h5 id="check_network_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `total > 100000`

<h5 id="check_network_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_network_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `critical`

<h5 id="check_network_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_network_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_network_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): Network interfaces seem ok.`

<h5 id="check_network_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_network_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${name} >${sent} <${received} bps`

<h5 id="check_network_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a name="check_network_filter_keys"/>
#### Filter keywords


| Option            | Description                                                                                                  |
|-------------------|--------------------------------------------------------------------------------------------------------------|
| MAC               | The MAC address                                                                                              |
| count             | Number of items matching the filter. Common option for all checks.                                           |
| crit_count        | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list         | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list       | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| enabled           | True if the network interface is enabled                                                                     |
| list              | A list of all items which matched the filter. Common option for all checks.                                  |
| name              | Network interface name                                                                                       |
| net_connection_id | Network connection id                                                                                        |
| ok_count          | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list           | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count     | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list      | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| received          | Bytes received per second                                                                                    |
| sent              | Bytes sent per second                                                                                        |
| speed             | The network interface speed                                                                                  |
| status            | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total             | Total number of items. Common option for all checks.                                                         |
| warn_count        | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list         | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_os_version

Check the version of the underlaying OS.

* [Samples](#check_os_version_samples)

* [Command-line Arguments](#check_os_version_options)
* [Filter keywords](#check_os_version_filter_keys)


<a name="check_os_version_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_os_version_samples.md)_

**Default check:**

```
check_os_Version
L     client CRITICAL: Windows 7 (6.1.7601)
L     client  Performance data: 'version'=61;50;50
```

Making sure the OS version is **Windows 8**:

```
check_os_Version "warn=version < 62"
L     client WARNING: Windows 7 (6.1.7601)
L     client  Performance data: 'version'=61;62;0
```

Default check **via NRPE**:

```
check_nrpe --host 192.168.56.103 --command check_os_version
Windows 2012 (6.2.9200)|'version'=62;50;50
```




<a name="check_os_version_warn"/>
<a name="check_os_version_crit"/>
<a name="check_os_version_debug"/>
<a name="check_os_version_show-all"/>
<a name="check_os_version_escape-html"/>
<a name="check_os_version_help"/>
<a name="check_os_version_help-pb"/>
<a name="check_os_version_show-default"/>
<a name="check_os_version_help-short"/>
<a name="check_os_version_options"/>
#### Command-line Arguments


| Option                                           | Default Value                           | Description                                                                                                      |
|--------------------------------------------------|-----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_os_version_filter)               |                                         | Filter which marks interesting items.                                                                            |
| [warning](#check_os_version_warning)             | version <= 50                           | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                         | Short alias for warning                                                                                          |
| [critical](#check_os_version_critical)           | version <= 50                           | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                         | Short alias for critical.                                                                                        |
| [ok](#check_os_version_ok)                       |                                         | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                                     | Show debugging information in the log                                                                            |
| show-all                                         | N/A                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_os_version_empty-state)     | ignored                                 | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_os_version_perf-config)     |                                         | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                                     | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                                     | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                                     | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                                     | Show default values for a given command                                                                          |
| help-short                                       | N/A                                     | Show help screen (short format).                                                                                 |
| [top-syntax](#check_os_version_top-syntax)       | ${status}: ${list}                      | Top level syntax.                                                                                                |
| [ok-syntax](#check_os_version_ok-syntax)         |                                         | ok syntax.                                                                                                       |
| [empty-syntax](#check_os_version_empty-syntax)   |                                         | Empty syntax.                                                                                                    |
| [detail-syntax](#check_os_version_detail-syntax) | ${version} (${major}.${minor}.${build}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_os_version_perf-syntax)     | version                                 | Performance alias syntax.                                                                                        |



<h5 id="check_os_version_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_os_version_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `version <= 50`

<h5 id="check_os_version_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `version <= 50`

<h5 id="check_os_version_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_os_version_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_os_version_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_os_version_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_os_version_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_os_version_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_os_version_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${version} (${major}.${minor}.${build})`

<h5 id="check_os_version_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `version`


<a name="check_os_version_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                                                                                                                                                                           |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| build         | Build version number                                                                                                                                                                                                                                                  |
| count         | Number of items matching the filter. Common option for all checks.                                                                                                                                                                                                    |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                                                                                                                                                                          |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                                                                                                                                                                                |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                                                                                                                                                                              |
| list          | A list of all items which matched the filter. Common option for all checks.                                                                                                                                                                                           |
| major         | Major version number                                                                                                                                                                                                                                                  |
| minor         | Minor version number                                                                                                                                                                                                                                                  |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                                                                                                                                                                                |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                                                                                                                                                                                      |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                                                                                                                                                                            |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks.                                                                                                                                                          |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                                                                                                                                                                             |
| suite         | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| total         | Total number of items. Common option for all checks.                                                                                                                                                                                                                  |
| version       | The system version                                                                                                                                                                                                                                                    |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                                                                                                                                                                           |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                                                                                                                                                                                 |


### check_pagefile

Check the size of the system pagefile(s).

* [Samples](#check_pagefile_samples)

* [Command-line Arguments](#check_pagefile_options)
* [Filter keywords](#check_pagefile_filter_keys)


<a name="check_pagefile_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_pagefile_samples.md)_

**Default options:**

```
check_pagefile
L     client WARNING: \Device\HarddiskVolume2\pagefile.sys 24.3M (32M)
L     client  Performance data: '\??\D:\pagefile.sys'=1G;14;19;0;23 '\??\D:\pagefile.sys %'=6%;59;79;0;100 '\Device\HarddiskVolume2\pagefile.sys'=24M;19;25;0;32 '\Device\HarddiskVolume2\pagefile.sys %'=75%;59;79;0;100 'total'=1G;14;19;0;23 'total %'=6%;59;79;0;100
```

Only showing the total amount of pagefile usage::

```
check_pagefile "filter=name = 'total'" "top-syntax=${list}"
OK: total 1.66G (24G)
Performance data: 'total'=1G;14;19;0;23 'total %'=6%;59;79;0;100

```

Getting help on avalible options::

```
check_pagefile help
...
  filter=ARG           Filter which marks interesting items.
					   Interesting items are items which will be included in
					   the check.
					   They do not denote warning or critical state but they
					   are checked use this to filter out unwanted items.
						   Avalible options:
					   free          Free memory in bytes (g,m,k,b) or percentages %
					   name          The name of the page file (location)
					   size          Total size of pagefile
					   used          Used memory in bytes (g,m,k,b) or percentages %
					   count         Number of items matching the filter
					   total         Total number of items
					   ok_count      Number of items matched the ok criteria
					   warn_count    Number of items matched the warning criteria
					   crit_count    Number of items matched the critical criteria
					   problem_count Number of items matched either warning or critical criteria
...
```



<a name="check_pagefile_warn"/>
<a name="check_pagefile_crit"/>
<a name="check_pagefile_debug"/>
<a name="check_pagefile_show-all"/>
<a name="check_pagefile_escape-html"/>
<a name="check_pagefile_help"/>
<a name="check_pagefile_help-pb"/>
<a name="check_pagefile_show-default"/>
<a name="check_pagefile_help-short"/>
<a name="check_pagefile_options"/>
#### Command-line Arguments


| Option                                         | Default Value             | Description                                                                                                      |
|------------------------------------------------|---------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_pagefile_filter)               |                           | Filter which marks interesting items.                                                                            |
| [warning](#check_pagefile_warning)             | used > 60%                | Filter which marks items which generates a warning state.                                                        |
| warn                                           |                           | Short alias for warning                                                                                          |
| [critical](#check_pagefile_critical)           | used > 80%                | Filter which marks items which generates a critical state.                                                       |
| crit                                           |                           | Short alias for critical.                                                                                        |
| [ok](#check_pagefile_ok)                       |                           | Filter which marks items which generates an ok state.                                                            |
| debug                                          | N/A                       | Show debugging information in the log                                                                            |
| show-all                                       | N/A                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_pagefile_empty-state)     | ignored                   | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_pagefile_perf-config)     |                           | Performance data generation configuration                                                                        |
| escape-html                                    | N/A                       | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                           | N/A                       | Show help screen (this screen)                                                                                   |
| help-pb                                        | N/A                       | Show help screen as a protocol buffer payload                                                                    |
| show-default                                   | N/A                       | Show default values for a given command                                                                          |
| help-short                                     | N/A                       | Show help screen (short format).                                                                                 |
| [top-syntax](#check_pagefile_top-syntax)       | ${status}: ${list}        | Top level syntax.                                                                                                |
| [ok-syntax](#check_pagefile_ok-syntax)         |                           | ok syntax.                                                                                                       |
| [empty-syntax](#check_pagefile_empty-syntax)   |                           | Empty syntax.                                                                                                    |
| [detail-syntax](#check_pagefile_detail-syntax) | ${name} ${used} (${size}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_pagefile_perf-syntax)     | ${name}                   | Performance alias syntax.                                                                                        |



<h5 id="check_pagefile_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_pagefile_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `used > 60%`

<h5 id="check_pagefile_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `used > 80%`

<h5 id="check_pagefile_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_pagefile_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_pagefile_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_pagefile_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_pagefile_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_pagefile_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_pagefile_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${name} ${used} (${size})`

<h5 id="check_pagefile_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a name="check_pagefile_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                              |
| free_pct      | % free memory                                                                                                |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| name          | The name of the page file (location)                                                                         |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| size          | Total size of pagefile                                                                                       |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                              |
| used_pct      | % used memory                                                                                                |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_pdh

Check the value of a performance (PDH) counter on the local or remote system.
The counters can also be added and polled periodcally to get average values. Performance Log Users group membership is required to check performance counters.

* [Samples](#check_pdh_samples)

* [Command-line Arguments](#check_pdh_options)
* [Filter keywords](#check_pdh_filter_keys)


<a name="check_pdh_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_pdh_samples.md)_

﻿**Checking specific Counter (\System\System Up Time):**

```
check_pdh "counter=\\System\\System Up Time" "warn=value > 5" "crit=value > 9999"
\System\System Up Time = 204213
'\System\System Up Time value'=204213;5;9999
```

Using the **expand index** to check for translated counters::

```
check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999" expand-index
Everything looks good
'\Minne\Dedikationsgräns value'=-2147483648;5;9999
```

Checking **translated counters** without expanding indexes::

```
check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999"
Everything looks good
'\4\30 value'=-2147483648;5;9999
```

Checking **large values** using the type=large keyword::

```
check_pdh "counter=\\4\\30" "warn=value > 5" "crit=value > 9999" flags=nocap100 expand-index type=large
\Minne\Dedikationsgräns = 25729224704
'\Minne\Dedikationsgräns value'=25729224704;5;9999
```

Using real-time checks to check avergae values over time.

Here we configure a counter to be checked at regular intervals and the value is added to a rrd buffer.
The configuration from nsclient.ini::

```
[/settings/system/windows/counters/foo]
collection strategy=rrd
type=large
counter=\Processor(_total)\% Processor Time
```

Then we can check the value (**current snapshot**)::

```
check_pdh "counter=foo" "warn=value > 80" "crit=value > 90"
Everything looks good
'foo value'=18;80;90
```

To check averages from the same counter we need to specify the time option::

```
check_pdh "counter=foo" "warn=value > 80" "crit=value > 90" time=30s
Everything looks good
'foo value'=3;80;90
```

Checking **all instances** of a given counter::

```
    check_pdh "counter=\Processor(*)\% processortid" instances
L     client OK: \\MIME-LAPTOP\Processor(0)\% processortid = 100, \\MIME-LAPTOP\Processor(1)\% processortid = 100, \\MIME-LAPTOP\Processor(2)\% processortid = 100, \\MIME-LAPTOP\Processor(3)\% processortid = 100, \\MIME-LAPTOP\Processor(4)\% processortid = 100, \\MIME-LAPTOP\Processor(5)\% processortid = 100, \\MIME-LAPTOP\Processor(6)\% processortid = 100, \\MIME-LAPTOP\Processor(7)\% processortid = 100, \\MIME-LAPTOP\Processor(_Total)\% processortid = 100
    L     client  Performance data: '\Processor(*)\% processortid_0'=100;0;0 '\Processor(*)\% processortid_1'=100;0;0 '\Processor(*)\% processortid_2'=100;0;0 '\Processor(*)\% processortid_3'=100;0;0 '\Processor(*)\% processortid_4'=100;0;0 '\Processor(*)\% processortid_5'=100;0;0 '\Processor(*)\% processortid_6'=100;0;0 '\Processor(*)\% processortid_7'=100;0;0 '\Processor(*)\% processortid__Total'=100;0;0
```



<a name="check_pdh_warn"/>
<a name="check_pdh_crit"/>
<a name="check_pdh_debug"/>
<a name="check_pdh_show-all"/>
<a name="check_pdh_escape-html"/>
<a name="check_pdh_help"/>
<a name="check_pdh_help-pb"/>
<a name="check_pdh_show-default"/>
<a name="check_pdh_help-short"/>
<a name="check_pdh_counter"/>
<a name="check_pdh_expand-index"/>
<a name="check_pdh_instances"/>
<a name="check_pdh_reload"/>
<a name="check_pdh_averages"/>
<a name="check_pdh_time"/>
<a name="check_pdh_flags"/>
<a name="check_pdh_ignore-errors"/>
<a name="check_pdh_options"/>
#### Command-line Arguments


| Option                                    | Default Value       | Description                                                                                                                          |
|-------------------------------------------|---------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_pdh_filter)               |                     | Filter which marks interesting items.                                                                                                |
| [warning](#check_pdh_warning)             |                     | Filter which marks items which generates a warning state.                                                                            |
| warn                                      |                     | Short alias for warning                                                                                                              |
| [critical](#check_pdh_critical)           |                     | Filter which marks items which generates a critical state.                                                                           |
| crit                                      |                     | Short alias for critical.                                                                                                            |
| [ok](#check_pdh_ok)                       |                     | Filter which marks items which generates an ok state.                                                                                |
| debug                                     | N/A                 | Show debugging information in the log                                                                                                |
| show-all                                  | N/A                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                     |
| [empty-state](#check_pdh_empty-state)     | unknown             | Return status to use when nothing matched filter.                                                                                    |
| [perf-config](#check_pdh_perf-config)     |                     | Performance data generation configuration                                                                                            |
| escape-html                               | N/A                 | Escape any < and > characters to prevent HTML encoding                                                                               |
| help                                      | N/A                 | Show help screen (this screen)                                                                                                       |
| help-pb                                   | N/A                 | Show help screen as a protocol buffer payload                                                                                        |
| show-default                              | N/A                 | Show default values for a given command                                                                                              |
| help-short                                | N/A                 | Show help screen (short format).                                                                                                     |
| [top-syntax](#check_pdh_top-syntax)       | ${status}: ${list}  | Top level syntax.                                                                                                                    |
| [ok-syntax](#check_pdh_ok-syntax)         |                     | ok syntax.                                                                                                                           |
| [empty-syntax](#check_pdh_empty-syntax)   |                     | Empty syntax.                                                                                                                        |
| [detail-syntax](#check_pdh_detail-syntax) | ${alias} = ${value} | Detail level syntax.                                                                                                                 |
| [perf-syntax](#check_pdh_perf-syntax)     | ${alias}            | Performance alias syntax.                                                                                                            |
| counter                                   |                     | Performance counter to check                                                                                                         |
| expand-index                              | N/A                 | Expand indexes in counter strings                                                                                                    |
| instances                                 | N/A                 | Expand wildcards and fetch all instances                                                                                             |
| reload                                    | N/A                 | Reload counters on errors (useful to check counters which are not added at boot)                                                     |
| averages                                  | N/A                 | Check average values (ie. wait for 1 second to collecting two samples)                                                               |
| time                                      |                     | Timeframe to use for named rrd counters                                                                                              |
| flags                                     |                     | Extra flags to configure the counter (nocap100, 1000, noscale)                                                                       |
| [type](#check_pdh_type)                   | large               | Format of value (double, long, large)                                                                                                |
| ignore-errors                             | N/A                 | If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors |



<h5 id="check_pdh_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_pdh_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_pdh_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_pdh_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_pdh_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_pdh_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_pdh_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_pdh_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_pdh_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_pdh_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${alias} = ${value}`

<h5 id="check_pdh_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${alias}`

<h5 id="check_pdh_type">type:</h5>

Format of value (double, long, large)

*Default Value:* `large`


<a name="check_pdh_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| alias         | The counter alias                                                                                            |
| count         | Number of items matching the filter. Common option for all checks.                                           |
| counter       | The counter name                                                                                             |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| time          | The time for rrd checks                                                                                      |
| total         | Total number of items. Common option for all checks.                                                         |
| value         | The counter value (either float or int)                                                                      |
| value_f       | The counter value (force float value)                                                                        |
| value_i       | The counter value (force int value)                                                                          |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_process

Check state/metrics of one or more of the processes running on the computer.

* [Samples](#check_process_samples)

* [Command-line Arguments](#check_process_options)
* [Filter keywords](#check_process_filter_keys)


<a name="check_process_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_process_samples.md)_

**Default check:**

```
check_process
SetPoint.exe=hung
Performance data: 'taskhost.exe'=1;1;0 'dwm.exe'=1;1;0 'explorer.exe'=1;1;0 ... 'chrome.exe'=1;1;0 'vcpkgsrv.exe'=1;1;0 'vcpkgsrv.exe'=1;1;0 
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_process
SetPoint.exe=hung|'smss.exe state'=1;0;0 'csrss.exe state'=1;0;0...
```

Check that **specific process** are running::

```
check_process process=explorer.exe process=foo.exe
foo.exe=stopped
Performance data: 'explorer.exe'=1;1;0 'foo.exe'=0;1;0
```

Check **memory footprint** from specific processes::

```
check_process process=explorer.exe "warn=working_set > 70m"
explorer.exe=started
Performance data: 'explorer.exe ws_size'=73M;70;0
```

**Extend the syntax** to display the attributes we are interested in::

```
check_process process=explorer.exe "warn=working_set > 70m" "detail-syntax=${exe} ws:${working_set}, handles: ${handles}, user time:${user}s"
explorer.exe ws:77271040, handles: 800, user time:107s
Performance data: 'explorer.exe ws_size'=73M;70;0
```

List all processes which use **more then 200m virtual memory** Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_process --arguments "filter=virtual > 200m"
OK all processes are ok.|'csrss.exe state'=1;0;0 'svchost.exe state'=1;0;0 'AvastSvc.exe state'=1;0;0 ...
```




<a name="check_process_warn"/>
<a name="check_process_crit"/>
<a name="check_process_debug"/>
<a name="check_process_show-all"/>
<a name="check_process_escape-html"/>
<a name="check_process_help"/>
<a name="check_process_help-pb"/>
<a name="check_process_show-default"/>
<a name="check_process_help-short"/>
<a name="check_process_process"/>
<a name="check_process_scan-info"/>
<a name="check_process_scan-16bit"/>
<a name="check_process_scan-unreadable"/>
<a name="check_process_total"/>
<a name="check_process_options"/>
#### Command-line Arguments


| Option                                        | Default Value                    | Description                                                                                                      |
|-----------------------------------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_process_filter)               | state != 'unreadable'            | Filter which marks interesting items.                                                                            |
| [warning](#check_process_warning)             | state not in ('started')         | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                  | Short alias for warning                                                                                          |
| [critical](#check_process_critical)           | state = 'stopped', count = 0     | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                  | Short alias for critical.                                                                                        |
| [ok](#check_process_ok)                       |                                  | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                              | Show debugging information in the log                                                                            |
| show-all                                      | N/A                              | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_process_empty-state)     | unknown                          | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_process_perf-config)     |                                  | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                              | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                              | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                              | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                              | Show default values for a given command                                                                          |
| help-short                                    | N/A                              | Show help screen (short format).                                                                                 |
| [top-syntax](#check_process_top-syntax)       | ${status}: ${problem_list}       | Top level syntax.                                                                                                |
| [ok-syntax](#check_process_ok-syntax)         | %(status): all processes are ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_process_empty-syntax)   | UNKNOWN: No processes found      | Empty syntax.                                                                                                    |
| [detail-syntax](#check_process_detail-syntax) | ${exe}=${state}                  | Detail level syntax.                                                                                             |
| [perf-syntax](#check_process_perf-syntax)     | ${exe}                           | Performance alias syntax.                                                                                        |
| process                                       |                                  | The service to check, set this to * to check all services                                                        |
| scan-info                                     |                                  | If all process metrics should be fetched (otherwise only status is fetched)                                      |
| scan-16bit                                    |                                  | If 16bit processes should be included                                                                            |
| [delta](#check_process_delta)                 |                                  | Calculate delta over one elapsed second.                                                                         |
| scan-unreadable                               |                                  | If unreadable processes should be included (will not have information)                                           |
| total                                         | N/A                              | Include the total of all matching files                                                                          |



<h5 id="check_process_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

*Default Value:* `state != 'unreadable'`

<h5 id="check_process_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `state not in ('started')`

<h5 id="check_process_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `state = 'stopped', count = 0`

<h5 id="check_process_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_process_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_process_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_process_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_process_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): all processes are ok.`

<h5 id="check_process_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `UNKNOWN: No processes found`

<h5 id="check_process_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${exe}=${state}`

<h5 id="check_process_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${exe}`

<h5 id="check_process_delta">delta:</h5>

Calculate delta over one elapsed second.
This call will measure values and then sleep for 2 second and then measure again calculating deltas.



<a name="check_process_filter_keys"/>
#### Filter keywords


| Option           | Description                                                                                                  |
|------------------|--------------------------------------------------------------------------------------------------------------|
| command_line     | Command line of process (not always available)                                                               |
| count            | Number of items matching the filter. Common option for all checks.                                           |
| creation         | Creation time                                                                                                |
| crit_count       | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list        | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list      | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| error            | Any error messages associated with fetching info                                                             |
| exe              | The name of the executable                                                                                   |
| filename         | Name of process (with path)                                                                                  |
| gdi_handles      | Number of handles                                                                                            |
| handles          | Number of handles                                                                                            |
| hung             | Process is hung                                                                                              |
| kernel           | Kernel time in seconds                                                                                       |
| legacy_state     | Get process status (for legacy use via check_nt only)                                                        |
| list             | A list of all items which matched the filter. Common option for all checks.                                  |
| new              | Process is new (can inly be used for real-time filters)                                                      |
| ok_count         | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list          | A list of all items which matched the ok criteria. Common option for all checks.                             |
| page_fault       | Page fault count                                                                                             |
| pagefile         | Peak page file use in bytes                                                                                  |
| peak_pagefile    | Page file usage in bytes                                                                                     |
| peak_virtual     | Peak virtual size in bytes                                                                                   |
| peak_working_set | Peak working set in bytes                                                                                    |
| pid              | Process id                                                                                                   |
| problem_count    | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list     | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| started          | Process is started                                                                                           |
| state            | The current state (started, stopped hung)                                                                    |
| status           | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| stopped          | Process is stopped                                                                                           |
| time             | User-kernel time in seconds                                                                                  |
| total            | Total number of items. Common option for all checks.                                                         |
| user             | User time in seconds                                                                                         |
| user_handles     | Number of handles                                                                                            |
| virtual          | Virtual size in bytes                                                                                        |
| warn_count       | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list        | A list of all items which matched the warning criteria. Common option for all checks.                        |
| working_set      | Working set in bytes                                                                                         |


### check_service

Check the state of one or more of the computer services.

* [Samples](#check_service_samples)

* [Command-line Arguments](#check_service_options)
* [Filter keywords](#check_service_filter_keys)


<a name="check_service_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_service_samples.md)_

**Default check:**

```
check_service
OK all services are ok.
```

**Excluding services using exclude**::

```
check_service "exclude=clr_optimization_v4.0.30319_32"  "exclude=clr_optimization_v4.0.30319_64"
WARNING: gupdate=stopped (auto), Net Driver HPZ12=stopped (auto), NSClientpp=stopped (auto), nscp=stopped (auto), Pml Driver HPZ12=stopped (auto), SkypeUpdate=stopped (auto), sppsvc=stopped (auto)
```

**Show all service by changing the syntax**::

```
check_service "top-syntax=${list}" "detail-syntax=${name}:${state}"
AdobeActiveFileMonitor10.0:running, AdobeARMservice:running, AdobeFlashPlayerUpdateSvc:stopped, ..., WwanSvc:stopped
```

**Excluding services using the filter**::

```
check_service "filter=start_type = 'auto' and name not in ('Bonjour Service', 'Net Driver HPZ12')"
AdobeActiveFileMonitor10.0: running, AdobeARMservice: running, AMD External Events Utility: running,  ... wuauserv: running
```

**Exclude versus filter**::

You can use both exclude and filter to exclude services the befnefit of exclude is that it is faster with the obvious drawback that it only works on the service name.
The upside to filters are that they are richer in terms of functionality i.e. substring matching (as below).

Regular check
```
check_service
L        cli CRITICAL: CRITICAL: nfoo=stopped (auto), nscp=stopped (auto), nscp2=stopped (auto), ...
```

Excluding nfoo service with exclude:
```
check_service exclude=nfoo
L        cli CRITICAL: CRITICAL: nscp=stopped (auto), nscp2=stopped (auto), ...
```

Excluding nscp2 with substring like mathcing filter:
```
check_service exclude=nfoo "filter=name not like 'nscp'"
L        cli CRITICAL: CRITICAL: ...
```


Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_service
WARNING: DPS=stopped (auto), MSDTC=stopped (auto), sppsvc=stopped (auto), UALSVC=stopped (auto)
```

**Check that a service is not started**::

```
check_service service=nscp "crit=state = 'started'" warn=none
```



<a name="check_service_warn"/>
<a name="check_service_crit"/>
<a name="check_service_debug"/>
<a name="check_service_show-all"/>
<a name="check_service_escape-html"/>
<a name="check_service_help"/>
<a name="check_service_help-pb"/>
<a name="check_service_show-default"/>
<a name="check_service_help-short"/>
<a name="check_service_computer"/>
<a name="check_service_service"/>
<a name="check_service_exclude"/>
<a name="check_service_only-essential"/>
<a name="check_service_only-ignored"/>
<a name="check_service_only-role"/>
<a name="check_service_only-supporting"/>
<a name="check_service_only-system"/>
<a name="check_service_only-user"/>
<a name="check_service_options"/>
#### Command-line Arguments


| Option                                        | Default Value                                   | Description                                                                                                                                           |
|-----------------------------------------------|-------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_service_filter)               |                                                 | Filter which marks interesting items.                                                                                                                 |
| [warning](#check_service_warning)             | not state_is_perfect()                          | Filter which marks items which generates a warning state.                                                                                             |
| warn                                          |                                                 | Short alias for warning                                                                                                                               |
| [critical](#check_service_critical)           | not state_is_ok()                               | Filter which marks items which generates a critical state.                                                                                            |
| crit                                          |                                                 | Short alias for critical.                                                                                                                             |
| [ok](#check_service_ok)                       |                                                 | Filter which marks items which generates an ok state.                                                                                                 |
| debug                                         | N/A                                             | Show debugging information in the log                                                                                                                 |
| show-all                                      | N/A                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                                      |
| [empty-state](#check_service_empty-state)     | unknown                                         | Return status to use when nothing matched filter.                                                                                                     |
| [perf-config](#check_service_perf-config)     |                                                 | Performance data generation configuration                                                                                                             |
| escape-html                                   | N/A                                             | Escape any < and > characters to prevent HTML encoding                                                                                                |
| help                                          | N/A                                             | Show help screen (this screen)                                                                                                                        |
| help-pb                                       | N/A                                             | Show help screen as a protocol buffer payload                                                                                                         |
| show-default                                  | N/A                                             | Show default values for a given command                                                                                                               |
| help-short                                    | N/A                                             | Show help screen (short format).                                                                                                                      |
| [top-syntax](#check_service_top-syntax)       | ${status}: ${crit_list}, delayed (${warn_list}) | Top level syntax.                                                                                                                                     |
| [ok-syntax](#check_service_ok-syntax)         | %(status): All %(count) service(s) are ok.      | ok syntax.                                                                                                                                            |
| [empty-syntax](#check_service_empty-syntax)   | %(status): No services found                    | Empty syntax.                                                                                                                                         |
| [detail-syntax](#check_service_detail-syntax) | ${name}=${state} (${start_type})                | Detail level syntax.                                                                                                                                  |
| [perf-syntax](#check_service_perf-syntax)     | ${name}                                         | Performance alias syntax.                                                                                                                             |
| computer                                      |                                                 | The name of the remote computer to check                                                                                                              |
| service                                       |                                                 | The service to check, set this to * to check all services                                                                                             |
| exclude                                       |                                                 | A list of services to ignore (mainly usefull in combination with service=*)                                                                           |
| [type](#check_service_type)                   | service                                         | The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process |
| [state](#check_service_state)                 | all                                             | The types of services to enumerate available states are active, inactive or all                                                                       |
| only-essential                                | N/A                                             | Set filter to classification = 'essential'                                                                                                            |
| only-ignored                                  | N/A                                             | Set filter to classification = 'ignored'                                                                                                              |
| only-role                                     | N/A                                             | Set filter to classification = 'role'                                                                                                                 |
| only-supporting                               | N/A                                             | Set filter to classification = 'supporting'                                                                                                           |
| only-system                                   | N/A                                             | Set filter to classification = 'system'                                                                                                               |
| only-user                                     | N/A                                             | Set filter to classification = 'user'                                                                                                                 |



<h5 id="check_service_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_service_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `not state_is_perfect()`

<h5 id="check_service_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `not state_is_ok()`

<h5 id="check_service_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_service_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_service_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_service_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${crit_list}, delayed (${warn_list})`

<h5 id="check_service_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All %(count) service(s) are ok.`

<h5 id="check_service_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `%(status): No services found`

<h5 id="check_service_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${name}=${state} (${start_type})`

<h5 id="check_service_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`

<h5 id="check_service_type">type:</h5>

The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process

*Default Value:* `service`

<h5 id="check_service_state">state:</h5>

The types of services to enumerate available states are active, inactive or all

*Default Value:* `all`


<a name="check_service_filter_keys"/>
#### Filter keywords


| Option             | Description                                                                                                  |
|--------------------|--------------------------------------------------------------------------------------------------------------|
| classification     | Get classification                                                                                           |
| count              | Number of items matching the filter. Common option for all checks.                                           |
| crit_count         | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list          | A list of all items which matched the critical criteria. Common option for all checks.                       |
| delayed            | If the service is delayed                                                                                    |
| desc               | Service description                                                                                          |
| detail_list        | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| is_trigger         | If the service is has associated triggers                                                                    |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                      |
| list               | A list of all items which matched the filter. Common option for all checks.                                  |
| name               | Service name                                                                                                 |
| ok_count           | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list            | A list of all items which matched the ok criteria. Common option for all checks.                             |
| pid                | Process id                                                                                                   |
| problem_count      | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list       | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| start_type         | The configured start type ()                                                                                 |
| state              | The current state ()                                                                                         |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)    |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                              |
| status             | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total              | Total number of items. Common option for all checks.                                                         |
| triggers           | The number of associated triggers for this service                                                           |
| warn_count         | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list          | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_uptime

Check time since last server re-boot.

* [Samples](#check_uptime_samples)

* [Command-line Arguments](#check_uptime_options)
* [Filter keywords](#check_uptime_filter_keys)


<a name="check_uptime_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_uptime_samples.md)_

**Default check:**

```
check_uptime
uptime: -9:02, boot: 2013-aug-18 08:29:13
'uptime uptime'=1376814553s;1376760683;1376803883
```

Adding **warning and critical thresholds**::

```
check_uptime "warn=uptime < -2d" "crit=uptime < -1d"
...
```

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_uptime
uptime: -0:3, boot: 2013-sep-08 18:41:06 (UCT)|'uptime'=1378665666;1378579481;1378622681
```




<a name="check_uptime_warn"/>
<a name="check_uptime_crit"/>
<a name="check_uptime_debug"/>
<a name="check_uptime_show-all"/>
<a name="check_uptime_escape-html"/>
<a name="check_uptime_help"/>
<a name="check_uptime_help-pb"/>
<a name="check_uptime_show-default"/>
<a name="check_uptime_help-short"/>
<a name="check_uptime_options"/>
#### Command-line Arguments


| Option                                       | Default Value                           | Description                                                                                                      |
|----------------------------------------------|-----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_uptime_filter)               |                                         | Filter which marks interesting items.                                                                            |
| [warning](#check_uptime_warning)             | uptime < 2d                             | Filter which marks items which generates a warning state.                                                        |
| warn                                         |                                         | Short alias for warning                                                                                          |
| [critical](#check_uptime_critical)           | uptime < 1d                             | Filter which marks items which generates a critical state.                                                       |
| crit                                         |                                         | Short alias for critical.                                                                                        |
| [ok](#check_uptime_ok)                       |                                         | Filter which marks items which generates an ok state.                                                            |
| debug                                        | N/A                                     | Show debugging information in the log                                                                            |
| show-all                                     | N/A                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_uptime_empty-state)     | ignored                                 | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_uptime_perf-config)     |                                         | Performance data generation configuration                                                                        |
| escape-html                                  | N/A                                     | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                         | N/A                                     | Show help screen (this screen)                                                                                   |
| help-pb                                      | N/A                                     | Show help screen as a protocol buffer payload                                                                    |
| show-default                                 | N/A                                     | Show default values for a given command                                                                          |
| help-short                                   | N/A                                     | Show help screen (short format).                                                                                 |
| [top-syntax](#check_uptime_top-syntax)       | ${status}: ${list}                      | Top level syntax.                                                                                                |
| [ok-syntax](#check_uptime_ok-syntax)         |                                         | ok syntax.                                                                                                       |
| [empty-syntax](#check_uptime_empty-syntax)   |                                         | Empty syntax.                                                                                                    |
| [detail-syntax](#check_uptime_detail-syntax) | uptime: ${uptime}h, boot: ${boot} (UTC) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_uptime_perf-syntax)     | uptime                                  | Performance alias syntax.                                                                                        |



<h5 id="check_uptime_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_uptime_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `uptime < 2d`

<h5 id="check_uptime_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `uptime < 1d`

<h5 id="check_uptime_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.



<h5 id="check_uptime_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_uptime_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_uptime_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_uptime_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_uptime_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_uptime_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).

*Default Value:* `uptime: ${uptime}h, boot: ${boot} (UTC)`

<h5 id="check_uptime_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `uptime`


<a name="check_uptime_filter_keys"/>
#### Filter keywords


| Option        | Description                                                                                                  |
|---------------|--------------------------------------------------------------------------------------------------------------|
| boot          | System boot time                                                                                             |
| count         | Number of items matching the filter. Common option for all checks.                                           |
| crit_count    | Number of items matched the critical criteria. Common option for all checks.                                 |
| crit_list     | A list of all items which matched the critical criteria. Common option for all checks.                       |
| detail_list   | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| list          | A list of all items which matched the filter. Common option for all checks.                                  |
| ok_count      | Number of items matched the ok criteria. Common option for all checks.                                       |
| ok_list       | A list of all items which matched the ok criteria. Common option for all checks.                             |
| problem_count | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| total         | Total number of items. Common option for all checks.                                                         |
| uptime        | Time since last boot                                                                                         |
| warn_count    | Number of items matched the warning criteria. Common option for all checks.                                  |
| warn_list     | A list of all items which matched the warning criteria. Common option for all checks.                        |


### checkcounter

Legacy version of check_pdh


* [Command-line Arguments](#checkcounter_options)





<a name="checkcounter_help"/>
<a name="checkcounter_help-pb"/>
<a name="checkcounter_show-default"/>
<a name="checkcounter_help-short"/>
<a name="checkcounter_Counter"/>
<a name="checkcounter_MaxWarn"/>
<a name="checkcounter_MaxCrit"/>
<a name="checkcounter_MinWarn"/>
<a name="checkcounter_MinCrit"/>
<a name="checkcounter_options"/>
#### Command-line Arguments


| Option                           | Default Value | Description                                                                                           |
|----------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| help                             | N/A           | Show help screen (this screen)                                                                        |
| help-pb                          | N/A           | Show help screen as a protocol buffer payload                                                         |
| show-default                     | N/A           | Show default values for a given command                                                               |
| help-short                       | N/A           | Show help screen (short format).                                                                      |
| Counter                          |               | The time to check                                                                                     |
| [ShowAll](#checkcounter_ShowAll) | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| MaxWarn                          |               | Maximum value before a warning is returned.                                                           |
| MaxCrit                          |               | Maximum value before a critical is returned.                                                          |
| MinWarn                          |               | Minimum value before a warning is returned.                                                           |
| MinCrit                          |               | Minimum value before a critical is returned.                                                          |



<h5 id="checkcounter_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`


### checkcpu

Legacy version of check_cpu


* [Command-line Arguments](#checkcpu_options)





<a name="checkcpu_help"/>
<a name="checkcpu_help-pb"/>
<a name="checkcpu_show-default"/>
<a name="checkcpu_help-short"/>
<a name="checkcpu_time"/>
<a name="checkcpu_MaxWarn"/>
<a name="checkcpu_MaxCrit"/>
<a name="checkcpu_MinWarn"/>
<a name="checkcpu_MinCrit"/>
<a name="checkcpu_warn"/>
<a name="checkcpu_crit"/>
<a name="checkcpu_options"/>
#### Command-line Arguments


| Option                       | Default Value | Description                                                                                           |
|------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| help                         | N/A           | Show help screen (this screen)                                                                        |
| help-pb                      | N/A           | Show help screen as a protocol buffer payload                                                         |
| show-default                 | N/A           | Show default values for a given command                                                               |
| help-short                   | N/A           | Show help screen (short format).                                                                      |
| time                         |               | The time to check                                                                                     |
| [ShowAll](#checkcpu_ShowAll) | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| MaxWarn                      |               | Maximum value before a warning is returned.                                                           |
| MaxCrit                      |               | Maximum value before a critical is returned.                                                          |
| MinWarn                      |               | Minimum value before a warning is returned.                                                           |
| MinCrit                      |               | Minimum value before a critical is returned.                                                          |
| warn                         |               | Maximum value before a warning is returned.                                                           |
| crit                         |               | Maximum value before a critical is returned.                                                          |



<h5 id="checkcpu_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`


### checkmem

Legacy version of check_mem


* [Command-line Arguments](#checkmem_options)





<a name="checkmem_help"/>
<a name="checkmem_help-pb"/>
<a name="checkmem_show-default"/>
<a name="checkmem_help-short"/>
<a name="checkmem_type"/>
<a name="checkmem_MaxWarn"/>
<a name="checkmem_MaxCrit"/>
<a name="checkmem_MinWarn"/>
<a name="checkmem_MinCrit"/>
<a name="checkmem_warn"/>
<a name="checkmem_crit"/>
<a name="checkmem_options"/>
#### Command-line Arguments


| Option                       | Default Value | Description                                                                                           |
|------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| help                         | N/A           | Show help screen (this screen)                                                                        |
| help-pb                      | N/A           | Show help screen as a protocol buffer payload                                                         |
| show-default                 | N/A           | Show default values for a given command                                                               |
| help-short                   | N/A           | Show help screen (short format).                                                                      |
| type                         |               | The types to check                                                                                    |
| [ShowAll](#checkmem_ShowAll) | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| MaxWarn                      |               | Maximum value before a warning is returned.                                                           |
| MaxCrit                      |               | Maximum value before a critical is returned.                                                          |
| MinWarn                      |               | Minimum value before a warning is returned.                                                           |
| MinCrit                      |               | Minimum value before a critical is returned.                                                          |
| warn                         |               | Maximum value before a warning is returned.                                                           |
| crit                         |               | Maximum value before a critical is returned.                                                          |



<h5 id="checkmem_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`


### checkprocstate

Legacy version of check_process


* [Command-line Arguments](#checkprocstate_options)





<a name="checkprocstate_help"/>
<a name="checkprocstate_help-pb"/>
<a name="checkprocstate_show-default"/>
<a name="checkprocstate_help-short"/>
<a name="checkprocstate_MaxWarnCount"/>
<a name="checkprocstate_MaxCritCount"/>
<a name="checkprocstate_MinWarnCount"/>
<a name="checkprocstate_MinCritCount"/>
<a name="checkprocstate_options"/>
#### Command-line Arguments


| Option                             | Default Value | Description                                                                                           |
|------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| help                               | N/A           | Show help screen (this screen)                                                                        |
| help-pb                            | N/A           | Show help screen as a protocol buffer payload                                                         |
| show-default                       | N/A           | Show default values for a given command                                                               |
| help-short                         | N/A           | Show help screen (short format).                                                                      |
| [ShowAll](#checkprocstate_ShowAll) | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| MaxWarnCount                       |               | Maximum value before a warning is returned.                                                           |
| MaxCritCount                       |               | Maximum value before a critical is returned.                                                          |
| MinWarnCount                       |               | Minimum value before a warning is returned.                                                           |
| MinCritCount                       |               | Minimum value before a critical is returned.                                                          |



<h5 id="checkprocstate_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`


### checkservicestate

Legacy version of check_service


* [Command-line Arguments](#checkservicestate_options)





<a name="checkservicestate_help"/>
<a name="checkservicestate_help-pb"/>
<a name="checkservicestate_show-default"/>
<a name="checkservicestate_help-short"/>
<a name="checkservicestate_exclude"/>
<a name="checkservicestate_options"/>
#### Command-line Arguments


| Option                                  | Default Value | Description                                                                                           |
|-----------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| help                                    | N/A           | Show help screen (this screen)                                                                        |
| help-pb                                 | N/A           | Show help screen as a protocol buffer payload                                                         |
| show-default                            | N/A           | Show default values for a given command                                                               |
| help-short                              | N/A           | Show help screen (short format).                                                                      |
| [CheckAll](#checkservicestate_CheckAll) | true          | Check all services.                                                                                   |
| exclude                                 |               | Exclude services                                                                                      |
| [ShowAll](#checkservicestate_ShowAll)   | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |



<h5 id="checkservicestate_CheckAll">CheckAll:</h5>

Check all services.

*Default Value:* `true`

<h5 id="checkservicestate_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`


### checkuptime

Legacy version of check_uptime


* [Command-line Arguments](#checkuptime_options)





<a name="checkuptime_help"/>
<a name="checkuptime_help-pb"/>
<a name="checkuptime_show-default"/>
<a name="checkuptime_help-short"/>
<a name="checkuptime_MaxWarn"/>
<a name="checkuptime_MaxCrit"/>
<a name="checkuptime_MinWarn"/>
<a name="checkuptime_MinCrit"/>
<a name="checkuptime_warn"/>
<a name="checkuptime_crit"/>
<a name="checkuptime_options"/>
#### Command-line Arguments


| Option                          | Default Value | Description                                                                                           |
|---------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| help                            | N/A           | Show help screen (this screen)                                                                        |
| help-pb                         | N/A           | Show help screen as a protocol buffer payload                                                         |
| show-default                    | N/A           | Show default values for a given command                                                               |
| help-short                      | N/A           | Show help screen (short format).                                                                      |
| [ShowAll](#checkuptime_ShowAll) | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| MaxWarn                         |               | Maximum value before a warning is returned.                                                           |
| MaxCrit                         |               | Maximum value before a critical is returned.                                                          |
| MinWarn                         |               | Minimum value before a warning is returned.                                                           |
| MinCrit                         |               | Minimum value before a critical is returned.                                                          |
| warn                            |               | Maximum value before a warning is returned.                                                           |
| crit                            |               | Maximum value before a critical is returned.                                                          |



<h5 id="checkuptime_ShowAll">ShowAll:</h5>

Configures display format (if set shows all items not only failures, if set to long shows all cores).

*Default Value:* `short`




## Configuration



| Path / Section                                                          | Description              |
|-------------------------------------------------------------------------|--------------------------|
| [/settings/system/windows](#windows-system)                             | Windows system           |
| [/settings/system/windows/counters](#pdh-counters)                      | PDH Counters             |
| [/settings/system/windows/real-time/checks](#legacy-generic-filters)    | Legacy generic filters   |
| [/settings/system/windows/real-time/cpu](#realtime-cpu-filters)         | Realtime cpu filters     |
| [/settings/system/windows/real-time/memory](#realtime-memory-filters)   | Realtime memory filters  |
| [/settings/system/windows/real-time/process](#realtime-process-filters) | Realtime process filters |



### Windows system <a id="/settings/system/windows"/>

Section for system checks and system settings




| Key                                           | Default Value | Description              |
|-----------------------------------------------|---------------|--------------------------|
| [default buffer length](#default-buffer-time) | 1h            | Default buffer time      |
| [disable](#disable-automatic-checks)          |               | Disable automatic checks |
| [subsystem](#pdh-subsystem)                   | default       | PDH subsystem            |



```ini
# Section for system checks and system settings
[/settings/system/windows]
default buffer length=1h
subsystem=default

```





#### Default buffer time <a id="/settings/system/windows/default buffer length"></a>

Used to define the default size of range buffer checks (ie. CPU).





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | default buffer length                                 |
| Default value: | `1h`                                                  |
| Used by:       | CheckSystem                                           |


**Sample:**

```
[/settings/system/windows]
# Default buffer time
default buffer length=1h
```



#### Disable automatic checks <a id="/settings/system/windows/disable"></a>

A comma separated list of checks to disable in the collector: cpu,handles,network,metrics,pdh. Please note disabling these will mean part of NSClient++ will no longer function as expected.






| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | disable                                               |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | _N/A_                                                 |
| Used by:       | CheckSystem                                           |


**Sample:**

```
[/settings/system/windows]
# Disable automatic checks
disable=
```



#### PDH subsystem <a id="/settings/system/windows/subsystem"></a>

Set which pdh subsystem to use.
Currently default and thread-safe are supported where thread-safe is slower but required if you have some problematic counters.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | subsystem                                             |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | `default`                                             |
| Used by:       | CheckSystem                                           |


**Sample:**

```
[/settings/system/windows]
# PDH subsystem
subsystem=default
```


### PDH Counters <a id="/settings/system/windows/counters"/>

Add counters to check


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description         |
|---------------------|---------------|---------------------|
| alias               |               | ALIAS               |
| buffer size         |               | BUFFER SIZE         |
| collection strategy |               | COLLECTION STRATEGY |
| counter             |               | COUNTER             |
| flags               |               | FLAGS               |
| instances           |               | Interpret instances |
| is template         | false         | IS TEMPLATE         |
| parent              | default       | PARENT              |
| type                |               | COUNTER TYPE        |


**Sample:**

```ini
# An example of a PDH Counters section
[/settings/system/windows/counters/sample]
#alias=...
#buffer size=...
#collection strategy=...
#counter=...
#flags=...
#instances=...
is template=false
parent=default
#type=...

```



**Known instances:**

*  disk_queue_length







### Legacy generic filters <a id="/settings/system/windows/real-time/checks"/>

A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key           | Default Value             | Description     |
|---------------|---------------------------|-----------------|
| check         | cpu                       | TYPE OF CHECK   |
| command       |                           | COMMAND NAME    |
| critical      |                           | CRITICAL FILTER |
| debug         |                           | DEBUG           |
| destination   |                           | DESTINATION     |
| detail syntax |                           | SYNTAX          |
| empty message | eventlog found no records | EMPTY MESSAGE   |
| escape html   |                           | ESCAPE HTML     |
| filter        |                           | FILTER          |
| maximum age   | 5m                        | MAGIMUM AGE     |
| ok            |                           | OK FILTER       |
| ok syntax     |                           | SYNTAX          |
| perf config   |                           | PERF CONFIG     |
| severity      |                           | SEVERITY        |
| silent period | false                     | Silent period   |
| source id     |                           | SOURCE ID       |
| target        |                           | DESTINATION     |
| target id     |                           | TARGET ID       |
| time          |                           | TIME            |
| times         |                           | FILES           |
| top syntax    |                           | SYNTAX          |
| warning       |                           | WARNING FILTER  |


**Sample:**

```ini
# An example of a Legacy generic filters section
[/settings/system/windows/real-time/checks/sample]
check=cpu
#command=...
#critical=...
#debug=...
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#time=...
#times=...
#top syntax=...
#warning=...

```






### Realtime cpu filters <a id="/settings/system/windows/real-time/cpu"/>

A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key           | Default Value             | Description     |
|---------------|---------------------------|-----------------|
| command       |                           | COMMAND NAME    |
| critical      |                           | CRITICAL FILTER |
| debug         |                           | DEBUG           |
| destination   |                           | DESTINATION     |
| detail syntax |                           | SYNTAX          |
| empty message | eventlog found no records | EMPTY MESSAGE   |
| escape html   |                           | ESCAPE HTML     |
| filter        |                           | FILTER          |
| maximum age   | 5m                        | MAGIMUM AGE     |
| ok            |                           | OK FILTER       |
| ok syntax     |                           | SYNTAX          |
| perf config   |                           | PERF CONFIG     |
| severity      |                           | SEVERITY        |
| silent period | false                     | Silent period   |
| source id     |                           | SOURCE ID       |
| target        |                           | DESTINATION     |
| target id     |                           | TARGET ID       |
| time          |                           | TIME            |
| top syntax    |                           | SYNTAX          |
| warning       |                           | WARNING FILTER  |


**Sample:**

```ini
# An example of a Realtime cpu filters section
[/settings/system/windows/real-time/cpu/sample]
#command=...
#critical=...
#debug=...
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#time=...
#top syntax=...
#warning=...

```






### Realtime memory filters <a id="/settings/system/windows/real-time/memory"/>

A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key           | Default Value             | Description     |
|---------------|---------------------------|-----------------|
| command       |                           | COMMAND NAME    |
| critical      |                           | CRITICAL FILTER |
| debug         |                           | DEBUG           |
| destination   |                           | DESTINATION     |
| detail syntax |                           | SYNTAX          |
| empty message | eventlog found no records | EMPTY MESSAGE   |
| escape html   |                           | ESCAPE HTML     |
| filter        |                           | FILTER          |
| maximum age   | 5m                        | MAGIMUM AGE     |
| ok            |                           | OK FILTER       |
| ok syntax     |                           | SYNTAX          |
| perf config   |                           | PERF CONFIG     |
| severity      |                           | SEVERITY        |
| silent period | false                     | Silent period   |
| source id     |                           | SOURCE ID       |
| target        |                           | DESTINATION     |
| target id     |                           | TARGET ID       |
| top syntax    |                           | SYNTAX          |
| type          |                           | MEMORY TYPE     |
| warning       |                           | WARNING FILTER  |


**Sample:**

```ini
# An example of a Realtime memory filters section
[/settings/system/windows/real-time/memory/sample]
#command=...
#critical=...
#debug=...
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#top syntax=...
#type=...
#warning=...

```






### Realtime process filters <a id="/settings/system/windows/real-time/process"/>

A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key           | Default Value             | Description     |
|---------------|---------------------------|-----------------|
| command       |                           | COMMAND NAME    |
| critical      |                           | CRITICAL FILTER |
| debug         |                           | DEBUG           |
| destination   |                           | DESTINATION     |
| detail syntax |                           | SYNTAX          |
| empty message | eventlog found no records | EMPTY MESSAGE   |
| escape html   |                           | ESCAPE HTML     |
| filter        |                           | FILTER          |
| maximum age   | 5m                        | MAGIMUM AGE     |
| ok            |                           | OK FILTER       |
| ok syntax     |                           | SYNTAX          |
| perf config   |                           | PERF CONFIG     |
| process       |                           | PROCESS         |
| severity      |                           | SEVERITY        |
| silent period | false                     | Silent period   |
| source id     |                           | SOURCE ID       |
| target        |                           | DESTINATION     |
| target id     |                           | TARGET ID       |
| top syntax    |                           | SYNTAX          |
| warning       |                           | WARNING FILTER  |


**Sample:**

```ini
# An example of a Realtime process filters section
[/settings/system/windows/real-time/process/sample]
#command=...
#critical=...
#debug=...
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#process=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#top syntax=...
#warning=...

```






