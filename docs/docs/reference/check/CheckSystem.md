# CheckSystem

=== "Windows"

    Various system related checks, such as CPU load, process state, service state memory usage and PDH counters.

=== "Linux"

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

| Command                                                 | Description                                                                                                                                      |
|---------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------|
| [check_battery](#check_battery)                         | Check battery status including charge level, power source, and battery health.                                                                   |
| [check_cpu](#check_cpu)                                 | Check that the load of the CPU(s) are within bounds.                                                                                             |
| [check_cpu_frequency](#check_cpu_frequency)             | Check CPU clock frequency (current vs max) per processor.                                                                                        |
| [check_cpu_utilization](#check_cpu_utilization)         | Check CPU utilization broken down by user/system/iowait/steal/guest.                                                                             |
| [check_kernel_stats](#check_kernel_stats)               | Check kernel activity: context-switch rate, fork rate and live thread count.                                                                     |
| [check_load](#check_load)                               | Check the system load average (1/5/15 minutes).                                                                                                  |
| [check_memory](#check_memory)                           | Check free/used memory on the system.                                                                                                            |
| [check_network](#check_network)                         | Check network interface status.                                                                                                                  |
| [check_os_updates](#check_os_updates)                   | Check for available Windows updates via the Windows Update Agent (WUA) API.                                                                      |
| [check_os_version](#check_os_version)                   | Check the version of the underlying OS.                                                                                                          |
| [check_pagefile](#check_pagefile)                       | Check the size of the system pagefile(s).                                                                                                        |
| [check_pdh](#check_pdh)                                 | Check the value of a performance (PDH) counter on the local or remote system.                                                                    |
| [check_process](#check_process)                         | Check state/metrics of one or more of the processes running on the computer.                                                                     |
| [check_process_history](#check_process_history)         | Check the history of processes that have been running since NSClient++ started. Useful for verifying if certain applications have been executed. |
| [check_process_history_new](#check_process_history_new) | Check for new processes that appeared within a specified time window. Useful for detecting unexpected or unauthorized applications.              |
| [check_registry_key](#check_registry_key)               | Check existence, last-write time, and child counts of one or more Windows registry keys.                                                         |
| [check_registry_value](#check_registry_value)           | Check the type, content, and size of one or more Windows registry values.                                                                        |
| [check_service](#check_service)                         | Check the state of one or more of the computer services.                                                                                         |
| [check_swap_io](#check_swap_io)                         | Check the swap in/out paging rate.                                                                                                               |
| [check_temperature](#check_temperature)                 | Check ACPI thermal zone temperatures.                                                                                                            |
| [check_uptime](#check_uptime)                           | Check time since last server re-boot.                                                                                                            |

**List of command aliases:**

A list of all short hand aliases for queries (check commands)

| Command       | Description                   |
|---------------|-------------------------------|
| check_counter | Alias for: :query:`check_pdh` |

### check_battery

=== "Windows"

    Check battery status including charge level, power source, and battery health.

=== "Linux"

    Check battery charge level, power source and health.

**Jump to section:**

* [Command-line Arguments](#check_battery_options)
* [Filter keywords](#check_battery_filter_keys)



<a id="check_battery_options"></a>
#### Command-line Arguments

<a id="check_battery_warn"></a>
<a id="check_battery_crit"></a>
<a id="check_battery_debug"></a>
<a id="check_battery_show-all"></a>
<a id="check_battery_escape-html"></a>
<a id="check_battery_help"></a>
<a id="check_battery_help-pb"></a>
<a id="check_battery_show-default"></a>
<a id="check_battery_help-short"></a>

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

=== "Windows"

    | Option             | Description                                                       |
    |--------------------|-------------------------------------------------------------------|
    | battery_present    | Whether a battery is present: 'true' or 'false'                   |
    | charge             | Battery charge level in percent (0-100)                           |
    | charge_rate        | Current charge rate in mW (when charging)                         |
    | design_capacity    | Design capacity in mWh                                            |
    | discharge_rate     | Current discharge rate in mW (when discharging)                   |
    | full_capacity      | Current full charge capacity in mWh                               |
    | health             | Battery health in percent (full_capacity / design_capacity * 100) |
    | name               | Battery name/identifier                                           |
    | power_source       | Power source: 'ac', 'battery', or 'unknown'                       |
    | remaining_capacity | Current remaining capacity in mWh                                 |
    | time_remaining     | Estimated time remaining in seconds (-1 if unknown or on AC)      |

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

=== "Linux"

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

=== "Windows"

    Check that the load of the CPU(s) are within bounds.

    The check_cpu command is a query based command which means it has a filter where you can use a filter expression with filter keywords to define which rows are relevant to the check.
    The filter is written using the filter query language and in it you can use various filter keywords to define the filtering logic.
    The filter keywords can also be used to create the bound expressions for the warning and critical which defines when a check returns warning or critical.

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

=== "Linux"

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

**Full user/system/idle breakdown as perfdata (parity with the Linux `check_cpu_utilization` graph):**

`idle` and `system` now emit perfdata (previously only `total`/`user` did), so the
full breakdown graphs without a custom `top-syntax`. `kernel` is a deprecated alias
of `system` and intentionally emits no separate perf column.

```
check_cpu "warn=idle < 5"
CPU Load ok
'total 5m load'=7%;80;90 'total 5m user'=4%;;; 'total 5m system'=3%;;; 'total 5m idle'=93%;;; ...
```



<a id="check_cpu_options"></a>
#### Command-line Arguments

=== "Windows"

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
    | cores                                     | N/A                        | This will remove the filter to  include the cores, if you use filter dont use this as well.                      |



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

=== "Linux"

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

=== "Windows"

    | Option  | Description                                  |
    |---------|----------------------------------------------|
    | core    | The core to check (total or core ##)         |
    | core_id | The core to check (total or core_##)         |
    | idle    | The current idle load for a given core       |
    | kernel  | deprecated (use system instead)              |
    | load    | deprecated (use total instead)               |
    | system  | The current load used by the system (kernel) |
    | time    | The time frame to check                      |
    | user    | The current load used by user applications   |

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

=== "Linux"

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

=== "Windows"

    Check CPU clock frequency (current vs max) per processor.

    Reports per-CPU-socket frequency and load, sourced from the `Win32_Processor`
    WMI class (one instance per physical socket).

    | Keyword | Description |
    |---|---|
    | `name` | CPU name / model string. |
    | `socket_id` | Socket device id (e.g. `CPU0`); use to filter a single socket. |
    | `socket` | Socket designation (e.g. `CPU 1`). |
    | `current_mhz` | Current clock speed in MHz (perf). |
    | `max_mhz` | Maximum clock speed in MHz (perf). |
    | `frequency_pct` | Current frequency as a percentage of maximum (perf). |
    | `load_pct` | Per-socket CPU load from `Win32_Processor.LoadPercentage` (perf). |
    | `cores` | Number of physical cores. |
    | `logical_processors` | Number of logical processors (threads). |

    There are no default warning/critical thresholds: modern CPUs legitimately clock
    far below their maximum at idle, so a `frequency_pct` default would warn on every
    idle machine. Use `load_pct` for a per-socket utilisation alert.

=== "Linux"

    Check the CPU clock frequency (current vs max) per core.

    Reports per-CPU-socket frequency and load, sourced from the `Win32_Processor`
    WMI class (one instance per physical socket).

    | Keyword | Description |
    |---|---|
    | `name` | CPU name / model string. |
    | `socket_id` | Socket device id (e.g. `CPU0`); use to filter a single socket. |
    | `socket` | Socket designation (e.g. `CPU 1`). |
    | `current_mhz` | Current clock speed in MHz (perf). |
    | `max_mhz` | Maximum clock speed in MHz (perf). |
    | `frequency_pct` | Current frequency as a percentage of maximum (perf). |
    | `load_pct` | Per-socket CPU load from `Win32_Processor.LoadPercentage` (perf). |
    | `cores` | Number of physical cores. |
    | `logical_processors` | Number of logical processors (threads). |

    There are no default warning/critical thresholds: modern CPUs legitimately clock
    far below their maximum at idle, so a `frequency_pct` default would warn on every
    idle machine. Use `load_pct` for a per-socket utilisation alert.

**Jump to section:**

* [Sample Commands](#check_cpu_frequency_samples)
* [Command-line Arguments](#check_cpu_frequency_options)
* [Filter keywords](#check_cpu_frequency_filter_keys)


<a id="check_cpu_frequency_samples"></a>
#### Sample Commands

**Default check:**

```
check_cpu_frequency
OK: Intel(R) Core(TM) i7-10700 CPU @ 2.90GHz: 2900/4800 MHz (60%)
'Intel...current_mhz'=2900MHz;;; 'Intel...max_mhz'=4800MHz;;; 'Intel...frequency_pct'=60%;;; 'Intel...load_pct'=12%;;;
```

**Per-socket filtering and load:**

`Win32_Processor` returns one row per physical CPU socket, exposed via `socket_id`
(DeviceID, e.g. `CPU0`) and `socket` (SocketDesignation, e.g. `CPU 1`). The
`load_pct` keyword reports `Win32_Processor.LoadPercentage` per socket.

```
check_cpu_frequency "filter=socket_id = 'CPU0'" "warn=load_pct > 90" "detail-syntax=${socket}: ${load_pct}% @ ${current_mhz}MHz"
OK: CPU 1: 12% @ 2900MHz
'CPU0 load_pct'=12%;90;;
```



<a id="check_cpu_frequency_options"></a>
#### Command-line Arguments

<a id="check_cpu_frequency_warn"></a>
<a id="check_cpu_frequency_crit"></a>
<a id="check_cpu_frequency_debug"></a>
<a id="check_cpu_frequency_show-all"></a>
<a id="check_cpu_frequency_escape-html"></a>
<a id="check_cpu_frequency_help"></a>
<a id="check_cpu_frequency_help-pb"></a>
<a id="check_cpu_frequency_show-default"></a>
<a id="check_cpu_frequency_help-short"></a>

| Option                                              | Default Value                                              | Description                                                                                                      |
|-----------------------------------------------------|------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_cpu_frequency_filter)               |                                                            | Filter which marks interesting items.                                                                            |
| [warning](#check_cpu_frequency_warning)             |                                                            | Filter which marks items which generates a warning state.                                                        |
| warn                                                |                                                            | Short alias for warning                                                                                          |
| [critical](#check_cpu_frequency_critical)           |                                                            | Filter which marks items which generates a critical state.                                                       |
| crit                                                |                                                            | Short alias for critical.                                                                                        |
| [ok](#check_cpu_frequency_ok)                       |                                                            | Filter which marks items which generates an ok state.                                                            |
| debug                                               | N/A                                                        | Show debugging information in the log                                                                            |
| show-all                                            | N/A                                                        | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_cpu_frequency_empty-state)     | ignored                                                    | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_cpu_frequency_perf-config)     |                                                            | Performance data generation configuration                                                                        |
| escape-html                                         | N/A                                                        | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                                | N/A                                                        | Show help screen (this screen)                                                                                   |
| help-pb                                             | N/A                                                        | Show help screen as a protocol buffer payload                                                                    |
| show-default                                        | N/A                                                        | Show default values for a given command                                                                          |
| help-short                                          | N/A                                                        | Show help screen (short format).                                                                                 |
| [top-syntax](#check_cpu_frequency_top-syntax)       | ${status}: ${list}                                         | Top level syntax.                                                                                                |
| [ok-syntax](#check_cpu_frequency_ok-syntax)         | %(status): All CPU frequencies seem ok.                    | ok syntax.                                                                                                       |
| [empty-syntax](#check_cpu_frequency_empty-syntax)   |                                                            | Empty syntax.                                                                                                    |
| [detail-syntax](#check_cpu_frequency_detail-syntax) | ${name}: ${current_mhz}/${max_mhz} MHz (${frequency_pct}%) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_cpu_frequency_perf-syntax)     | ${name}                                                    | Performance alias syntax.                                                                                        |



<h5 id="check_cpu_frequency_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_cpu_frequency_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_cpu_frequency_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_cpu_frequency_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_cpu_frequency_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_cpu_frequency_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_cpu_frequency_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_cpu_frequency_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All CPU frequencies seem ok.`

<h5 id="check_cpu_frequency_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.


<h5 id="check_cpu_frequency_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${name}: ${current_mhz}/${max_mhz} MHz (${frequency_pct}%)`

<h5 id="check_cpu_frequency_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${name}`


<a id="check_cpu_frequency_filter_keys"></a>
#### Filter keywords

| Option             | Description                                |
|--------------------|--------------------------------------------|
| cores              | Number of physical cores                   |
| current_mhz        | Current clock speed in MHz                 |
| frequency_pct      | Current frequency as percentage of maximum |
| logical_processors | Number of logical processors (threads)     |
| max_mhz            | Maximum clock speed in MHz                 |
| name               | CPU name / model string                    |

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

### check_cpu_utilization

*Available on Linux only.*

Check CPU utilization broken down by user/system/iowait/steal/guest.

**Jump to section:**

* [Command-line Arguments](#check_cpu_utilization_options)
* [Filter keywords](#check_cpu_utilization_filter_keys)



<a id="check_cpu_utilization_options"></a>
#### Command-line Arguments

<a id="check_cpu_utilization_warn"></a>
<a id="check_cpu_utilization_crit"></a>
<a id="check_cpu_utilization_debug"></a>
<a id="check_cpu_utilization_show-all"></a>
<a id="check_cpu_utilization_escape-html"></a>
<a id="check_cpu_utilization_help"></a>
<a id="check_cpu_utilization_help-pb"></a>
<a id="check_cpu_utilization_show-default"></a>
<a id="check_cpu_utilization_help-short"></a>

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

*Available on Linux only.*

Check kernel activity: context-switch rate, fork rate and live thread count.

**Jump to section:**

* [Command-line Arguments](#check_kernel_stats_options)
* [Filter keywords](#check_kernel_stats_filter_keys)



<a id="check_kernel_stats_options"></a>
#### Command-line Arguments

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

*Available on Linux only.*

Check the system load average (1/5/15 minutes).

**Jump to section:**

* [Command-line Arguments](#check_load_options)
* [Filter keywords](#check_load_filter_keys)



<a id="check_load_options"></a>
#### Command-line Arguments

<a id="check_load_warn"></a>
<a id="check_load_crit"></a>
<a id="check_load_debug"></a>
<a id="check_load_show-all"></a>
<a id="check_load_escape-html"></a>
<a id="check_load_help"></a>
<a id="check_load_help-pb"></a>
<a id="check_load_show-default"></a>
<a id="check_load_help-short"></a>

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

#### Memory paging rate (`\Memory\Pages/sec`)

A sustained high hard-page-fault rate is one of the strongest signals of memory
pressure. NSClient++ collects `\Memory\Pages/sec` by default under the alias
`memory_pages_sec`, so you can alert on it directly with `check_pdh` without
declaring the counter yourself:

```
check_pdh "counter=memory_pages_sec" "warn=value > 1000" "crit=value > 5000"
```

**Jump to section:**

* [Sample Commands](#check_memory_samples)
* [Command-line Arguments](#check_memory_options)
* [Filter keywords](#check_memory_filter_keys)


<a id="check_memory_samples"></a>
#### Sample Commands

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



<a id="check_memory_options"></a>
#### Command-line Arguments

=== "Windows"

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

    | Option                                       | Default Value            | Description                                                                                                      |
    |----------------------------------------------|--------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_memory_filter)               |                          | Filter which marks interesting items.                                                                            |
    | [warning](#check_memory_warning)             | used > 80%               | Filter which marks items which generates a warning state.                                                        |
    | warn                                         |                          | Short alias for warning                                                                                          |
    | [critical](#check_memory_critical)           | used > 90%               | Filter which marks items which generates a critical state.                                                       |
    | crit                                         |                          | Short alias for critical.                                                                                        |
    | [ok](#check_memory_ok)                       |                          | Filter which marks items which generates an ok state.                                                            |
    | debug                                        | N/A                      | Show debugging information in the log                                                                            |
    | show-all                                     | N/A                      | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_memory_empty-state)     | ignored                  | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_memory_perf-config)     |                          | Performance data generation configuration                                                                        |
    | escape-html                                  | N/A                      | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                         | N/A                      | Show help screen (this screen)                                                                                   |
    | help-pb                                      | N/A                      | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                 | N/A                      | Show default values for a given command                                                                          |
    | help-short                                   | N/A                      | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_memory_top-syntax)       | ${status}: ${list}       | Top level syntax.                                                                                                |
    | [ok-syntax](#check_memory_ok-syntax)         |                          | ok syntax.                                                                                                       |
    | [empty-syntax](#check_memory_empty-syntax)   |                          | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_memory_detail-syntax) | ${type}: ${used}/${size} | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_memory_perf-syntax)     | ${type}                  | Performance alias syntax.                                                                                        |
    | type                                         |                          | The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE)               |



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

    *Default Value:* `${type}: ${used}/${size}`

    <h5 id="check_memory_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${type}`

=== "Linux"

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

=== "Windows"

    | Option   | Description                                     |
    |----------|-------------------------------------------------|
    | free     | Free memory in bytes (g,m,k,b) or percentages % |
    | free_pct | % free memory                                   |
    | size     | Total size of memory                            |
    | type     | The type of memory to check                     |
    | used     | Used memory in bytes (g,m,k,b) or percentages % |
    | used_pct | % used memory                                   |

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

=== "Linux"

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

=== "Windows"

    Check network interface status.

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

    #### Packet, error and discard counters

    In addition to the byte-rate counters, `check_network` exposes per-second
    packet, error and discard rates. Each is derived from the cumulative 
    `Win32_PerfRawData_Tcpip_Network*` counters, so a healthy NIC reports 
    approximately `0` errors/discards per second and any sustained non-zero 
    rate is an alertable signal. All six emit perfdata.

    | Variable       | Description                            |
    |----------------|----------------------------------------|
    | `packets_in`   | Packets received per second.           |
    | `packets_out`  | Packets sent per second.               |
    | `errors_in`    | Inbound packet errors per second.      |
    | `errors_out`   | Outbound packet errors per second.     |
    | `discards_in`  | Inbound packets discarded per second.  |
    | `discards_out` | Outbound packets discarded per second. |

    ```
    check_network "filter=name = 'Ethernet 1'" \
                  "warning=errors_in > 0 or errors_out > 0" \
                  "critical=discards_in > 10 or discards_out > 10"
    ```

    #### NIC team membership

    When the Windows LBFO WMI provider is available (`ROOT\StandardCimv2\MSFT_NetLbfoTeamMember`),
    each adapter is annotated with its team:

    | Variable | Description |
    |---|---|
    | `team` | Name of the NIC team this adapter belongs to. Empty when the adapter is not a team member, or when the LBFO provider is unavailable (client SKUs, older Windows, no teams configured). |
    | `team_status` | The raw `MSFT_NetLbfoTeamMember.OperationalStatus` of this team member, rendered as a string. Empty for non-members. |

    Team annotation is best-effort and self-disabling: if the provider or namespace
    is absent, the fields stay empty and the check does not fail. Use `team != ''`
    to scope a check to teamed adapters.

=== "Linux"

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

    #### Packet, error and discard counters

    In addition to the byte-rate counters, `check_network` exposes per-second
    packet, error and discard rates. Each is derived from the cumulative 
    `Win32_PerfRawData_Tcpip_Network*` counters, so a healthy NIC reports 
    approximately `0` errors/discards per second and any sustained non-zero 
    rate is an alertable signal. All six emit perfdata.

    | Variable       | Description                            |
    |----------------|----------------------------------------|
    | `packets_in`   | Packets received per second.           |
    | `packets_out`  | Packets sent per second.               |
    | `errors_in`    | Inbound packet errors per second.      |
    | `errors_out`   | Outbound packet errors per second.     |
    | `discards_in`  | Inbound packets discarded per second.  |
    | `discards_out` | Outbound packets discarded per second. |

    ```
    check_network "filter=name = 'Ethernet 1'" \
                  "warning=errors_in > 0 or errors_out > 0" \
                  "critical=discards_in > 10 or discards_out > 10"
    ```

    #### NIC team membership

    When the Windows LBFO WMI provider is available (`ROOT\StandardCimv2\MSFT_NetLbfoTeamMember`),
    each adapter is annotated with its team:

    | Variable | Description |
    |---|---|
    | `team` | Name of the NIC team this adapter belongs to. Empty when the adapter is not a team member, or when the LBFO provider is unavailable (client SKUs, older Windows, no teams configured). |
    | `team_status` | The raw `MSFT_NetLbfoTeamMember.OperationalStatus` of this team member, rendered as a string. Empty for non-members. |

    Team annotation is best-effort and self-disabling: if the provider or namespace
    is absent, the fields stay empty and the check does not fail. Use `team != ''`
    to scope a check to teamed adapters.

**Jump to section:**

* [Command-line Arguments](#check_network_options)
* [Filter keywords](#check_network_filter_keys)



<a id="check_network_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_network_warn"></a>
    <a id="check_network_crit"></a>
    <a id="check_network_debug"></a>
    <a id="check_network_show-all"></a>
    <a id="check_network_escape-html"></a>
    <a id="check_network_help"></a>
    <a id="check_network_help-pb"></a>
    <a id="check_network_show-default"></a>
    <a id="check_network_help-short"></a>

    | Option                                        | Default Value                                 | Description                                                                                                                                                                                                                                                         |
    |-----------------------------------------------|-----------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_network_filter)               |                                               | Filter which marks interesting items.                                                                                                                                                                                                                               |
    | [warning](#check_network_warning)             | total > 10000                                 | Filter which marks items which generates a warning state.                                                                                                                                                                                                           |
    | warn                                          |                                               | Short alias for warning                                                                                                                                                                                                                                             |
    | [critical](#check_network_critical)           | total > 100000                                | Filter which marks items which generates a critical state.                                                                                                                                                                                                          |
    | crit                                          |                                               | Short alias for critical.                                                                                                                                                                                                                                           |
    | [ok](#check_network_ok)                       |                                               | Filter which marks items which generates an ok state.                                                                                                                                                                                                               |
    | debug                                         | N/A                                           | Show debugging information in the log                                                                                                                                                                                                                               |
    | show-all                                      | N/A                                           | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                                                                                                                                                    |
    | [empty-state](#check_network_empty-state)     | critical                                      | Return status to use when nothing matched filter.                                                                                                                                                                                                                   |
    | [perf-config](#check_network_perf-config)     |                                               | Performance data generation configuration                                                                                                                                                                                                                           |
    | escape-html                                   | N/A                                           | Escape any < and > characters to prevent HTML encoding                                                                                                                                                                                                              |
    | help                                          | N/A                                           | Show help screen (this screen)                                                                                                                                                                                                                                      |
    | help-pb                                       | N/A                                           | Show help screen as a protocol buffer payload                                                                                                                                                                                                                       |
    | show-default                                  | N/A                                           | Show default values for a given command                                                                                                                                                                                                                             |
    | help-short                                    | N/A                                           | Show help screen (short format).                                                                                                                                                                                                                                    |
    | [top-syntax](#check_network_top-syntax)       | ${status}: ${list}                            | Top level syntax.                                                                                                                                                                                                                                                   |
    | [ok-syntax](#check_network_ok-syntax)         | %(status): Network interfaces seem ok.        | ok syntax.                                                                                                                                                                                                                                                          |
    | [empty-syntax](#check_network_empty-syntax)   |                                               | Empty syntax.                                                                                                                                                                                                                                                       |
    | [detail-syntax](#check_network_detail-syntax) | ${name} >${sent_human}/s <${received_human}/s | Detail level syntax.                                                                                                                                                                                                                                                |
    | [perf-syntax](#check_network_perf-syntax)     | ${name}                                       | Performance alias syntax.                                                                                                                                                                                                                                           |
    | [mode](#check_network_mode)                   | interface                                     | Which WMI source to report from: 'interface' (default; Win32_PerfRawData_Tcpip_NetworkInterface, physical adapters only), 'adapter' (Win32_PerfRawData_Tcpip_NetworkAdapter, includes NIC team aggregates), or 'both' (every interface reported under both sources) |



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

    <h5 id="check_network_mode">mode:</h5>

    Which WMI source to report from: 'interface' (default; Win32_PerfRawData_Tcpip_NetworkInterface, physical adapters only), 'adapter' (Win32_PerfRawData_Tcpip_NetworkAdapter, includes NIC team aggregates), or 'both' (every interface reported under both sources)

    *Default Value:* `interface`

=== "Linux"

    <a id="check_network_warn"></a>
    <a id="check_network_crit"></a>
    <a id="check_network_debug"></a>
    <a id="check_network_show-all"></a>
    <a id="check_network_escape-html"></a>
    <a id="check_network_help"></a>
    <a id="check_network_help-pb"></a>
    <a id="check_network_show-default"></a>
    <a id="check_network_help-short"></a>

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

=== "Windows"

    | Option            | Description                                                                                                                                                                                                         |
    |-------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | MAC               | The MAC address                                                                                                                                                                                                     |
    | enabled           | True if the network interface is enabled                                                                                                                                                                            |
    | name              | Network interface name                                                                                                                                                                                              |
    | net_connection_id | Network connection id                                                                                                                                                                                               |
    | received          | Bytes received per second                                                                                                                                                                                           |
    | received_human    | Bytes received per second, formatted as a human-readable string (auto-scaled).                                                                                                                                      |
    | sent              | Bytes sent per second                                                                                                                                                                                               |
    | sent_human        | Bytes sent per second, formatted as a human-readable string (auto-scaled).                                                                                                                                          |
    | source            | WMI source: 'interface' or 'adapter'                                                                                                                                                                                |
    | speed             | The network interface speed (raw WMI value, e.g. "1000000000" or "Unknown")                                                                                                                                         |
    | speed_bps         | Negotiated link speed in bits/sec, parsed from the WMI Speed property. BEST-EFFORT: 0 when the speed is Unknown/empty (virtual adapters, some teams). Filter on speed_bps > 0 before relying on usage_in/out/total. |
    | total_human       | Bytes total per second, formatted as a human-readable string (auto-scaled).                                                                                                                                         |
    | usage_in          | Percent of negotiated link speed used by received traffic. BEST-EFFORT: reads as 0 when speed is unknown - filter on speed_bps > 0 to distinguish idle from unknown.                                                |
    | usage_out         | Percent of negotiated link speed used by sent traffic. BEST-EFFORT: reads as 0 when speed is unknown - filter on speed_bps > 0 to distinguish idle from unknown.                                                    |
    | usage_total       | Percent of negotiated link speed used by total traffic. BEST-EFFORT: reads as 0 when speed is unknown - filter on speed_bps > 0 to distinguish idle from unknown.                                                   |

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

=== "Linux"

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

=== "Windows"

    Check for available Windows updates via the Windows Update Agent (WUA) API.

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

    `reboot_required` counts updates that *would* require a reboot once installed.
    To detect a reboot that is *already pending* system-wide — including reboots
    queued by updates that have already been installed (which `reboot_required` no
    longer reflects) — use `reboot_pending`, sourced from the Windows Update
    `RebootRequired` registry key:

    ```
    check_os_updates "crit=reboot_pending = 1" "detail-syntax=reboot pending: ${reboot_pending}"
    ```

    **Defender / definition and rollup categories**

    Defender/antivirus definition updates churn several times a day, so most admins
    threshold them separately from OS patches. `defender` counts updates in the
    `Definition Updates` / `Microsoft Defender Antivirus` categories, and `rollups`
    counts monthly `Update Rollup` updates:

    ```
    check_os_updates "warning=count - defender > 0" "detail-syntax=${count} total, ${defender} defender, ${rollups} rollups"
    ```

    **Filtering by title**

    `update-filter=<substring>` restricts the check to updates whose title contains
    the (case-insensitive) substring; all counters (`count`, `security`, …) are then
    recomputed over just the matching subset:

    ```
    check_os_updates update-filter=".NET" "detail-syntax=${count} .NET updates: ${titles}"
    ```

    > **Note:** the WUA search criteria is `Type='Software'`, so **driver updates are
    > excluded** by design. This keeps the count focused on OS/application patches.

    **Customizing the output**

    You can use the syntax options to format the output string:

    ```
    check_os_updates "top-syntax=${status}: Found ${count} missing updates. Security: ${security}, Critical: ${critical}" "detail-syntax=${titles}" show-all
    ```

=== "Linux"

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

    `reboot_required` counts updates that *would* require a reboot once installed.
    To detect a reboot that is *already pending* system-wide — including reboots
    queued by updates that have already been installed (which `reboot_required` no
    longer reflects) — use `reboot_pending`, sourced from the Windows Update
    `RebootRequired` registry key:

    ```
    check_os_updates "crit=reboot_pending = 1" "detail-syntax=reboot pending: ${reboot_pending}"
    ```

    **Defender / definition and rollup categories**

    Defender/antivirus definition updates churn several times a day, so most admins
    threshold them separately from OS patches. `defender` counts updates in the
    `Definition Updates` / `Microsoft Defender Antivirus` categories, and `rollups`
    counts monthly `Update Rollup` updates:

    ```
    check_os_updates "warning=count - defender > 0" "detail-syntax=${count} total, ${defender} defender, ${rollups} rollups"
    ```

    **Filtering by title**

    `update-filter=<substring>` restricts the check to updates whose title contains
    the (case-insensitive) substring; all counters (`count`, `security`, …) are then
    recomputed over just the matching subset:

    ```
    check_os_updates update-filter=".NET" "detail-syntax=${count} .NET updates: ${titles}"
    ```

    > **Note:** the WUA search criteria is `Type='Software'`, so **driver updates are
    > excluded** by design. This keeps the count focused on OS/application patches.

    **Customizing the output**

    You can use the syntax options to format the output string:

    ```
    check_os_updates "top-syntax=${status}: Found ${count} missing updates. Security: ${security}, Critical: ${critical}" "detail-syntax=${titles}" show-all
    ```

**Jump to section:**

* [Command-line Arguments](#check_os_updates_options)
* [Filter keywords](#check_os_updates_filter_keys)



<a id="check_os_updates_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_os_updates_warn"></a>
    <a id="check_os_updates_crit"></a>
    <a id="check_os_updates_debug"></a>
    <a id="check_os_updates_show-all"></a>
    <a id="check_os_updates_escape-html"></a>
    <a id="check_os_updates_help"></a>
    <a id="check_os_updates_help-pb"></a>
    <a id="check_os_updates_show-default"></a>
    <a id="check_os_updates_help-short"></a>

    | Option                                           | Default Value                                                                      | Description                                                                                                      |
    |--------------------------------------------------|------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_os_updates_filter)               |                                                                                    | Filter which marks interesting items.                                                                            |
    | [warning](#check_os_updates_warning)             | count > 0                                                                          | Filter which marks items which generates a warning state.                                                        |
    | warn                                             |                                                                                    | Short alias for warning                                                                                          |
    | [critical](#check_os_updates_critical)           | security > 0 or critical > 0                                                       | Filter which marks items which generates a critical state.                                                       |
    | crit                                             |                                                                                    | Short alias for critical.                                                                                        |
    | [ok](#check_os_updates_ok)                       |                                                                                    | Filter which marks items which generates an ok state.                                                            |
    | debug                                            | N/A                                                                                | Show debugging information in the log                                                                            |
    | show-all                                         | N/A                                                                                | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_os_updates_empty-state)     | ok                                                                                 | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_os_updates_perf-config)     |                                                                                    | Performance data generation configuration                                                                        |
    | escape-html                                      | N/A                                                                                | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                             | N/A                                                                                | Show help screen (this screen)                                                                                   |
    | help-pb                                          | N/A                                                                                | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                     | N/A                                                                                | Show default values for a given command                                                                          |
    | help-short                                       | N/A                                                                                | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_os_updates_top-syntax)       | ${status}: ${count} updates available (${security} security, ${critical} critical) | Top level syntax.                                                                                                |
    | [ok-syntax](#check_os_updates_ok-syntax)         | %(status): No updates available.                                                   | ok syntax.                                                                                                       |
    | [empty-syntax](#check_os_updates_empty-syntax)   |                                                                                    | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_os_updates_detail-syntax) | ${count} updates (${security} security, ${critical} critical)                      | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_os_updates_perf-syntax)     | updates                                                                            | Performance alias syntax.                                                                                        |



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


    *Default Value:* `security > 0 or critical > 0`

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

    *Default Value:* `${status}: ${count} updates available (${security} security, ${critical} critical)`

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

    *Default Value:* `${count} updates (${security} security, ${critical} critical)`

    <h5 id="check_os_updates_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `updates`

=== "Linux"

    <a id="check_os_updates_warn"></a>
    <a id="check_os_updates_crit"></a>
    <a id="check_os_updates_debug"></a>
    <a id="check_os_updates_show-all"></a>
    <a id="check_os_updates_escape-html"></a>
    <a id="check_os_updates_help"></a>
    <a id="check_os_updates_help-pb"></a>
    <a id="check_os_updates_show-default"></a>
    <a id="check_os_updates_help-short"></a>

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

=== "Windows"

    | Option          | Description                                              |
    |-----------------|----------------------------------------------------------|
    | critical        | Number of critical updates                               |
    | error           | Last error message from the WUA search (if any)          |
    | important       | Number of updates with MSRC severity 'Important'         |
    | reboot_required | Number of updates requiring a reboot                     |
    | security        | Number of security updates                               |
    | titles          | Semicolon separated list of available update titles      |
    | update_status   | Aggregated status: ok, warning, critical, pending, error |

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

=== "Linux"

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

Reports the version of the underlying Windows OS, sourced from the OS version
information, the registry (UBR), `GetNativeSystemInfo` for the processor
architecture, and `Win32_BIOS` (WMI) for the inventory fields.

| Keyword          | Description                                                                                                                                                                                                    |
|------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `version`        | System version (numeric for thresholds, e.g. `major*10+minor`; friendly product name in output, e.g. `Windows 11 23H2`).                                                                                       |
| `major`          | Major version number (perf).                                                                                                                                                                                   |
| `minor`          | Minor version number (perf).                                                                                                                                                                                   |
| `build`          | Build number (perf).                                                                                                                                                                                           |
| `ubr`            | Update Build Revision — the patch level within a build (the `.3803` in `10.0.19045.3803`). Read from the registry; `0` when unavailable (pre-Windows 10).                                                      |
| `kernel_version` | NT kernel version shorthand as `major.minor.build.ubr` (on Windows the kernel version tracks the OS version).                                                                                                  |
| `arch`           | Native processor architecture: `x64`, `x86`, `arm64`, `arm`, `ia64` or `unknown`. Reported via `GetNativeSystemInfo`, so a 32-bit agent under WOW64 still reports the true hardware architecture (e.g. `x64`). |
| `suite`          | Installed suites (Datacenter Edition, Enterprise Edition, Terminal Services, …).                                                                                                                               |
| `serial`         | BIOS/system serial number (`Win32_BIOS.SerialNumber`). Inventory only.                                                                                                                                         |
| `bios_version`   | BIOS version (`Win32_BIOS.SMBIOSBIOSVersion`). Inventory only.                                                                                                                                                 |
| `manufacturer`   | BIOS manufacturer / vendor (`Win32_BIOS.Manufacturer`). Inventory only.                                                                                                                                        |

The default warning/critical thresholds (`version <= 50`, i.e. pre-Windows-XP)
exist only to flag ancient/unsupported platforms; they never trip on a supported
OS. Set your own threshold on `build`/`ubr` to alert on a minimum patch level, or
filter on `arch` to assert a fleet's architecture.

`serial`, `bios_version` and `manufacturer` are **inventory-only**: they are read
best-effort from WMI, are empty when WMI is unavailable, are not part of the
default output, and are not intended for alerting. Reference them in a custom
`detail-syntax` (or `top-syntax`) to pull inventory.

**Jump to section:**

* [Sample Commands](#check_os_version_samples)
* [Command-line Arguments](#check_os_version_options)
* [Filter keywords](#check_os_version_filter_keys)


<a id="check_os_version_samples"></a>
#### Sample Commands

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

**Kernel version and architecture** (the default output is now
`${version} (${kernel_version}) ${arch}`, where `kernel_version` is the full
`major.minor.build.ubr`):

```
check_os_version
OK: Windows 11 23H2 (10.0.22631.3810) x64|'version'=110;50;50 'major'=10 'minor'=0 'build'=22631
```

Alert on a minimum patch level using `ubr`, and assert a 64-bit fleet:

```
check_os_version "warn=ubr < 3800" "crit=arch != 'x64'"
OK: Windows 11 23H2 (10.0.22631.3810) x64|'version'=110;50;50 'major'=10 'minor'=0 'build'=22631
```

**Inventory pull** — BIOS serial / version / manufacturer via a custom
`detail-syntax` (these fields never alert and are empty if WMI is unavailable):

```
check_os_version "detail-syntax=${serial} / ${manufacturer} BIOS ${bios_version} / ${kernel_version} ${arch}"
OK: 5CG1234ABC / American Megatrends Inc. BIOS 1.7.0 / 10.0.22631.3810 x64|'version'=110;50;50 'major'=10 'minor'=0 'build'=22631
```




<a id="check_os_version_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_os_version_warn"></a>
    <a id="check_os_version_crit"></a>
    <a id="check_os_version_debug"></a>
    <a id="check_os_version_show-all"></a>
    <a id="check_os_version_escape-html"></a>
    <a id="check_os_version_help"></a>
    <a id="check_os_version_help-pb"></a>
    <a id="check_os_version_show-default"></a>
    <a id="check_os_version_help-short"></a>

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

    *Default Value:* `${version} (${major}.${minor}.${build})`

    <h5 id="check_os_version_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `version`

=== "Linux"

    <a id="check_os_version_warn"></a>
    <a id="check_os_version_crit"></a>
    <a id="check_os_version_debug"></a>
    <a id="check_os_version_show-all"></a>
    <a id="check_os_version_escape-html"></a>
    <a id="check_os_version_help"></a>
    <a id="check_os_version_help-pb"></a>
    <a id="check_os_version_show-default"></a>
    <a id="check_os_version_help-short"></a>

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

=== "Windows"

    | Option  | Description                                                                                                                                                                                                                                                           |
    |---------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | build   | Build version number                                                                                                                                                                                                                                                  |
    | major   | Major version number                                                                                                                                                                                                                                                  |
    | minor   | Minor version number                                                                                                                                                                                                                                                  |
    | suite   | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
    | version | The system version                                                                                                                                                                                                                                                    |

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

=== "Linux"

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

Alerting on the peak commit charge since boot (high-water mark), not just current usage::

```
check_pagefile "warn=peak_used_pct > 80" "crit=peak_used_pct > 90" "detail-syntax=${name} peak ${peak_used} (${peak_used_pct}%)"
OK: total peak 3.1G (12%)
Performance data: 'total peak_used'=3G;... 'total peak_used_pct'=12;80;90
```

The `peak_used` (bytes, scaled) and `peak_used_pct` keywords expose
`SystemPageFileInformation`'s PeakUsage — the highest pagefile commit reached
since boot — so a machine that spiked and recovered still alerts.

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



<a id="check_pagefile_options"></a>
#### Command-line Arguments

<a id="check_pagefile_warn"></a>
<a id="check_pagefile_crit"></a>
<a id="check_pagefile_debug"></a>
<a id="check_pagefile_show-all"></a>
<a id="check_pagefile_escape-html"></a>
<a id="check_pagefile_help"></a>
<a id="check_pagefile_help-pb"></a>
<a id="check_pagefile_show-default"></a>
<a id="check_pagefile_help-short"></a>

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

=== "Windows"

    | Option   | Description                                     |
    |----------|-------------------------------------------------|
    | free     | Free memory in bytes (g,m,k,b) or percentages % |
    | free_pct | % free memory                                   |
    | name     | The name of the page file (location)            |
    | size     | Total size of pagefile                          |
    | used     | Used memory in bytes (g,m,k,b) or percentages % |
    | used_pct | % used memory                                   |

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

=== "Linux"

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

### check_pdh

*Available on Windows only.*

Check the value of a performance (PDH) counter on the local or remote system.
The counters can also be added and polled periodically to get average values. Performance Log Users group membership is required to check performance counters.

**Jump to section:**

* [Sample Commands](#check_pdh_samples)
* [Command-line Arguments](#check_pdh_options)
* [Filter keywords](#check_pdh_filter_keys)


<a id="check_pdh_samples"></a>
#### Sample Commands

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

Using real-time checks to check average values over time.

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



<a id="check_pdh_options"></a>
#### Command-line Arguments

<a id="check_pdh_warn"></a>
<a id="check_pdh_crit"></a>
<a id="check_pdh_debug"></a>
<a id="check_pdh_show-all"></a>
<a id="check_pdh_escape-html"></a>
<a id="check_pdh_help"></a>
<a id="check_pdh_help-pb"></a>
<a id="check_pdh_show-default"></a>
<a id="check_pdh_help-short"></a>
<a id="check_pdh_counter"></a>
<a id="check_pdh_time"></a>
<a id="check_pdh_flags"></a>

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
| [expand-index](#check_pdh_expand-index)   | 1)] (=0             | Expand indexes in counter strings                                                                                                    |
| [instances](#check_pdh_instances)         | 1)] (=0             | Expand wildcards and fetch all instances                                                                                             |
| [reload](#check_pdh_reload)               | 1)] (=0             | Reload counters on errors (useful to check counters which are not added at boot)                                                     |
| [averages](#check_pdh_averages)           | 1)] (=0             | Check average values (ie. wait for 1 second to collecting two samples)                                                               |
| time                                      |                     | Timeframe to use for named rrd counters                                                                                              |
| flags                                     |                     | Extra flags to configure the counter (nocap100, 1000, noscale)                                                                       |
| [type](#check_pdh_type)                   | large               | Format of value (double, long, large)                                                                                                |
| [ignore-errors](#check_pdh_ignore-errors) | 1)] (=0             | If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors |



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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

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
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${alias} = ${value}`

<h5 id="check_pdh_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${alias}`

<h5 id="check_pdh_expand-index">expand-index:</h5>

Expand indexes in counter strings

*Default Value:* `1)] (=0`

<h5 id="check_pdh_instances">instances:</h5>

Expand wildcards and fetch all instances

*Default Value:* `1)] (=0`

<h5 id="check_pdh_reload">reload:</h5>

Reload counters on errors (useful to check counters which are not added at boot)

*Default Value:* `1)] (=0`

<h5 id="check_pdh_averages">averages:</h5>

Check average values (ie. wait for 1 second to collecting two samples)

*Default Value:* `1)] (=0`

<h5 id="check_pdh_type">type:</h5>

Format of value (double, long, large)

*Default Value:* `large`

<h5 id="check_pdh_ignore-errors">ignore-errors:</h5>

If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors

*Default Value:* `1)] (=0`


<a id="check_pdh_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                        |
|-----------------|--------------------------------------------------------------------------------------------------------------------|
| alias           | The counter alias                                                                                                  |
| convert_bytes() | Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.           |
| counter         | The counter name                                                                                                   |
| format_bytes()  | Format a number as a human-readable byte string.                                                                   |
| scale()         | Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)). |
| time            | The time for rrd checks                                                                                            |
| value           | The counter value (either float or int)                                                                            |
| value_f         | The counter value (force float value)                                                                              |
| value_gb        | Counter value in GB (1024-based).                                                                                  |
| value_human     | Counter value formatted as a human-readable byte string, auto-scaled to B/KB/MB/GB/...                             |
| value_i         | The counter value (force int value)                                                                                |
| value_kb        | Counter value in KB (1024-based).                                                                                  |
| value_mb        | Counter value in MB (1024-based).                                                                                  |

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

#### Showing only the top processes (sorting and limiting)

`check_process` does not sort or limit its output: every matching process is evaluated and
returned. To report only the few most interesting processes (for example the 10 biggest
memory consumers) wrap the check in
[`filter_perf`](../check/CheckHelpers.md#filter_perf), which post-processes the performance
data produced by a check, sorting it (`sort=normal`, biggest first) and limiting it
(`limit=N`).

For example, the top 10 processes by working set (RAM), excluding SQL Server:

```
filter_perf sort=normal limit=10 command=check_process arguments "filter=working_set > 0 and exe not in ('sqlservr.exe')" "warn=working_set > 3G" "crit=working_set > 5G" "detail-syntax=%(exe) ws=%(working_set)"
```

The same approach works for CPU usage. Pass `delta=true` so that `%(time)` reports the CPU
time consumed since the previous check (a percentage-like rate) rather than the total CPU
time since the process started, for example the top 10 processes by CPU:

```
filter_perf sort=normal limit=10 command=check_process arguments delta=true "warn=time > 50" "crit=time > 90" "detail-syntax=%(exe) cpu=%(time)%"
```

Note that `limit` only trims the performance data; the warning/critical status is still
evaluated against every matching process, so an alert is raised even if the offending
process is not among the items shown.

**Jump to section:**

* [Sample Commands](#check_process_samples)
* [Command-line Arguments](#check_process_options)
* [Filter keywords](#check_process_filter_keys)


<a id="check_process_samples"></a>
#### Sample Commands

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

**Thread count**::

```
check_process process=chrome.exe "warn=thread_count > 400" "detail-syntax=${exe}: ${thread_count} threads"
OK: chrome.exe: 212 threads
Performance data: 'chrome.exe threads'=212;400;0
```

**Percentage-of-RAM / percentage-of-commit** thresholds::

```
check_process process=sqlservr.exe "warn=working_set_pct > 25" "crit=working_set_pct > 40" "detail-syntax=${exe}: ${working_set_pct}% RAM, ${pagefile_pct}% commit"
OK: sqlservr.exe: 12% RAM, 8% commit
Performance data: 'sqlservr.exe ws_pct'=12%;25;40 'sqlservr.exe pf_pct'=8%;;
```

`working_set_pct` is the process working set as a percentage of total physical
RAM; `pagefile_pct` is its pagefile (commit) usage as a percentage of the system
commit limit (RAM + pagefile). Both work with `total=true` aggregation.




<a id="check_process_options"></a>
#### Command-line Arguments

=== "Windows"

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
    <a id="check_process_scan-info"></a>
    <a id="check_process_scan-16bit"></a>
    <a id="check_process_scan-unreadable"></a>

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
    | [delta](#check_process_delta)                 |                                  | Measure CPU usage as a delta over a one second interval.                                                         |
    | scan-unreadable                               |                                  | If unreadable processes should be included (will not have information)                                           |
    | [total](#check_process_total)                 | 1)] (=0                          | Include the total of all matching files                                                                          |



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


    <h5 id="check_process_total">total:</h5>

    Include the total of all matching files

    *Default Value:* `1)] (=0`

=== "Linux"

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
    | [total](#check_process_total)                 | 1)] (=0                          | Include the total of all matching processes                                                                      |



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


    <h5 id="check_process_total">total:</h5>

    Include the total of all matching processes

    *Default Value:* `1)] (=0`


<a id="check_process_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option           | Description                                             |
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
    | pagefile         | Peak page file use in bytes (g,m,k,b)                   |
    | peak_pagefile    | Page file usage in bytes (g,m,k,b)                      |
    | peak_virtual     | Peak virtual size in bytes (g,m,k,b)                    |
    | peak_working_set | Peak working set in bytes (g,m,k,b)                     |
    | pid              | Process id                                              |
    | started          | Process is started                                      |
    | state            | The current state (started, stopped hung)               |
    | stopped          | Process is stopped                                      |
    | time             | User-kernel time in seconds                             |
    | user             | User time in seconds                                    |
    | user_handles     | Number of handles                                       |
    | virtual          | Virtual size in bytes (g,m,k,b)                         |
    | working_set      | Working set in bytes (g,m,k,b)                          |

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

=== "Linux"

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

=== "Windows"

    Check the history of processes that have been running since NSClient++ started. Useful for verifying if certain applications have been executed.

=== "Linux"

    Check the history of processes seen since NSClient++ started (requires 'process history = true').

**Jump to section:**

* [Command-line Arguments](#check_process_history_options)
* [Filter keywords](#check_process_history_filter_keys)



<a id="check_process_history_options"></a>
#### Command-line Arguments

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

=== "Windows"

    Check for new processes that appeared within a specified time window. Useful for detecting unexpected or unauthorized applications.

=== "Linux"

    Check for processes first seen within a recent time window (requires 'process history = true').

**Jump to section:**

* [Command-line Arguments](#check_process_history_new_options)
* [Filter keywords](#check_process_history_new_filter_keys)



<a id="check_process_history_new_options"></a>
#### Command-line Arguments

<a id="check_process_history_new_warn"></a>
<a id="check_process_history_new_crit"></a>
<a id="check_process_history_new_debug"></a>
<a id="check_process_history_new_show-all"></a>
<a id="check_process_history_new_escape-html"></a>
<a id="check_process_history_new_help"></a>
<a id="check_process_history_new_help-pb"></a>
<a id="check_process_history_new_show-default"></a>
<a id="check_process_history_new_help-short"></a>

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

### check_registry_key

*Available on Windows only.*

Check existence, last-write time, and child counts of one or more Windows registry keys.

**Jump to section:**

* [Sample Commands](#check_registry_key_samples)
* [Command-line Arguments](#check_registry_key_options)
* [Filter keywords](#check_registry_key_filter_keys)


<a id="check_registry_key_samples"></a>
#### Sample Commands

**Default check (single key, just verifies it exists):**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion"
OK: All 1 registry key(s) are ok.
```

**Key that does not exist (default `crit=not exists`):**

```
check_registry_key "key=HKLM\Software\DoesNotExist"
CRITICAL: HKLM\Software\DoesNotExist: exists=false, subkeys=0, values=0
```

**Check several keys in one call:**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" "key=HKLM\Software\NSClient"
OK: All 2 registry key(s) are ok.
```

**Wildcard / recursive enumeration of immediate sub-keys:**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" recursive max-depth=1 "top-syntax=%(status): %(list)" "detail-syntax=%(name) (subkeys=%(subkey_count), values=%(value_count))"
OK: AeDebug (subkeys=1, values=2), Compatibility32 (subkeys=0, values=0), Console (subkeys=4, values=18), ...
```

**Force a 32-bit or 64-bit registry view (WoW64):**

```
check_registry_key "key=HKLM\Software\NSClient" view=64
OK: All 1 registry key(s) are ok.

check_registry_key "key=HKLM\Software\NSClient" view=32
CRITICAL: HKLM\Software\NSClient: exists=false, subkeys=0, values=0
```

**Exclude noisy sub-keys when recursing:**

```
check_registry_key "key=HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall" recursive max-depth=1 exclude=KB5005463 exclude=KB5005539
OK: All 248 registry key(s) are ok.
```

**Alert when a key is unexpectedly empty:**

```
check_registry_key "key=HKLM\Software\NSClient" "warn=value_count < 5" "crit=value_count = 0 or not exists"
OK: HKLM\Software\NSClient: exists=true, subkeys=2, values=12
```

**Alert when a key has not been written for over 30 days (configuration drift watchdog):**

```
check_registry_key "key=HKLM\Software\NSClient" "warn=age > 7d" "crit=age > 30d or not exists"
OK: HKLM\Software\NSClient: exists=true, subkeys=2, values=12
```

**Custom output text:**

```
check_registry_key "key=HKLM\Software\NSClient" "top-syntax=%(status): %(list)" "detail-syntax=%(path) last-written %(written_s)"
OK: HKLM\Software\NSClient last-written 2026-04-15 09:12:33
```

**Remote computer / 32-bit view via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_registry_key --argument "key=HKLM\Software\NSClient" --argument "view=64"
OK: All 1 registry key(s) are ok.
```



<a id="check_registry_key_options"></a>
#### Command-line Arguments

<a id="check_registry_key_warn"></a>
<a id="check_registry_key_crit"></a>
<a id="check_registry_key_debug"></a>
<a id="check_registry_key_show-all"></a>
<a id="check_registry_key_escape-html"></a>
<a id="check_registry_key_help"></a>
<a id="check_registry_key_help-pb"></a>
<a id="check_registry_key_show-default"></a>
<a id="check_registry_key_help-short"></a>
<a id="check_registry_key_key"></a>
<a id="check_registry_key_exclude"></a>
<a id="check_registry_key_computer"></a>
<a id="check_registry_key_max-depth"></a>

| Option                                             | Default Value                                                             | Description                                                                                                      |
|----------------------------------------------------|---------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_registry_key_filter)               |                                                                           | Filter which marks interesting items.                                                                            |
| [warning](#check_registry_key_warning)             |                                                                           | Filter which marks items which generates a warning state.                                                        |
| warn                                               |                                                                           | Short alias for warning                                                                                          |
| [critical](#check_registry_key_critical)           | not exists                                                                | Filter which marks items which generates a critical state.                                                       |
| crit                                               |                                                                           | Short alias for critical.                                                                                        |
| [ok](#check_registry_key_ok)                       |                                                                           | Filter which marks items which generates an ok state.                                                            |
| debug                                              | N/A                                                                       | Show debugging information in the log                                                                            |
| show-all                                           | N/A                                                                       | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_registry_key_empty-state)     | unknown                                                                   | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_registry_key_perf-config)     |                                                                           | Performance data generation configuration                                                                        |
| escape-html                                        | N/A                                                                       | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                               | N/A                                                                       | Show help screen (this screen)                                                                                   |
| help-pb                                            | N/A                                                                       | Show help screen as a protocol buffer payload                                                                    |
| show-default                                       | N/A                                                                       | Show default values for a given command                                                                          |
| help-short                                         | N/A                                                                       | Show help screen (short format).                                                                                 |
| [top-syntax](#check_registry_key_top-syntax)       | ${status}: ${problem_list}                                                | Top level syntax.                                                                                                |
| [ok-syntax](#check_registry_key_ok-syntax)         | ${status}: All %(count) registry key(s) are ok.                           | ok syntax.                                                                                                       |
| [empty-syntax](#check_registry_key_empty-syntax)   | ${status}: No registry keys found                                         | Empty syntax.                                                                                                    |
| [detail-syntax](#check_registry_key_detail-syntax) | ${path}: exists=${exists}, subkeys=${subkey_count}, values=${value_count} | Detail level syntax.                                                                                             |
| [perf-syntax](#check_registry_key_perf-syntax)     | ${path}                                                                   | Performance alias syntax.                                                                                        |
| key                                                |                                                                           | One or more registry key paths to check (e.g. HKLM\Software\MyApp).                                              |
| exclude                                            |                                                                           | Registry key names to exclude from enumeration                                                                   |
| computer                                           |                                                                           | Remote computer to connect to (empty = local)                                                                    |
| [view](#check_registry_key_view)                   | default                                                                   | Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)                                      |
| [recursive](#check_registry_key_recursive)         | 1)] (=0                                                                   | Recursively enumerate all sub-keys below each starting key                                                       |
| max-depth                                          |                                                                           | Maximum recursion depth (requires --recursive; -1 = unlimited)                                                   |



<h5 id="check_registry_key_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_registry_key_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_registry_key_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `not exists`

<h5 id="check_registry_key_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_registry_key_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_registry_key_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_registry_key_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_registry_key_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: All %(count) registry key(s) are ok.`

<h5 id="check_registry_key_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `${status}: No registry keys found`

<h5 id="check_registry_key_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${path}: exists=${exists}, subkeys=${subkey_count}, values=${value_count}`

<h5 id="check_registry_key_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${path}`

<h5 id="check_registry_key_view">view:</h5>

Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)

*Default Value:* `default`

<h5 id="check_registry_key_recursive">recursive:</h5>

Recursively enumerate all sub-keys below each starting key

*Default Value:* `1)] (=0`


<a id="check_registry_key_filter_keys"></a>
#### Filter keywords

| Option       | Description                                                      |
|--------------|------------------------------------------------------------------|
| age          | Seconds since the key was last written                           |
| class        | Key class string (rarely set)                                    |
| depth        | Depth below the starting key (0 = the key itself)                |
| exists       | Whether the key exists (true/false)                              |
| hive         | Hive abbreviation (HKLM, HKCU, HKCR, HKU, HKCC)                  |
| name         | Leaf key name                                                    |
| parent       | Parent key path (full, including hive)                           |
| path         | Full registry key path including hive (e.g. HKLM\Software\MyApp) |
| subkey_count | Number of immediate sub-keys                                     |
| value_count  | Number of values in this key                                     |
| written      | Last-write time (epoch seconds; supports date comparisons)       |
| written_s    | Last-write time as a human-readable string                       |

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

### check_registry_value

*Available on Windows only.*

Check the type, content, and size of one or more Windows registry values.

**Jump to section:**

* [Sample Commands](#check_registry_value_samples)
* [Command-line Arguments](#check_registry_value_options)
* [Filter keywords](#check_registry_value_filter_keys)


<a id="check_registry_value_samples"></a>
#### Sample Commands

**Read a single value (default: enumerates all values in the key):**

```
check_registry_value "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" value=ProductName
OK: HKLM\Software\Microsoft\Windows NT\CurrentVersion\ProductName: Windows 10 Pro (type=REG_SZ)
```

**Read multiple specific values from the same key:**

```
check_registry_value "key=HKLM\Software\Microsoft\Windows NT\CurrentVersion" value=ProductName value=CurrentBuild value=ReleaseId
OK: All 3 registry value(s) are ok.
```

**Enumerate every value in a key:**

```
check_registry_value "key=HKLM\Software\NSClient" "top-syntax=%(status): %(list)" "detail-syntax=%(name)=%(string_value)"
OK: ConfigFile=C:\Program Files\NSClient++\nsclient.ini, InstallVersion=0.6.0, ...
```

**Value that does not exist (default `crit=not exists`):**

```
check_registry_value "key=HKLM\Software\NSClient" value=NoSuchValue
CRITICAL: HKLM\Software\NSClient\NoSuchValue: (type=REG_NONE)
```

**Type assertion (alert if a value isn't the expected type):**

```
check_registry_value "key=HKLM\Software\NSClient" value=InstallVersion "crit=type != 'REG_SZ' or not exists"
OK: HKLM\Software\NSClient\InstallVersion: 0.6.0 (type=REG_SZ)
```

**Numeric DWORD / QWORD comparison:**

```
check_registry_value "key=HKLM\System\CurrentControlSet\Services\W32Time\Config" value=MaxPollInterval "warn=int_value > 14" "crit=int_value > 17"
OK: HKLM\System\CurrentControlSet\Services\W32Time\Config\MaxPollInterval: 10 (type=REG_DWORD)
```

**String / content match:**

```
check_registry_value "key=HKLM\Software\NSClient" value=ConfigFile "crit=string_value not like 'C:\\Program Files\\NSClient++\\nsclient.ini'"
OK: HKLM\Software\NSClient\ConfigFile: C:\Program Files\NSClient++\nsclient.ini (type=REG_SZ)
```

**Size watchdog (alert if a binary blob grows unexpectedly):**

```
check_registry_value "key=HKLM\Software\NSClient" value=Cache "warn=size > 4096" "crit=size > 16384"
OK: HKLM\Software\NSClient\Cache: 0xDEADBEEF... (type=REG_BINARY)
```

**Force the 32-bit registry view (WoW64):**

```
check_registry_value "key=HKLM\Software\NSClient" value=InstallDir view=32
OK: HKLM\Software\NSClient\InstallDir: C:\Program Files (x86)\NSClient++\ (type=REG_SZ)
```

**Recursive enumeration of values across an entire sub-tree:**

```
check_registry_value "key=HKLM\Software\NSClient" recursive max-depth=2 "top-syntax=%(status): %(list)" "detail-syntax=%(path)=%(string_value)"
OK: HKLM\Software\NSClient\ConfigFile=..., HKLM\Software\NSClient\modules\enabled=1, ...
```

**Exclude noisy values during enumeration:**

```
check_registry_value "key=HKCU\Software\NSClient" exclude=LastRun exclude=Cache
OK: All 5 registry value(s) are ok.
```

**Custom output text including type / size:**

```
check_registry_value "key=HKLM\Software\NSClient" value=InstallVersion "top-syntax=%(status): %(list)" "detail-syntax=%(name) [%(type)] = %(string_value) (%(size)B)"
OK: InstallVersion [REG_SZ] = 0.6.0 (12B)
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_registry_value --argument "key=HKLM\Software\NSClient" --argument "value=InstallVersion"
OK: HKLM\Software\NSClient\InstallVersion: 0.6.0 (type=REG_SZ)
```



<a id="check_registry_value_options"></a>
#### Command-line Arguments

<a id="check_registry_value_warn"></a>
<a id="check_registry_value_crit"></a>
<a id="check_registry_value_debug"></a>
<a id="check_registry_value_show-all"></a>
<a id="check_registry_value_escape-html"></a>
<a id="check_registry_value_help"></a>
<a id="check_registry_value_help-pb"></a>
<a id="check_registry_value_show-default"></a>
<a id="check_registry_value_help-short"></a>
<a id="check_registry_value_key"></a>
<a id="check_registry_value_value"></a>
<a id="check_registry_value_exclude"></a>
<a id="check_registry_value_computer"></a>
<a id="check_registry_value_max-depth"></a>

| Option                                               | Default Value                           | Description                                                                                                      |
|------------------------------------------------------|-----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_registry_value_filter)               |                                         | Filter which marks interesting items.                                                                            |
| [warning](#check_registry_value_warning)             |                                         | Filter which marks items which generates a warning state.                                                        |
| warn                                                 |                                         | Short alias for warning                                                                                          |
| [critical](#check_registry_value_critical)           | not exists                              | Filter which marks items which generates a critical state.                                                       |
| crit                                                 |                                         | Short alias for critical.                                                                                        |
| [ok](#check_registry_value_ok)                       |                                         | Filter which marks items which generates an ok state.                                                            |
| debug                                                | N/A                                     | Show debugging information in the log                                                                            |
| show-all                                             | N/A                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_registry_value_empty-state)     | unknown                                 | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_registry_value_perf-config)     |                                         | Performance data generation configuration                                                                        |
| escape-html                                          | N/A                                     | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                                 | N/A                                     | Show help screen (this screen)                                                                                   |
| help-pb                                              | N/A                                     | Show help screen as a protocol buffer payload                                                                    |
| show-default                                         | N/A                                     | Show default values for a given command                                                                          |
| help-short                                           | N/A                                     | Show help screen (short format).                                                                                 |
| [top-syntax](#check_registry_value_top-syntax)       | ${status}: ${problem_list}              | Top level syntax.                                                                                                |
| [ok-syntax](#check_registry_value_ok-syntax)         | ${status}: %(list).                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_registry_value_empty-syntax)   | ${status}: No registry values found     | Empty syntax.                                                                                                    |
| [detail-syntax](#check_registry_value_detail-syntax) | ${path}: ${string_value} (type=${type}) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_registry_value_perf-syntax)     | ${path}                                 | Performance alias syntax.                                                                                        |
| key                                                  |                                         | One or more registry key paths whose values to check (e.g. HKLM\Software\MyApp)                                  |
| value                                                |                                         | Restrict to specific value names (default: all values). Supports '*' to enumerate all.                           |
| exclude                                              |                                         | Value names to exclude from enumeration                                                                          |
| computer                                             |                                         | Remote computer to connect to (empty = local)                                                                    |
| [view](#check_registry_value_view)                   | default                                 | Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)                                      |
| [recursive](#check_registry_value_recursive)         | 1)] (=0                                 | Recursively enumerate values in all sub-keys                                                                     |
| max-depth                                            |                                         | Maximum recursion depth for --recursive (-1 = unlimited)                                                         |



<h5 id="check_registry_value_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_registry_value_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_registry_value_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `not exists`

<h5 id="check_registry_value_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_registry_value_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_registry_value_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_registry_value_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_registry_value_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `${status}: %(list).`

<h5 id="check_registry_value_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `${status}: No registry values found`

<h5 id="check_registry_value_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${path}: ${string_value} (type=${type})`

<h5 id="check_registry_value_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${path}`

<h5 id="check_registry_value_view">view:</h5>

Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)

*Default Value:* `default`

<h5 id="check_registry_value_recursive">recursive:</h5>

Recursively enumerate values in all sub-keys

*Default Value:* `1)] (=0`


<a id="check_registry_value_filter_keys"></a>
#### Filter keywords

| Option       | Description                                                                                 |
|--------------|---------------------------------------------------------------------------------------------|
| age          | Seconds since parent key was last written                                                   |
| exists       | Whether the value exists (true/false)                                                       |
| hive         | Hive abbreviation (HKLM, HKCU, HKCR, HKU, HKCC)                                             |
| int_value    | Numeric value (REG_DWORD / REG_QWORD); 0 for non-numeric types                              |
| key          | Parent key path (full, including hive)                                                      |
| name         | Value name ('(default)' for the unnamed default value)                                      |
| path         | Full path: key\name                                                                         |
| size         | Raw byte size of the value data                                                             |
| string_value | Value rendered as a string (REG_SZ expanded, REG_DWORD as decimal, REG_BINARY as hex, etc.) |
| type         | Value type (REG_SZ, REG_DWORD, etc.)                                                        |
| written      | Parent key last-write time (epoch seconds; supports date comparisons)                       |
| written_s    | Parent key last-write time as a human-readable string                                       |

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

**Dashboard rollup with `summary` (aggregate state-count perfdata)**::

Adding `summary` emits per-state counts across all enumerated services as
performance data, so a dashboard gets running/stopped/paused/pending/total
rollups without a custom `top-syntax`:

```
check_service summary "filter=none"
OK: All 214 service(s) are ok.
'running_services'=118 'stopped_services'=94 'paused_services'=0 'pending_services'=2 'service_count'=214
```

The counts cover every matched service regardless of the warning/critical
filter, so the rollup is stable even when the check itself is OK.



<a id="check_service_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_service_warn"></a>
    <a id="check_service_crit"></a>
    <a id="check_service_debug"></a>
    <a id="check_service_show-all"></a>
    <a id="check_service_escape-html"></a>
    <a id="check_service_help"></a>
    <a id="check_service_help-pb"></a>
    <a id="check_service_show-default"></a>
    <a id="check_service_help-short"></a>
    <a id="check_service_computer"></a>
    <a id="check_service_service"></a>
    <a id="check_service_exclude"></a>

    | Option                                            | Default Value                                           | Description                                                                                                                                           |
    |---------------------------------------------------|---------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_service_filter)                   |                                                         | Filter which marks interesting items.                                                                                                                 |
    | [warning](#check_service_warning)                 | not state_is_perfect()                                  | Filter which marks items which generates a warning state.                                                                                             |
    | warn                                              |                                                         | Short alias for warning                                                                                                                               |
    | [critical](#check_service_critical)               | not state_is_ok()                                       | Filter which marks items which generates a critical state.                                                                                            |
    | crit                                              |                                                         | Short alias for critical.                                                                                                                             |
    | [ok](#check_service_ok)                           |                                                         | Filter which marks items which generates an ok state.                                                                                                 |
    | debug                                             | N/A                                                     | Show debugging information in the log                                                                                                                 |
    | show-all                                          | N/A                                                     | Show details for all matches regardless of status (normally details are only showed for warnings and criticals).                                      |
    | [empty-state](#check_service_empty-state)         | unknown                                                 | Return status to use when nothing matched filter.                                                                                                     |
    | [perf-config](#check_service_perf-config)         |                                                         | Performance data generation configuration                                                                                                             |
    | escape-html                                       | N/A                                                     | Escape any < and > characters to prevent HTML encoding                                                                                                |
    | help                                              | N/A                                                     | Show help screen (this screen)                                                                                                                        |
    | help-pb                                           | N/A                                                     | Show help screen as a protocol buffer payload                                                                                                         |
    | show-default                                      | N/A                                                     | Show default values for a given command                                                                                                               |
    | help-short                                        | N/A                                                     | Show help screen (short format).                                                                                                                      |
    | [top-syntax](#check_service_top-syntax)           | ${status}: ${crit_list}, delayed (${warn_list})         | Top level syntax.                                                                                                                                     |
    | [ok-syntax](#check_service_ok-syntax)             | %(status): All %(count) service(s) are ok.              | ok syntax.                                                                                                                                            |
    | [empty-syntax](#check_service_empty-syntax)       | %(status): No services found                            | Empty syntax.                                                                                                                                         |
    | [detail-syntax](#check_service_detail-syntax)     | ${name}=${state}, exit=%(exit_code), type=%(start_type) | Detail level syntax.                                                                                                                                  |
    | [perf-syntax](#check_service_perf-syntax)         | ${name}                                                 | Performance alias syntax.                                                                                                                             |
    | computer                                          |                                                         | The name of the remote computer to check                                                                                                              |
    | service                                           |                                                         | The service to check, set this to * to check all services                                                                                             |
    | exclude                                           |                                                         | A list of services to ignore (mainly useful in combination with service=*)                                                                            |
    | [type](#check_service_type)                       | service                                                 | The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process |
    | [state](#check_service_state)                     | all                                                     | The types of services to enumerate available states are active, inactive or all                                                                       |
    | [only-essential](#check_service_only-essential)   | 1)] (=0                                                 | Set filter to classification = 'essential'                                                                                                            |
    | [only-ignored](#check_service_only-ignored)       | 1)] (=0                                                 | Set filter to classification = 'ignored'                                                                                                              |
    | [only-role](#check_service_only-role)             | 1)] (=0                                                 | Set filter to classification = 'role'                                                                                                                 |
    | [only-supporting](#check_service_only-supporting) | 1)] (=0                                                 | Set filter to classification = 'supporting'                                                                                                           |
    | [only-system](#check_service_only-system)         | 1)] (=0                                                 | Set filter to classification = 'system'                                                                                                               |
    | [only-user](#check_service_only-user)             | 1)] (=0                                                 | Set filter to classification = 'user'                                                                                                                 |



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

    *Default Value:* `${name}=${state}, exit=%(exit_code), type=%(start_type)`

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

    <h5 id="check_service_only-essential">only-essential:</h5>

    Set filter to classification = 'essential'

    *Default Value:* `1)] (=0`

    <h5 id="check_service_only-ignored">only-ignored:</h5>

    Set filter to classification = 'ignored'

    *Default Value:* `1)] (=0`

    <h5 id="check_service_only-role">only-role:</h5>

    Set filter to classification = 'role'

    *Default Value:* `1)] (=0`

    <h5 id="check_service_only-supporting">only-supporting:</h5>

    Set filter to classification = 'supporting'

    *Default Value:* `1)] (=0`

    <h5 id="check_service_only-system">only-system:</h5>

    Set filter to classification = 'system'

    *Default Value:* `1)] (=0`

    <h5 id="check_service_only-user">only-user:</h5>

    Set filter to classification = 'user'

    *Default Value:* `1)] (=0`

=== "Linux"

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

    | Option                                        | Default Value                                                                                   | Description                                                                                                      |
    |-----------------------------------------------|-------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_service_filter)               | active != 'inactive'                                                                            | Filter which marks interesting items.                                                                            |
    | [warning](#check_service_warning)             |                                                                                                 | Filter which marks items which generates a warning state.                                                        |
    | warn                                          |                                                                                                 | Short alias for warning                                                                                          |
    | [critical](#check_service_critical)           | ( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled' | Filter which marks items which generates a critical state.                                                       |
    | crit                                          |                                                                                                 | Short alias for critical.                                                                                        |
    | [ok](#check_service_ok)                       |                                                                                                 | Filter which marks items which generates an ok state.                                                            |
    | debug                                         | N/A                                                                                             | Show debugging information in the log                                                                            |
    | show-all                                      | N/A                                                                                             | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_service_empty-state)     | unknown                                                                                         | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_service_perf-config)     |                                                                                                 | Performance data generation configuration                                                                        |
    | escape-html                                   | N/A                                                                                             | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                          | N/A                                                                                             | Show help screen (this screen)                                                                                   |
    | help-pb                                       | N/A                                                                                             | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                  | N/A                                                                                             | Show default values for a given command                                                                          |
    | help-short                                    | N/A                                                                                             | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_service_top-syntax)       | ${status}: ${crit_list}                                                                         | Top level syntax.                                                                                                |
    | [ok-syntax](#check_service_ok-syntax)         | %(status): All %(count) service(s) are ok.                                                      | ok syntax.                                                                                                       |
    | [empty-syntax](#check_service_empty-syntax)   | %(status): No services found                                                                    | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_service_detail-syntax) | ${name}=${state}                                                                                | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_service_perf-syntax)     | ${name}                                                                                         | Performance alias syntax.                                                                                        |
    | service                                       |                                                                                                 | The service to check, set this to * to check all services                                                        |
    | exclude                                       |                                                                                                 | A list of services to ignore (mainly useful in combination with service=*)                                       |
    | [state](#check_service_state)                 | all                                                                                             | The state of services to enumerate: active, inactive, failed, or all                                             |



    <h5 id="check_service_filter">filter:</h5>

    Filter which marks interesting items.
    Interesting items are items which will be included in the check.
    They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

    *Default Value:* `active != 'inactive'`

    <h5 id="check_service_warning">warning:</h5>

    Filter which marks items which generates a warning state.
    If anything matches this filter the return status will be escalated to warning.



    <h5 id="check_service_critical">critical:</h5>

    Filter which marks items which generates a critical state.
    If anything matches this filter the return status will be escalated to critical.


    *Default Value:* `( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled'`

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

    *Default Value:* `${status}: ${crit_list}`

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

    *Default Value:* `${name}=${state}`

    <h5 id="check_service_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

    <h5 id="check_service_state">state:</h5>

    The state of services to enumerate: active, inactive, failed, or all

    *Default Value:* `all`


<a id="check_service_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option             | Description                                                                                                  |
    |--------------------|--------------------------------------------------------------------------------------------------------------|
    | classification     | Get classification                                                                                           |
    | delayed            | If the service is delayed                                                                                    |
    | desc               | Service description                                                                                          |
    | exit_code          | The Win32 exit code of the service                                                                           |
    | is_trigger         | If the service is has associated triggers                                                                    |
    | legacy_state       | Get legacy state (deprecated and only used by check_nt)                                                      |
    | name               | Service name                                                                                                 |
    | pid                | Process id                                                                                                   |
    | start_type         | The configured start type ()                                                                                 |
    | state              | The current state ()                                                                                         |
    | state_is_ok()      | Check if the state is ok, i.e. all running services are running (delayed services are allowed to be stopped) |
    | state_is_perfect() | Check if the state is ok, i.e. all running services are running                                              |
    | triggers           | The number of associated triggers for this service                                                           |

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

=== "Linux"

    | Option             | Description                                                                                         |
    |--------------------|-----------------------------------------------------------------------------------------------------|
    | active             | Raw systemd ActiveState (active, inactive, failed)                                                  |
    | age                | Seconds since the main process started                                                              |
    | cpu                | CPU usage of the main process in percent (lifetime average)                                         |
    | created            | Unix timestamp when the main process started                                                        |
    | desc               | Service description                                                                                 |
    | name               | Service name                                                                                        |
    | pid                | Main process id                                                                                     |
    | preset             | Vendor preset (enabled, disabled)                                                                   |
    | rss                | Resident memory of the main process in bytes                                                        |
    | service            | Alias for name                                                                                      |
    | start_type         | The configured start type (enabled, disabled, static, masked)                                       |
    | started            | Service is started/active                                                                           |
    | state              | The mapped service state (stopped, starting, oneshot, running, static, unknown)                     |
    | state_is_ok()      | Check if the state is ok (enabled services running or starting, disabled services can be any state) |
    | state_is_perfect() | Check if the state is perfect (enabled services running, disabled services stopped)                 |
    | stopped            | Service is stopped/inactive                                                                         |
    | sub_state          | Raw systemd SubState (running, dead, exited, ...)                                                   |
    | tasks              | Number of tasks (cgroup) for this service                                                           |
    | vms                | Virtual memory of the main process in bytes                                                         |

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

*Available on Linux only.*

Check the swap in/out paging rate.

Reports system paging (swap) I/O **rates**, sourced from the Windows memory
performance counters `\Memory\Pages Input/sec` and `\Memory\Pages Output/sec`
(sampled over a ~1 second window). Windows has no per-pagefile I/O counter, so
this is a single system-wide aggregate row.

The keyword vocabulary matches the Linux `check_swap_io`, so warning/critical
expressions and detail-syntax port between platforms.

| Keyword          | Description                                                                          |
|------------------|--------------------------------------------------------------------------------------|
| `name`           | Always `swap` (single aggregate row).                                                |
| `swap_count`     | Number of page files on the system.                                                  |
| `swap_in`        | Pages paged in from disk per second (perf, `io_swap_in`).                            |
| `swap_out`       | Pages paged out to disk per second (perf, `io_swap_out`).                            |
| `swap_in_bytes`  | Bytes paged in per second — `swap_in` × system page size (perf, `io_swap_in_bytes`). |
| `swap_out_bytes` | Bytes paged out per second (perf, `io_swap_out_bytes`).                              |

There are no default warning/critical thresholds: sustained paging is workload
dependent, and a default would warn on legitimately busy hosts. Set a threshold
on `swap_in`/`swap_out` (pages/s) or `swap_in_bytes`/`swap_out_bytes` (bytes/s)
for the host in question.

> Note: on Windows these are system-wide paging rates (pages moved between disk
> and physical memory) — the correct analogue of Linux swap-in/out — not literal
> per-pagefile read/write bytes.

**Jump to section:**

* [Sample Commands](#check_swap_io_samples)
* [Command-line Arguments](#check_swap_io_options)
* [Filter keywords](#check_swap_io_filter_keys)


<a id="check_swap_io_samples"></a>
#### Sample Commands

**Default check:**

```
check_swap_io
OK: 1 page file(s), in 0 pages/s, out 0 pages/s
'io_swap_in'=0;;; 'io_swap_out'=0;;; 'io_swap_in_bytes'=0B;;; 'io_swap_out_bytes'=0B;;;
```

**Alert on sustained paging (pages/s):**

```
check_swap_io "warn=swap_in > 1000" "crit=swap_in > 5000"
OK: 1 page file(s), in 42 pages/s, out 7 pages/s
'io_swap_in'=42;1000;5000; 'io_swap_out'=7;;; 'io_swap_in_bytes'=172032B;;; 'io_swap_out_bytes'=28672B;;;
```

**Threshold on throughput (bytes/s) with a custom output line:**

```
check_swap_io "crit=swap_out_bytes > 10485760" "detail-syntax=in ${swap_in_bytes}B/s, out ${swap_out_bytes}B/s"
OK: in 172032B/s, out 28672B/s
'io_swap_in_bytes'=172032B;;; 'io_swap_out_bytes'=28672B;;10485760;
```



<a id="check_swap_io_options"></a>
#### Command-line Arguments

<a id="check_swap_io_warn"></a>
<a id="check_swap_io_crit"></a>
<a id="check_swap_io_debug"></a>
<a id="check_swap_io_show-all"></a>
<a id="check_swap_io_escape-html"></a>
<a id="check_swap_io_help"></a>
<a id="check_swap_io_help-pb"></a>
<a id="check_swap_io_show-default"></a>
<a id="check_swap_io_help-short"></a>

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

=== "Windows"

    Check ACPI thermal zone temperatures.

=== "Linux"

    Check temperature sensors (thermal zones / hwmon).

**Jump to section:**

* [Command-line Arguments](#check_temperature_options)
* [Filter keywords](#check_temperature_filter_keys)



<a id="check_temperature_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_temperature_warn"></a>
    <a id="check_temperature_crit"></a>
    <a id="check_temperature_debug"></a>
    <a id="check_temperature_show-all"></a>
    <a id="check_temperature_escape-html"></a>
    <a id="check_temperature_help"></a>
    <a id="check_temperature_help-pb"></a>
    <a id="check_temperature_show-default"></a>
    <a id="check_temperature_help-short"></a>

    | Option                                            | Default Value                         | Description                                                                                                      |
    |---------------------------------------------------|---------------------------------------|------------------------------------------------------------------------------------------------------------------|
    | [filter](#check_temperature_filter)               |                                       | Filter which marks interesting items.                                                                            |
    | [warning](#check_temperature_warning)             | temperature > 70                      | Filter which marks items which generates a warning state.                                                        |
    | warn                                              |                                       | Short alias for warning                                                                                          |
    | [critical](#check_temperature_critical)           | temperature > 90                      | Filter which marks items which generates a critical state.                                                       |
    | crit                                              |                                       | Short alias for critical.                                                                                        |
    | [ok](#check_temperature_ok)                       |                                       | Filter which marks items which generates an ok state.                                                            |
    | debug                                             | N/A                                   | Show debugging information in the log                                                                            |
    | show-all                                          | N/A                                   | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
    | [empty-state](#check_temperature_empty-state)     | critical                              | Return status to use when nothing matched filter.                                                                |
    | [perf-config](#check_temperature_perf-config)     |                                       | Performance data generation configuration                                                                        |
    | escape-html                                       | N/A                                   | Escape any < and > characters to prevent HTML encoding                                                           |
    | help                                              | N/A                                   | Show help screen (this screen)                                                                                   |
    | help-pb                                           | N/A                                   | Show help screen as a protocol buffer payload                                                                    |
    | show-default                                      | N/A                                   | Show default values for a given command                                                                          |
    | help-short                                        | N/A                                   | Show help screen (short format).                                                                                 |
    | [top-syntax](#check_temperature_top-syntax)       | ${status}: ${list}                    | Top level syntax.                                                                                                |
    | [ok-syntax](#check_temperature_ok-syntax)         | %(status): All thermal zones seem ok. | ok syntax.                                                                                                       |
    | [empty-syntax](#check_temperature_empty-syntax)   |                                       | Empty syntax.                                                                                                    |
    | [detail-syntax](#check_temperature_detail-syntax) | ${name}: ${temperature} C             | Detail level syntax.                                                                                             |
    | [perf-syntax](#check_temperature_perf-syntax)     | ${name}                               | Performance alias syntax.                                                                                        |



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

    *Default Value:* `%(status): All thermal zones seem ok.`

    <h5 id="check_temperature_empty-syntax">empty-syntax:</h5>

    Empty syntax.
    DEPRECATED! This is the syntax for when nothing matches the filter.


    <h5 id="check_temperature_detail-syntax">detail-syntax:</h5>

    Detail level syntax.
    Used to format each resulting item in the message.
    %(list) will be replaced with all the items formated by this syntax string in the top-syntax.
    To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

    *Default Value:* `${name}: ${temperature} C`

    <h5 id="check_temperature_perf-syntax">perf-syntax:</h5>

    Performance alias syntax.
    This is the syntax for the base names of the performance data.

    *Default Value:* `${name}`

=== "Linux"

    <a id="check_temperature_warn"></a>
    <a id="check_temperature_crit"></a>
    <a id="check_temperature_debug"></a>
    <a id="check_temperature_show-all"></a>
    <a id="check_temperature_escape-html"></a>
    <a id="check_temperature_help"></a>
    <a id="check_temperature_help-pb"></a>
    <a id="check_temperature_show-default"></a>
    <a id="check_temperature_help-short"></a>

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

=== "Windows"

    | Option           | Description                        |
    |------------------|------------------------------------|
    | active           | True if the thermal zone is active |
    | name             | Thermal zone name                  |
    | temperature      | Temperature in degrees Celsius     |
    | throttle_reasons | Throttle reasons bitmask           |

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

=== "Linux"

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




<a id="check_uptime_options"></a>
#### Command-line Arguments

<a id="check_uptime_warn"></a>
<a id="check_uptime_crit"></a>
<a id="check_uptime_debug"></a>
<a id="check_uptime_show-all"></a>
<a id="check_uptime_escape-html"></a>
<a id="check_uptime_help"></a>
<a id="check_uptime_help-pb"></a>
<a id="check_uptime_show-default"></a>
<a id="check_uptime_help-short"></a>

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

| Path / Section                                                          | Description              |
|-------------------------------------------------------------------------|--------------------------|
| [/settings/default](#default-values)                                    | Default values           |
| [/settings/system/unix](#unix-system)                                   | Unix system              |
| [/settings/system/unix/real-time/cpu](#realtime-cpu-filters)            | Realtime cpu filters     |
| [/settings/system/unix/real-time/memory](#realtime-memory-filters)      | Realtime memory filters  |
| [/settings/system/unix/real-time/process](#realtime-process-filters)    | Realtime process filters |
| [/settings/system/windows](#windows-system)                             | Windows system           |
| [/settings/system/windows/counters](#pdh-counters)                      | PDH Counters             |
| [/settings/system/windows/real-time/checks](#legacy-generic-filters)    | Legacy generic filters   |
| [/settings/system/windows/real-time/cpu](#realtime-cpu-filters)         | Realtime cpu filters     |
| [/settings/system/windows/real-time/memory](#realtime-memory-filters)   | Realtime memory filters  |
| [/settings/system/windows/real-time/process](#realtime-process-filters) | Realtime process filters |


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

*Available on Linux only.*


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

*Available on Linux only.*


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

*Available on Linux only.*


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

*Available on Linux only.*


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






### Windows system <a id="/settings/system/windows"></a>

*Available on Windows only.*


Section for system checks and system settings

| Key                                           | Default Value | Description               |
|-----------------------------------------------|---------------|---------------------------|
| [default buffer length](#default-buffer-time) | 1h            | Default buffer time       |
| [disable](#disable-automatic-checks)          |               | Disable automatic checks  |
| [fetch core loads](#fetch-core-load)          | true          | Fetch core load           |
| [process history](#track-process-history)     | false         | Track process history     |
| [subsystem](#pdh-subsystem)                   | default       | PDH subsystem             |
| [timezone](#timezone)                         | local         | Timezone                  |
| [use pdh for cpu](#use-pdh-to-fetch-cpu-load) | false         | Use PDH to fetch CPU load |


```ini
# Section for system checks and system settings
[/settings/system/windows]
default buffer length=1h
fetch core loads=true
process history=false
subsystem=default
timezone=local
use pdh for cpu=false
```

#### Default buffer time <a id="/settings/system/windows/default buffer length"></a>

Used to define the default size of range buffer checks (ie. CPU).


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | default buffer length                                 |
| Default value: | `1h`                                                  |


**Sample:**

```
[/settings/system/windows]
# Default buffer time
default buffer length=1h
```

#### Disable automatic checks <a id="/settings/system/windows/disable"></a>

A comma separated list of checks to disable in the collector: battery,cpu,handles,network,temperature,cpu_frequency,os_updates,metrics,pdh. Please note disabling these will mean part of NSClient++ will no longer function as expected.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | disable                                               |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/system/windows]
# Disable automatic checks
disable=
```

#### Fetch core load <a id="/settings/system/windows/fetch core loads"></a>

Set to false to use a different API for fetching CPU load (will not provide core load, and will not show exact same values as task manager).


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | fetch core loads                                      |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | `true`                                                |


**Sample:**

```
[/settings/system/windows]
# Fetch core load
fetch core loads=true
```

#### Track process history <a id="/settings/system/windows/process history"></a>

Enable tracking of process history for use with check_process_history and check_process_history_new commands.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | process history                                       |
| Default value: | `false`                                               |


**Sample:**

```
[/settings/system/windows]
# Track process history
process history=false
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


**Sample:**

```
[/settings/system/windows]
# PDH subsystem
subsystem=default
```

#### Timezone <a id="/settings/system/windows/timezone"></a>

Timezone used to render dates such as boot time. Accepts 'local' (default), 'utc', or any POSIX TZ string parseable by Boost.Date_time (e.g. 'MST-07' or 'EST-05EDT,M3.2.0,M11.1.0').


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | timezone                                              |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | `local`                                               |


**Sample:**

```
[/settings/system/windows]
# Timezone
timezone=local
```

#### Use PDH to fetch CPU load <a id="/settings/system/windows/use pdh for cpu"></a>

When using PDH you might get better accuracy and hel alleviate invalid CPU values on multi core systems. The drawback is that PDH counters are sometimes missing and have invalid indexes so your milage may vary


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | use pdh for cpu                                       |
| Advanced:      | Yes (means it is not commonly used)                   |
| Default value: | `false`                                               |


**Sample:**

```
[/settings/system/windows]
# Use PDH to fetch CPU load
use pdh for cpu=false
```

### PDH Counters <a id="/settings/system/windows/counters"></a>

*Available on Windows only.*


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







### Legacy generic filters <a id="/settings/system/windows/real-time/checks"></a>

*Available on Windows only.*


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






### Realtime cpu filters <a id="/settings/system/windows/real-time/cpu"></a>

*Available on Windows only.*


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






### Realtime memory filters <a id="/settings/system/windows/real-time/memory"></a>

*Available on Windows only.*


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






### Realtime process filters <a id="/settings/system/windows/real-time/process"></a>

*Available on Windows only.*


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





