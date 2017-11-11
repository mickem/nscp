# CheckSystemUnix

Various system related checks, such as CPU load, process state and memory.



**List of commands:**

A list of all available queries (check commands)

| Command                               | Description                              |
|---------------------------------------|------------------------------------------|
| [check_memory](#check_memory)         | Check free/used memory on the system.    |
| [check_os_version](#check_os_version) | Check the version of the underlaying OS. |
| [check_uptime](#check_uptime)         | Check time since last server re-boot.    |







## Queries

A quick reference for all available queries (check commands) in the CheckSystemUnix module.

### check_memory

Check free/used memory on the system.

* [Samples](#check_memory_samples)

* [Command-line Arguments](#check_memory_options)
* [Filter keywords](#check_memory_filter_keys)


<a name="check_memory_samples"/>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystemUnix_check_memory_samples.md)_

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



<a name="check_memory_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.





<a name="check_memory_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



*Deafult Value:* | `used > 80%`



<a name="check_memory_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



*Deafult Value:* | `used > 90%`



<a name="check_memory_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_memory_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Deafult Value:* | `ignored`



<a name="check_memory_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_memory_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${status}: ${list}`



<a name="check_memory_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).





<a name="check_memory_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.





<a name="check_memory_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${type} = ${used}`



<a name="check_memory_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Deafult Value:* | `${type}`




<a name="check_memory_filter_keys"/>
#### Filter keywords


| Option                                       | Description                                                                                                  |
|----------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [count](#check_memory_count)                 | Number of items matching the filter. Common option for all checks.                                           |
| [crit_count](#check_memory_crit_count)       | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_memory_crit_list)         | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [detail_list](#check_memory_detail_list)     | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [free](#check_memory_free)                   | Free memory in bytes (g,m,k,b) or percentages %                                                              |
| [list](#check_memory_list)                   | A list of all items which matched the filter. Common option for all checks.                                  |
| [ok_count](#check_memory_ok_count)           | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_memory_ok_list)             | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [problem_count](#check_memory_problem_count) | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_memory_problem_list)   | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [size](#check_memory_size)                   | Total size of memory                                                                                         |
| [status](#check_memory_status)               | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [total](#check_memory_total)                 | Total number of items. Common option for all checks.                                                         |
| [type](#check_memory_type)                   | The type of memory to check                                                                                  |
| [used](#check_memory_used)                   | Used memory in bytes (g,m,k,b) or percentages %                                                              |
| [warn_count](#check_memory_warn_count)       | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_memory_warn_list)         | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_os_version

Check the version of the underlaying OS.


* [Command-line Arguments](#check_os_version_options)
* [Filter keywords](#check_os_version_filter_keys)





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
| [warning](#check_os_version_warning)             | version > 50                            | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                         | Short alias for warning                                                                                          |
| [critical](#check_os_version_critical)           | version > 50                            | Filter which marks items which generates a critical state.                                                       |
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



<a name="check_os_version_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.





<a name="check_os_version_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



*Deafult Value:* | `version > 50`



<a name="check_os_version_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



*Deafult Value:* | `version > 50`



<a name="check_os_version_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_os_version_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Deafult Value:* | `ignored`



<a name="check_os_version_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_os_version_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${status}: ${list}`



<a name="check_os_version_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).





<a name="check_os_version_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.





<a name="check_os_version_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${version} (${major}.${minor}.${build})`



<a name="check_os_version_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Deafult Value:* | `version`




<a name="check_os_version_filter_keys"/>
#### Filter keywords


| Option                                             | Description                                                                                                  |
|----------------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [count](#check_os_version_count)                   | Number of items matching the filter. Common option for all checks.                                           |
| [crit_count](#check_os_version_crit_count)         | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_os_version_crit_list)           | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [detail_list](#check_os_version_detail_list)       | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [kernel_name](#check_os_version_kernel_name)       | Kernel name                                                                                                  |
| [kernel_release](#check_os_version_kernel_release) | Kernel release                                                                                               |
| [kernel_version](#check_os_version_kernel_version) | Kernel version                                                                                               |
| [list](#check_os_version_list)                     | A list of all items which matched the filter. Common option for all checks.                                  |
| [machine](#check_os_version_machine)               | Machine hardware name                                                                                        |
| [nodename](#check_os_version_nodename)             | Network node hostname                                                                                        |
| [ok_count](#check_os_version_ok_count)             | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_os_version_ok_list)               | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [os](#check_os_version_os)                         | Operating system                                                                                             |
| [problem_count](#check_os_version_problem_count)   | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_os_version_problem_list)     | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [processor](#check_os_version_processor)           | Processor type or unknown                                                                                    |
| [status](#check_os_version_status)                 | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [total](#check_os_version_total)                   | Total number of items. Common option for all checks.                                                         |
| [warn_count](#check_os_version_warn_count)         | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_os_version_warn_list)           | A list of all items which matched the warning criteria. Common option for all checks.                        |


### check_uptime

Check time since last server re-boot.


* [Command-line Arguments](#check_uptime_options)
* [Filter keywords](#check_uptime_filter_keys)





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



<a name="check_uptime_filter"/>
**filter:**

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.





<a name="check_uptime_warning"/>
**warning:**

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



*Deafult Value:* | `uptime < 2d`



<a name="check_uptime_critical"/>
**critical:**

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



*Deafult Value:* | `uptime < 1d`



<a name="check_uptime_ok"/>
**ok:**

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.






<a name="check_uptime_empty-state"/>
**empty-state:**

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.


*Deafult Value:* | `ignored`



<a name="check_uptime_perf-config"/>
**perf-config:**

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)





<a name="check_uptime_top-syntax"/>
**top-syntax:**

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `${status}: ${list}`



<a name="check_uptime_ok-syntax"/>
**ok-syntax:**

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).





<a name="check_uptime_empty-syntax"/>
**empty-syntax:**

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.





<a name="check_uptime_detail-syntax"/>
**detail-syntax:**

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to excpae on linux).


*Deafult Value:* | `uptime: ${uptime}h, boot: ${boot} (UTC)`



<a name="check_uptime_perf-syntax"/>
**perf-syntax:**

Performance alias syntax.
This is the syntax for the base names of the performance data.


*Deafult Value:* | `uptime`




<a name="check_uptime_filter_keys"/>
#### Filter keywords


| Option                                       | Description                                                                                                  |
|----------------------------------------------|--------------------------------------------------------------------------------------------------------------|
| [boot](#check_uptime_boot)                   | System boot time                                                                                             |
| [count](#check_uptime_count)                 | Number of items matching the filter. Common option for all checks.                                           |
| [crit_count](#check_uptime_crit_count)       | Number of items matched the critical criteria. Common option for all checks.                                 |
| [crit_list](#check_uptime_crit_list)         | A list of all items which matched the critical criteria. Common option for all checks.                       |
| [detail_list](#check_uptime_detail_list)     | A special list with critical, then warning and finally ok. Common option for all checks.                     |
| [list](#check_uptime_list)                   | A list of all items which matched the filter. Common option for all checks.                                  |
| [ok_count](#check_uptime_ok_count)           | Number of items matched the ok criteria. Common option for all checks.                                       |
| [ok_list](#check_uptime_ok_list)             | A list of all items which matched the ok criteria. Common option for all checks.                             |
| [problem_count](#check_uptime_problem_count) | Number of items matched either warning or critical criteria. Common option for all checks.                   |
| [problem_list](#check_uptime_problem_list)   | A list of all items which matched either the critical or the warning criteria. Common option for all checks. |
| [status](#check_uptime_status)               | The returned status (OK/WARN/CRIT/UNKNOWN). Common option for all checks.                                    |
| [total](#check_uptime_total)                 | Total number of items. Common option for all checks.                                                         |
| [uptime](#check_uptime_uptime)               | Time since last boot                                                                                         |
| [warn_count](#check_uptime_warn_count)       | Number of items matched the warning criteria. Common option for all checks.                                  |
| [warn_list](#check_uptime_warn_list)         | A list of all items which matched the warning criteria. Common option for all checks.                        |




