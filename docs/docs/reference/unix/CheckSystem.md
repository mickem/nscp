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

| Command                                                 | Description                                                                                        |
|---------------------------------------------------------|----------------------------------------------------------------------------------------------------|
| [check_battery](#check_battery)                         | Check battery charge level, power source and health.                                               |
| [check_cpu](#check_cpu)                                 | Check that the load of the CPU(s) are within bounds.                                               |
| [check_cpu_frequency](#check_cpu_frequency)             | Check the CPU clock frequency (current vs max) per core.                                           |
| [check_cpu_utilization](#check_cpu_utilization)         | Check CPU utilization broken down by user/system/iowait/steal/guest.                               |
| [check_kernel_stats](#check_kernel_stats)               | Check kernel activity: context-switch rate, fork rate and live thread count.                       |
| [check_load](#check_load)                               | Check the system load average (1/5/15 minutes).                                                    |
| [check_memory](#check_memory)                           | Check free/used memory on the system.                                                              |
| [check_network](#check_network)                         | Check network interface status and throughput.                                                     |
| [check_os_updates](#check_os_updates)                   | Check for available OS package updates via the system package manager (apt/dnf/yum/zypper/pacman). |
| [check_os_version](#check_os_version)                   | Check the version of the underlying OS.                                                            |
| [check_pagefile](#check_pagefile)                       | Check the size of the system pagefile(s).                                                          |
| [check_process](#check_process)                         | Check state/metrics of one or more of the processes running on the computer.                       |
| [check_process_history](#check_process_history)         | Check the history of processes seen since NSClient++ started (requires 'process history = true').  |
| [check_process_history_new](#check_process_history_new) | Check for processes first seen within a recent time window (requires 'process history = true').    |
| [check_service](#check_service)                         | Check the state of one or more of the computer services.                                           |
| [check_swap_io](#check_swap_io)                         | Check the swap in/out paging rate.                                                                 |
| [check_temperature](#check_temperature)                 | Check temperature sensors (thermal zones / hwmon).                                                 |
| [check_uptime](#check_uptime)                           | Check time since last server re-boot.                                                              |




### check_battery

Check battery charge level, power source and health.


**Jump to section:**

* [Command-line Arguments](#check_battery_options)
* [Filter keywords](#check_battery_filter_keys)





<a id="check_battery_warn"></a>
<a id="check_battery_crit"></a>
<a id="check_battery_debug"></a>
<a id="check_battery_show-all"></a>
<a id="check_battery_escape-html"></a>
<a id="check_battery_help"></a>
<a id="check_battery_help-pb"></a>
<a id="check_battery_show-default"></a>
<a id="check_battery_help-short"></a>
<a id="check_battery_options"></a>
#### Command-line Arguments


| Option                                        | Default Value                                    | Description                                                                                                      |
|-----------------------------------------------|--------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_battery_filter)               | battery_present = 'true'                         | Filter which marks interesting items.                                                                            |
| [warning](#check_battery_warning)             | charge < 20                                      | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                                  | Short alias for warning                                                                                          |
| [critical](#check_battery_critical)           | charge < 10                                      | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                                  | Short alias for critical.                                                                                        |
| [ok](#check_battery_ok)                       |                                                  | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                              | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                              | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_battery_empty-state)     | warning                                          | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_battery_perf-config)     |                                                  | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                              | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                              | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                              | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                              | Show default values for a given command                                                                          |
| help-short                                    | N/A                                              | Show help screen (short format).                                                                                 |
| [top-syntax](#check_battery_top-syntax)       | ${status}: ${list}                               | Top level syntax.                                                                                                |
| [ok-syntax](#check_battery_ok-syntax)         | %(status): No battery found or all batteries ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_battery_empty-syntax)   | No battery found                                 | Empty syntax.                                                                                                    |
| [detail-syntax](#check_battery_detail-syntax) | ${name}: ${charge}% (${power_source}, ${status}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_battery_perf-syntax)     | ${name}                                          | Performance alias syntax.                                                                                        |



<h5 id="check_battery_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

*Default Value:* `battery_present = 'true'`

<h5 id="check_battery_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `charge < 20`

<h5 id="check_battery_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `charge < 10`

<h5 id="check_battery_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_battery_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `warning`

<h5 id="check_battery_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_battery_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_battery_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): No battery found or all batteries ok.`

<h5 id="check_battery_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No battery found`

<h5 id="check_battery_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name}: ${charge}% (${power_source}, ${status})`

<h5 id="check_battery_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_battery_filter_keys"></a>
#### Filter keywords


| Option             | Description                                                  |
|--------------------|--------------------------------------------------------------|
| battery_present    | Whether a battery is present: 'true' or 'false'              |
| charge             | Battery charge percent                                       |
| charge_rate        | Current charge rate in mW (when charging)                    |
| design_capacity    | Design capacity in mWh                                       |
| discharge_rate     | Current discharge rate in mW (when discharging)              |
| full_capacity      | Current full charge capacity in mWh                          |
| health             | Battery health percent (full/design capacity)                |
| name               | Battery name                                                 |
| power_source       | Power source: ac/battery/unknown                             |
| present            | Alias for battery_present                                    |
| remaining_capacity | Current remaining capacity in mWh                            |
| time_remaining     | Estimated time remaining in seconds (-1 if unknown or on AC) |

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


### check_cpu

Check that the load of the CPU(s) are within bounds.

#### How CPU load is measured (historical buffer)

`check_cpu` does not measure the CPU load at the moment the check is executed. Instead, NSClient++
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


**Jump to section:**

* [Sample Commands](#check_cpu_samples)
* [Command-line Arguments](#check_cpu_options)
* [Filter keywords](#check_cpu_filter_keys)


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


### check_cpu_frequency

Check the CPU clock frequency (current vs max) per core.


**Jump to section:**

* [Command-line Arguments](#check_cpu_frequency_options)





<a id="check_cpu_frequency_options"></a>
#### Command-line Arguments






### check_cpu_utilization

Check CPU utilization broken down by user/system/iowait/steal/guest.


**Jump to section:**

* [Command-line Arguments](#check_cpu_utilization_options)
* [Filter keywords](#check_cpu_utilization_filter_keys)





<a id="check_cpu_utilization_warn"></a>
<a id="check_cpu_utilization_crit"></a>
<a id="check_cpu_utilization_debug"></a>
<a id="check_cpu_utilization_show-all"></a>
<a id="check_cpu_utilization_escape-html"></a>
<a id="check_cpu_utilization_help"></a>
<a id="check_cpu_utilization_help-pb"></a>
<a id="check_cpu_utilization_show-default"></a>
<a id="check_cpu_utilization_help-short"></a>
<a id="check_cpu_utilization_options"></a>
#### Command-line Arguments


| Option                                                | Default Value                                                                        | Description                                                                                                      |
|-------------------------------------------------------|--------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_cpu_utilization_filter)               |                                                                                      | Filter which marks interesting items.                                                                            |
| [warning](#check_cpu_utilization_warning)             | total > 90                                                                           | Filter which marks items which generates a warning state.                                                        |
| warn                                                  |                                                                                      | Short alias for warning                                                                                          |
| [critical](#check_cpu_utilization_critical)           | total > 95                                                                           | Filter which marks items which generates a critical state.                                                       |
| crit                                                  |                                                                                      | Short alias for critical.                                                                                        |
| [ok](#check_cpu_utilization_ok)                       |                                                                                      | Filter which marks items which generates an ok state.                                                            |
| debug                                                 | N/A                                                                                  | Show debugging information in the log                                                                            |
| show-all                                              | N/A                                                                                  | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_cpu_utilization_empty-state)     | ignored                                                                              | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_cpu_utilization_perf-config)     |                                                                                      | Performance data generation configuration                                                                        |
| escape-html                                           | N/A                                                                                  | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                                  | N/A                                                                                  | Show help screen (this screen)                                                                                   |
| help-pb                                               | N/A                                                                                  | Show help screen as a protocol buffer payload                                                                    |
| show-default                                          | N/A                                                                                  | Show default values for a given command                                                                          |
| help-short                                            | N/A                                                                                  | Show help screen (short format).                                                                                 |
| [top-syntax](#check_cpu_utilization_top-syntax)       | ${status}: ${list}                                                                   | Top level syntax.                                                                                                |
| [ok-syntax](#check_cpu_utilization_ok-syntax)         |                                                                                      | ok syntax.                                                                                                       |
| [empty-syntax](#check_cpu_utilization_empty-syntax)   |                                                                                      | Empty syntax.                                                                                                    |
| [detail-syntax](#check_cpu_utilization_detail-syntax) | user: ${user}% system: ${system}% iowait: ${iowait}% steal: ${steal}% idle: ${idle}% | Detail level syntax.                                                                                             |
| [perf-syntax](#check_cpu_utilization_perf-syntax)     | cpu                                                                                  | Performance alias syntax.                                                                                        |



<h5 id="check_cpu_utilization_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_cpu_utilization_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `total > 90`

<h5 id="check_cpu_utilization_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `total > 95`

<h5 id="check_cpu_utilization_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_cpu_utilization_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_cpu_utilization_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_cpu_utilization_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_cpu_utilization_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_cpu_utilization_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_cpu_utilization_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `user: ${user}% system: ${system}% iowait: ${iowait}% steal: ${steal}% idle: ${idle}%`

<h5 id="check_cpu_utilization_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `cpu`


<a id="check_cpu_utilization_filter_keys"></a>
#### Filter keywords


| Option  | Description                                         |
|---------|-----------------------------------------------------|
| guest   | Guest (incl. guest_nice) CPU utilization in percent |
| idle    | Idle CPU in percent                                 |
| iowait  | I/O-wait CPU utilization in percent                 |
| irq     | Hardware-interrupt CPU utilization in percent       |
| name    | Always 'total' (single aggregate row)               |
| softirq | Soft-interrupt CPU utilization in percent           |
| steal   | Stolen (hypervisor) CPU utilization in percent      |
| system  | System/kernel CPU utilization in percent            |
| user    | User (incl. nice) CPU utilization in percent        |

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


### check_kernel_stats

Check kernel activity: context-switch rate, fork rate and live thread count.


**Jump to section:**

* [Command-line Arguments](#check_kernel_stats_options)
* [Filter keywords](#check_kernel_stats_filter_keys)





<a id="check_kernel_stats_warn"></a>
<a id="check_kernel_stats_crit"></a>
<a id="check_kernel_stats_debug"></a>
<a id="check_kernel_stats_show-all"></a>
<a id="check_kernel_stats_escape-html"></a>
<a id="check_kernel_stats_help"></a>
<a id="check_kernel_stats_help-pb"></a>
<a id="check_kernel_stats_show-default"></a>
<a id="check_kernel_stats_help-short"></a>
<a id="check_kernel_stats_type"></a>
<a id="check_kernel_stats_options"></a>
#### Command-line Arguments


| Option                                             | Default Value                        | Description                                                                                                      |
|----------------------------------------------------|--------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_kernel_stats_filter)               |                                      | Filter which marks interesting items.                                                                            |
| [warning](#check_kernel_stats_warning)             | name = 'threads' and current > 8000  | Filter which marks items which generates a warning state.                                                        |
| warn                                               |                                      | Short alias for warning                                                                                          |
| [critical](#check_kernel_stats_critical)           | name = 'threads' and current > 10000 | Filter which marks items which generates a critical state.                                                       |
| crit                                               |                                      | Short alias for critical.                                                                                        |
| [ok](#check_kernel_stats_ok)                       |                                      | Filter which marks items which generates an ok state.                                                            |
| debug                                              | N/A                                  | Show debugging information in the log                                                                            |
| show-all                                           | N/A                                  | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_kernel_stats_empty-state)     | ignored                              | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_kernel_stats_perf-config)     |                                      | Performance data generation configuration                                                                        |
| escape-html                                        | N/A                                  | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                               | N/A                                  | Show help screen (this screen)                                                                                   |
| help-pb                                            | N/A                                  | Show help screen as a protocol buffer payload                                                                    |
| show-default                                       | N/A                                  | Show default values for a given command                                                                          |
| help-short                                         | N/A                                  | Show help screen (short format).                                                                                 |
| [top-syntax](#check_kernel_stats_top-syntax)       | ${status} - ${list}                  | Top level syntax.                                                                                                |
| [ok-syntax](#check_kernel_stats_ok-syntax)         |                                      | ok syntax.                                                                                                       |
| [empty-syntax](#check_kernel_stats_empty-syntax)   |                                      | Empty syntax.                                                                                                    |
| [detail-syntax](#check_kernel_stats_detail-syntax) | ${label} ${human}                    | Detail level syntax.                                                                                             |
| [perf-syntax](#check_kernel_stats_perf-syntax)     | ${name}                              | Performance alias syntax.                                                                                        |
| type                                               |                                      | Select metric type(s) to show: ctxt, processes or threads (repeatable; default: all)                             |



<h5 id="check_kernel_stats_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_kernel_stats_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `name = 'threads' and current > 8000`

<h5 id="check_kernel_stats_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `name = 'threads' and current > 10000`

<h5 id="check_kernel_stats_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_kernel_stats_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_kernel_stats_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_kernel_stats_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status} - ${list}`

<h5 id="check_kernel_stats_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_kernel_stats_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_kernel_stats_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${label} ${human}`

<h5 id="check_kernel_stats_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_kernel_stats_filter_keys"></a>
#### Filter keywords


| Option  | Description                                             |
|---------|---------------------------------------------------------|
| current | Current raw value (cumulative counter, or thread count) |
| human   | Human-readable value                                    |
| label   | Human-friendly metric label                             |
| name    | Metric name: ctxt, processes or threads                 |
| rate    | Per-second rate (0 for the threads row)                 |

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


### check_load

Check the system load average (1/5/15 minutes).


**Jump to section:**

* [Command-line Arguments](#check_load_options)
* [Filter keywords](#check_load_filter_keys)





<a id="check_load_warn"></a>
<a id="check_load_crit"></a>
<a id="check_load_debug"></a>
<a id="check_load_show-all"></a>
<a id="check_load_escape-html"></a>
<a id="check_load_help"></a>
<a id="check_load_help-pb"></a>
<a id="check_load_show-default"></a>
<a id="check_load_help-short"></a>
<a id="check_load_options"></a>
#### Command-line Arguments


| Option                                     | Default Value                                       | Description                                                                                                      |
|--------------------------------------------|-----------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_load_filter)               |                                                     | Filter which marks interesting items.                                                                            |
| [warning](#check_load_warning)             |                                                     | Filter which marks items which generates a warning state.                                                        |
| warn                                       |                                                     | Short alias for warning                                                                                          |
| [critical](#check_load_critical)           |                                                     | Filter which marks items which generates a critical state.                                                       |
| crit                                       |                                                     | Short alias for critical.                                                                                        |
| [ok](#check_load_ok)                       |                                                     | Filter which marks items which generates an ok state.                                                            |
| debug                                      | N/A                                                 | Show debugging information in the log                                                                            |
| show-all                                   | N/A                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_load_empty-state)     | ignored                                             | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_load_perf-config)     |                                                     | Performance data generation configuration                                                                        |
| escape-html                                | N/A                                                 | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                       | N/A                                                 | Show help screen (this screen)                                                                                   |
| help-pb                                    | N/A                                                 | Show help screen as a protocol buffer payload                                                                    |
| show-default                               | N/A                                                 | Show default values for a given command                                                                          |
| help-short                                 | N/A                                                 | Show help screen (short format).                                                                                 |
| [top-syntax](#check_load_top-syntax)       | ${status}: ${list}                                  | Top level syntax.                                                                                                |
| [ok-syntax](#check_load_ok-syntax)         |                                                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_load_empty-syntax)   |                                                     | Empty syntax.                                                                                                    |
| [detail-syntax](#check_load_detail-syntax) | ${type} load average: ${load1}, ${load5}, ${load15} | Detail level syntax.                                                                                             |
| [perf-syntax](#check_load_perf-syntax)     | ${type}                                             | Performance alias syntax.                                                                                        |
| [percpu](#check_load_percpu)               | 1)] (=0                                             | Divide the load averages by the number of CPUs (reports the 'scaled' per-core load)                              |



<h5 id="check_load_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_load_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_load_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_load_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_load_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_load_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_load_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_load_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_load_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_load_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${type} load average: ${load1}, ${load5}, ${load15}`

<h5 id="check_load_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${type}`

<h5 id="check_load_percpu">percpu:</h5>

Divide the load averages by the number of CPUs (reports the 'scaled' per-core load)

*Default Value:* `1)] (=0`


<a id="check_load_filter_keys"></a>
#### Filter keywords


| Option        | Description                                             |
|---------------|---------------------------------------------------------|
| load          | The largest of load1, load5 and load15                  |
| load1         | Load average over the last 1 minute                     |
| load15        | Load average over the last 15 minutes                   |
| load5         | Load average over the last 5 minutes                    |
| procs_running | Number of currently runnable kernel scheduling entities |
| procs_total   | Total number of kernel scheduling entities              |
| type          | 'total' or (with --percpu) 'scaled'                     |

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


### check_network

Check network interface status and throughput.

#### Choosing a mode

`check_network` collects throughput counters from one of two WMI classes.
Which one to use is controlled by the `mode=` argument:

* `interface` (default) — Counters are read from `Win32_PerfRawData_Tcpip_NetworkInterface`.
  This is the historical behaviour and reports one row per *physical* network
  interface. NIC team aggregates (the virtual adapter that represents the team
  as a whole) are **not** visible here, only the underlying physical adapters.
  Use this mode unless you specifically need team statistics — it preserves the
  output that existing dashboards and thresholds were built against.

* `adapter` — Counters are read from `Win32_PerfRawData_Tcpip_NetworkAdapter`.
  This superset also reports the aggregated team adapter (e.g. a row called
  "Production Network" alongside the physical team members), which is what you
  want when monitoring traffic across an LBFO/Switch-Embedded team rather than
  individual ports. Note that the friendly Windows adapter name is used here,
  so interface names may differ slightly from `interface` mode for the same
  physical NIC.

* `both` — Every adapter is reported twice, once from each source. The `source`
  filter keyword (`source = 'interface'` or `source = 'adapter'`) can then be
  used inside warning/critical/filter expressions to distinguish them. This
  mode is mainly useful when you want to alert on the team aggregate *and* the
  individual members from one check.

A `source` filter keyword is available in every mode (its value is the literal
string `interface` or `adapter`), so you can write expressions such as
`source = 'adapter' and total > 100000000` to scope thresholds to a particular
source.

##### Identifying teamed adapters

Team aggregates only have perfraw data; they have no matching
`Win32_NetworkAdapter` row, so `MAC`, `speed`, `enabled` and
`net_connection_id` are empty for them. You can identify them with
`MAC = ''` in a filter expression when running in `adapter` or `both` mode.

#### Link speed and percent-usage values are best-effort

The `speed`, `speed_bps`, `usage_in`, `usage_out` and `usage_total`
variables all derive from the Windows `Win32_NetworkAdapter.Speed`
property. That property is the **negotiated link speed**, which is not
always the same thing as the actual usable throughput. The check reports
what Windows tells it; it does not measure the link.

**When the reported speed is unreliable or missing:**

* **Virtual adapters** (VPN tunnels, loopback, Hyper-V vNICs, some VMware
  paravirtual NICs) frequently report `Speed` as "Unknown" or empty. The
  check stores `speed_bps = 0` for those.
* **NIC team aggregates** (visible in `mode=adapter` or `mode=both`) may
  report `0`, `~0ULL`, or an arithmetically inconsistent value depending
  on the driver and team mode (LACP vs switch-independent vs static).
  Sometimes the team aggregate's Speed is the *sum* of member-link speeds;
  sometimes it is a *single* member-link's speed.
* **Wireless adapters** typically report the negotiated PHY rate (for
  example 866 Mbps for 802.11ac). Real-world throughput is usually
  40-60% of that because of MAC overhead, retransmits and rate adaptation,
  so a saturated wireless link may read as ~50% in `usage_*` rather than
  the 100% you'd expect.
* **Drivers that report a stale value** during link renegotiation can
  briefly show the wrong rate immediately after a cable change or
  speed switch.

**Variables affected by this:**

| Variable      | Best-effort behaviour when Speed is unknown                 |
|---------------|-------------------------------------------------------------|
| `speed`       | Raw string from WMI - may be `"Unknown"` or empty           |
| `speed_bps`   | Reads as `0` (the "unknown" sentinel)                       |
| `usage_in`    | Reads as `0` - indistinguishable from a genuinely idle link |
| `usage_out`   | Reads as `0` - indistinguishable from a genuinely idle link |
| `usage_total` | Reads as `0` - indistinguishable from a genuinely idle link |

The byte-rate variables (`received`, `sent`, `total`) and their
`*_human` companions are **not** derived from `Speed` and are unaffected
by these caveats. They come straight from
`Win32_PerfRawData_Tcpip_NetworkInterface` / `NetworkAdapter` cumulative
counters.

**Writing reliable percent-based alerts:**

The `0`-when-unknown sentinel was chosen so dashboards and `<`-style
alert rules behave naturally without special-casing. The trade-off is
that an unknown-speed link looks identical to a genuinely idle one. If
you need to distinguish them, filter on `speed_bps > 0` *before*
applying the percent threshold:

```
check_network "filter=speed_bps > 0" \
              "warning=usage_total > 80" \
              "critical=usage_total > 95"
```

For environments where percent thresholds are not viable (mixed wireless,
heavy NIC-team use, lots of virtual adapters), prefer absolute byte-rate
thresholds against `received`/`sent`/`total`, scoped to specific
interfaces by name:

```
check_network "filter=name = 'Ethernet 1'" \
              "warning=total > 800000000" \
              "critical=total > 950000000"
```

Both styles can be combined in a single check by using `filter` to scope
which interfaces participate, then `warning`/`critical` to set the
threshold.


**Jump to section:**

* [Command-line Arguments](#check_network_options)
* [Filter keywords](#check_network_filter_keys)





<a id="check_network_warn"></a>
<a id="check_network_crit"></a>
<a id="check_network_debug"></a>
<a id="check_network_show-all"></a>
<a id="check_network_escape-html"></a>
<a id="check_network_help"></a>
<a id="check_network_help-pb"></a>
<a id="check_network_show-default"></a>
<a id="check_network_help-short"></a>
<a id="check_network_options"></a>
#### Command-line Arguments


| Option                                        | Default Value                                 | Description                                                                                                      |
|-----------------------------------------------|-----------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_network_filter)               |                                               | Filter which marks interesting items.                                                                            |
| [warning](#check_network_warning)             | total > 10000                                 | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                               | Short alias for warning                                                                                          |
| [critical](#check_network_critical)           | total > 100000                                | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                               | Short alias for critical.                                                                                        |
| [ok](#check_network_ok)                       |                                               | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                           | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                           | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_network_empty-state)     | critical                                      | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_network_perf-config)     |                                               | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                           | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                           | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                           | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                           | Show default values for a given command                                                                          |
| help-short                                    | N/A                                           | Show help screen (short format).                                                                                 |
| [top-syntax](#check_network_top-syntax)       | ${status}: ${list}                            | Top level syntax.                                                                                                |
| [ok-syntax](#check_network_ok-syntax)         | %(status): Network interfaces seem ok.        | ok syntax.                                                                                                       |
| [empty-syntax](#check_network_empty-syntax)   |                                               | Empty syntax.                                                                                                    |
| [detail-syntax](#check_network_detail-syntax) | ${name} >${sent_human}/s <${received_human}/s | Detail level syntax.                                                                                             |
| [perf-syntax](#check_network_perf-syntax)     | ${name}                                       | Performance alias syntax.                                                                                        |



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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name} >${sent_human}/s <${received_human}/s`

<h5 id="check_network_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_network_filter_keys"></a>
#### Filter keywords


| Option           | Description                                                           |
|------------------|-----------------------------------------------------------------------|
| MAC              | The hardware (MAC) address                                            |
| enabled          | True if the interface link is up                                      |
| name             | Network interface name                                                |
| received         | Bytes received per second                                             |
| received_human   | Bytes received per second (human readable, auto-scaled)               |
| received_packets | Packets received per second                                           |
| rx_errors        | Cumulative receive errors since boot                                  |
| sent             | Bytes sent per second                                                 |
| sent_human       | Bytes sent per second (human readable, auto-scaled)                   |
| sent_packets     | Packets sent per second                                               |
| speed_bps        | Link speed in bits/sec (0 when unknown, e.g. virtual interfaces)      |
| total_human      | Bytes total per second (human readable, auto-scaled)                  |
| tx_errors        | Cumulative transmit errors since boot                                 |
| usage_in         | Percent of link speed used by received traffic (0 when speed unknown) |
| usage_out        | Percent of link speed used by sent traffic (0 when speed unknown)     |
| usage_total      | Percent of link speed used by total traffic (0 when speed unknown)    |

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


### check_os_updates

Check for available OS package updates via the system package manager (apt/dnf/yum/zypper/pacman).

#### Checking for Windows Updates

The `check_os_updates` command allows you to monitor for missing Windows updates via the Windows Update Agent (WUA) API. You can filter the results based on severity, reboot requirements, and other attributes. 

**Basic usage**

To simply check if there are any pending updates:

```
check_os_updates
```

If there are any pending updates, this will return a warning state by default (because the default `warning` filter is `count > 0`).

**Checking for critical updates**

Often, you only want to be alerted if there are *security* or *critical* updates missing. You can configure this using the `warning` and `critical` filters:

```
check_os_updates "warning=important > 0" "critical=security > 0 or critical > 0"
```

This will return `WARNING` if there are updates with the 'Important' severity, and `CRITICAL` if there are any security updates or updates explicitly marked 'Critical'.

**Checking if a reboot is required**

If you want to know if the system needs a reboot after installing updates:

```
check_os_updates "warning=reboot_required > 0"
```

**Customizing the output**

You can use the syntax options to format the output string:

```
check_os_updates "top-syntax=${status}: Found ${count} missing updates. Security: ${security}, Critical: ${critical}" "detail-syntax=${titles}" show-all
```


**Jump to section:**

* [Command-line Arguments](#check_os_updates_options)
* [Filter keywords](#check_os_updates_filter_keys)





<a id="check_os_updates_warn"></a>
<a id="check_os_updates_crit"></a>
<a id="check_os_updates_debug"></a>
<a id="check_os_updates_show-all"></a>
<a id="check_os_updates_escape-html"></a>
<a id="check_os_updates_help"></a>
<a id="check_os_updates_help-pb"></a>
<a id="check_os_updates_show-default"></a>
<a id="check_os_updates_help-short"></a>
<a id="check_os_updates_options"></a>
#### Command-line Arguments


| Option                                           | Default Value                                                               | Description                                                                                                      |
|--------------------------------------------------|-----------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_os_updates_filter)               |                                                                             | Filter which marks interesting items.                                                                            |
| [warning](#check_os_updates_warning)             | count > 0                                                                   | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                                                             | Short alias for warning                                                                                          |
| [critical](#check_os_updates_critical)           | security > 0                                                                | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                                                             | Short alias for critical.                                                                                        |
| [ok](#check_os_updates_ok)                       |                                                                             | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                                                                         | Show debugging information in the log                                                                            |
| show-all                                         | N/A                                                                         | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_os_updates_empty-state)     | ok                                                                          | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_os_updates_perf-config)     |                                                                             | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                                                                         | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                                                                         | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                                                                         | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                                                                         | Show default values for a given command                                                                          |
| help-short                                       | N/A                                                                         | Show help screen (short format).                                                                                 |
| [top-syntax](#check_os_updates_top-syntax)       | ${status}: ${count} updates available (${security} security) via ${manager} | Top level syntax.                                                                                                |
| [ok-syntax](#check_os_updates_ok-syntax)         | %(status): No updates available.                                            | ok syntax.                                                                                                       |
| [empty-syntax](#check_os_updates_empty-syntax)   |                                                                             | Empty syntax.                                                                                                    |
| [detail-syntax](#check_os_updates_detail-syntax) | ${count} updates (${security} security) via ${manager}                      | Detail level syntax.                                                                                             |
| [perf-syntax](#check_os_updates_perf-syntax)     | updates                                                                     | Performance alias syntax.                                                                                        |



<h5 id="check_os_updates_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_os_updates_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `count > 0`

<h5 id="check_os_updates_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `security > 0`

<h5 id="check_os_updates_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_os_updates_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_os_updates_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_os_updates_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${count} updates available (${security} security) via ${manager}`

<h5 id="check_os_updates_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): No updates available.`

<h5 id="check_os_updates_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_os_updates_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${count} updates (${security} security) via ${manager}`

<h5 id="check_os_updates_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `updates`


<a id="check_os_updates_filter_keys"></a>
#### Filter keywords


| Option   | Description                                       |
|----------|---------------------------------------------------|
| manager  | Package manager used to query updates             |
| packages | Comma separated list of available package updates |
| security | Number of available security updates              |

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


| Option                                           | Default Value                    | Description                                                                                                      |
|--------------------------------------------------|----------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_os_version_filter)               |                                  | Filter which marks interesting items.                                                                            |
| [warning](#check_os_version_warning)             |                                  | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                  | Short alias for warning                                                                                          |
| [critical](#check_os_version_critical)           |                                  | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                  | Short alias for critical.                                                                                        |
| [ok](#check_os_version_ok)                       |                                  | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                              | Show debugging information in the log                                                                            |
| show-all                                         | N/A                              | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_os_version_empty-state)     | ignored                          | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_os_version_perf-config)     |                                  | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                              | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                              | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                              | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                              | Show default values for a given command                                                                          |
| help-short                                       | N/A                              | Show help screen (short format).                                                                                 |
| [top-syntax](#check_os_version_top-syntax)       | ${status}: ${list}               | Top level syntax.                                                                                                |
| [ok-syntax](#check_os_version_ok-syntax)         |                                  | ok syntax.                                                                                                       |
| [empty-syntax](#check_os_version_empty-syntax)   |                                  | Empty syntax.                                                                                                    |
| [detail-syntax](#check_os_version_detail-syntax) | ${os} (kernel ${kernel_release}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_os_version_perf-syntax)     | kernel_release                   | Performance alias syntax.                                                                                        |



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

*Default Value:* `${os} (kernel ${kernel_release})`

<h5 id="check_os_version_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `kernel_release`


<a id="check_os_version_filter_keys"></a>
#### Filter keywords


| Option            | Description                                                         |
|-------------------|---------------------------------------------------------------------|
| distribution      | Distribution id, e.g. 'ubuntu' (from /etc/os-release ID)            |
| distribution_name | Distribution name, e.g. 'Ubuntu' (from NAME)                        |
| family            | Distribution family, e.g. 'debian' (from ID_LIKE/ID)                |
| kernel_name       | Kernel name                                                         |
| kernel_release    | Kernel release                                                      |
| kernel_version    | Kernel version                                                      |
| machine           | Machine hardware name                                               |
| nodename          | Network node hostname                                               |
| os                | Operating system (distribution pretty name, or kernel when unknown) |
| processor         | Processor / machine architecture                                    |
| version           | Distribution version, e.g. '22.04' (from VERSION_ID)                |

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
| [delta](#check_process_delta)                 |                                  | Measure CPU usage as a delta over a one second interval.                                                         |
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

<h5 id="check_process_delta">delta:</h5>

Measure CPU usage as a delta over a one second interval.
The check samples process and system CPU times, sleeps for one second, then samples again. With delta=true the 'time' (and 'kernel'/'user') fields report the process CPU usage during that second as a whole percentage of total CPU, instead of cumulative CPU seconds.



<a id="check_process_filter_keys"></a>
#### Filter keywords


| Option           | Description                                      |
|------------------|--------------------------------------------------|
| command_line     | Command line of process                          |
| creation         | Creation time                                    |
| error            | Any error messages associated with fetching info |
| exe              | The name of the executable                       |
| filename         | Name of process (with path)                      |
| kernel           | Kernel time in seconds                           |
| page_fault       | Page fault count                                 |
| page_faults      | Page fault count                                 |
| peak_virtual     | Peak virtual size in bytes                       |
| peak_working_set | Peak working set in bytes                        |
| pid              | Process id                                       |
| started          | Process is started                               |
| state            | The current state (started, stopped, hung)       |
| stopped          | Process is stopped                               |
| time             | User-kernel time in seconds                      |
| user             | User time in seconds                             |
| virtual          | Virtual size in bytes                            |
| working_set      | Working set (RSS) in bytes                       |

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


### check_process_history

Check the history of processes seen since NSClient++ started (requires 'process history = true').


**Jump to section:**

* [Command-line Arguments](#check_process_history_options)
* [Filter keywords](#check_process_history_filter_keys)





<a id="check_process_history_warn"></a>
<a id="check_process_history_crit"></a>
<a id="check_process_history_debug"></a>
<a id="check_process_history_show-all"></a>
<a id="check_process_history_escape-html"></a>
<a id="check_process_history_help"></a>
<a id="check_process_history_help-pb"></a>
<a id="check_process_history_show-default"></a>
<a id="check_process_history_help-short"></a>
<a id="check_process_history_process"></a>
<a id="check_process_history_options"></a>
#### Command-line Arguments


| Option                                                | Default Value                             | Description                                                                                                              |
|-------------------------------------------------------|-------------------------------------------|--------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_process_history_filter)               |                                           | Filter which marks interesting items.                                                                                    |
| [warning](#check_process_history_warning)             |                                           | Filter which marks items which generates a warning state.                                                                |
| warn                                                  |                                           | Short alias for warning                                                                                                  |
| [critical](#check_process_history_critical)           |                                           | Filter which marks items which generates a critical state.                                                               |
| crit                                                  |                                           | Short alias for critical.                                                                                                |
| [ok](#check_process_history_ok)                       |                                           | Filter which marks items which generates an ok state.                                                                    |
| debug                                                 | N/A                                       | Show debugging information in the log                                                                                    |
| show-all                                              | N/A                                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).         |
| [empty-state](#check_process_history_empty-state)     | ok                                        | Return status to use when nothing matched filter.                                                                        |
| [perf-config](#check_process_history_perf-config)     |                                           | Performance data generation configuration                                                                                |
| escape-html                                           | N/A                                       | Escape any < and > characters to prevent HTML encoding                                                                   |
| help                                                  | N/A                                       | Show help screen (this screen)                                                                                           |
| help-pb                                               | N/A                                       | Show help screen as a protocol buffer payload                                                                            |
| show-default                                          | N/A                                       | Show default values for a given command                                                                                  |
| help-short                                            | N/A                                       | Show help screen (short format).                                                                                         |
| [top-syntax](#check_process_history_top-syntax)       | ${status}: ${problem_list}                | Top level syntax.                                                                                                        |
| [ok-syntax](#check_process_history_ok-syntax)         | %(status): ${count} processes in history. | ok syntax.                                                                                                               |
| [empty-syntax](#check_process_history_empty-syntax)   |                                           | Empty syntax.                                                                                                            |
| [detail-syntax](#check_process_history_detail-syntax) | ${exe} (${running})                       | Detail level syntax.                                                                                                     |
| [perf-syntax](#check_process_history_perf-syntax)     | ${exe}                                    | Performance alias syntax.                                                                                                |
| process                                               |                                           | Filter to specific process names. Can be specified multiple times. If not specified, all processes in history are shown. |



<h5 id="check_process_history_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_process_history_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_process_history_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_process_history_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_process_history_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_process_history_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_process_history_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_process_history_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): ${count} processes in history.`

<h5 id="check_process_history_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_process_history_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${exe} (${running})`

<h5 id="check_process_history_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${exe}`


<a id="check_process_history_filter_keys"></a>
#### Filter keywords


| Option            | Description                                                 |
|-------------------|-------------------------------------------------------------|
| currently_running | Whether the process is currently running (1/0)              |
| exe               | The name of the executable                                  |
| first_seen        | Unix timestamp when process was first seen                  |
| last_seen         | Unix timestamp when process was last seen                   |
| running           | Whether the process is currently running: 'true' or 'false' |
| times_seen        | Number of times the process has been observed running       |

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


### check_process_history_new

Check for processes first seen within a recent time window (requires 'process history = true').


**Jump to section:**

* [Command-line Arguments](#check_process_history_new_options)
* [Filter keywords](#check_process_history_new_filter_keys)





<a id="check_process_history_new_warn"></a>
<a id="check_process_history_new_crit"></a>
<a id="check_process_history_new_debug"></a>
<a id="check_process_history_new_show-all"></a>
<a id="check_process_history_new_escape-html"></a>
<a id="check_process_history_new_help"></a>
<a id="check_process_history_new_help-pb"></a>
<a id="check_process_history_new_show-default"></a>
<a id="check_process_history_new_help-short"></a>
<a id="check_process_history_new_options"></a>
#### Command-line Arguments


| Option                                                    | Default Value                      | Description                                                                                                             |
|-----------------------------------------------------------|------------------------------------|-------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_process_history_new_filter)               |                                    | Filter which marks interesting items.                                                                                   |
| [warning](#check_process_history_new_warning)             |                                    | Filter which marks items which generates a warning state.                                                               |
| warn                                                      |                                    | Short alias for warning                                                                                                 |
| [critical](#check_process_history_new_critical)           |                                    | Filter which marks items which generates a critical state.                                                              |
| crit                                                      |                                    | Short alias for critical.                                                                                               |
| [ok](#check_process_history_new_ok)                       |                                    | Filter which marks items which generates an ok state.                                                                   |
| debug                                                     | N/A                                | Show debugging information in the log                                                                                   |
| show-all                                                  | N/A                                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).        |
| [empty-state](#check_process_history_new_empty-state)     | ok                                 | Return status to use when nothing matched filter.                                                                       |
| [perf-config](#check_process_history_new_perf-config)     |                                    | Performance data generation configuration                                                                               |
| escape-html                                               | N/A                                | Escape any < and > characters to prevent HTML encoding                                                                  |
| help                                                      | N/A                                | Show help screen (this screen)                                                                                          |
| help-pb                                                   | N/A                                | Show help screen as a protocol buffer payload                                                                           |
| show-default                                              | N/A                                | Show default values for a given command                                                                                 |
| help-short                                                | N/A                                | Show help screen (short format).                                                                                        |
| [top-syntax](#check_process_history_new_top-syntax)       | ${status}: ${list}                 | Top level syntax.                                                                                                       |
| [ok-syntax](#check_process_history_new_ok-syntax)         | %(status): No new processes found. | ok syntax.                                                                                                              |
| [empty-syntax](#check_process_history_new_empty-syntax)   |                                    | Empty syntax.                                                                                                           |
| [detail-syntax](#check_process_history_new_detail-syntax) | ${exe} (first seen: ${first_seen}) | Detail level syntax.                                                                                                    |
| [perf-syntax](#check_process_history_new_perf-syntax)     | ${exe}                             | Performance alias syntax.                                                                                               |
| [time](#check_process_history_new_time)                   | 5m                                 | Time window to check for new processes (e.g., 5m, 1h, 30s). Processes first seen within this window are considered new. |



<h5 id="check_process_history_new_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_process_history_new_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_process_history_new_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_process_history_new_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_process_history_new_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ok`

<h5 id="check_process_history_new_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_process_history_new_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_process_history_new_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): No new processes found.`

<h5 id="check_process_history_new_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_process_history_new_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${exe} (first seen: ${first_seen})`

<h5 id="check_process_history_new_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${exe}`

<h5 id="check_process_history_new_time">time:</h5>

Time window to check for new processes (e.g., 5m, 1h, 30s). Processes first seen within this window are considered new.

*Default Value:* `5m`


<a id="check_process_history_new_filter_keys"></a>
#### Filter keywords


| Option            | Description                                                 |
|-------------------|-------------------------------------------------------------|
| currently_running | Whether the process is currently running (1/0)              |
| exe               | The name of the executable                                  |
| first_seen        | Unix timestamp when process was first seen                  |
| last_seen         | Unix timestamp when process was last seen                   |
| running           | Whether the process is currently running: 'true' or 'false' |
| times_seen        | Number of times the process has been observed running       |

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

#### `state_is_ok`

Helper function that checks if the state of a service is "OK". It returns `True` if the state is "OK" and `False` otherwise.
This can be used in filter expressions to warn about services that are not running properly.

| Configured            | State     | exit_code | Result of `state_is_ok` |
|-----------------------|-----------|-----------|-------------------------|
| auto-start            | running   | any       | ✅ ok                    |
| delayed auto-start    | stopped   | any       | ✅ ok                    |
| auto-start + triggers | stopped   | any       | ✅ ok                    |
| auto-start            | stopped   | 0         | ✅ ok                    |
| auto-start            | stopped   | non zero  | ❌ not ok                |
| demand-start          | any state | any       | ✅ ok                    |

#### `state_is_perfect`

Helper function that checks if the state of a service is "perfect". It returns `True` if the state is "perfect" and `False` otherwise.
This can be used in filter expressions to warn about services that are not running perfectly.

| Configured            | State     | Result of `state_is_perfect` |
|-----------------------|-----------|------------------------------|
| auto-start            | running   | ✅ perfect                    |
| auto-start            | stopped   | ❌ not perfect                |
| auto-start + triggers | stopped   | ✅ perfect                    |
| demand-start          | any state | ✅ perfect                    |
| disabled              | stopped   | ✅ perfect                    |


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


### check_swap_io

Check the swap in/out paging rate.


**Jump to section:**

* [Command-line Arguments](#check_swap_io_options)
* [Filter keywords](#check_swap_io_filter_keys)





<a id="check_swap_io_warn"></a>
<a id="check_swap_io_crit"></a>
<a id="check_swap_io_debug"></a>
<a id="check_swap_io_show-all"></a>
<a id="check_swap_io_escape-html"></a>
<a id="check_swap_io_help"></a>
<a id="check_swap_io_help-pb"></a>
<a id="check_swap_io_show-default"></a>
<a id="check_swap_io_help-short"></a>
<a id="check_swap_io_options"></a>
#### Command-line Arguments


| Option                                        | Default Value                                                               | Description                                                                                                      |
|-----------------------------------------------|-----------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_swap_io_filter)               |                                                                             | Filter which marks interesting items.                                                                            |
| [warning](#check_swap_io_warning)             |                                                                             | Filter which marks items which generates a warning state.                                                        |
| warn                                          |                                                                             | Short alias for warning                                                                                          |
| [critical](#check_swap_io_critical)           |                                                                             | Filter which marks items which generates a critical state.                                                       |
| crit                                          |                                                                             | Short alias for critical.                                                                                        |
| [ok](#check_swap_io_ok)                       |                                                                             | Filter which marks items which generates an ok state.                                                            |
| debug                                         | N/A                                                                         | Show debugging information in the log                                                                            |
| show-all                                      | N/A                                                                         | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_swap_io_empty-state)     | ignored                                                                     | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_swap_io_perf-config)     |                                                                             | Performance data generation configuration                                                                        |
| escape-html                                   | N/A                                                                         | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                          | N/A                                                                         | Show help screen (this screen)                                                                                   |
| help-pb                                       | N/A                                                                         | Show help screen as a protocol buffer payload                                                                    |
| show-default                                  | N/A                                                                         | Show default values for a given command                                                                          |
| help-short                                    | N/A                                                                         | Show help screen (short format).                                                                                 |
| [top-syntax](#check_swap_io_top-syntax)       | ${status}: ${list}                                                          | Top level syntax.                                                                                                |
| [ok-syntax](#check_swap_io_ok-syntax)         |                                                                             | ok syntax.                                                                                                       |
| [empty-syntax](#check_swap_io_empty-syntax)   |                                                                             | Empty syntax.                                                                                                    |
| [detail-syntax](#check_swap_io_detail-syntax) | ${swap_count} swap device(s) in ${swap_in} pages/s, out ${swap_out} pages/s | Detail level syntax.                                                                                             |
| [perf-syntax](#check_swap_io_perf-syntax)     | io                                                                          | Performance alias syntax.                                                                                        |



<h5 id="check_swap_io_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_swap_io_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_swap_io_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_swap_io_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_swap_io_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_swap_io_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_swap_io_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_swap_io_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).


<h5 id="check_swap_io_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_swap_io_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${swap_count} swap device(s) in ${swap_in} pages/s, out ${swap_out} pages/s`

<h5 id="check_swap_io_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `io`


<a id="check_swap_io_filter_keys"></a>
#### Filter keywords


| Option         | Description                          |
|----------------|--------------------------------------|
| name           | Always 'swap' (single aggregate row) |
| swap_count     | Number of active swap devices        |
| swap_in        | Pages swapped in per second          |
| swap_in_bytes  | Bytes swapped in per second          |
| swap_out       | Pages swapped out per second         |
| swap_out_bytes | Bytes swapped out per second         |

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


### check_temperature

Check temperature sensors (thermal zones / hwmon).


**Jump to section:**

* [Command-line Arguments](#check_temperature_options)
* [Filter keywords](#check_temperature_filter_keys)





<a id="check_temperature_warn"></a>
<a id="check_temperature_crit"></a>
<a id="check_temperature_debug"></a>
<a id="check_temperature_show-all"></a>
<a id="check_temperature_escape-html"></a>
<a id="check_temperature_help"></a>
<a id="check_temperature_help-pb"></a>
<a id="check_temperature_show-default"></a>
<a id="check_temperature_help-short"></a>
<a id="check_temperature_options"></a>
#### Command-line Arguments


| Option                                            | Default Value                 | Description                                                                                                      |
|---------------------------------------------------|-------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_temperature_filter)               |                               | Filter which marks interesting items.                                                                            |
| [warning](#check_temperature_warning)             | temperature > 70              | Filter which marks items which generates a warning state.                                                        |
| warn                                              |                               | Short alias for warning                                                                                          |
| [critical](#check_temperature_critical)           | temperature > 90              | Filter which marks items which generates a critical state.                                                       |
| crit                                              |                               | Short alias for critical.                                                                                        |
| [ok](#check_temperature_ok)                       |                               | Filter which marks items which generates an ok state.                                                            |
| debug                                             | N/A                           | Show debugging information in the log                                                                            |
| show-all                                          | N/A                           | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_temperature_empty-state)     | critical                      | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_temperature_perf-config)     |                               | Performance data generation configuration                                                                        |
| escape-html                                       | N/A                           | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                              | N/A                           | Show help screen (this screen)                                                                                   |
| help-pb                                           | N/A                           | Show help screen as a protocol buffer payload                                                                    |
| show-default                                      | N/A                           | Show default values for a given command                                                                          |
| help-short                                        | N/A                           | Show help screen (short format).                                                                                 |
| [top-syntax](#check_temperature_top-syntax)       | ${status}: ${list}            | Top level syntax.                                                                                                |
| [ok-syntax](#check_temperature_ok-syntax)         | %(status): Temperature is ok. | ok syntax.                                                                                                       |
| [empty-syntax](#check_temperature_empty-syntax)   |                               | Empty syntax.                                                                                                    |
| [detail-syntax](#check_temperature_detail-syntax) | ${name}: ${temperature}C      | Detail level syntax.                                                                                             |
| [perf-syntax](#check_temperature_perf-syntax)     | ${name}                       | Performance alias syntax.                                                                                        |



<h5 id="check_temperature_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_temperature_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `temperature > 70`

<h5 id="check_temperature_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `temperature > 90`

<h5 id="check_temperature_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_temperature_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `critical`

<h5 id="check_temperature_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_temperature_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_temperature_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): Temperature is ok.`

<h5 id="check_temperature_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_temperature_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name}: ${temperature}C`

<h5 id="check_temperature_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_temperature_filter_keys"></a>
#### Filter keywords


| Option      | Description                    |
|-------------|--------------------------------|
| active      | Whether the sensor is active   |
| name        | Thermal zone / sensor name     |
| temperature | Temperature in degrees Celsius |

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
uptime: -9:02, boot: 2013-aug-18 08:29:13 (local)
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
uptime: -0:3, boot: 2013-sep-08 18:41:06 (local)|'uptime'=1378665666;1378579481;1378622681
```

**Configuring the timezone** (added in 0.6.x). The default syntax renders
the boot timestamp in the configured zone and surfaces a short label via
the `${tz}` placeholder. The value is cached by each plugin in its
`loadModuleEx` and is read from the global `/settings/default/timezone`
setting. Accepted values: `local` (default), `utc`, or any POSIX TZ string
parseable by Boost.Date_time (for example `MST-07` or
`EST-05EDT,M3.2.0,M11.1.0`).

**Choosing the display granularity for `${uptime}`** (issue #590). The
`max-unit` argument selects the largest unit allowed when rendering
`${uptime}`. Accepted values: `s|m|h|d|w` (default `w`). For example, on
a host that has been up six weeks, `max-unit=w` renders `6w 0d 00:00`,
`max-unit=d` renders `42d 00:00`, and `max-unit=h` renders `1008:00`:

```
check_uptime max-unit=d "detail-syntax=uptime: ${uptime}, boot: ${boot} (${tz})"
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


| Option                                       | Default Value                             | Description                                                                                                                              |
|----------------------------------------------|-------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------|
| [filter](#check_uptime_filter)               |                                           | Filter which marks interesting items.                                                                                                    |
| [warning](#check_uptime_warning)             | uptime < 2d                               | Filter which marks items which generates a warning state.                                                                                |
| warn                                         |                                           | Short alias for warning                                                                                                                  |
| [critical](#check_uptime_critical)           | uptime < 1d                               | Filter which marks items which generates a critical state.                                                                               |
| crit                                         |                                           | Short alias for critical.                                                                                                                |
| [ok](#check_uptime_ok)                       |                                           | Filter which marks items which generates an ok state.                                                                                    |
| debug                                        | N/A                                       | Show debugging information in the log                                                                                                    |
| show-all                                     | N/A                                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                         |
| [empty-state](#check_uptime_empty-state)     | ignored                                   | Return status to use when nothing matched filter.                                                                                        |
| [perf-config](#check_uptime_perf-config)     |                                           | Performance data generation configuration                                                                                                |
| escape-html                                  | N/A                                       | Escape any < and > characters to prevent HTML encoding                                                                                   |
| help                                         | N/A                                       | Show help screen (this screen)                                                                                                           |
| help-pb                                      | N/A                                       | Show help screen as a protocol buffer payload                                                                                            |
| show-default                                 | N/A                                       | Show default values for a given command                                                                                                  |
| help-short                                   | N/A                                       | Show help screen (short format).                                                                                                         |
| [top-syntax](#check_uptime_top-syntax)       | ${status}: ${list}                        | Top level syntax.                                                                                                                        |
| [ok-syntax](#check_uptime_ok-syntax)         |                                           | ok syntax.                                                                                                                               |
| [empty-syntax](#check_uptime_empty-syntax)   |                                           | Empty syntax.                                                                                                                            |
| [detail-syntax](#check_uptime_detail-syntax) | uptime: ${uptime}h, boot: ${boot} (${tz}) | Detail level syntax.                                                                                                                     |
| [perf-syntax](#check_uptime_perf-syntax)     | uptime                                    | Performance alias syntax.                                                                                                                |
| [max-unit](#check_uptime_max-unit)           | w                                         | Largest time unit used to render ${uptime}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'. |



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

*Default Value:* `uptime: ${uptime}h, boot: ${boot} (${tz})`

<h5 id="check_uptime_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `uptime`

<h5 id="check_uptime_max-unit">max-unit:</h5>

Largest time unit used to render ${uptime}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'.

*Default Value:* `w`


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




## Configuration



| Path / Section                                                       | Description              |
|----------------------------------------------------------------------|--------------------------|
| [/settings/default](#default-values)                                 | Default values           |
| [/settings/system/unix](#unix-system)                                | Unix system              |
| [/settings/system/unix/real-time/cpu](#realtime-cpu-filters)         | Realtime cpu filters     |
| [/settings/system/unix/real-time/memory](#realtime-memory-filters)   | Realtime memory filters  |
| [/settings/system/unix/real-time/process](#realtime-process-filters) | Realtime process filters |



### Default values <a id="/settings/default"></a>

Default values used in other config sections.




| Key                                                 | Default Value | Description                 |
|-----------------------------------------------------|---------------|-----------------------------|
| [allowed hosts](#allowed-hosts)                     | 127.0.0.1     | Allowed hosts               |
| [bind to](#bind-to-address)                         |               | BIND TO ADDRESS             |
| [cache allowed hosts](#cache-list-of-allowed-hosts) | true          | Cache list of allowed hosts |
| [encoding](#nrpe-payload-encoding)                  |               | NRPE PAYLOAD ENCODING       |
| [inbox](#inbox)                                     | inbox         | INBOX                       |
| [password](#password)                               |               | Password                    |
| [socket queue size](#listen-queue)                  | 0             | LISTEN QUEUE                |
| [thread pool](#thread-pool)                         | 10            | THREAD POOL                 |
| [timeout](#timeout)                                 | 30            | TIMEOUT                     |
| [timezone](#timezone)                               | local         | Timezone                    |



```ini
# Default values used in other config sections.
[/settings/default]
allowed hosts=127.0.0.1
cache allowed hosts=true
inbox=inbox
socket queue size=0
thread pool=10
timeout=30
timezone=local

```





#### Allowed hosts <a id="/settings/default/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.





| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | allowed hosts                           |
| Default value: | `127.0.0.1`                             |


**Sample:**

```
[/settings/default]
# Allowed hosts
allowed hosts=127.0.0.1
```



#### BIND TO ADDRESS <a id="/settings/default/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.






| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | bind to                                 |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# BIND TO ADDRESS
bind to=
```



#### Cache list of allowed hosts <a id="/settings/default/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.





| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | cache allowed hosts                     |
| Default value: | `true`                                  |


**Sample:**

```
[/settings/default]
# Cache list of allowed hosts
cache allowed hosts=true
```



#### NRPE PAYLOAD ENCODING <a id="/settings/default/encoding"></a>








| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | encoding                                |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# NRPE PAYLOAD ENCODING
encoding=
```



#### INBOX <a id="/settings/default/inbox"></a>

The default channel to post incoming messages on





| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | inbox                                   |
| Default value: | `inbox`                                 |


**Sample:**

```
[/settings/default]
# INBOX
inbox=inbox
```



#### Password <a id="/settings/default/password"></a>

Password used to authenticate against server






| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | password                                |
| Default value: | _N/A_                                   |


**Sample:**

```
[/settings/default]
# Password
password=
```



#### LISTEN QUEUE <a id="/settings/default/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.





| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | socket queue size                       |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `0`                                     |


**Sample:**

```
[/settings/default]
# LISTEN QUEUE
socket queue size=0
```



#### THREAD POOL <a id="/settings/default/thread pool"></a>







| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | thread pool                             |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `10`                                    |


**Sample:**

```
[/settings/default]
# THREAD POOL
thread pool=10
```



#### TIMEOUT <a id="/settings/default/timeout"></a>

Timeout (in seconds) when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.





| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | timeout                                 |
| Default value: | `30`                                    |


**Sample:**

```
[/settings/default]
# TIMEOUT
timeout=30
```



#### Timezone <a id="/settings/default/timezone"></a>

Timezone used to render dates such as boot time. Accepts 'local' (default), 'utc', or any POSIX TZ string parseable by Boost.Date_time (e.g. 'MST-07' or 'EST-05EDT,M3.2.0,M11.1.0').





| Key            | Description                             |
|----------------|-----------------------------------------|
| Path:          | [/settings/default](#/settings/default) |
| Key:           | timezone                                |
| Advanced:      | Yes (means it is not commonly used)     |
| Default value: | `local`                                 |


**Sample:**

```
[/settings/default]
# Timezone
timezone=local
```


### Unix system <a id="/settings/system/unix"></a>

Section for system checks and system settings




| Key                                           | Default Value | Description           |
|-----------------------------------------------|---------------|-----------------------|
| [default buffer length](#default-buffer-time) | 1h            | Default buffer time   |
| [process history](#track-process-history)     | false         | Track process history |
| [timezone](#timezone)                         | local         | Timezone              |



```ini
# Section for system checks and system settings
[/settings/system/unix]
default buffer length=1h
process history=false
timezone=local

```





#### Default buffer time <a id="/settings/system/unix/default buffer length"></a>

Used to define the default size of range buffer checks (ie. CPU).





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/system/unix](#/settings/system/unix) |
| Key:           | default buffer length                           |
| Default value: | `1h`                                            |


**Sample:**

```
[/settings/system/unix]
# Default buffer time
default buffer length=1h
```



#### Track process history <a id="/settings/system/unix/process history"></a>

Enable tracking of process history for use with the check_process_history and check_process_history_new commands.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/system/unix](#/settings/system/unix) |
| Key:           | process history                                 |
| Default value: | `false`                                         |


**Sample:**

```
[/settings/system/unix]
# Track process history
process history=false
```



#### Timezone <a id="/settings/system/unix/timezone"></a>

Timezone used to render dates such as boot time. Accepts 'local' (default), 'utc', or any POSIX TZ string parseable by Boost.Date_time (e.g. 'MST-07' or 'EST-05EDT,M3.2.0,M11.1.0').





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/system/unix](#/settings/system/unix) |
| Key:           | timezone                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `local`                                         |


**Sample:**

```
[/settings/system/unix]
# Timezone
timezone=local
```


### Realtime cpu filters <a id="/settings/system/unix/real-time/cpu"></a>

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
| maximum age   | 5m                        | MAXIMUM AGE     |
| ok            |                           | OK FILTER       |
| ok syntax     |                           | SYNTAX          |
| perf config   |                           | PERF CONFIG     |
| severity      |                           | SEVERITY        |
| silent period | false                     | Silent period   |
| source id     |                           | SOURCE ID       |
| target        |                           | DESTINATION     |
| target id     |                           | TARGET ID       |
| time          |                           | TIME            |
| times         |                           | TIMES           |
| top syntax    |                           | SYNTAX          |
| warning       |                           | WARNING FILTER  |


**Sample:**

```ini
# An example of a Realtime cpu filters section
[/settings/system/unix/real-time/cpu/sample]
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






### Realtime memory filters <a id="/settings/system/unix/real-time/memory"></a>

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
| maximum age   | 5m                        | MAXIMUM AGE     |
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
| types         |                           | MEMORY TYPES    |
| warning       |                           | WARNING FILTER  |


**Sample:**

```ini
# An example of a Realtime memory filters section
[/settings/system/unix/real-time/memory/sample]
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
#types=...
#warning=...

```






### Realtime process filters <a id="/settings/system/unix/real-time/process"></a>

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
| maximum age   | 5m                        | MAXIMUM AGE     |
| ok            |                           | OK FILTER       |
| ok syntax     |                           | SYNTAX          |
| perf config   |                           | PERF CONFIG     |
| process       |                           | PROCESS         |
| processes     |                           | PROCESSES       |
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
[/settings/system/unix/real-time/process/sample]
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
#processes=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#top syntax=...
#warning=...

```






