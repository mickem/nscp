# CheckSystem

Various system related checks, such as CPU load, process state and memory.



## Enable module

To enable this module and and allow using the commands you need to ass `CheckSystem = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckSystem = enabled
```


## Queries

A quick reference for all available queries (check commands) in the CheckSystem module.

**List of commands:**

A list of all available queries (check commands)

| Command                               | Description                                                                  |
|---------------------------------------|------------------------------------------------------------------------------|
| [check_cpu](#check_cpu)               | Check that the load of the CPU(s) are within bounds.                         |
| [check_memory](#check_memory)         | Check free/used memory on the system.                                        |
| [check_os_version](#check_os_version) | Check the version of the underlying OS.                                      |
| [check_pagefile](#check_pagefile)     | Check the size of the system pagefile(s).                                    |
| [check_process](#check_process)       | Check state/metrics of one or more of the processes running on the computer. |
| [check_service](#check_service)       | Check the state of one or more of the computer services.                     |
| [check_uptime](#check_uptime)         | Check time since last server re-boot.                                        |




### check_cpu

Check that the load of the CPU(s) are within bounds.


**Jump to section:**

* [How CPU load is measured (historical buffer)](#check_cpu_buffer)
* [Sample Commands](#check_cpu_samples)
* [Command-line Arguments](#check_cpu_options)
* [Filter keywords](#check_cpu_filter_keys)


<a id="check_cpu_buffer"></a>
#### How CPU load is measured (historical buffer)

`check_cpu` does not measure the CPU load at the moment the check is executed. Instead NSClient++
runs a background collector thread that samples the CPU load roughly **once per second** and pushes
each sample into an in-memory ring buffer. Whenever you run `check_cpu` the values reported are
**averages computed from this buffer** for one or more time windows.

The time windows are controlled by the `time=` option. The default is to compute three averages:
`5m`, `1m` and `5s` (which is why the default output contains rows like `total 5m load`,
`total 1m load` and `total 5s load`). You can override this with one or more `time=` arguments,
for example `time=10m` or `time=30s time=2m`.

**Buffer size and configuration**

The size of the historical buffer is controlled by the `default buffer length` setting on the
CheckSystem section. The default is `1h`, meaning the last hour of samples is retained. The buffer
size puts an upper bound on the time windows you can use:

* If you ask for a window that is **shorter than or equal to** the buffer length, the result is the
  average of all samples collected during that window.
* If you ask for a window that is **longer than** the buffer length, the result will only cover the
  samples that are actually present in the buffer (effectively capped to the buffer length).
* If NSClient++ was started **less time ago than the requested window**, the result will only
  reflect the samples collected since startup. Right after start-up `5m` and `1m` averages will
  therefore be based on fewer samples than they normally would be.

If you need to check on longer windows (for example `2h` or `6h`) you must increase
`default buffer length` accordingly. Note that a larger buffer uses more memory, so only increase
it as far as you actually need.

**Impact on measurements**

Because every value reported by `check_cpu` is an average over a time window, the choice of `time=`
has a direct impact on what the check sees:

* **Short windows** (e.g. `5s`, `10s`) are very reactive and will show short spikes in CPU load,
  but they also produce a lot of noise. They are useful for catching transient bursts but can also
  generate flapping alerts.
* **Medium windows** (e.g. `1m`, `5m`) are a good compromise for most monitoring use cases. They
  smooth out short spikes while still reacting to sustained load within a few minutes.
* **Long windows** (e.g. `15m`, `1h`) smooth out almost all transients and only fire when the CPU
  has been busy for an extended period of time. They are well suited to detecting sustained load
  but will be slow to react and slow to recover.

A common pattern is to combine windows, for example warning on a long window and critical on a
short one (or vice versa), so that the check both catches sustained problems and ignores brief
spikes. The default check (`5m`, `1m`, `5s`) is an example of this approach.

Because the values are averages, they will not match the instantaneous CPU load shown by tools such
as `top` at the moment the check is executed, and very short spikes that fall between collection
ticks may be missed entirely.


<a id="check_cpu_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckSystem_check_cpu_samples.md)_

**Default check:**

```
check_cpu
CPU Load ok
'total 5m load'=0%;80;90 'total 1m load'=0%;80;90 'total 5s load'=7%;80;90
```

**Checking all cores by adding filter=none (disabling the default filter):**

```
check_cpu filter=none "warn=load > 80" "crit=load > 90"
CPU Load ok
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

**Adding kernel times to the check:**

```
check_cpu filter=none "warn=kernel > 10 or load > 80" "crit=load > 90" "top-syntax=${list}"
core 0 > 3, core 1 > 0, core 2 > 0, core  ... , core 7 > 15, total > 7
'core 0 5m kernel'=1%;10;0 'core 0 5m load'=3%;80;90 'core 1 5m kernel'=0%;10;0 'core 1 5m load'=0%;80;90 ...  'core 7 5s load'=15%;80;90 'total 5s kernel'=3%;10;0 'total 5s load'=7%;80;90
```

**Default check via NRPE:**

```
check_nscp --host 192.168.56.103 --command check_cpu
CPU Load ok|'total 5m'=16%;80;90 'total 1m'=13%;80;90 'total 5s'=13%;80;90
```


**Customizing the output syntax to include CPU load in text:**

```
check_cpu "top-syntax=%(status): %(list)"
L        cli OK: OK: 5m: 16%, 1m: 30%, 5s: 23%
```

**Customizing the output syntax to only show CPU load as text:**

```
check_cpu "top-syntax=%(status): Cpu usage is %(list)" time=5m "detail-syntax=%(load) %"
L        cli OK: OK: Cpu usage is 26 %
```



<a id="check_cpu_warn"></a>
<a id="check_cpu_crit"></a>
<a id="check_cpu_debug"></a>
<a id="check_cpu_show-all"></a>
<a id="check_cpu_escape-html"></a>
<a id="check_cpu_help"></a>
<a id="check_cpu_help-pb"></a>
<a id="check_cpu_show-default"></a>
<a id="check_cpu_help-short"></a>
<a id="check_cpu_time"></a>
<a id="check_cpu_cores"></a>
<a id="check_cpu_options"></a>
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
| cores                                     | N/A                        | This will remove the filter to include the cores, if you use filter don't use this as well.                      |



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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${time}: ${load}%`

<h5 id="check_cpu_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${core} ${time}`


<a id="check_cpu_filter_keys"></a>
#### Filter keywords


| Option  | Description                                               |
|---------|-----------------------------------------------------------|
| core    | The core to check (total or core ##)                      |
| core_id | The core to check (total or core_##)                      |
| idle    | The current idle load for a given core                    |
| kernel  | deprecated (use system instead)                           |
| load    | The current load for a given core (deprecated, use total) |
| system  | The current load used by the system (kernel)              |
| time    | The time frame to check                                   |
| user    | The current load used by user applications                |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_memory

Check free/used memory on the system.

#### Kinds of memory

There are several different kinds of memory that a computer system uses to manage data and processes. 
Here are the main types:

* `physical` Memory (RAM): This is the actual, tangible memory chips installed in your computer.  It's often referred to as RAM (Random Access Memory).
* `committed` Memory: Committed memory refers to the amount of virtual memory that has been reserved by processes. 
  When a program requests memory from the operating system, that memory is "committed."
  This committed memory is guaranteed to be available to the process, meaning Windows has set aside enough resources (either physical RAM or space in the page file) to back that memory.
* `virtual` Memory: Virtual memory is an abstraction layer created by the operating system (Windows) to provide a larger, contiguous address space to each process than the physical RAM actually available.


**Jump to section:**

* [Sample Commands](#check_memory_samples)
* [Command-line Arguments](#check_memory_options)
* [Filter keywords](#check_memory_filter_keys)


<a id="check_memory_samples"></a>
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

Most "byte" checks such as memory have an auto scaling feature which means values will go from 800M to 1.2G between checks.
Some graphing systems does not honor the units in performance data in which case you can get unexpected large values (such as 800G).
To remedy this you can lock the unit by adding `perf-config=*(unit:G)`

```
check_memory perf-config=*(unit:G)
page = 8.05G, physical = 7.85G
'page free'=15G;4;2 'page free %'=66%;19;9 'physical free'=4G;2;1 'physical free %'=34%;19;9
```



<a id="check_memory_warn"></a>
<a id="check_memory_crit"></a>
<a id="check_memory_debug"></a>
<a id="check_memory_show-all"></a>
<a id="check_memory_escape-html"></a>
<a id="check_memory_help"></a>
<a id="check_memory_help-pb"></a>
<a id="check_memory_show-default"></a>
<a id="check_memory_help-short"></a>
<a id="check_memory_type"></a>
<a id="check_memory_options"></a>
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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${type} = ${used}`

<h5 id="check_memory_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${type}`


<a id="check_memory_filter_keys"></a>
#### Filter keywords


| Option | Description                                     |
|--------|-------------------------------------------------|
| free   | Free memory in bytes (g,m,k,b) or percentages % |
| size   | Total size of memory                            |
| type   | The type of memory to check                     |
| used   | Used memory in bytes (g,m,k,b) or percentages % |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_os_version

Check the version of the underlying OS.


**Jump to section:**

* [Sample Commands](#check_os_version_samples)
* [Command-line Arguments](#check_os_version_options)
* [Filter keywords](#check_os_version_filter_keys)


<a id="check_os_version_samples"></a>
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




<a id="check_os_version_warn"></a>
<a id="check_os_version_crit"></a>
<a id="check_os_version_debug"></a>
<a id="check_os_version_show-all"></a>
<a id="check_os_version_escape-html"></a>
<a id="check_os_version_help"></a>
<a id="check_os_version_help-pb"></a>
<a id="check_os_version_show-default"></a>
<a id="check_os_version_help-short"></a>
<a id="check_os_version_options"></a>
#### Command-line Arguments


| Option                                           | Default Value                      | Description                                                                                                      |
|--------------------------------------------------|------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_os_version_filter)               |                                    | Filter which marks interesting items.                                                                            |
| [warning](#check_os_version_warning)             |                                    | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                    | Short alias for warning                                                                                          |
| [critical](#check_os_version_critical)           |                                    | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                    | Short alias for critical.                                                                                        |
| [ok](#check_os_version_ok)                       |                                    | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                                | Show debugging information in the log                                                                            |
| show-all                                         | N/A                                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_os_version_empty-state)     | ignored                            | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_os_version_perf-config)     |                                    | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                                | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                                | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                                | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                                | Show default values for a given command                                                                          |
| help-short                                       | N/A                                | Show help screen (short format).                                                                                 |
| [top-syntax](#check_os_version_top-syntax)       | ${status}: ${list}                 | Top level syntax.                                                                                                |
| [ok-syntax](#check_os_version_ok-syntax)         |                                    | ok syntax.                                                                                                       |
| [empty-syntax](#check_os_version_empty-syntax)   |                                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_os_version_detail-syntax) | ${kernel_name} (${kernel_release}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_os_version_perf-syntax)     | kernel_release                     | Performance alias syntax.                                                                                        |



<h5 id="check_os_version_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_os_version_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_os_version_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${kernel_name} (${kernel_release})`

<h5 id="check_os_version_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `kernel_release`


<a id="check_os_version_filter_keys"></a>
#### Filter keywords


| Option         | Description               |
|----------------|---------------------------|
| kernel_name    | Kernel name               |
| kernel_release | Kernel release            |
| kernel_version | Kernel version            |
| machine        | Machine hardware name     |
| nodename       | Network node hostname     |
| os             | Operating system          |
| processor      | Processor type or unknown |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_pagefile

Check the size of the system pagefile(s).


**Jump to section:**

* [Sample Commands](#check_pagefile_samples)
* [Command-line Arguments](#check_pagefile_options)
* [Filter keywords](#check_pagefile_filter_keys)


<a id="check_pagefile_samples"></a>
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

Getting help on available options::

```
check_pagefile help
...
  filter=ARG           Filter which marks interesting items.
					   Interesting items are items which will be included in
					   the check.
					   They do not denote warning or critical state but they
					   are checked use this to filter out unwanted items.
						   Available options:
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



<a id="check_pagefile_warn"></a>
<a id="check_pagefile_crit"></a>
<a id="check_pagefile_debug"></a>
<a id="check_pagefile_show-all"></a>
<a id="check_pagefile_escape-html"></a>
<a id="check_pagefile_help"></a>
<a id="check_pagefile_help-pb"></a>
<a id="check_pagefile_show-default"></a>
<a id="check_pagefile_help-short"></a>
<a id="check_pagefile_options"></a>
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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name} ${used} (${size})`

<h5 id="check_pagefile_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_pagefile_filter_keys"></a>
#### Filter keywords


| Option | Description                                     |
|--------|-------------------------------------------------|
| free   | Free memory in bytes (g,m,k,b) or percentages % |
| name   | The name of the page file (swap)                |
| size   | Total size of pagefile/swap                     |
| used   | Used memory in bytes (g,m,k,b) or percentages % |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_process

Check state/metrics of one or more of the processes running on the computer.


**Jump to section:**

* [Sample Commands](#check_process_samples)
* [Command-line Arguments](#check_process_options)
* [Filter keywords](#check_process_filter_keys)


<a id="check_process_samples"></a>
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
WARNING: Explorer.EXE ws:431.812MB, handles: 5639, user time:2535s
Performance data: 'explorer.exe ws_size'=73M;70;0
```

List all processes which use **more then 200m virtual memory** Default check **via NRPE**::

```
check_nrpe --host 192.168.56.103 --command check_process --arguments "filter=virtual > 200m"
OK all processes are ok.|'csrss.exe state'=1;0;0 'svchost.exe state'=1;0;0 'AvastSvc.exe state'=1;0;0 ...
```




<a id="check_process_warn"></a>
<a id="check_process_crit"></a>
<a id="check_process_debug"></a>
<a id="check_process_show-all"></a>
<a id="check_process_escape-html"></a>
<a id="check_process_help"></a>
<a id="check_process_help-pb"></a>
<a id="check_process_show-default"></a>
<a id="check_process_help-short"></a>
<a id="check_process_process"></a>
<a id="check_process_total"></a>
<a id="check_process_options"></a>
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
| process                                       |                                  | The process to check, set this to * to check all processes                                                       |
| total                                         | N/A                              | Include the total of all matching processes                                                                      |



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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${exe}=${state}`

<h5 id="check_process_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${exe}`


<a id="check_process_filter_keys"></a>
#### Filter keywords


| Option       | Description                                      |
|--------------|--------------------------------------------------|
| command_line | Command line of process                          |
| error        | Any error messages associated with fetching info |
| exe          | The name of the executable                       |
| filename     | Name of process (with path)                      |
| kernel       | Kernel time in seconds                           |
| page_faults  | Page fault count                                 |
| pid          | Process id                                       |
| started      | Process is started                               |
| state        | The current state (started, stopped, hung)       |
| stopped      | Process is stopped                               |
| time         | User + kernel time in seconds                    |
| user         | User time in seconds                             |
| virtual      | Virtual size in bytes                            |
| working_set  | Working set (RSS) in bytes                       |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_service

Check the state of one or more of the computer services.


**Jump to section:**

* [Sample Commands](#check_service_samples)
* [Command-line Arguments](#check_service_options)
* [Filter keywords](#check_service_filter_keys)


<a id="check_service_samples"></a>
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

Excluding nscp2 with substring like matching filter:
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



<a id="check_service_warn"></a>
<a id="check_service_crit"></a>
<a id="check_service_debug"></a>
<a id="check_service_show-all"></a>
<a id="check_service_escape-html"></a>
<a id="check_service_help"></a>
<a id="check_service_help-pb"></a>
<a id="check_service_show-default"></a>
<a id="check_service_help-short"></a>
<a id="check_service_service"></a>
<a id="check_service_exclude"></a>
<a id="check_service_options"></a>
#### Command-line Arguments


| Option                                        | Default Value                                   | Description                                                                                                      |
|-----------------------------------------------|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_service_filter)               |                                                 | Filter which marks interesting items.                                                                            |
| [warning](#check_service_warning)             | not state_is_perfect()                          | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                                 | Short alias for warning                                                                                          |
| [critical](#check_service_critical)           | not state_is_ok()                               | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                                 | Short alias for critical.                                                                                        |
| [ok](#check_service_ok)                       |                                                 | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                             | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_service_empty-state)     | unknown                                         | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_service_perf-config)     |                                                 | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                             | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                             | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                             | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                             | Show default values for a given command                                                                          |
| help-short                                    | N/A                                             | Show help screen (short format).                                                                                 |
| [top-syntax](#check_service_top-syntax)       | ${status}: ${crit_list}, delayed (${warn_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_service_ok-syntax)         | %(status): All %(count) service(s) are ok.      | ok syntax.                                                                                                       |
| [empty-syntax](#check_service_empty-syntax)   | %(status): No services found                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_service_detail-syntax) | ${name}=${state} (${start_type})                | Detail level syntax.                                                                                             |
| [perf-syntax](#check_service_perf-syntax)     | ${name}                                         | Performance alias syntax.                                                                                        |
| service                                       |                                                 | The service to check, set this to * to check all services                                                        |
| exclude                                       |                                                 | A list of services to ignore (mainly useful in combination with service=*)                                       |
| [state](#check_service_state)                 | all                                             | The state of services to enumerate: active, inactive, failed, or all                                             |



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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name}=${state} (${start_type})`

<h5 id="check_service_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`

<h5 id="check_service_state">state:</h5>

The state of services to enumerate: active, inactive, failed, or all

*Default Value:* `all`


<a id="check_service_filter_keys"></a>
#### Filter keywords


| Option             | Description                                                                                         |
|--------------------|-----------------------------------------------------------------------------------------------------|
| desc               | Service description                                                                                 |
| name               | Service name                                                                                        |
| pid                | Process id                                                                                          |
| start_type         | The configured start type (enabled, disabled, static, masked)                                       |
| started            | Service is started/active                                                                           |
| state              | The current state (active, inactive, failed)                                                        |
| state_is_ok()      | Check if the state is ok (enabled services running or starting, disabled services can be any state) |
| state_is_perfect() | Check if the state is perfect (enabled services running, disabled services stopped)                 |
| stopped            | Service is stopped/inactive                                                                         |
| sub_state          | Service sub-state (running, dead, exited, etc.)                                                     |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_uptime

Check time since last server re-boot.


**Jump to section:**

* [Sample Commands](#check_uptime_samples)
* [Command-line Arguments](#check_uptime_options)
* [Filter keywords](#check_uptime_filter_keys)


<a id="check_uptime_samples"></a>
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




<a id="check_uptime_warn"></a>
<a id="check_uptime_crit"></a>
<a id="check_uptime_debug"></a>
<a id="check_uptime_show-all"></a>
<a id="check_uptime_escape-html"></a>
<a id="check_uptime_help"></a>
<a id="check_uptime_help-pb"></a>
<a id="check_uptime_show-default"></a>
<a id="check_uptime_help-short"></a>
<a id="check_uptime_options"></a>
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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `uptime: ${uptime}h, boot: ${boot} (UTC)`

<h5 id="check_uptime_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `uptime`


<a id="check_uptime_filter_keys"></a>
#### Filter keywords


| Option | Description          |
|--------|----------------------|
| boot   | System boot time     |
| uptime | Time since last boot |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |




