# CheckSystem

Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.



## List of commands

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


## List of command aliases

A list of all short hand aliases for queries (check commands)


| Command       | Description                   |
|---------------|-------------------------------|
| check_counter | Alias for: :query:`check_pdh` |


## List of Configuration


### Common Keys

| Path / Section                                                                          | Key                                                                                   | Description         |
|-----------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------|---------------------|
| [/settings/system/windows](#/settings/system/windows)                                   | [default buffer length](#/settings/system/windows_default buffer length)              | DEFAULT LENGTH      |
| [/settings/system/windows/counters](#/settings/system/windows/counters)                 | [disk_queue_length](#/settings/system/windows/counters_disk_queue_length)             | disk_queue_length   |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [buffer size](#/settings/system/windows/counters/default_buffer size)                 | BUFFER SIZE         |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [collection strategy](#/settings/system/windows/counters/default_collection strategy) | COLLECTION STRATEGY |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [counter](#/settings/system/windows/counters/default_counter)                         | COUNTER             |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [flags](#/settings/system/windows/counters/default_flags)                             | FLAGS               |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [instances](#/settings/system/windows/counters/default_instances)                     | Interpret instances |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [type](#/settings/system/windows/counters/default_type)                               | COUNTER TYPE        |

### Advanced keys

| Path / Section                                                                          | Key                                                                   | Description    |
|-----------------------------------------------------------------------------------------|-----------------------------------------------------------------------|----------------|
| [/settings/system/windows](#/settings/system/windows)                                   | [disable](#/settings/system/windows_disable)                          | DISABLE CHECKS |
| [/settings/system/windows](#/settings/system/windows)                                   | [subsystem](#/settings/system/windows_subsystem)                      | PDH SUBSYSTEM  |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [alias](#/settings/system/windows/counters/default_alias)             | ALIAS          |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [is template](#/settings/system/windows/counters/default_is template) | IS TEMPLATE    |
| [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) | [parent](#/settings/system/windows/counters/default_parent)           | PARENT         |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample)   | [alias](#/settings/system/windows/counters/sample_alias)              | ALIAS          |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample)   | [is template](#/settings/system/windows/counters/sample_is template)  | IS TEMPLATE    |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample)   | [parent](#/settings/system/windows/counters/sample_parent)            | PARENT         |

### Sample keys

| Path / Section                                                                        | Key                                                                                  | Description         |
|---------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------|---------------------|
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) | [buffer size](#/settings/system/windows/counters/sample_buffer size)                 | BUFFER SIZE         |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) | [collection strategy](#/settings/system/windows/counters/sample_collection strategy) | COLLECTION STRATEGY |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) | [counter](#/settings/system/windows/counters/sample_counter)                         | COUNTER             |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) | [flags](#/settings/system/windows/counters/sample_flags)                             | FLAGS               |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) | [instances](#/settings/system/windows/counters/sample_instances)                     | Interpret instances |
| [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) | [type](#/settings/system/windows/counters/sample_type)                               | COUNTER TYPE        |



# Queries

A quick reference for all available queries (check commands) in the CheckSystem module.

## check_cpu

Check that the load of the CPU(s) are within bounds.

### Usage

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

### Usage


| Option                                    | Default Value              | Description                                                                                                      |
|-------------------------------------------|----------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_cpu_filter)               | core = 'total'             | Filter which marks interesting items.                                                                            |
| [warning](#check_cpu_warning)             | load > 80                  | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_cpu_warn)                   |                            | Short alias for warning                                                                                          |
| [critical](#check_cpu_critical)           | load > 90                  | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_cpu_crit)                   |                            | Short alias for critical.                                                                                        |
| [ok](#check_cpu_ok)                       |                            | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_cpu_debug)                 | N/A                        | Show debugging information in the log                                                                            |
| [show-all](#check_cpu_show-all)           | N/A                        | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_cpu_empty-state)     | ignored                    | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_cpu_perf-config)     |                            | Performance data generation configuration                                                                        |
| [escape-html](#check_cpu_escape-html)     | N/A                        | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_cpu_help)                   | N/A                        | Show help screen (this screen)                                                                                   |
| [help-pb](#check_cpu_help-pb)             | N/A                        | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_cpu_show-default)   | N/A                        | Show default values for a given command                                                                          |
| [help-short](#check_cpu_help-short)       | N/A                        | Show help screen (short format).                                                                                 |
| [top-syntax](#check_cpu_top-syntax)       | ${status}: ${problem_list} | Top level syntax.                                                                                                |
| [ok-syntax](#check_cpu_ok-syntax)         | %(status): CPU load is ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_cpu_empty-syntax)   |                            | Empty syntax.                                                                                                    |
| [detail-syntax](#check_cpu_detail-syntax) | ${time}: ${load}%          | Detail level syntax.                                                                                             |
| [perf-syntax](#check_cpu_perf-syntax)     | ${core} ${time}            | Performance alias syntax.                                                                                        |
| [time](#check_cpu_time)                   |                            | The time to check                                                                                                |


<a name="check_cpu_filter"/>
### filter


**Deafult Value:** core = 'total'

**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| core          | The core to check (total or core ##)                                                                          |
| core_id       | The core to check (total or core_##)                                                                          |
| idle          | The current idle load for a given core                                                                        |
| kernel        | The current kernel load for a given core                                                                      |
| load          | The current load for a given core                                                                             |
| time          | The time frame to check                                                                                       |







<a name="check_cpu_warning"/>
### warning


**Deafult Value:** load > 80

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| core          | The core to check (total or core ##)                                                                          |
| core_id       | The core to check (total or core_##)                                                                          |
| idle          | The current idle load for a given core                                                                        |
| kernel        | The current kernel load for a given core                                                                      |
| load          | The current load for a given core                                                                             |
| time          | The time frame to check                                                                                       |







<a name="check_cpu_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_cpu_critical"/>
### critical


**Deafult Value:** load > 90

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| core          | The core to check (total or core ##)                                                                          |
| core_id       | The core to check (total or core_##)                                                                          |
| idle          | The current idle load for a given core                                                                        |
| kernel        | The current kernel load for a given core                                                                      |
| load          | The current load for a given core                                                                             |
| time          | The time frame to check                                                                                       |







<a name="check_cpu_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_cpu_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| core          | The core to check (total or core ##)                                                                          |
| core_id       | The core to check (total or core_##)                                                                          |
| idle          | The current idle load for a given core                                                                        |
| kernel        | The current kernel load for a given core                                                                      |
| load          | The current load for a given core                                                                             |
| time          | The time frame to check                                                                                       |







<a name="check_cpu_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_cpu_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_cpu_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_cpu_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_cpu_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_cpu_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_cpu_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_cpu_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_cpu_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_cpu_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${problem_list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_cpu_ok-syntax"/>
### ok-syntax


**Deafult Value:** %(status): CPU load is ok.

**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_cpu_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_cpu_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${time}: ${load}%

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key     | Value                                    |
|---------|------------------------------------------|
| core    | The core to check (total or core ##)     |
| core_id | The core to check (total or core_##)     |
| idle    | The current idle load for a given core   |
| kernel  | The current kernel load for a given core |
| load    | The current load for a given core        |
| time    | The time frame to check                  |






<a name="check_cpu_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${core} ${time}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key     | Value                                    |
|---------|------------------------------------------|
| core    | The core to check (total or core ##)     |
| core_id | The core to check (total or core_##)     |
| idle    | The current idle load for a given core   |
| kernel  | The current kernel load for a given core |
| load    | The current load for a given core        |
| time    | The time frame to check                  |






<a name="check_cpu_time"/>
### time



**Description:**
The time to check

## check_memory

Check free/used memory on the system.

### Usage

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

### Usage


| Option                                       | Default Value      | Description                                                                                                      |
|----------------------------------------------|--------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_memory_filter)               |                    | Filter which marks interesting items.                                                                            |
| [warning](#check_memory_warning)             | used > 80%         | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_memory_warn)                   |                    | Short alias for warning                                                                                          |
| [critical](#check_memory_critical)           | used > 90%         | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_memory_crit)                   |                    | Short alias for critical.                                                                                        |
| [ok](#check_memory_ok)                       |                    | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_memory_debug)                 | N/A                | Show debugging information in the log                                                                            |
| [show-all](#check_memory_show-all)           | N/A                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_memory_empty-state)     | ignored            | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_memory_perf-config)     |                    | Performance data generation configuration                                                                        |
| [escape-html](#check_memory_escape-html)     | N/A                | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_memory_help)                   | N/A                | Show help screen (this screen)                                                                                   |
| [help-pb](#check_memory_help-pb)             | N/A                | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_memory_show-default)   | N/A                | Show default values for a given command                                                                          |
| [help-short](#check_memory_help-short)       | N/A                | Show help screen (short format).                                                                                 |
| [top-syntax](#check_memory_top-syntax)       | ${status}: ${list} | Top level syntax.                                                                                                |
| [ok-syntax](#check_memory_ok-syntax)         |                    | ok syntax.                                                                                                       |
| [empty-syntax](#check_memory_empty-syntax)   |                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_memory_detail-syntax) | ${type} = ${used}  | Detail level syntax.                                                                                             |
| [perf-syntax](#check_memory_perf-syntax)     | ${type}            | Performance alias syntax.                                                                                        |
| [type](#check_memory_type)                   |                    | The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)               |


<a name="check_memory_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| size          | Total size of memory                                                                                          |
| type          | The type of memory to check                                                                                   |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_memory_warning"/>
### warning


**Deafult Value:** used > 80%

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| size          | Total size of memory                                                                                          |
| type          | The type of memory to check                                                                                   |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_memory_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_memory_critical"/>
### critical


**Deafult Value:** used > 90%

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| size          | Total size of memory                                                                                          |
| type          | The type of memory to check                                                                                   |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_memory_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_memory_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| size          | Total size of memory                                                                                          |
| type          | The type of memory to check                                                                                   |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_memory_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_memory_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_memory_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_memory_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_memory_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_memory_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_memory_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_memory_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_memory_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_memory_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_memory_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_memory_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_memory_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${type} = ${used}

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key      | Value                                           |
|----------|-------------------------------------------------|
| free     | Free memory in bytes (g,m,k,b) or percentages % |
| free_pct | % free memory                                   |
| size     | Total size of memory                            |
| type     | The type of memory to check                     |
| used     | Used memory in bytes (g,m,k,b) or percentages % |
| used_pct | % used memory                                   |






<a name="check_memory_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${type}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key      | Value                                           |
|----------|-------------------------------------------------|
| free     | Free memory in bytes (g,m,k,b) or percentages % |
| free_pct | % free memory                                   |
| size     | Total size of memory                            |
| type     | The type of memory to check                     |
| used     | Used memory in bytes (g,m,k,b) or percentages % |
| used_pct | % used memory                                   |






<a name="check_memory_type"/>
### type



**Description:**
The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)

## check_network

Check network interface status.


### Usage


| Option                                        | Default Value                          | Description                                                                                                      |
|-----------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_network_filter)               |                                        | Filter which marks interesting items.                                                                            |
| [warning](#check_network_warning)             | total > 10000                          | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_network_warn)                   |                                        | Short alias for warning                                                                                          |
| [critical](#check_network_critical)           | total > 100000                         | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_network_crit)                   |                                        | Short alias for critical.                                                                                        |
| [ok](#check_network_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_network_debug)                 | N/A                                    | Show debugging information in the log                                                                            |
| [show-all](#check_network_show-all)           | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_network_empty-state)     | critical                               | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_network_perf-config)     |                                        | Performance data generation configuration                                                                        |
| [escape-html](#check_network_escape-html)     | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_network_help)                   | N/A                                    | Show help screen (this screen)                                                                                   |
| [help-pb](#check_network_help-pb)             | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_network_show-default)   | N/A                                    | Show default values for a given command                                                                          |
| [help-short](#check_network_help-short)       | N/A                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#check_network_top-syntax)       | ${status}: ${list}                     | Top level syntax.                                                                                                |
| [ok-syntax](#check_network_ok-syntax)         | %(status): Network interfaces seem ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_network_empty-syntax)   |                                        | Empty syntax.                                                                                                    |
| [detail-syntax](#check_network_detail-syntax) | ${name} >${sent} <${received} bps      | Detail level syntax.                                                                                             |
| [perf-syntax](#check_network_perf-syntax)     | ${name}                                | Performance alias syntax.                                                                                        |


<a name="check_network_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key               | Value                                                                                                         |
|-------------------|---------------------------------------------------------------------------------------------------------------|
| count             | Number of items matching the filter. Common option for all checks.                                            |
| total             |  Total number of items. Common option for all checks.                                                         |
| ok_count          |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count        |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count        |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count     |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list              |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list           |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list         |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list         |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list      |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list       |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status            |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| MAC               | The MAC address                                                                                               |
| enabled           | True if the network interface is enabled                                                                      |
| name              | Network interface name                                                                                        |
| net_connection_id | Network connection id                                                                                         |
| received          | Bytes received per second                                                                                     |
| sent              | Bytes sent per second                                                                                         |
| speed             | The network interface speed                                                                                   |
| status            | Network connection status                                                                                     |
| total             | Bytes total per second                                                                                        |







<a name="check_network_warning"/>
### warning


**Deafult Value:** total > 10000

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key               | Value                                                                                                         |
|-------------------|---------------------------------------------------------------------------------------------------------------|
| count             | Number of items matching the filter. Common option for all checks.                                            |
| total             |  Total number of items. Common option for all checks.                                                         |
| ok_count          |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count        |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count        |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count     |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list              |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list           |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list         |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list         |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list      |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list       |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status            |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| MAC               | The MAC address                                                                                               |
| enabled           | True if the network interface is enabled                                                                      |
| name              | Network interface name                                                                                        |
| net_connection_id | Network connection id                                                                                         |
| received          | Bytes received per second                                                                                     |
| sent              | Bytes sent per second                                                                                         |
| speed             | The network interface speed                                                                                   |
| status            | Network connection status                                                                                     |
| total             | Bytes total per second                                                                                        |







<a name="check_network_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_network_critical"/>
### critical


**Deafult Value:** total > 100000

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key               | Value                                                                                                         |
|-------------------|---------------------------------------------------------------------------------------------------------------|
| count             | Number of items matching the filter. Common option for all checks.                                            |
| total             |  Total number of items. Common option for all checks.                                                         |
| ok_count          |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count        |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count        |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count     |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list              |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list           |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list         |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list         |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list      |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list       |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status            |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| MAC               | The MAC address                                                                                               |
| enabled           | True if the network interface is enabled                                                                      |
| name              | Network interface name                                                                                        |
| net_connection_id | Network connection id                                                                                         |
| received          | Bytes received per second                                                                                     |
| sent              | Bytes sent per second                                                                                         |
| speed             | The network interface speed                                                                                   |
| status            | Network connection status                                                                                     |
| total             | Bytes total per second                                                                                        |







<a name="check_network_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_network_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key               | Value                                                                                                         |
|-------------------|---------------------------------------------------------------------------------------------------------------|
| count             | Number of items matching the filter. Common option for all checks.                                            |
| total             |  Total number of items. Common option for all checks.                                                         |
| ok_count          |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count        |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count        |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count     |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list              |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list           |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list         |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list         |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list      |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list       |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status            |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| MAC               | The MAC address                                                                                               |
| enabled           | True if the network interface is enabled                                                                      |
| name              | Network interface name                                                                                        |
| net_connection_id | Network connection id                                                                                         |
| received          | Bytes received per second                                                                                     |
| sent              | Bytes sent per second                                                                                         |
| speed             | The network interface speed                                                                                   |
| status            | Network connection status                                                                                     |
| total             | Bytes total per second                                                                                        |







<a name="check_network_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_network_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_network_empty-state"/>
### empty-state


**Deafult Value:** critical

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_network_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_network_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_network_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_network_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_network_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_network_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_network_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_network_ok-syntax"/>
### ok-syntax


**Deafult Value:** %(status): Network interfaces seem ok.

**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_network_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_network_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${name} >${sent} <${received} bps

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key               | Value                                    |
|-------------------|------------------------------------------|
| MAC               | The MAC address                          |
| enabled           | True if the network interface is enabled |
| name              | Network interface name                   |
| net_connection_id | Network connection id                    |
| received          | Bytes received per second                |
| sent              | Bytes sent per second                    |
| speed             | The network interface speed              |
| status            | Network connection status                |
| total             | Bytes total per second                   |






<a name="check_network_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${name}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key               | Value                                    |
|-------------------|------------------------------------------|
| MAC               | The MAC address                          |
| enabled           | True if the network interface is enabled |
| name              | Network interface name                   |
| net_connection_id | Network connection id                    |
| received          | Bytes received per second                |
| sent              | Bytes sent per second                    |
| speed             | The network interface speed              |
| status            | Network connection status                |
| total             | Bytes total per second                   |






## check_os_version

Check the version of the underlaying OS.

### Usage

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


### Usage


| Option                                           | Default Value                           | Description                                                                                                      |
|--------------------------------------------------|-----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_os_version_filter)               |                                         | Filter which marks interesting items.                                                                            |
| [warning](#check_os_version_warning)             | version <= 50                           | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_os_version_warn)                   |                                         | Short alias for warning                                                                                          |
| [critical](#check_os_version_critical)           | version <= 50                           | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_os_version_crit)                   |                                         | Short alias for critical.                                                                                        |
| [ok](#check_os_version_ok)                       |                                         | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_os_version_debug)                 | N/A                                     | Show debugging information in the log                                                                            |
| [show-all](#check_os_version_show-all)           | N/A                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_os_version_empty-state)     | ignored                                 | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_os_version_perf-config)     |                                         | Performance data generation configuration                                                                        |
| [escape-html](#check_os_version_escape-html)     | N/A                                     | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_os_version_help)                   | N/A                                     | Show help screen (this screen)                                                                                   |
| [help-pb](#check_os_version_help-pb)             | N/A                                     | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_os_version_show-default)   | N/A                                     | Show default values for a given command                                                                          |
| [help-short](#check_os_version_help-short)       | N/A                                     | Show help screen (short format).                                                                                 |
| [top-syntax](#check_os_version_top-syntax)       | ${status}: ${list}                      | Top level syntax.                                                                                                |
| [ok-syntax](#check_os_version_ok-syntax)         |                                         | ok syntax.                                                                                                       |
| [empty-syntax](#check_os_version_empty-syntax)   |                                         | Empty syntax.                                                                                                    |
| [detail-syntax](#check_os_version_detail-syntax) | ${version} (${major}.${minor}.${build}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_os_version_perf-syntax)     | version                                 | Performance alias syntax.                                                                                        |


<a name="check_os_version_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                                                                                                                                                                                 |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                                                                                                                                                                                    |
| total         |  Total number of items. Common option for all checks.                                                                                                                                                                                                                 |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                                                                                                                                                                               |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                                                                                                                                                                          |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                                                                                                                                                                         |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                                                                                                                                                                           |
| list          |  A list of all items which matched the filter. Common option for all checks.                                                                                                                                                                                          |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                                                                                                                                                                                     |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                                                                                                                                                                                |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                                                                                                                                                                               |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks.                                                                                                                                                         |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                                                                                                                                                                             |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                                                                                                                                                                            |
| build         | Build version number                                                                                                                                                                                                                                                  |
| major         | Major version number                                                                                                                                                                                                                                                  |
| minor         | Minor version number                                                                                                                                                                                                                                                  |
| suite         | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| version       | The system version                                                                                                                                                                                                                                                    |







<a name="check_os_version_warning"/>
### warning


**Deafult Value:** version <= 50

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                                                                                                                                                                                 |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                                                                                                                                                                                    |
| total         |  Total number of items. Common option for all checks.                                                                                                                                                                                                                 |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                                                                                                                                                                               |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                                                                                                                                                                          |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                                                                                                                                                                         |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                                                                                                                                                                           |
| list          |  A list of all items which matched the filter. Common option for all checks.                                                                                                                                                                                          |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                                                                                                                                                                                     |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                                                                                                                                                                                |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                                                                                                                                                                               |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks.                                                                                                                                                         |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                                                                                                                                                                             |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                                                                                                                                                                            |
| build         | Build version number                                                                                                                                                                                                                                                  |
| major         | Major version number                                                                                                                                                                                                                                                  |
| minor         | Minor version number                                                                                                                                                                                                                                                  |
| suite         | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| version       | The system version                                                                                                                                                                                                                                                    |







<a name="check_os_version_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_os_version_critical"/>
### critical


**Deafult Value:** version <= 50

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                                                                                                                                                                                 |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                                                                                                                                                                                    |
| total         |  Total number of items. Common option for all checks.                                                                                                                                                                                                                 |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                                                                                                                                                                               |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                                                                                                                                                                          |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                                                                                                                                                                         |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                                                                                                                                                                           |
| list          |  A list of all items which matched the filter. Common option for all checks.                                                                                                                                                                                          |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                                                                                                                                                                                     |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                                                                                                                                                                                |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                                                                                                                                                                               |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks.                                                                                                                                                         |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                                                                                                                                                                             |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                                                                                                                                                                            |
| build         | Build version number                                                                                                                                                                                                                                                  |
| major         | Major version number                                                                                                                                                                                                                                                  |
| minor         | Minor version number                                                                                                                                                                                                                                                  |
| suite         | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| version       | The system version                                                                                                                                                                                                                                                    |







<a name="check_os_version_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_os_version_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                                                                                                                                                                                 |
|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                                                                                                                                                                                    |
| total         |  Total number of items. Common option for all checks.                                                                                                                                                                                                                 |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                                                                                                                                                                               |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                                                                                                                                                                          |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                                                                                                                                                                         |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                                                                                                                                                                           |
| list          |  A list of all items which matched the filter. Common option for all checks.                                                                                                                                                                                          |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                                                                                                                                                                                     |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                                                                                                                                                                                |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                                                                                                                                                                               |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks.                                                                                                                                                         |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                                                                                                                                                                             |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                                                                                                                                                                            |
| build         | Build version number                                                                                                                                                                                                                                                  |
| major         | Major version number                                                                                                                                                                                                                                                  |
| minor         | Minor version number                                                                                                                                                                                                                                                  |
| suite         | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| version       | The system version                                                                                                                                                                                                                                                    |







<a name="check_os_version_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_os_version_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_os_version_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_os_version_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_os_version_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_os_version_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_os_version_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_os_version_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_os_version_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_os_version_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_os_version_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_os_version_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_os_version_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${version} (${major}.${minor}.${build})

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key     | Value                                                                                                                                                                                                                                                                 |
|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| build   | Build version number                                                                                                                                                                                                                                                  |
| major   | Major version number                                                                                                                                                                                                                                                  |
| minor   | Minor version number                                                                                                                                                                                                                                                  |
| suite   | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| version | The system version                                                                                                                                                                                                                                                    |






<a name="check_os_version_perf-syntax"/>
### perf-syntax


**Deafult Value:** version

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key     | Value                                                                                                                                                                                                                                                                 |
|---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| build   | Build version number                                                                                                                                                                                                                                                  |
| major   | Major version number                                                                                                                                                                                                                                                  |
| minor   | Minor version number                                                                                                                                                                                                                                                  |
| suite   | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
| version | The system version                                                                                                                                                                                                                                                    |






## check_pagefile

Check the size of the system pagefile(s).

### Usage

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

### Usage


| Option                                         | Default Value             | Description                                                                                                      |
|------------------------------------------------|---------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_pagefile_filter)               |                           | Filter which marks interesting items.                                                                            |
| [warning](#check_pagefile_warning)             | used > 60%                | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_pagefile_warn)                   |                           | Short alias for warning                                                                                          |
| [critical](#check_pagefile_critical)           | used > 80%                | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_pagefile_crit)                   |                           | Short alias for critical.                                                                                        |
| [ok](#check_pagefile_ok)                       |                           | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_pagefile_debug)                 | N/A                       | Show debugging information in the log                                                                            |
| [show-all](#check_pagefile_show-all)           | N/A                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_pagefile_empty-state)     | ignored                   | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_pagefile_perf-config)     |                           | Performance data generation configuration                                                                        |
| [escape-html](#check_pagefile_escape-html)     | N/A                       | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_pagefile_help)                   | N/A                       | Show help screen (this screen)                                                                                   |
| [help-pb](#check_pagefile_help-pb)             | N/A                       | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_pagefile_show-default)   | N/A                       | Show default values for a given command                                                                          |
| [help-short](#check_pagefile_help-short)       | N/A                       | Show help screen (short format).                                                                                 |
| [top-syntax](#check_pagefile_top-syntax)       | ${status}: ${list}        | Top level syntax.                                                                                                |
| [ok-syntax](#check_pagefile_ok-syntax)         |                           | ok syntax.                                                                                                       |
| [empty-syntax](#check_pagefile_empty-syntax)   |                           | Empty syntax.                                                                                                    |
| [detail-syntax](#check_pagefile_detail-syntax) | ${name} ${used} (${size}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_pagefile_perf-syntax)     | ${name}                   | Performance alias syntax.                                                                                        |


<a name="check_pagefile_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| name          | The name of the page file (location)                                                                          |
| size          | Total size of pagefile                                                                                        |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_pagefile_warning"/>
### warning


**Deafult Value:** used > 60%

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| name          | The name of the page file (location)                                                                          |
| size          | Total size of pagefile                                                                                        |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_pagefile_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_pagefile_critical"/>
### critical


**Deafult Value:** used > 80%

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| name          | The name of the page file (location)                                                                          |
| size          | Total size of pagefile                                                                                        |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_pagefile_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_pagefile_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| free          | Free memory in bytes (g,m,k,b) or percentages %                                                               |
| free_pct      | % free memory                                                                                                 |
| name          | The name of the page file (location)                                                                          |
| size          | Total size of pagefile                                                                                        |
| used          | Used memory in bytes (g,m,k,b) or percentages %                                                               |
| used_pct      | % used memory                                                                                                 |







<a name="check_pagefile_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_pagefile_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_pagefile_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_pagefile_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_pagefile_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_pagefile_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_pagefile_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_pagefile_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_pagefile_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_pagefile_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_pagefile_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_pagefile_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_pagefile_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${name} ${used} (${size})

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key      | Value                                           |
|----------|-------------------------------------------------|
| free     | Free memory in bytes (g,m,k,b) or percentages % |
| free_pct | % free memory                                   |
| name     | The name of the page file (location)            |
| size     | Total size of pagefile                          |
| used     | Used memory in bytes (g,m,k,b) or percentages % |
| used_pct | % used memory                                   |






<a name="check_pagefile_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${name}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key      | Value                                           |
|----------|-------------------------------------------------|
| free     | Free memory in bytes (g,m,k,b) or percentages % |
| free_pct | % free memory                                   |
| name     | The name of the page file (location)            |
| size     | Total size of pagefile                          |
| used     | Used memory in bytes (g,m,k,b) or percentages % |
| used_pct | % used memory                                   |






## check_pdh

Check the value of a performance (PDH) counter on the local or remote system.
The counters can also be added and polled periodcally to get average values. Performance Log Users group membership is required to check performance counters.

### Usage

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

### Usage


| Option                                    | Default Value       | Description                                                                                                      |
|-------------------------------------------|---------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_pdh_filter)               |                     | Filter which marks interesting items.                                                                            |
| [warning](#check_pdh_warning)             |                     | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_pdh_warn)                   |                     | Short alias for warning                                                                                          |
| [critical](#check_pdh_critical)           |                     | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_pdh_crit)                   |                     | Short alias for critical.                                                                                        |
| [ok](#check_pdh_ok)                       |                     | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_pdh_debug)                 | N/A                 | Show debugging information in the log                                                                            |
| [show-all](#check_pdh_show-all)           | N/A                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_pdh_empty-state)     | unknown             | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_pdh_perf-config)     |                     | Performance data generation configuration                                                                        |
| [escape-html](#check_pdh_escape-html)     | N/A                 | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_pdh_help)                   | N/A                 | Show help screen (this screen)                                                                                   |
| [help-pb](#check_pdh_help-pb)             | N/A                 | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_pdh_show-default)   | N/A                 | Show default values for a given command                                                                          |
| [help-short](#check_pdh_help-short)       | N/A                 | Show help screen (short format).                                                                                 |
| [top-syntax](#check_pdh_top-syntax)       | ${status}: ${list}  | Top level syntax.                                                                                                |
| [ok-syntax](#check_pdh_ok-syntax)         |                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_pdh_empty-syntax)   |                     | Empty syntax.                                                                                                    |
| [detail-syntax](#check_pdh_detail-syntax) | ${alias} = ${value} | Detail level syntax.                                                                                             |
| [perf-syntax](#check_pdh_perf-syntax)     | ${alias}            | Performance alias syntax.                                                                                        |
| [counter](#check_pdh_counter)             |                     | Performance counter to check                                                                                     |
| [expand-index](#check_pdh_expand-index)   | N/A                 | Expand indexes in counter strings                                                                                |
| [instances](#check_pdh_instances)         | N/A                 | Expand wildcards and fetch all instances                                                                         |
| [reload](#check_pdh_reload)               | N/A                 | Reload counters on errors (useful to check counters which are not added at boot)                                 |
| [averages](#check_pdh_averages)           | N/A                 | Check average values (ie. wait for 1 second to collecting two samples)                                           |
| [time](#check_pdh_time)                   |                     | Timeframe to use for named rrd counters                                                                          |
| [flags](#check_pdh_flags)                 |                     | Extra flags to configure the counter (nocap100, 1000, noscale)                                                   |
| [type](#check_pdh_type)                   | large               | Format of value (double, long, large)                                                                            |


<a name="check_pdh_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| alias         | The counter alias                                                                                             |
| counter       | The counter name                                                                                              |
| time          | The time for rrd checks                                                                                       |
| value         | The counter value (either float or int)                                                                       |
| value_f       | The counter value (force float value)                                                                         |
| value_i       | The counter value (force int value)                                                                           |







<a name="check_pdh_warning"/>
### warning



**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| alias         | The counter alias                                                                                             |
| counter       | The counter name                                                                                              |
| time          | The time for rrd checks                                                                                       |
| value         | The counter value (either float or int)                                                                       |
| value_f       | The counter value (force float value)                                                                         |
| value_i       | The counter value (force int value)                                                                           |







<a name="check_pdh_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_pdh_critical"/>
### critical



**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| alias         | The counter alias                                                                                             |
| counter       | The counter name                                                                                              |
| time          | The time for rrd checks                                                                                       |
| value         | The counter value (either float or int)                                                                       |
| value_f       | The counter value (force float value)                                                                         |
| value_i       | The counter value (force int value)                                                                           |







<a name="check_pdh_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_pdh_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| alias         | The counter alias                                                                                             |
| counter       | The counter name                                                                                              |
| time          | The time for rrd checks                                                                                       |
| value         | The counter value (either float or int)                                                                       |
| value_f       | The counter value (force float value)                                                                         |
| value_i       | The counter value (force int value)                                                                           |







<a name="check_pdh_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_pdh_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_pdh_empty-state"/>
### empty-state


**Deafult Value:** unknown

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_pdh_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_pdh_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_pdh_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_pdh_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_pdh_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_pdh_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_pdh_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_pdh_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_pdh_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_pdh_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${alias} = ${value}

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key     | Value                                   |
|---------|-----------------------------------------|
| alias   | The counter alias                       |
| counter | The counter name                        |
| time    | The time for rrd checks                 |
| value   | The counter value (either float or int) |
| value_f | The counter value (force float value)   |
| value_i | The counter value (force int value)     |






<a name="check_pdh_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${alias}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key     | Value                                   |
|---------|-----------------------------------------|
| alias   | The counter alias                       |
| counter | The counter name                        |
| time    | The time for rrd checks                 |
| value   | The counter value (either float or int) |
| value_f | The counter value (force float value)   |
| value_i | The counter value (force int value)     |






<a name="check_pdh_counter"/>
### counter



**Description:**
Performance counter to check

<a name="check_pdh_expand-index"/>
### expand-index



**Description:**
Expand indexes in counter strings

<a name="check_pdh_instances"/>
### instances



**Description:**
Expand wildcards and fetch all instances

<a name="check_pdh_reload"/>
### reload



**Description:**
Reload counters on errors (useful to check counters which are not added at boot)

<a name="check_pdh_averages"/>
### averages



**Description:**
Check average values (ie. wait for 1 second to collecting two samples)

<a name="check_pdh_time"/>
### time



**Description:**
Timeframe to use for named rrd counters

<a name="check_pdh_flags"/>
### flags



**Description:**
Extra flags to configure the counter (nocap100, 1000, noscale)

<a name="check_pdh_type"/>
### type


**Deafult Value:** large

**Description:**
Format of value (double, long, large)

## check_process

Check state/metrics of one or more of the processes running on the computer.

### Usage

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


### Usage


| Option                                            | Default Value                    | Description                                                                                                      |
|---------------------------------------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_process_filter)                   | state != 'unreadable'            | Filter which marks interesting items.                                                                            |
| [warning](#check_process_warning)                 | state not in ('started')         | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_process_warn)                       |                                  | Short alias for warning                                                                                          |
| [critical](#check_process_critical)               | state = 'stopped', count = 0     | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_process_crit)                       |                                  | Short alias for critical.                                                                                        |
| [ok](#check_process_ok)                           |                                  | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_process_debug)                     | N/A                              | Show debugging information in the log                                                                            |
| [show-all](#check_process_show-all)               | N/A                              | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_process_empty-state)         | unknown                          | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_process_perf-config)         |                                  | Performance data generation configuration                                                                        |
| [escape-html](#check_process_escape-html)         | N/A                              | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_process_help)                       | N/A                              | Show help screen (this screen)                                                                                   |
| [help-pb](#check_process_help-pb)                 | N/A                              | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_process_show-default)       | N/A                              | Show default values for a given command                                                                          |
| [help-short](#check_process_help-short)           | N/A                              | Show help screen (short format).                                                                                 |
| [top-syntax](#check_process_top-syntax)           | ${status}: ${problem_list}       | Top level syntax.                                                                                                |
| [ok-syntax](#check_process_ok-syntax)             | %(status): all processes are ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_process_empty-syntax)       | %(status): No processes found    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_process_detail-syntax)     | ${exe}=${state}                  | Detail level syntax.                                                                                             |
| [perf-syntax](#check_process_perf-syntax)         | ${exe}                           | Performance alias syntax.                                                                                        |
| [process](#check_process_process)                 |                                  | The service to check, set this to * to check all services                                                        |
| [scan-info](#check_process_scan-info)             |                                  | If all process metrics should be fetched (otherwise only status is fetched)                                      |
| [scan-16bit](#check_process_scan-16bit)           |                                  | If 16bit processes should be included                                                                            |
| [delta](#check_process_delta)                     |                                  | Calculate delta over one elapsed second.                                                                         |
| [scan-unreadable](#check_process_scan-unreadable) |                                  | If unreadable processes should be included (will not have information)                                           |
| [total](#check_process_total)                     | N/A                              | Include the total of all matching files                                                                          |


<a name="check_process_filter"/>
### filter


**Deafult Value:** state != 'unreadable'

**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<a name="check_process_warning"/>
### warning


**Deafult Value:** state not in ('started')

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


<a name="check_process_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_process_critical"/>
### critical


**Deafult Value:** state = 'stopped', count = 0

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


<a name="check_process_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_process_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key              | Value                                                                                                         |
|------------------|---------------------------------------------------------------------------------------------------------------|
| count            | Number of items matching the filter. Common option for all checks.                                            |
| total            |  Total number of items. Common option for all checks.                                                         |
| ok_count         |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count       |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count       |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count    |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list             |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list          |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list        |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list        |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list     |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list      |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status           |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| command_line     | Command line of process (not always available)                                                                |
| creation         | Creation time                                                                                                 |
| error            | Any error messages associated with fetching info                                                              |
| exe              | The name of the executable                                                                                    |
| filename         | Name of process (with path)                                                                                   |
| gdi_handles      | Number of handles                                                                                             |
| handles          | Number of handles                                                                                             |
| hung             | Process is hung                                                                                               |
| kernel           | Kernel time in seconds                                                                                        |
| legacy_state     | Get process status (for legacy use via check_nt only)                                                         |
| new              | Process is new (can inly be used for real-time filters)                                                       |
| page_fault       | Page fault count                                                                                              |
| pagefile         | Peak page file use in bytes                                                                                   |
| peak_pagefile    | Page file usage in bytes                                                                                      |
| peak_virtual     | Peak virtual size in bytes                                                                                    |
| peak_working_set | Peak working set in bytes                                                                                     |
| pid              | Process id                                                                                                    |
| started          | Process is started                                                                                            |
| state            | The current state (started, stopped hung)                                                                     |
| stopped          | Process is stopped                                                                                            |
| time             | User-kernel time in seconds                                                                                   |
| user             | User time in seconds                                                                                          |
| user_handles     | Number of handles                                                                                             |
| virtual          | Virtual size in bytes                                                                                         |
| working_set      | Working set in bytes                                                                                          |







<a name="check_process_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_process_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_process_empty-state"/>
### empty-state


**Deafult Value:** unknown

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_process_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_process_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_process_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_process_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_process_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_process_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_process_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${problem_list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_process_ok-syntax"/>
### ok-syntax


**Deafult Value:** %(status): all processes are ok.

**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_process_empty-syntax"/>
### empty-syntax


**Deafult Value:** %(status): No processes found

**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_process_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${exe}=${state}

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key              | Value                                                   |
|------------------|---------------------------------------------------------|
| command_line     | Command line of process (not always available)          |
| creation         | Creation time                                           |
| error            | Any error messages associated with fetching info        |
| exe              | The name of the executable                              |
| filename         | Name of process (with path)                             |
| gdi_handles      | Number of handles                                       |
| handles          | Number of handles                                       |
| hung             | Process is hung                                         |
| kernel           | Kernel time in seconds                                  |
| legacy_state     | Get process status (for legacy use via check_nt only)   |
| new              | Process is new (can inly be used for real-time filters) |
| page_fault       | Page fault count                                        |
| pagefile         | Peak page file use in bytes                             |
| peak_pagefile    | Page file usage in bytes                                |
| peak_virtual     | Peak virtual size in bytes                              |
| peak_working_set | Peak working set in bytes                               |
| pid              | Process id                                              |
| started          | Process is started                                      |
| state            | The current state (started, stopped hung)               |
| stopped          | Process is stopped                                      |
| time             | User-kernel time in seconds                             |
| user             | User time in seconds                                    |
| user_handles     | Number of handles                                       |
| virtual          | Virtual size in bytes                                   |
| working_set      | Working set in bytes                                    |






<a name="check_process_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${exe}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key              | Value                                                   |
|------------------|---------------------------------------------------------|
| command_line     | Command line of process (not always available)          |
| creation         | Creation time                                           |
| error            | Any error messages associated with fetching info        |
| exe              | The name of the executable                              |
| filename         | Name of process (with path)                             |
| gdi_handles      | Number of handles                                       |
| handles          | Number of handles                                       |
| hung             | Process is hung                                         |
| kernel           | Kernel time in seconds                                  |
| legacy_state     | Get process status (for legacy use via check_nt only)   |
| new              | Process is new (can inly be used for real-time filters) |
| page_fault       | Page fault count                                        |
| pagefile         | Peak page file use in bytes                             |
| peak_pagefile    | Page file usage in bytes                                |
| peak_virtual     | Peak virtual size in bytes                              |
| peak_working_set | Peak working set in bytes                               |
| pid              | Process id                                              |
| started          | Process is started                                      |
| state            | The current state (started, stopped hung)               |
| stopped          | Process is stopped                                      |
| time             | User-kernel time in seconds                             |
| user             | User time in seconds                                    |
| user_handles     | Number of handles                                       |
| virtual          | Virtual size in bytes                                   |
| working_set      | Working set in bytes                                    |






<a name="check_process_process"/>
### process



**Description:**
The service to check, set this to * to check all services

<a name="check_process_scan-info"/>
### scan-info



**Description:**
If all process metrics should be fetched (otherwise only status is fetched)

<a name="check_process_scan-16bit"/>
### scan-16bit



**Description:**
If 16bit processes should be included

<a name="check_process_delta"/>
### delta



**Description:**
Calculate delta over one elapsed second.
This call will measure values and then sleep for 2 second and then measure again calculating deltas.

<a name="check_process_scan-unreadable"/>
### scan-unreadable



**Description:**
If unreadable processes should be included (will not have information)

<a name="check_process_total"/>
### total



**Description:**
Include the total of all matching files

## check_service

Check the state of one or more of the computer services.

### Usage

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

Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_service
WARNING: DPS=stopped (auto), MSDTC=stopped (auto), sppsvc=stopped (auto), UALSVC=stopped (auto)
```

**Check that a service is not started**::

```
check_service service=nscp "crit=state = 'started'" warn=none
```

### Usage


| Option                                            | Default Value                                   | Description                                                                                                                                           |
|---------------------------------------------------|-------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_service_filter)                   |                                                 | Filter which marks interesting items.                                                                                                                 |
| [warning](#check_service_warning)                 | not state_is_perfect()                          | Filter which marks items which generates a warning state.                                                                                             |
| [warn](#check_service_warn)                       |                                                 | Short alias for warning                                                                                                                               |
| [critical](#check_service_critical)               | not state_is_ok()                               | Filter which marks items which generates a critical state.                                                                                            |
| [crit](#check_service_crit)                       |                                                 | Short alias for critical.                                                                                                                             |
| [ok](#check_service_ok)                           |                                                 | Filter which marks items which generates an ok state.                                                                                                 |
| [debug](#check_service_debug)                     | N/A                                             | Show debugging information in the log                                                                                                                 |
| [show-all](#check_service_show-all)               | N/A                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                                      |
| [empty-state](#check_service_empty-state)         | unknown                                         | Return status to use when nothing matched filter.                                                                                                     |
| [perf-config](#check_service_perf-config)         |                                                 | Performance data generation configuration                                                                                                             |
| [escape-html](#check_service_escape-html)         | N/A                                             | Escape any < and > characters to prevent HTML encoding                                                                                                |
| [help](#check_service_help)                       | N/A                                             | Show help screen (this screen)                                                                                                                        |
| [help-pb](#check_service_help-pb)                 | N/A                                             | Show help screen as a protocol buffer payload                                                                                                         |
| [show-default](#check_service_show-default)       | N/A                                             | Show default values for a given command                                                                                                               |
| [help-short](#check_service_help-short)           | N/A                                             | Show help screen (short format).                                                                                                                      |
| [top-syntax](#check_service_top-syntax)           | ${status}: ${crit_list}, delayed (${warn_list}) | Top level syntax.                                                                                                                                     |
| [ok-syntax](#check_service_ok-syntax)             | %(status): All %(count) service(s) are ok.      | ok syntax.                                                                                                                                            |
| [empty-syntax](#check_service_empty-syntax)       | %(status): No services found                    | Empty syntax.                                                                                                                                         |
| [detail-syntax](#check_service_detail-syntax)     | ${name}=${state} (${start_type})                | Detail level syntax.                                                                                                                                  |
| [perf-syntax](#check_service_perf-syntax)         | ${name}                                         | Performance alias syntax.                                                                                                                             |
| [computer](#check_service_computer)               |                                                 | The name of the remote computer to check                                                                                                              |
| [service](#check_service_service)                 |                                                 | The service to check, set this to * to check all services                                                                                             |
| [exclude](#check_service_exclude)                 |                                                 | A list of services to ignore (mainly usefull in combination with service=*)                                                                           |
| [type](#check_service_type)                       | service                                         | The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process |
| [state](#check_service_state)                     | all                                             | The types of services to enumerate available states are active, inactive or all                                                                       |
| [only-essential](#check_service_only-essential)   | N/A                                             | Set filter to classification = 'essential'                                                                                                            |
| [only-ignored](#check_service_only-ignored)       | N/A                                             | Set filter to classification = 'ignored'                                                                                                              |
| [only-role](#check_service_only-role)             | N/A                                             | Set filter to classification = 'role'                                                                                                                 |
| [only-supporting](#check_service_only-supporting) | N/A                                             | Set filter to classification = 'supporting'                                                                                                           |
| [only-system](#check_service_only-system)         | N/A                                             | Set filter to classification = 'system'                                                                                                               |
| [only-user](#check_service_only-user)             | N/A                                             | Set filter to classification = 'user'                                                                                                                 |


<a name="check_service_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key                | Value                                                                                                         |
|--------------------|---------------------------------------------------------------------------------------------------------------|
| count              | Number of items matching the filter. Common option for all checks.                                            |
| total              |  Total number of items. Common option for all checks.                                                         |
| ok_count           |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count         |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count         |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count      |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list               |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list            |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list          |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list          |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list       |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list        |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status             |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| classification     | Get classification                                                                                            |
| delayed            | If the service is delayed                                                                                     |
| desc               | Service description                                                                                           |
| is_trigger         | If the service is has associated triggers                                                                     |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                       |
| name               | Service name                                                                                                  |
| pid                | Process id                                                                                                    |
| start_type         | The configured start type ()                                                                                  |
| state              | The current state ()                                                                                          |
| triggers           | The number of associated triggers for this service                                                            |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)     |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                               |







<a name="check_service_warning"/>
### warning


**Deafult Value:** not state_is_perfect()

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key                | Value                                                                                                         |
|--------------------|---------------------------------------------------------------------------------------------------------------|
| count              | Number of items matching the filter. Common option for all checks.                                            |
| total              |  Total number of items. Common option for all checks.                                                         |
| ok_count           |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count         |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count         |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count      |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list               |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list            |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list          |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list          |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list       |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list        |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status             |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| classification     | Get classification                                                                                            |
| delayed            | If the service is delayed                                                                                     |
| desc               | Service description                                                                                           |
| is_trigger         | If the service is has associated triggers                                                                     |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                       |
| name               | Service name                                                                                                  |
| pid                | Process id                                                                                                    |
| start_type         | The configured start type ()                                                                                  |
| state              | The current state ()                                                                                          |
| triggers           | The number of associated triggers for this service                                                            |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)     |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                               |







<a name="check_service_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_service_critical"/>
### critical


**Deafult Value:** not state_is_ok()

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key                | Value                                                                                                         |
|--------------------|---------------------------------------------------------------------------------------------------------------|
| count              | Number of items matching the filter. Common option for all checks.                                            |
| total              |  Total number of items. Common option for all checks.                                                         |
| ok_count           |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count         |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count         |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count      |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list               |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list            |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list          |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list          |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list       |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list        |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status             |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| classification     | Get classification                                                                                            |
| delayed            | If the service is delayed                                                                                     |
| desc               | Service description                                                                                           |
| is_trigger         | If the service is has associated triggers                                                                     |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                       |
| name               | Service name                                                                                                  |
| pid                | Process id                                                                                                    |
| start_type         | The configured start type ()                                                                                  |
| state              | The current state ()                                                                                          |
| triggers           | The number of associated triggers for this service                                                            |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)     |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                               |







<a name="check_service_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_service_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key                | Value                                                                                                         |
|--------------------|---------------------------------------------------------------------------------------------------------------|
| count              | Number of items matching the filter. Common option for all checks.                                            |
| total              |  Total number of items. Common option for all checks.                                                         |
| ok_count           |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count         |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count         |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count      |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list               |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list            |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list          |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list          |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list       |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list        |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status             |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| classification     | Get classification                                                                                            |
| delayed            | If the service is delayed                                                                                     |
| desc               | Service description                                                                                           |
| is_trigger         | If the service is has associated triggers                                                                     |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                       |
| name               | Service name                                                                                                  |
| pid                | Process id                                                                                                    |
| start_type         | The configured start type ()                                                                                  |
| state              | The current state ()                                                                                          |
| triggers           | The number of associated triggers for this service                                                            |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped)     |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                               |







<a name="check_service_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_service_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_service_empty-state"/>
### empty-state


**Deafult Value:** unknown

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_service_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_service_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_service_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_service_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_service_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_service_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_service_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${crit_list}, delayed (${warn_list})

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_service_ok-syntax"/>
### ok-syntax


**Deafult Value:** %(status): All %(count) service(s) are ok.

**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_service_empty-syntax"/>
### empty-syntax


**Deafult Value:** %(status): No services found

**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_service_detail-syntax"/>
### detail-syntax


**Deafult Value:** ${name}=${state} (${start_type})

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key                | Value                                                                                                     |
|--------------------|-----------------------------------------------------------------------------------------------------------|
| classification     | Get classification                                                                                        |
| delayed            | If the service is delayed                                                                                 |
| desc               | Service description                                                                                       |
| is_trigger         | If the service is has associated triggers                                                                 |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                   |
| name               | Service name                                                                                              |
| pid                | Process id                                                                                                |
| start_type         | The configured start type ()                                                                              |
| state              | The current state ()                                                                                      |
| triggers           | The number of associated triggers for this service                                                        |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped) |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                           |






<a name="check_service_perf-syntax"/>
### perf-syntax


**Deafult Value:** ${name}

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key                | Value                                                                                                     |
|--------------------|-----------------------------------------------------------------------------------------------------------|
| classification     | Get classification                                                                                        |
| delayed            | If the service is delayed                                                                                 |
| desc               | Service description                                                                                       |
| is_trigger         | If the service is has associated triggers                                                                 |
| legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                   |
| name               | Service name                                                                                              |
| pid                | Process id                                                                                                |
| start_type         | The configured start type ()                                                                              |
| state              | The current state ()                                                                                      |
| triggers           | The number of associated triggers for this service                                                        |
| state_is_ok()      | Check if the state is ok, i.e. all running services are runningelayed services are allowed to be stopped) |
| state_is_perfect() | Check if the state is ok, i.e. all running services are running                                           |






<a name="check_service_computer"/>
### computer



**Description:**
The name of the remote computer to check

<a name="check_service_service"/>
### service



**Description:**
The service to check, set this to * to check all services

<a name="check_service_exclude"/>
### exclude



**Description:**
A list of services to ignore (mainly usefull in combination with service=*)

<a name="check_service_type"/>
### type


**Deafult Value:** service

**Description:**
The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process

<a name="check_service_state"/>
### state


**Deafult Value:** all

**Description:**
The types of services to enumerate available states are active, inactive or all

<a name="check_service_only-essential"/>
### only-essential



**Description:**
Set filter to classification = 'essential'

<a name="check_service_only-ignored"/>
### only-ignored



**Description:**
Set filter to classification = 'ignored'

<a name="check_service_only-role"/>
### only-role



**Description:**
Set filter to classification = 'role'

<a name="check_service_only-supporting"/>
### only-supporting



**Description:**
Set filter to classification = 'supporting'

<a name="check_service_only-system"/>
### only-system



**Description:**
Set filter to classification = 'system'

<a name="check_service_only-user"/>
### only-user



**Description:**
Set filter to classification = 'user'

## check_uptime

Check time since last server re-boot.

### Usage

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


### Usage


| Option                                       | Default Value                           | Description                                                                                                      |
|----------------------------------------------|-----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_uptime_filter)               |                                         | Filter which marks interesting items.                                                                            |
| [warning](#check_uptime_warning)             | uptime < 2d                             | Filter which marks items which generates a warning state.                                                        |
| [warn](#check_uptime_warn)                   |                                         | Short alias for warning                                                                                          |
| [critical](#check_uptime_critical)           | uptime < 1d                             | Filter which marks items which generates a critical state.                                                       |
| [crit](#check_uptime_crit)                   |                                         | Short alias for critical.                                                                                        |
| [ok](#check_uptime_ok)                       |                                         | Filter which marks items which generates an ok state.                                                            |
| [debug](#check_uptime_debug)                 | N/A                                     | Show debugging information in the log                                                                            |
| [show-all](#check_uptime_show-all)           | N/A                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_uptime_empty-state)     | ignored                                 | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_uptime_perf-config)     |                                         | Performance data generation configuration                                                                        |
| [escape-html](#check_uptime_escape-html)     | N/A                                     | Escape any < and > characters to prevent HTML encoding                                                           |
| [help](#check_uptime_help)                   | N/A                                     | Show help screen (this screen)                                                                                   |
| [help-pb](#check_uptime_help-pb)             | N/A                                     | Show help screen as a protocol buffer payload                                                                    |
| [show-default](#check_uptime_show-default)   | N/A                                     | Show default values for a given command                                                                          |
| [help-short](#check_uptime_help-short)       | N/A                                     | Show help screen (short format).                                                                                 |
| [top-syntax](#check_uptime_top-syntax)       | ${status}: ${list}                      | Top level syntax.                                                                                                |
| [ok-syntax](#check_uptime_ok-syntax)         |                                         | ok syntax.                                                                                                       |
| [empty-syntax](#check_uptime_empty-syntax)   |                                         | Empty syntax.                                                                                                    |
| [detail-syntax](#check_uptime_detail-syntax) | uptime: ${uptime}h, boot: ${boot} (UTC) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_uptime_perf-syntax)     | uptime                                  | Performance alias syntax.                                                                                        |


<a name="check_uptime_filter"/>
### filter



**Description:**
Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| boot          | System boot time                                                                                              |
| uptime        | Time since last boot                                                                                          |







<a name="check_uptime_warning"/>
### warning


**Deafult Value:** uptime < 2d

**Description:**
Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| boot          | System boot time                                                                                              |
| uptime        | Time since last boot                                                                                          |







<a name="check_uptime_warn"/>
### warn



**Description:**
Short alias for warning

<a name="check_uptime_critical"/>
### critical


**Deafult Value:** uptime < 1d

**Description:**
Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| boot          | System boot time                                                                                              |
| uptime        | Time since last boot                                                                                          |







<a name="check_uptime_crit"/>
### crit



**Description:**
Short alias for critical.

<a name="check_uptime_ok"/>
### ok



**Description:**
Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.
Available options : 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| boot          | System boot time                                                                                              |
| uptime        | Time since last boot                                                                                          |







<a name="check_uptime_debug"/>
### debug



**Description:**
Show debugging information in the log

<a name="check_uptime_show-all"/>
### show-all



**Description:**
Show details for all matches regardless of status (normally details are only showed for warnings and criticals).

<a name="check_uptime_empty-state"/>
### empty-state


**Deafult Value:** ignored

**Description:**
Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

<a name="check_uptime_perf-config"/>
### perf-config



**Description:**
Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)

<a name="check_uptime_escape-html"/>
### escape-html



**Description:**
Escape any < and > characters to prevent HTML encoding

<a name="check_uptime_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="check_uptime_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="check_uptime_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="check_uptime_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="check_uptime_top-syntax"/>
### top-syntax


**Deafult Value:** ${status}: ${list}

**Description:**
Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_uptime_ok-syntax"/>
### ok-syntax



**Description:**
ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

<a name="check_uptime_empty-syntax"/>
### empty-syntax



**Description:**
Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.
Possible values are: 

| Key           | Value                                                                                                         |
|---------------|---------------------------------------------------------------------------------------------------------------|
| count         | Number of items matching the filter. Common option for all checks.                                            |
| total         |  Total number of items. Common option for all checks.                                                         |
| ok_count      |  Number of items matched the ok criteria. Common option for all checks.                                       |
| warn_count    |  Number of items matched the warning criteria. Common option for all checks.                                  |
| crit_count    |  Number of items matched the critical criteria. Common option for all checks.                                 |
| problem_count |  Number of items matched either warning or critical criteria. Common option for all checks.                   |
| list          |  A list of all items which matched the filter. Common option for all checks.                                  |
| ok_list       |  A list of all items which matched the ok criteria. Common option for all checks.                             |
| warn_list     |  A list of all items which matched the warning criteria. Common option for all checks.                        |
| crit_list     |  A list of all items which matched the critical criteria. Common option for all checks.                       |
| problem_list  |  A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| detail_list   |  A special list with critical, then warning and finally ok. Common option for all checks.                     |
| status        |  The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |






<a name="check_uptime_detail-syntax"/>
### detail-syntax


**Deafult Value:** uptime: ${uptime}h, boot: ${boot} (UTC)

**Description:**
Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).
The available keywords are: 

| Key    | Value                |
|--------|----------------------|
| boot   | System boot time     |
| uptime | Time since last boot |






<a name="check_uptime_perf-syntax"/>
### perf-syntax


**Deafult Value:** uptime

**Description:**
Performance alias syntax.
This is the syntax for the base names of the performance data.
Possible values are: 

| Key    | Value                |
|--------|----------------------|
| boot   | System boot time     |
| uptime | Time since last boot |






## checkcounter

Legacy version of check_pdh


### Usage


| Option                                     | Default Value | Description                                                                                           |
|--------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| [help](#checkcounter_help)                 | N/A           | Show help screen (this screen)                                                                        |
| [help-pb](#checkcounter_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                                         |
| [show-default](#checkcounter_show-default) | N/A           | Show default values for a given command                                                               |
| [help-short](#checkcounter_help-short)     | N/A           | Show help screen (short format).                                                                      |
| [Counter](#checkcounter_Counter)           |               | The time to check                                                                                     |
| [ShowAll](#checkcounter_ShowAll)           | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| [MaxWarn](#checkcounter_MaxWarn)           |               | Maximum value before a warning is returned.                                                           |
| [MaxCrit](#checkcounter_MaxCrit)           |               | Maximum value before a critical is returned.                                                          |
| [MinWarn](#checkcounter_MinWarn)           |               | Minimum value before a warning is returned.                                                           |
| [MinCrit](#checkcounter_MinCrit)           |               | Minimum value before a critical is returned.                                                          |


<a name="checkcounter_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checkcounter_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checkcounter_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checkcounter_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checkcounter_Counter"/>
### Counter



**Description:**
The time to check

<a name="checkcounter_ShowAll"/>
### ShowAll


**Deafult Value:** short

**Description:**
Configures display format (if set shows all items not only failures, if set to long shows all cores).

<a name="checkcounter_MaxWarn"/>
### MaxWarn



**Description:**
Maximum value before a warning is returned.

<a name="checkcounter_MaxCrit"/>
### MaxCrit



**Description:**
Maximum value before a critical is returned.

<a name="checkcounter_MinWarn"/>
### MinWarn



**Description:**
Minimum value before a warning is returned.

<a name="checkcounter_MinCrit"/>
### MinCrit



**Description:**
Minimum value before a critical is returned.

## checkcpu

Legacy version of check_cpu


### Usage


| Option                                 | Default Value | Description                                                                                           |
|----------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| [help](#checkcpu_help)                 | N/A           | Show help screen (this screen)                                                                        |
| [help-pb](#checkcpu_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                                         |
| [show-default](#checkcpu_show-default) | N/A           | Show default values for a given command                                                               |
| [help-short](#checkcpu_help-short)     | N/A           | Show help screen (short format).                                                                      |
| [time](#checkcpu_time)                 |               | The time to check                                                                                     |
| [ShowAll](#checkcpu_ShowAll)           | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| [MaxWarn](#checkcpu_MaxWarn)           |               | Maximum value before a warning is returned.                                                           |
| [MaxCrit](#checkcpu_MaxCrit)           |               | Maximum value before a critical is returned.                                                          |
| [MinWarn](#checkcpu_MinWarn)           |               | Minimum value before a warning is returned.                                                           |
| [MinCrit](#checkcpu_MinCrit)           |               | Minimum value before a critical is returned.                                                          |
| [warn](#checkcpu_warn)                 |               | Maximum value before a warning is returned.                                                           |
| [crit](#checkcpu_crit)                 |               | Maximum value before a critical is returned.                                                          |


<a name="checkcpu_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checkcpu_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checkcpu_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checkcpu_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checkcpu_time"/>
### time



**Description:**
The time to check

<a name="checkcpu_ShowAll"/>
### ShowAll


**Deafult Value:** short

**Description:**
Configures display format (if set shows all items not only failures, if set to long shows all cores).

<a name="checkcpu_MaxWarn"/>
### MaxWarn



**Description:**
Maximum value before a warning is returned.

<a name="checkcpu_MaxCrit"/>
### MaxCrit



**Description:**
Maximum value before a critical is returned.

<a name="checkcpu_MinWarn"/>
### MinWarn



**Description:**
Minimum value before a warning is returned.

<a name="checkcpu_MinCrit"/>
### MinCrit



**Description:**
Minimum value before a critical is returned.

<a name="checkcpu_warn"/>
### warn



**Description:**
Maximum value before a warning is returned.

<a name="checkcpu_crit"/>
### crit



**Description:**
Maximum value before a critical is returned.

## checkmem

Legacy version of check_mem


### Usage


| Option                                 | Default Value | Description                                                                                           |
|----------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| [help](#checkmem_help)                 | N/A           | Show help screen (this screen)                                                                        |
| [help-pb](#checkmem_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                                         |
| [show-default](#checkmem_show-default) | N/A           | Show default values for a given command                                                               |
| [help-short](#checkmem_help-short)     | N/A           | Show help screen (short format).                                                                      |
| [type](#checkmem_type)                 |               | The types to check                                                                                    |
| [ShowAll](#checkmem_ShowAll)           | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| [MaxWarn](#checkmem_MaxWarn)           |               | Maximum value before a warning is returned.                                                           |
| [MaxCrit](#checkmem_MaxCrit)           |               | Maximum value before a critical is returned.                                                          |
| [MinWarn](#checkmem_MinWarn)           |               | Minimum value before a warning is returned.                                                           |
| [MinCrit](#checkmem_MinCrit)           |               | Minimum value before a critical is returned.                                                          |
| [warn](#checkmem_warn)                 |               | Maximum value before a warning is returned.                                                           |
| [crit](#checkmem_crit)                 |               | Maximum value before a critical is returned.                                                          |


<a name="checkmem_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checkmem_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checkmem_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checkmem_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checkmem_type"/>
### type



**Description:**
The types to check

<a name="checkmem_ShowAll"/>
### ShowAll


**Deafult Value:** short

**Description:**
Configures display format (if set shows all items not only failures, if set to long shows all cores).

<a name="checkmem_MaxWarn"/>
### MaxWarn



**Description:**
Maximum value before a warning is returned.

<a name="checkmem_MaxCrit"/>
### MaxCrit



**Description:**
Maximum value before a critical is returned.

<a name="checkmem_MinWarn"/>
### MinWarn



**Description:**
Minimum value before a warning is returned.

<a name="checkmem_MinCrit"/>
### MinCrit



**Description:**
Minimum value before a critical is returned.

<a name="checkmem_warn"/>
### warn



**Description:**
Maximum value before a warning is returned.

<a name="checkmem_crit"/>
### crit



**Description:**
Maximum value before a critical is returned.

## checkprocstate

Legacy version of check_process


### Usage


| Option                                       | Default Value | Description                                                                                           |
|----------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| [help](#checkprocstate_help)                 | N/A           | Show help screen (this screen)                                                                        |
| [help-pb](#checkprocstate_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                                         |
| [show-default](#checkprocstate_show-default) | N/A           | Show default values for a given command                                                               |
| [help-short](#checkprocstate_help-short)     | N/A           | Show help screen (short format).                                                                      |
| [ShowAll](#checkprocstate_ShowAll)           | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| [MaxWarnCount](#checkprocstate_MaxWarnCount) |               | Maximum value before a warning is returned.                                                           |
| [MaxCritCount](#checkprocstate_MaxCritCount) |               | Maximum value before a critical is returned.                                                          |
| [MinWarnCount](#checkprocstate_MinWarnCount) |               | Minimum value before a warning is returned.                                                           |
| [MinCritCount](#checkprocstate_MinCritCount) |               | Minimum value before a critical is returned.                                                          |


<a name="checkprocstate_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checkprocstate_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checkprocstate_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checkprocstate_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checkprocstate_ShowAll"/>
### ShowAll


**Deafult Value:** short

**Description:**
Configures display format (if set shows all items not only failures, if set to long shows all cores).

<a name="checkprocstate_MaxWarnCount"/>
### MaxWarnCount



**Description:**
Maximum value before a warning is returned.

<a name="checkprocstate_MaxCritCount"/>
### MaxCritCount



**Description:**
Maximum value before a critical is returned.

<a name="checkprocstate_MinWarnCount"/>
### MinWarnCount



**Description:**
Minimum value before a warning is returned.

<a name="checkprocstate_MinCritCount"/>
### MinCritCount



**Description:**
Minimum value before a critical is returned.

## checkservicestate

Legacy version of check_service


### Usage


| Option                                          | Default Value | Description                                                                                           |
|-------------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| [help](#checkservicestate_help)                 | N/A           | Show help screen (this screen)                                                                        |
| [help-pb](#checkservicestate_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                                         |
| [show-default](#checkservicestate_show-default) | N/A           | Show default values for a given command                                                               |
| [help-short](#checkservicestate_help-short)     | N/A           | Show help screen (short format).                                                                      |
| [CheckAll](#checkservicestate_CheckAll)         | true          | Check all services.                                                                                   |
| [exclude](#checkservicestate_exclude)           |               | Exclude services                                                                                      |
| [ShowAll](#checkservicestate_ShowAll)           | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |


<a name="checkservicestate_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checkservicestate_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checkservicestate_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checkservicestate_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checkservicestate_CheckAll"/>
### CheckAll


**Deafult Value:** true

**Description:**
Check all services.

<a name="checkservicestate_exclude"/>
### exclude



**Description:**
Exclude services

<a name="checkservicestate_ShowAll"/>
### ShowAll


**Deafult Value:** short

**Description:**
Configures display format (if set shows all items not only failures, if set to long shows all cores).

## checkuptime

Legacy version of check_uptime


### Usage


| Option                                    | Default Value | Description                                                                                           |
|-------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------|
| [help](#checkuptime_help)                 | N/A           | Show help screen (this screen)                                                                        |
| [help-pb](#checkuptime_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                                         |
| [show-default](#checkuptime_show-default) | N/A           | Show default values for a given command                                                               |
| [help-short](#checkuptime_help-short)     | N/A           | Show help screen (short format).                                                                      |
| [ShowAll](#checkuptime_ShowAll)           | short         | Configures display format (if set shows all items not only failures, if set to long shows all cores). |
| [MaxWarn](#checkuptime_MaxWarn)           |               | Maximum value before a warning is returned.                                                           |
| [MaxCrit](#checkuptime_MaxCrit)           |               | Maximum value before a critical is returned.                                                          |
| [MinWarn](#checkuptime_MinWarn)           |               | Minimum value before a warning is returned.                                                           |
| [MinCrit](#checkuptime_MinCrit)           |               | Minimum value before a critical is returned.                                                          |
| [warn](#checkuptime_warn)                 |               | Maximum value before a warning is returned.                                                           |
| [crit](#checkuptime_crit)                 |               | Maximum value before a critical is returned.                                                          |


<a name="checkuptime_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="checkuptime_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="checkuptime_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="checkuptime_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="checkuptime_ShowAll"/>
### ShowAll


**Deafult Value:** short

**Description:**
Configures display format (if set shows all items not only failures, if set to long shows all cores).

<a name="checkuptime_MaxWarn"/>
### MaxWarn



**Description:**
Maximum value before a warning is returned.

<a name="checkuptime_MaxCrit"/>
### MaxCrit



**Description:**
Maximum value before a critical is returned.

<a name="checkuptime_MinWarn"/>
### MinWarn



**Description:**
Minimum value before a warning is returned.

<a name="checkuptime_MinCrit"/>
### MinCrit



**Description:**
Minimum value before a critical is returned.

<a name="checkuptime_warn"/>
### warn



**Description:**
Maximum value before a warning is returned.

<a name="checkuptime_crit"/>
### crit



**Description:**
Maximum value before a critical is returned.



# Configuration

<a name="/settings/system/windows"/>
## WINDOWS CHECK SYSTEM

Section for system checks and system settings

```ini
# Section for system checks and system settings
[/settings/system/windows]
default buffer length=1h
subsystem=default

```


| Key                                                                      | Default Value | Description    |
|--------------------------------------------------------------------------|---------------|----------------|
| [default buffer length](#/settings/system/windows_default buffer length) | 1h            | DEFAULT LENGTH |
| [disable](#/settings/system/windows_disable)                             |               | DISABLE CHECKS |
| [subsystem](#/settings/system/windows_subsystem)                         | default       | PDH SUBSYSTEM  |




<a name="/settings/system/windows_default buffer length"/>
### default buffer length

**DEFAULT LENGTH**

Used to define the default interval for range buffer checks (ie. CPU).




| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | default buffer length                                 |
| Default value: | `1h`                                                  |
| Used by:       | CheckSystem                                           |


#### Sample

```
[/settings/system/windows]
# DEFAULT LENGTH
default buffer length=1h
```


<a name="/settings/system/windows_disable"/>
### disable

**DISABLE CHECKS**

A comma separated list of checks to disable in the collector: cpu,handles,network,metrics,pdh. Please note disabling these will mean part of NSClient++ will no longer function as expected.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | disable                                               |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | _N/A_                                                 |
| Used by:       | CheckSystem                                           |


#### Sample

```
[/settings/system/windows]
# DISABLE CHECKS
disable=
```


<a name="/settings/system/windows_subsystem"/>
### subsystem

**PDH SUBSYSTEM**

Set which pdh subsystem to use.




| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | subsystem                                             |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | `default`                                             |
| Used by:       | CheckSystem                                           |


#### Sample

```
[/settings/system/windows]
# PDH SUBSYSTEM
subsystem=default
```


<a name="/settings/system/windows/counters"/>
## COUNTERS

Add counters to check

```ini
# Add counters to check
[/settings/system/windows/counters]

```


| Key                                                                       | Default Value | Description       |
|---------------------------------------------------------------------------|---------------|-------------------|
| [disk_queue_length](#/settings/system/windows/counters_disk_queue_length) |               | disk_queue_length |




<a name="/settings/system/windows/counters_disk_queue_length"/>
### disk_queue_length

**disk_queue_length**

To configure this create a section under: /settings/system/windows/counters/disk_queue_length





| Key            | Description                                                             |
|----------------|-------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters](#/settings/system/windows/counters) |
| Key:           | disk_queue_length                                                       |
| Default value: | _N/A_                                                                   |
| Used by:       | CheckSystem                                                             |


#### Sample

```
[/settings/system/windows/counters]
# disk_queue_length
disk_queue_length=
```


<a name="/settings/system/windows/counters/default"/>
## COUNTER

Definition for counter: default

```ini
# Definition for counter: default
[/settings/system/windows/counters/default]
is template=false
parent=default

```


| Key                                                                                   | Default Value | Description         |
|---------------------------------------------------------------------------------------|---------------|---------------------|
| [alias](#/settings/system/windows/counters/default_alias)                             |               | ALIAS               |
| [buffer size](#/settings/system/windows/counters/default_buffer size)                 |               | BUFFER SIZE         |
| [collection strategy](#/settings/system/windows/counters/default_collection strategy) |               | COLLECTION STRATEGY |
| [counter](#/settings/system/windows/counters/default_counter)                         |               | COUNTER             |
| [flags](#/settings/system/windows/counters/default_flags)                             |               | FLAGS               |
| [instances](#/settings/system/windows/counters/default_instances)                     |               | Interpret instances |
| [is template](#/settings/system/windows/counters/default_is template)                 | false         | IS TEMPLATE         |
| [parent](#/settings/system/windows/counters/default_parent)                           | default       | PARENT              |
| [type](#/settings/system/windows/counters/default_type)                               |               | COUNTER TYPE        |




<a name="/settings/system/windows/counters/default_alias"/>
### alias

**ALIAS**

The alias (service name) to report to server





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | alias                                                                                   |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# ALIAS
alias=
```


<a name="/settings/system/windows/counters/default_buffer size"/>
### buffer size

**BUFFER SIZE**

Size of buffer (in seconds) larger buffer use more memory





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | buffer size                                                                             |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# BUFFER SIZE
buffer size=
```


<a name="/settings/system/windows/counters/default_collection strategy"/>
### collection strategy

**COLLECTION STRATEGY**

The way to handled values when collecting them: static means we keep the last known value, rrd means we store values in a buffer from which you can retrieve the average





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | collection strategy                                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# COLLECTION STRATEGY
collection strategy=
```


<a name="/settings/system/windows/counters/default_counter"/>
### counter

**COUNTER**

The counter to check





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | counter                                                                                 |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# COUNTER
counter=
```


<a name="/settings/system/windows/counters/default_flags"/>
### flags

**FLAGS**

Extra flags to configure the counter (nocap100, 1000, noscale)





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | flags                                                                                   |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# FLAGS
flags=
```


<a name="/settings/system/windows/counters/default_instances"/>
### instances

**Interpret instances**

IF we shoul interpret instance (default auto). Values: auto, true, false





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | instances                                                                               |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# Interpret instances
instances=
```


<a name="/settings/system/windows/counters/default_is template"/>
### is template

**IS TEMPLATE**

Declare this object as a template (this means it will not be available as a separate object)




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | is template                                                                             |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | `false`                                                                                 |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# IS TEMPLATE
is template=false
```


<a name="/settings/system/windows/counters/default_parent"/>
### parent

**PARENT**

The parent the target inherits from




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | parent                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | `default`                                                                               |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# PARENT
parent=default
```


<a name="/settings/system/windows/counters/default_type"/>
### type

**COUNTER TYPE**

The type of counter to use long, large and double





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/default](#/settings/system/windows/counters/default) |
| Key:           | type                                                                                    |
| Default value: | _N/A_                                                                                   |
| Used by:       | CheckSystem                                                                             |


#### Sample

```
[/settings/system/windows/counters/default]
# COUNTER TYPE
type=
```


<a name="/settings/system/windows/counters/sample"/>
## COUNTER

Definition for counter: sample

```ini
# Definition for counter: sample
[/settings/system/windows/counters/sample]
is template=false
parent=default

```


| Key                                                                                  | Default Value | Description         |
|--------------------------------------------------------------------------------------|---------------|---------------------|
| [alias](#/settings/system/windows/counters/sample_alias)                             |               | ALIAS               |
| [buffer size](#/settings/system/windows/counters/sample_buffer size)                 |               | BUFFER SIZE         |
| [collection strategy](#/settings/system/windows/counters/sample_collection strategy) |               | COLLECTION STRATEGY |
| [counter](#/settings/system/windows/counters/sample_counter)                         |               | COUNTER             |
| [flags](#/settings/system/windows/counters/sample_flags)                             |               | FLAGS               |
| [instances](#/settings/system/windows/counters/sample_instances)                     |               | Interpret instances |
| [is template](#/settings/system/windows/counters/sample_is template)                 | false         | IS TEMPLATE         |
| [parent](#/settings/system/windows/counters/sample_parent)                           | default       | PARENT              |
| [type](#/settings/system/windows/counters/sample_type)                               |               | COUNTER TYPE        |




<a name="/settings/system/windows/counters/sample_alias"/>
### alias

**ALIAS**

The alias (service name) to report to server





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | alias                                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | _N/A_                                                                                 |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# ALIAS
alias=
```


<a name="/settings/system/windows/counters/sample_buffer size"/>
### buffer size

**BUFFER SIZE**

Size of buffer (in seconds) larger buffer use more memory





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | buffer size                                                                           |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# BUFFER SIZE
buffer size=
```


<a name="/settings/system/windows/counters/sample_collection strategy"/>
### collection strategy

**COLLECTION STRATEGY**

The way to handled values when collecting them: static means we keep the last known value, rrd means we store values in a buffer from which you can retrieve the average





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | collection strategy                                                                   |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# COLLECTION STRATEGY
collection strategy=
```


<a name="/settings/system/windows/counters/sample_counter"/>
### counter

**COUNTER**

The counter to check





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | counter                                                                               |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# COUNTER
counter=
```


<a name="/settings/system/windows/counters/sample_flags"/>
### flags

**FLAGS**

Extra flags to configure the counter (nocap100, 1000, noscale)





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | flags                                                                                 |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# FLAGS
flags=
```


<a name="/settings/system/windows/counters/sample_instances"/>
### instances

**Interpret instances**

IF we shoul interpret instance (default auto). Values: auto, true, false





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | instances                                                                             |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# Interpret instances
instances=
```


<a name="/settings/system/windows/counters/sample_is template"/>
### is template

**IS TEMPLATE**

Declare this object as a template (this means it will not be available as a separate object)




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | is template                                                                           |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | `false`                                                                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# IS TEMPLATE
is template=false
```


<a name="/settings/system/windows/counters/sample_parent"/>
### parent

**PARENT**

The parent the target inherits from




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | parent                                                                                |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | `default`                                                                             |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# PARENT
parent=default
```


<a name="/settings/system/windows/counters/sample_type"/>
### type

**COUNTER TYPE**

The type of counter to use long, large and double





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/system/windows/counters/sample](#/settings/system/windows/counters/sample) |
| Key:           | type                                                                                  |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CheckSystem                                                                           |


#### Sample

```
[/settings/system/windows/counters/sample]
# COUNTER TYPE
type=
```


<a name="/settings/system/windows/real-time"/>
## CONFIGURE REALTIME CHECKING

A set of options to configure the real time checks

```ini
# A set of options to configure the real time checks
[/settings/system/windows/real-time]

```






<a name="/settings/system/windows/real-time/checks"/>
## REALTIME FILTERS

A set of filters to use in real-time mode

```ini
# A set of filters to use in real-time mode
[/settings/system/windows/real-time/checks]

```






<a name="/settings/system/windows/real-time/cpu"/>
## REALTIME FILTERS

A set of filters to use in real-time mode

```ini
# A set of filters to use in real-time mode
[/settings/system/windows/real-time/cpu]

```






<a name="/settings/system/windows/real-time/memory"/>
## REALTIME FILTERS

A set of filters to use in real-time mode

```ini
# A set of filters to use in real-time mode
[/settings/system/windows/real-time/memory]

```






<a name="/settings/system/windows/real-time/process"/>
## REALTIME FILTERS

A set of filters to use in real-time mode

```ini
# A set of filters to use in real-time mode
[/settings/system/windows/real-time/process]

```






<a name="/settings/system/windows/service mapping"/>
## SERVICE MAPPING SECTION

Configure which services has to be in which state

```ini
# Configure which services has to be in which state
[/settings/system/windows/service mapping]

```






