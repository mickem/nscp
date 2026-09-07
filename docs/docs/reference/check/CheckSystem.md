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

| Command                                                 | Description                                                                                                                                                                         |
|---------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [check_battery](#check_battery)                         | Check battery status including charge level, power source, and battery health.                                                                                                      |
| [check_cpu](#check_cpu)                                 | Check that the load of the CPU(s) are within bounds.                                                                                                                                |
| [check_cpu_frequency](#check_cpu_frequency)             | Check CPU clock frequency (current vs max) per processor.                                                                                                                           |
| [check_cpu_utilization](#check_cpu_utilization)         | Check CPU utilization broken down by user/system/iowait/steal/guest.                                                                                                                |
| [check_hardware](#check_hardware)                       | Check hardware inventory (vendor, model, serial, chassis type, memory modules) with pinned-expectation alerting: serial changed, DIMM dropped, laptop in a server fleet.            |
| [check_hostname](#check_hostname)                       | Check host identity: hostname, FQDN, DNS domain and domain-join state, with drift detection for the name mismatches that silently break auth and monitoring.                        |
| [check_installed_software](#check_installed_software)   | Check installed software from the registry Uninstall hives (64-bit, 32-bit and per-user views): inventory, unwanted/EOL software policy and recent-install detection.               |
| [check_kernel_memory](#check_kernel_memory)             | Check kernel memory-manager health: paged/nonpaged pool bytes, file-cache bytes and page-fault rates â€” the pool-exhaustion and hard-fault-storm signals free-RAM thresholds miss. |
| [check_kernel_stats](#check_kernel_stats)               | Check system-wide kernel activity: context-switch and system-call rates plus live process and thread counts.                                                                        |
| [check_load](#check_load)                               | Check the system load average (1/5/15 minutes), synthesised from the processor queue length plus busy cores.                                                                        |
| [check_memory](#check_memory)                           | Check free/used memory on the system.                                                                                                                                               |
| [check_network](#check_network)                         | Check network interface status.                                                                                                                                                     |
| [check_os_updates](#check_os_updates)                   | Check for available Windows updates via the Windows Update Agent (WUA) API.                                                                                                         |
| [check_os_version](#check_os_version)                   | Check the version of the underlying OS.                                                                                                                                             |
| [check_pagefile](#check_pagefile)                       | Check the size of the system pagefile(s).                                                                                                                                           |
| [check_patch_age](#check_patch_age)                     | Check installed-hotfix hygiene: how long since the newest hotfix was installed and whether specific required hotfixes are present.                                                  |
| [check_pdh](#check_pdh)                                 | Check the value of a performance (PDH) counter on the local or remote system.                                                                                                       |
| [check_pending_reboot](#check_pending_reboot)           | Check whether the system is waiting for a reboot, aggregating the servicing, Windows Update, file-rename, computer-rename and domain-join signals.                                  |
| [check_printjobs](#check_printjobs)                     | Check individual Windows print jobs: document, owner, size, pages, age and spooler status of every queued job.                                                                      |
| [check_printqueue](#check_printqueue)                   | Check Windows print queues: queue depth, oldest-job age, offline and error states plus the driver, port and sharing of each printer.                                                |
| [check_process](#check_process)                         | Check state/metrics of one or more of the processes running on the computer.                                                                                                        |
| [check_process_history](#check_process_history)         | Check the history of processes that have been running since NSClient++ started. Useful for verifying if certain applications have been executed.                                    |
| [check_process_history_new](#check_process_history_new) | Check for new processes that appeared within a specified time window. Useful for detecting unexpected or unauthorized applications.                                                 |
| [check_registry_key](#check_registry_key)               | Check existence, last-write time, and child counts of one or more Windows registry keys.                                                                                            |
| [check_registry_value](#check_registry_value)           | Check the type, content, and size of one or more Windows registry values.                                                                                                           |
| [check_service](#check_service)                         | Check the state of one or more of the computer services.                                                                                                                            |
| [check_swap_io](#check_swap_io)                         | Check system paging (swap) I/O rates: pages/bytes paged in and out per second.                                                                                                      |
| [check_temperature](#check_temperature)                 | Check ACPI thermal zone temperatures.                                                                                                                                               |
| [check_uptime](#check_uptime)                           | Check time since last server re-boot.                                                                                                                                               |
| [check_w32time](#check_w32time)                         | Check the Windows Time service: whether the machine is following a time source at all, which one, the computed clock offset and the configured peers.                               |

**List of command aliases:**

A list of all short hand aliases for queries (check commands)

| Command       | Description                   |
|---------------|-------------------------------|
| check_counter | Alias for: :query:`check_pdh` |

### check_battery

=== "Windows"

    Check battery status including charge level, power source, and battery health.

    #### About `check_battery`

    `check_battery` reports the state of the machine's batteries: charge level,
    which power source it is on, and battery health. One record is returned per
    battery.

    The default filter is `battery_present = 'true'`, and the thresholds are
    `charge < 20` (warning) and `charge < 10` (critical).

    On a machine with no battery — a desktop, a server, a VM — the filter matches
    nothing, and **the empty state is `warning`**, so the check reports
    `WARNING: No battery found`. That is almost never what you want on
    infrastructure: set `empty-state=ok` (or `empty-state=ignored`) on any host
    where a missing battery is normal, otherwise every server running this check
    sits permanently in WARNING.

    ##### The three questions it answers

    **"Is this laptop about to die?"** is the default, and rarely the interesting
    one on monitored infrastructure.

    **"Is this machine on mains power?"** is often the real check.
    `power_source` reads `ac`, `battery` or `unknown`, so a UPS-backed or
    laptop-as-server host can alert the moment it drops to battery, long before the
    charge level matters:

    ```
    check_battery "crit=power_source = 'battery'" "warn=none"
    ```

    **"Is the battery worn out?"** is what `health` is for — full charge capacity as
    a percentage of design capacity. A battery at 60% health still charges to "100%"
    and looks fine to a charge-level check while holding barely half its rated
    runtime:

    ```
    check_battery "warn=health < 70" "crit=health < 50"
    ```

    `health` is emitted as performance data, so the decline is visible as a trend
    long before it crosses a threshold.

    ##### Runtime and rates

    `time_remaining` is the estimated seconds left, and is **`-1` when unknown or on
    AC** — so guard any threshold on it with a `power_source = 'battery'` clause,
    otherwise `time_remaining < 600` fires on every mains-powered host.
    `charge_rate` and `discharge_rate` (mW) and the `design_capacity` /
    `full_capacity` / `remaining_capacity` triple (mWh) are available for the
    detailed view.

    ##### Data sources

    On Windows the data comes from the Windows power/battery APIs. On Linux it is
    read from `/sys/class/power_supply`, so batteries exposed by the ACPI or
    platform driver are visible; a battery behind a vendor-specific driver that does
    not populate sysfs will not be. `battery_status` carries the charging state
    (`status` is a deprecated alias — the name clashes with the generic status
    summary keyword and resolves to that in `top-syntax`).

=== "Linux"

    Check battery charge level, power source and health.

    #### About `check_battery`

    `check_battery` reports the state of the machine's batteries: charge level,
    which power source it is on, and battery health. One record is returned per
    battery.

    The default filter is `battery_present = 'true'`, and the thresholds are
    `charge < 20` (warning) and `charge < 10` (critical).

    On a machine with no battery — a desktop, a server, a VM — the filter matches
    nothing, and **the empty state is `warning`**, so the check reports
    `WARNING: No battery found`. That is almost never what you want on
    infrastructure: set `empty-state=ok` (or `empty-state=ignored`) on any host
    where a missing battery is normal, otherwise every server running this check
    sits permanently in WARNING.

    ##### The three questions it answers

    **"Is this laptop about to die?"** is the default, and rarely the interesting
    one on monitored infrastructure.

    **"Is this machine on mains power?"** is often the real check.
    `power_source` reads `ac`, `battery` or `unknown`, so a UPS-backed or
    laptop-as-server host can alert the moment it drops to battery, long before the
    charge level matters:

    ```
    check_battery "crit=power_source = 'battery'" "warn=none"
    ```

    **"Is the battery worn out?"** is what `health` is for — full charge capacity as
    a percentage of design capacity. A battery at 60% health still charges to "100%"
    and looks fine to a charge-level check while holding barely half its rated
    runtime:

    ```
    check_battery "warn=health < 70" "crit=health < 50"
    ```

    `health` is emitted as performance data, so the decline is visible as a trend
    long before it crosses a threshold.

    ##### Runtime and rates

    `time_remaining` is the estimated seconds left, and is **`-1` when unknown or on
    AC** — so guard any threshold on it with a `power_source = 'battery'` clause,
    otherwise `time_remaining < 600` fires on every mains-powered host.
    `charge_rate` and `discharge_rate` (mW) and the `design_capacity` /
    `full_capacity` / `remaining_capacity` triple (mWh) are available for the
    detailed view.

    ##### Data sources

    On Windows the data comes from the Windows power/battery APIs. On Linux it is
    read from `/sys/class/power_supply`, so batteries exposed by the ACPI or
    platform driver are visible; a battery behind a vendor-specific driver that does
    not populate sysfs will not be. `battery_status` carries the charging state
    (`status` is a deprecated alias — the name clashes with the generic status
    summary keyword and resolves to that in `top-syntax`).

**Jump to section:**

* [Sample Commands](#check_battery_samples)
* [Command-line Arguments](#check_battery_options)
* [Filter keywords](#check_battery_filter_keys)


<a id="check_battery_samples"></a>
#### Sample Commands

**A host with no battery:**

The default filter is `battery_present = 'true'`, and the empty state is
`warning` — so a desktop, server or VM reports WARNING out of the box.

```
check_battery
WARNING: No battery found
```

**Which is almost never what you want on infrastructure:**

```
check_battery "empty-state=ok"
OK: No battery found
```

Set `empty-state=ok` (or `ignored`) on every host where a missing battery is
normal, or the check sits permanently in WARNING.

**Default check on a laptop (`charge < 20` warns, `< 10` is critical):**

```
check_battery
OK: BAT0: 87% (ac, charging)|'BAT0_charge'=87%;20;10;0;100 'BAT0_health'=92%;0;0;0;100
```

**Alert the moment the host drops off mains power:**

Often the real check on a UPS-backed or laptop-as-server host — long before the
charge level matters.

```
check_battery "crit=power_source = 'battery'" "warn=none"
CRITICAL: BAT0: 87% (battery, discharging)
```

**Alert on a worn-out battery:**

`health` is full charge capacity as a percentage of design capacity. A battery
at 60% health still charges to "100%" and looks fine to a charge-level check.

```
check_battery "warn=health < 70" "crit=health < 50"
WARNING: BAT0: 100% (ac, full)|'BAT0_health'=64%;70;50;0;100
```

**Threshold on remaining runtime — guard it with a power-source clause:**

`time_remaining` is `-1` when unknown or on AC, so an unguarded threshold fires
on every mains-powered host.

```
check_battery "crit=power_source = 'battery' and time_remaining < 600" "detail-syntax=${name}: ${charge}% ${time_remaining}s left"
OK: BAT0: 87% -1s left
```

**Inspect the capacity figures:**

```
check_battery "detail-syntax=${name} rem=${remaining_capacity} full=${full_capacity} design=${design_capacity} rate=${discharge_rate}"
OK: BAT0 rem=48120 full=55300 design=60000 rate=0
```



<a id="check_battery_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                        | Default Value                                            |
|---------------------------------------------------------------------------------------------------------------|----------------------------------------------------------|
| <a id="check_battery_filter"></a>[filter](../common-options.md#filter)                                        | battery_present = 'true'                                 |
| <a id="check_battery_warning"></a>[warning](../common-options.md#warning)                                     | charge < 20                                              |
| <a id="check_battery_warn"></a>[warn](../common-options.md#warn)                                              |                                                          |
| <a id="check_battery_critical"></a>[critical](../common-options.md#critical)                                  | charge < 10                                              |
| <a id="check_battery_crit"></a>[crit](../common-options.md#crit)                                              |                                                          |
| <a id="check_battery_ok"></a>[ok](../common-options.md#ok)                                                    |                                                          |
| <a id="check_battery_debug"></a>[debug](../common-options.md#debug)                                           | false                                                    |
| <a id="check_battery_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                    |
| <a id="check_battery_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | warning                                                  |
| <a id="check_battery_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                          |
| <a id="check_battery_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                    |
| <a id="check_battery_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                        |
| <a id="check_battery_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                       |
| <a id="check_battery_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): No battery found or all batteries ok.         |
| <a id="check_battery_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No battery found                                         |
| <a id="check_battery_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${charge}% (${power_source}, ${battery_status}) |
| <a id="check_battery_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                  |
| <a id="check_battery_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                          |
| <a id="check_battery_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                          |
| <a id="check_battery_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                       |
| <a id="check_battery_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_battery_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option             | Description                                                                                      |
    |--------------------|--------------------------------------------------------------------------------------------------|
    | battery_present    | Whether a battery is present: 'true' or 'false'                                                  |
    | battery_status     | Battery status: 'charging', 'discharging', 'high', 'low', 'critical', 'no_battery', or 'unknown' |
    | charge             | Battery charge level in percent (0-100)                                                          |
    | charge_rate        | Current charge rate in mW (when charging)                                                        |
    | design_capacity    | Design capacity in mWh                                                                           |
    | discharge_rate     | Current discharge rate in mW (when discharging)                                                  |
    | full_capacity      | Current full charge capacity in mWh                                                              |
    | health             | Battery health in percent (full_capacity / design_capacity * 100)                                |
    | name               | Battery name/identifier                                                                          |
    | power_source       | Power source: 'ac', 'battery', or 'unknown'                                                      |
    | remaining_capacity | Current remaining capacity in mWh                                                                |
    | time_remaining     | Estimated time remaining in seconds (-1 if unknown or on AC)                                     |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option             | Description                                                  |
    |--------------------|--------------------------------------------------------------|
    | battery_present    | Whether a battery is present: 'true' or 'false'              |
    | battery_status     | Charge status                                                |
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

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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

    **Interaction with the `disable` setting (Windows only)**

    On Windows the collector that feeds this buffer can be turned off with `disable = cpu` in
    `[/settings/system/windows]`. In that case `check_cpu` returns UNKNOWN with an explanatory message
    rather than reporting values from a buffer that is no longer updated. The entries in `disable` are
    matched as whole tokens, so `disable = cpu_frequency` only disables the CPU frequency collector and
    leaves `check_cpu` unaffected.

    The Linux module has no `disable` setting; its collector is always running.

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

    **Interaction with the `disable` setting (Windows only)**

    On Windows the collector that feeds this buffer can be turned off with `disable = cpu` in
    `[/settings/system/windows]`. In that case `check_cpu` returns UNKNOWN with an explanatory message
    rather than reporting values from a buffer that is no longer updated. The entries in `disable` are
    matched as whole tokens, so `disable = cpu_frequency` only disables the CPU frequency collector and
    leaves `check_cpu` unaffected.

    The Linux module has no `disable` setting; its collector is always running.

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

`idle` and `system` now emit perfdata (previously only `usage`/`user` did), so the
full breakdown graphs without a custom `top-syntax`. `kernel` is a deprecated alias
of `system` and intentionally emits no separate perf column; `total` is a deprecated
alias of `usage` (the name clashes with the generic `total` summary keyword).

```
check_cpu "warn=idle < 5"
CPU Load ok
'total 5m load'=7%;80;90 'total 5m user'=4%;;; 'total 5m system'=3%;;; 'total 5m idle'=93%;;; ...
```



<a id="check_cpu_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_cpu_time"></a>
    <a id="check_cpu_cores"></a>

    | Option | Default Value | Description                                                                                 |
    |--------|---------------|---------------------------------------------------------------------------------------------|
    | time   |               | The time to check                                                                           |
    | cores  | N/A           | This will remove the filter to  include the cores, if you use filter dont use this as well. |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                    | Default Value              |
    |-----------------------------------------------------------------------------------------------------------|----------------------------|
    | <a id="check_cpu_filter"></a>[filter](../common-options.md#filter)                                        | core = 'total'             |
    | <a id="check_cpu_warning"></a>[warning](../common-options.md#warning)                                     | load > 80                  |
    | <a id="check_cpu_warn"></a>[warn](../common-options.md#warn)                                              |                            |
    | <a id="check_cpu_critical"></a>[critical](../common-options.md#critical)                                  | load > 90                  |
    | <a id="check_cpu_crit"></a>[crit](../common-options.md#crit)                                              |                            |
    | <a id="check_cpu_ok"></a>[ok](../common-options.md#ok)                                                    |                            |
    | <a id="check_cpu_debug"></a>[debug](../common-options.md#debug)                                           | false                      |
    | <a id="check_cpu_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                      |
    | <a id="check_cpu_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                    |
    | <a id="check_cpu_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                            |
    | <a id="check_cpu_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                      |
    | <a id="check_cpu_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                          |
    | <a id="check_cpu_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list} |
    | <a id="check_cpu_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): CPU load is ok. |
    | <a id="check_cpu_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                            |
    | <a id="check_cpu_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${time}: ${load}%          |
    | <a id="check_cpu_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${core} ${time}            |
    | <a id="check_cpu_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                            |
    | <a id="check_cpu_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                            |
    | <a id="check_cpu_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                         |
    | <a id="check_cpu_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                            |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_cpu_time"></a>
    <a id="check_cpu_cores"></a>

    | Option | Default Value | Description                                                                                 |
    |--------|---------------|---------------------------------------------------------------------------------------------|
    | time   |               | The time to check                                                                           |
    | cores  | N/A           | This will remove the filter to include the cores, if you use filter don't use this as well. |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                    | Default Value              |
    |-----------------------------------------------------------------------------------------------------------|----------------------------|
    | <a id="check_cpu_filter"></a>[filter](../common-options.md#filter)                                        | core = 'total'             |
    | <a id="check_cpu_warning"></a>[warning](../common-options.md#warning)                                     | load > 80                  |
    | <a id="check_cpu_warn"></a>[warn](../common-options.md#warn)                                              |                            |
    | <a id="check_cpu_critical"></a>[critical](../common-options.md#critical)                                  | load > 90                  |
    | <a id="check_cpu_crit"></a>[crit](../common-options.md#crit)                                              |                            |
    | <a id="check_cpu_ok"></a>[ok](../common-options.md#ok)                                                    |                            |
    | <a id="check_cpu_debug"></a>[debug](../common-options.md#debug)                                           | false                      |
    | <a id="check_cpu_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                      |
    | <a id="check_cpu_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                    |
    | <a id="check_cpu_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                            |
    | <a id="check_cpu_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                      |
    | <a id="check_cpu_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                          |
    | <a id="check_cpu_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list} |
    | <a id="check_cpu_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): CPU load is ok. |
    | <a id="check_cpu_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                            |
    | <a id="check_cpu_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${time}: ${load}%          |
    | <a id="check_cpu_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${core} ${time}            |
    | <a id="check_cpu_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                            |
    | <a id="check_cpu_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                            |
    | <a id="check_cpu_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                         |
    | <a id="check_cpu_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                            |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cpu_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option  | Description                                  |
    |---------|----------------------------------------------|
    | core    | The core to check (total or core ##)         |
    | core_id | The core to check (total or core_##)         |
    | idle    | The current idle load for a given core       |
    | kernel  | deprecated (use system instead)              |
    | load    | deprecated (use usage instead)               |
    | system  | The current load used by the system (kernel) |
    | time    | The time frame to check                      |
    | usage   | The current load used by user and system     |
    | user    | The current load used by user applications   |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option  | Description                                               |
    |---------|-----------------------------------------------------------|
    | core    | The core to check (total or core ##)                      |
    | core_id | The core to check (total or core_##)                      |
    | idle    | The current idle load for a given core                    |
    | kernel  | deprecated (use system instead)                           |
    | load    | The current load for a given core (deprecated, use usage) |
    | system  | The current load used by the system (kernel)              |
    | time    | The time frame to check                                   |
    | usage   | The current load used by user and system                  |
    | user    | The current load used by user applications                |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_cpu_frequency

=== "Windows"

    Check CPU clock frequency (current vs max) per processor.

    Reports per-CPU-socket frequency, load and hardware inventory, sourced from the
    `Win32_Processor` WMI class (one instance per physical socket).

    There are no default warning/critical thresholds: modern CPUs legitimately clock
    far below their maximum at idle, so a `frequency_pct` default would warn on every
    idle machine. Use `load_pct` for a per-socket utilisation alert. WMI does not
    always have a load sample ready: on such a cycle `load_pct` renders as
    `no load sample`, compares false against every numeric threshold and emits no
    perfdata (it is never a fabricated `0`, which is a valid idle reading);
    `load_pct = 'no load sample'` is the presence test. The inventory
    columns (`architecture`, `l2_cache`, `l3_cache`) make the check double as the
    per-socket CPU hardware inventory; pin them to detect a re-imaged or migrated
    box (`crit=architecture != 'x64'`).

=== "Linux"

    Check the CPU clock frequency (current vs max) per core.

    Reports per-CPU-socket frequency, load and hardware inventory, sourced from the
    `Win32_Processor` WMI class (one instance per physical socket).

    There are no default warning/critical thresholds: modern CPUs legitimately clock
    far below their maximum at idle, so a `frequency_pct` default would warn on every
    idle machine. Use `load_pct` for a per-socket utilisation alert. WMI does not
    always have a load sample ready: on such a cycle `load_pct` renders as
    `no load sample`, compares false against every numeric threshold and emits no
    perfdata (it is never a fabricated `0`, which is a valid idle reading);
    `load_pct = 'no load sample'` is the presence test. The inventory
    columns (`architecture`, `l2_cache`, `l3_cache`) make the check double as the
    per-socket CPU hardware inventory; pin them to detect a re-imaged or migrated
    box (`crit=architecture != 'x64'`).

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
'Intel...'=2900MHz;;; 'Intel..._max_mhz'=4800MHz;;; 'Intel..._frequency_pct'=60%;;; 'Intel..._load_pct'=12%;;;
```

**Per-socket filtering and load:**

`Win32_Processor` returns one row per physical CPU socket, exposed via `socket_id`
(DeviceID, e.g. `CPU0`) and `socket` (SocketDesignation, e.g. `CPU 1`). The
`load_pct` keyword reports `Win32_Processor.LoadPercentage` per socket.

```
check_cpu_frequency "filter=socket_id = 'CPU0'" "warn=load_pct > 90" "detail-syntax=${socket}: ${load_pct}% @ ${current_mhz}MHz"
OK: CPU 1: 12% @ 2900MHz
'Intel..._load_pct'=12%;90;;
```

**CPU hardware inventory (model, architecture, cores/threads, cache):**

```
check_cpu_frequency "detail-syntax=${name}: ${architecture}, ${cores}c/${logical_processors}t, L2 ${l2_cache}, L3 ${l3_cache}"
OK: Intel(R) Core(TM) Ultra 7 265H: x64, 16c/16t, L2 28MB, L3 24MB
```

**Pin expected hardware (re-imaged / migrated box detection):**

```
check_cpu_frequency "warn=l3_cache < 1M" "crit=architecture != 'x64'"
OK: Intel(R) Core(TM) Ultra 7 265H: 2200/2200 MHz (100%)
```



<a id="check_cpu_frequency_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                                              |
|---------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------|
| <a id="check_cpu_frequency_filter"></a>[filter](../common-options.md#filter)                                        |                                                            |
| <a id="check_cpu_frequency_warning"></a>[warning](../common-options.md#warning)                                     |                                                            |
| <a id="check_cpu_frequency_warn"></a>[warn](../common-options.md#warn)                                              |                                                            |
| <a id="check_cpu_frequency_critical"></a>[critical](../common-options.md#critical)                                  |                                                            |
| <a id="check_cpu_frequency_crit"></a>[crit](../common-options.md#crit)                                              |                                                            |
| <a id="check_cpu_frequency_ok"></a>[ok](../common-options.md#ok)                                                    |                                                            |
| <a id="check_cpu_frequency_debug"></a>[debug](../common-options.md#debug)                                           | false                                                      |
| <a id="check_cpu_frequency_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                      |
| <a id="check_cpu_frequency_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                    |
| <a id="check_cpu_frequency_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                            |
| <a id="check_cpu_frequency_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                      |
| <a id="check_cpu_frequency_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                          |
| <a id="check_cpu_frequency_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                         |
| <a id="check_cpu_frequency_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All CPU frequencies seem ok.                    |
| <a id="check_cpu_frequency_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                            |
| <a id="check_cpu_frequency_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${current_mhz}/${max_mhz} MHz (${frequency_pct}%) |
| <a id="check_cpu_frequency_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                    |
| <a id="check_cpu_frequency_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                            |
| <a id="check_cpu_frequency_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                            |
| <a id="check_cpu_frequency_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                         |
| <a id="check_cpu_frequency_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cpu_frequency_filter_keys"></a>
#### Filter keywords

| Option             | Description                                                                                                                       |
|--------------------|-----------------------------------------------------------------------------------------------------------------------------------|
| architecture       | Processor architecture (x86, x64, ARM64, ...)                                                                                     |
| cores              | Number of physical cores                                                                                                          |
| current_mhz        | Current clock speed in MHz (perfdata)                                                                                             |
| frequency_pct      | Current frequency as percentage of maximum (perfdata)                                                                             |
| l2_cache           | L2 cache size (size units work, e.g. 'l2_cache < 1M'); renders human-readable; 0 when not reported                                |
| l3_cache           | L3 cache size; 0 when not reported (common on VMs)                                                                                |
| load_pct           | Per-socket CPU load as reported by Win32_Processor.LoadPercentage (perfdata); 'no load sample' when WMI has no reading this cycle |
| logical_processors | Number of logical processors (threads)                                                                                            |
| max_mhz            | Maximum clock speed in MHz (perfdata)                                                                                             |
| name               | CPU name / model string                                                                                                           |
| socket             | Socket designation (e.g. "CPU 1"), for per-socket filtering                                                                       |
| socket_id          | Socket device id (e.g. CPU0), for per-socket filtering                                                                            |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_cpu_utilization

*Available on Linux only.*

Check CPU utilization broken down by user/system/iowait/steal/guest.

#### About `check_cpu_utilization`

`check_cpu_utilization` reports how the CPU's time is being spent, broken down
by mode. It reads the aggregate `cpu` line from `/proc/stat`, waits ~1 second,
reads it again, and reports the delta as percentages — so it measures live
utilization over that sampling window rather than since boot.

All numeric keywords are percentages (0–100).

Default thresholds: **warning** `usage > 90`, **critical** `usage > 95`
(`total` still works as a deprecated alias for `usage`; it was renamed to avoid
clashing with the generic `total` summary keyword). This
differs from [`check_cpu`](#check_cpu), which averages utilization over rolling
time windows (`1m`/`5m`/`15m`) from the background collector; `check_cpu_utilization`
takes a single fresh 1-second sample and exposes the per-mode breakdown, which
is what you want to distinguish user vs. `iowait` vs. `steal` pressure.

**Jump to section:**

* [Sample Commands](#check_cpu_utilization_samples)
* [Command-line Arguments](#check_cpu_utilization_options)
* [Filter keywords](#check_cpu_utilization_filter_keys)


<a id="check_cpu_utilization_samples"></a>
#### Sample Commands

**CPU utilization broken down by mode:**

```
check_cpu_utilization
OK: user: 1.39% system: 1.54% iowait: 0% steal: 0% idle: 95.73%
L        cli  Performance data: 'cpu_usage'=4.27;90;95 'cpu_user'=1.39;0;0 'cpu_system'=1.54;0;0 'cpu_iowait'=0;0;0 'cpu_steal'=0;0;0 'cpu_idle'=95.73;0;0 ...
```

**Custom thresholds on total busy percentage (the default is `usage > 90` / `> 95`):**

```
check_cpu_utilization "warn=usage > 80" "crit=usage > 95"
OK: user: 2.1% system: 1.8% iowait: 0.2% steal: 0% idle: 95.9%
```

**Alert specifically on I/O wait (storage saturation):**

```
check_cpu_utilization "warn=iowait > 20" "crit=iowait > 50"
OK: user: 1.4% system: 1.5% iowait: 0% steal: 0% idle: 95.7%
```

**Alert on steal time (noisy-neighbour on a VM):**

```
check_cpu_utilization "warn=steal > 5" "crit=steal > 15"
OK: user: 1.4% system: 1.5% iowait: 0% steal: 0% idle: 95.7%
```



<a id="check_cpu_utilization_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                | Default Value                                                                        |
|-----------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------|
| <a id="check_cpu_utilization_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                      |
| <a id="check_cpu_utilization_warning"></a>[warning](../common-options.md#warning)                                     | usage > 90                                                                           |
| <a id="check_cpu_utilization_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                      |
| <a id="check_cpu_utilization_critical"></a>[critical](../common-options.md#critical)                                  | usage > 95                                                                           |
| <a id="check_cpu_utilization_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                      |
| <a id="check_cpu_utilization_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                      |
| <a id="check_cpu_utilization_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                |
| <a id="check_cpu_utilization_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                |
| <a id="check_cpu_utilization_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                                              |
| <a id="check_cpu_utilization_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                      |
| <a id="check_cpu_utilization_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                |
| <a id="check_cpu_utilization_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                    |
| <a id="check_cpu_utilization_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                   |
| <a id="check_cpu_utilization_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                      |
| <a id="check_cpu_utilization_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                                      |
| <a id="check_cpu_utilization_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | user: ${user}% system: ${system}% iowait: ${iowait}% steal: ${steal}% idle: ${idle}% |
| <a id="check_cpu_utilization_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | cpu                                                                                  |
| <a id="check_cpu_utilization_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                      |
| <a id="check_cpu_utilization_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                      |
| <a id="check_cpu_utilization_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                   |
| <a id="check_cpu_utilization_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cpu_utilization_filter_keys"></a>
#### Filter keywords

| Option  | Description                                                                     |
|---------|---------------------------------------------------------------------------------|
| guest   | CPU time spent running a guest under this kernel, in percent (incl. guest_nice) |
| idle    | Idle CPU in percent                                                             |
| iowait  | I/O-wait CPU utilization in percent                                             |
| irq     | Hardware-interrupt CPU utilization in percent                                   |
| name    | Always 'total' (single aggregate row)                                           |
| softirq | Soft-interrupt CPU utilization in percent                                       |
| steal   | CPU time stolen by the hypervisor in percent (VM guests)                        |
| system  | System/kernel CPU utilization in percent                                        |
| usage   | Non-idle CPU utilization in percent (100 - idle - iowait)                       |
| user    | User (incl. nice) CPU utilization in percent                                    |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_hardware

*Available on Windows only.*

Check hardware inventory (vendor, model, serial, chassis type, memory modules) with pinned-expectation alerting: serial changed, DIMM dropped, laptop in a server fleet.

#### About `check_hardware`

`check_hardware` reports BIOS/chassis/memory hardware inventory from WMI
(`root\CIMV2`): `Win32_ComputerSystemProduct` (vendor, model, UUID, serial),
`Win32_SystemEnclosure` (chassis type, enclosure serial, asset tag),
`Win32_PhysicalMemory` (per-DIMM inventory) and `Win32_PhysicalMemoryArray`
(total sockets). It complements `check_os_version` — that check answers "what
OS am I running" (and carries the BIOS serial/version for back-compat), this
one answers "what box am I".

The useful alerts are **pinned expectations and changes**, not thresholds on a
moving value:

- **"Did the hardware change?"** — `crit=serial != 'ABC1234'` catches a
  re-imaged, replaced or cloned box.
- **"Did a DIMM drop?"** — `warn=modules < 8` / `crit=memory < 64G` catch a
  failed module long before the OS-level memory checks look abnormal.
- **"Is this the right kind of machine?"** — `crit=chassis like 'Laptop'` in a
  server fleet, or `warn=modules < slots`-style capacity planning via the
  `slots` count.

There are no default thresholds (a bare call is an inventory line); `memory`
and `modules` are always emitted as perf data (`hardware_memory`,
`hardware_modules`). Per-item keywords such as `module_list` belong in
`detail-syntax` (the default `top-syntax` embeds them via `${list}`).

**Caveats:** VMs and OEM boards frequently report blank or placeholder values —
serials like `To be filled by O.E.M.`, chassis `Other`, `slots` 0 — so baseline
a host before pinning expectations on it. Each WMI class is read best-effort: a
class that is missing (stripped-down VMs) leaves its fields empty rather than
failing the check, and the check only errors when *no* class answers (WMI
down). `ConfiguredClockSpeed` falls back to the raw `Speed` on older Windows.

**Jump to section:**

* [Sample Commands](#check_hardware_samples)
* [Command-line Arguments](#check_hardware_options)
* [Filter keywords](#check_hardware_filter_keys)


<a id="check_hardware_samples"></a>
#### Sample Commands

**Default check (inventory line with memory/module perf):**

```
check_hardware
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB|'hardware_memory'=34359738368;0;0 'hardware_modules'=2;0;0
```

**Pin the expected serial (CRITICAL when the box was replaced or re-imaged):**

```
check_hardware "crit=serial != 'ABC1234'"
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB

check_hardware "crit=serial != 'XYZ0000'"
CRITICAL: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB
```

**Detect a dropped DIMM (module count or total capacity shrank):**

```
check_hardware "warn=modules < 2" "crit=memory < 16G"
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB|'hardware_memory'=34359738368;0;17179869184 'hardware_modules'=2;2;0
```

**Enforce machine class (no laptops in the server fleet):**

```
check_hardware "warn=chassis like 'Laptop' or chassis like 'Notebook'"
WARNING: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB
```

**Per-DIMM inventory and socket usage:**

```
check_hardware "detail-syntax=${module_list} (slots=${slots}, speed=${memory_speed}MHz, chassis=${chassis})"
OK: DIMM A: 16GB@5600MHz; DIMM B: 16GB@5600MHz (slots=2, speed=5600MHz, chassis=Notebook)
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_hardware --argument "crit=serial != 'ABC1234'"
OK: Dell Inc. Dell Pro Max 16 MC16250 (Notebook), serial=ABC1234, 2 memory module(s), 32GB
```



<a id="check_hardware_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                         | Default Value                                                                             |
|----------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------|
| <a id="check_hardware_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                           |
| <a id="check_hardware_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                           |
| <a id="check_hardware_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                           |
| <a id="check_hardware_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                           |
| <a id="check_hardware_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                           |
| <a id="check_hardware_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                           |
| <a id="check_hardware_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                     |
| <a id="check_hardware_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                     |
| <a id="check_hardware_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                                                   |
| <a id="check_hardware_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                           |
| <a id="check_hardware_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                     |
| <a id="check_hardware_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                         |
| <a id="check_hardware_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                        |
| <a id="check_hardware_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                           |
| <a id="check_hardware_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                                           |
| <a id="check_hardware_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${vendor} ${model} (${chassis}), serial=${serial}, ${modules} memory module(s), ${memory} |
| <a id="check_hardware_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | hardware                                                                                  |
| <a id="check_hardware_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                           |
| <a id="check_hardware_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                           |
| <a id="check_hardware_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                        |
| <a id="check_hardware_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                           |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_hardware_filter_keys"></a>
#### Filter keywords

| Option         | Description                                                                               |
|----------------|-------------------------------------------------------------------------------------------|
| asset_tag      | SMBIOS asset tag                                                                          |
| chassis        | Chassis type name (Desktop, Laptop, Rack Mount Chassis, ...)                              |
| chassis_serial | Enclosure serial number                                                                   |
| chassis_type   | Raw SMBIOS chassis type number (0 when unknown)                                           |
| memory         | Total installed memory (supports size units, e.g. 'memory < 64G'); renders human-readable |
| memory_speed   | Slowest populated module's configured clock in MHz (0 when unknown)                       |
| model          | System model / product name                                                               |
| module_list    | Semicolon-separated per-DIMM inventory (slot: size@speed, e.g. 'DIMM_A1: 32GB@4800MHz')   |
| modules        | Number of populated memory modules                                                        |
| serial         | System serial number (often blank or placeholder on VMs and OEM boards)                   |
| slots          | Total memory sockets on the board (0 when not reported)                                   |
| uuid           | SMBIOS system UUID                                                                        |
| vendor         | System vendor/manufacturer                                                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_hostname

=== "Windows"

    Check host identity: hostname, FQDN, DNS domain and domain-join state, with drift detection for the name mismatches that silently break auth and monitoring.

    #### About `check_hostname`

    `check_hostname` reports host identity — hostname, FQDN and DNS domain (plus
    domain-join state on Windows) — and detects the name drift that silently breaks
    Kerberos auth, certificate validation and monitoring host-matching.

    The shared keywords (`hostname`, `fqdn`, `domain`, `fqdn_consistent`) carry the
    same meaning on both platforms. The check returns a single aggregate row, has no
    default thresholds — whether `workgroup` is wrong is site policy — and emits no
    performance data (there is no meaningful number here). Comparisons are
    case-insensitive, since DNS is case-insensitive and case differences are not
    drift.

    ##### Windows

    Reads `GetComputerNameEx` (NetBIOS name, DNS hostname, DNS suffix, FQDN) and
    `NetGetJoinInformation` (joined domain or workgroup); no WMI involved. Two extra
    keywords are available here: `join` / `join_name` (Active Directory membership)
    and `netbios_matches_dns`.

    The useful alerts are **pinned expectations**:

    - **"Is this box still on the domain?"** — `crit=join != 'domain'` or
      `crit=domain != 'corp.example.com'` catches domain-join / workgroup drift.
    - **"Is the name coherent?"** — `warn=fqdn_consistent = 0` (the FQDN no longer
      equals `dns_hostname.domain`: DNS-suffix drift) and
      `warn=netbios_matches_dns = 0` (NetBIOS name diverged from the DNS hostname
      after a rename or re-image).
    - **"Is this the host I think it is?"** — `crit=hostname != 'WEB01'` on
      cloned/re-imaged machines.

    A host with no DNS suffix reports `fqdn == hostname` as consistent, not as
    drift, and the NetBIOS comparison tolerates the 15-character truncation of
    longer DNS names.

    ##### Linux

    Reads `gethostname()` and canonicalises it with `getaddrinfo(AI_CANONNAME)`;
    when the host cannot be resolved (containers, hosts without DNS) the FQDN falls
    back to the bare hostname, which is treated as consistent rather than as drift.
    `join` / `join_name` / `netbios_matches_dns` have no clean Linux equivalent and
    are absent here.

    The useful alerts are **pinned expectations**:

    - **"Is this the host I think it is?"** — `crit=hostname != 'web01'`,
      `crit=domain != 'corp.example.com'`.
    - **"Is the name coherent?"** — `warn=fqdn_consistent = 0` flags the resolver
      canonicalising this host under a *different* name (stale `/etc/hosts`
      entries, CNAME chains, re-imaged boxes keeping an old DNS record).

    ##### See also

    CheckSecurity's `check_nla` covers the runtime side of the same question — which
    network profile (domain/private/public) the host is currently on.

=== "Linux"

    Check host identity: hostname, canonical FQDN and DNS domain, with drift detection for the name mismatches that silently break auth and monitoring.

    #### About `check_hostname`

    `check_hostname` reports host identity — hostname, FQDN and DNS domain (plus
    domain-join state on Windows) — and detects the name drift that silently breaks
    Kerberos auth, certificate validation and monitoring host-matching.

    The shared keywords (`hostname`, `fqdn`, `domain`, `fqdn_consistent`) carry the
    same meaning on both platforms. The check returns a single aggregate row, has no
    default thresholds — whether `workgroup` is wrong is site policy — and emits no
    performance data (there is no meaningful number here). Comparisons are
    case-insensitive, since DNS is case-insensitive and case differences are not
    drift.

    ##### Windows

    Reads `GetComputerNameEx` (NetBIOS name, DNS hostname, DNS suffix, FQDN) and
    `NetGetJoinInformation` (joined domain or workgroup); no WMI involved. Two extra
    keywords are available here: `join` / `join_name` (Active Directory membership)
    and `netbios_matches_dns`.

    The useful alerts are **pinned expectations**:

    - **"Is this box still on the domain?"** — `crit=join != 'domain'` or
      `crit=domain != 'corp.example.com'` catches domain-join / workgroup drift.
    - **"Is the name coherent?"** — `warn=fqdn_consistent = 0` (the FQDN no longer
      equals `dns_hostname.domain`: DNS-suffix drift) and
      `warn=netbios_matches_dns = 0` (NetBIOS name diverged from the DNS hostname
      after a rename or re-image).
    - **"Is this the host I think it is?"** — `crit=hostname != 'WEB01'` on
      cloned/re-imaged machines.

    A host with no DNS suffix reports `fqdn == hostname` as consistent, not as
    drift, and the NetBIOS comparison tolerates the 15-character truncation of
    longer DNS names.

    ##### Linux

    Reads `gethostname()` and canonicalises it with `getaddrinfo(AI_CANONNAME)`;
    when the host cannot be resolved (containers, hosts without DNS) the FQDN falls
    back to the bare hostname, which is treated as consistent rather than as drift.
    `join` / `join_name` / `netbios_matches_dns` have no clean Linux equivalent and
    are absent here.

    The useful alerts are **pinned expectations**:

    - **"Is this the host I think it is?"** — `crit=hostname != 'web01'`,
      `crit=domain != 'corp.example.com'`.
    - **"Is the name coherent?"** — `warn=fqdn_consistent = 0` flags the resolver
      canonicalising this host under a *different* name (stale `/etc/hosts`
      entries, CNAME chains, re-imaged boxes keeping an old DNS record).

    ##### See also

    CheckSecurity's `check_nla` covers the runtime side of the same question — which
    network profile (domain/private/public) the host is currently on.

**Jump to section:**

* [Sample Commands](#check_hostname_samples)
* [Command-line Arguments](#check_hostname_options)
* [Filter keywords](#check_hostname_filter_keys)


<a id="check_hostname_samples"></a>
#### Sample Commands

**Default check (identity line):**

```
check_hostname
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

On a Windows workgroup machine:

```
check_hostname
OK: MYPC (MyPC), workgroup=WORKGROUP
```

On a Linux host without DNS (e.g. a container), the FQDN falls back to the hostname:

```
check_hostname
OK: container123 (container123), domain=
```

**Pin the expected identity (cloned or re-imaged box detection):**

```
check_hostname "crit=hostname != 'web01' or domain != 'corp.example.com'"
OK: web01 (web01.corp.example.com), domain=corp.example.com
```

**Detect name drift (the resolver canonicalises this host under another name):**

```
check_hostname "warn=fqdn_consistent = 0"
WARNING: web01 (web01-old.corp.example.com), domain=corp.example.com
```

**Require domain membership — Windows only (CRITICAL on domain-join / workgroup drift):**

```
check_hostname "crit=join != 'domain'"
CRITICAL: MYPC (MyPC), workgroup=WORKGROUP

check_hostname "crit=join != 'domain' or domain != 'corp.example.com'"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Detect NetBIOS drift — Windows only:**

```
check_hostname "warn=fqdn_consistent = 0 or netbios_matches_dns = 0"
OK: WEB01 (web01.corp.example.com), domain=corp.example.com
```

**Inspect all identity fields:**

```
check_hostname "detail-syntax=h=${hostname} f=${fqdn} d=${domain} ok=${fqdn_consistent}"
OK: h=web01 f=web01.corp.example.com d=corp.example.com ok=1
```

On Windows the NetBIOS and DNS names are separate fields:

```
check_hostname "detail-syntax=nb=${hostname} dns=${dns_hostname} dom=${domain} fq=${fqdn} ok=${fqdn_consistent}/${netbios_matches_dns}"
OK: nb=WEB01 dns=web01 dom=corp.example.com fq=web01.corp.example.com ok=1/1
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_hostname --arguments "crit=domain != 'corp.example.com'"
OK: web01 (web01.corp.example.com), domain=corp.example.com
```



<a id="check_hostname_options"></a>
#### Command-line Arguments

=== "Windows"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                         | Default Value                               |
    |----------------------------------------------------------------------------------------------------------------|---------------------------------------------|
    | <a id="check_hostname_filter"></a>[filter](../common-options.md#filter)                                        |                                             |
    | <a id="check_hostname_warning"></a>[warning](../common-options.md#warning)                                     |                                             |
    | <a id="check_hostname_warn"></a>[warn](../common-options.md#warn)                                              |                                             |
    | <a id="check_hostname_critical"></a>[critical](../common-options.md#critical)                                  |                                             |
    | <a id="check_hostname_crit"></a>[crit](../common-options.md#crit)                                              |                                             |
    | <a id="check_hostname_ok"></a>[ok](../common-options.md#ok)                                                    |                                             |
    | <a id="check_hostname_debug"></a>[debug](../common-options.md#debug)                                           | false                                       |
    | <a id="check_hostname_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                       |
    | <a id="check_hostname_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                     |
    | <a id="check_hostname_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                             |
    | <a id="check_hostname_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                       |
    | <a id="check_hostname_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                           |
    | <a id="check_hostname_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                          |
    | <a id="check_hostname_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                             |
    | <a id="check_hostname_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                             |
    | <a id="check_hostname_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${hostname} (${fqdn}), ${join}=${join_name} |
    | <a id="check_hostname_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | hostname                                    |
    | <a id="check_hostname_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                             |
    | <a id="check_hostname_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                             |
    | <a id="check_hostname_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                          |
    | <a id="check_hostname_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                             |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                         | Default Value                           |
    |----------------------------------------------------------------------------------------------------------------|-----------------------------------------|
    | <a id="check_hostname_filter"></a>[filter](../common-options.md#filter)                                        |                                         |
    | <a id="check_hostname_warning"></a>[warning](../common-options.md#warning)                                     |                                         |
    | <a id="check_hostname_warn"></a>[warn](../common-options.md#warn)                                              |                                         |
    | <a id="check_hostname_critical"></a>[critical](../common-options.md#critical)                                  |                                         |
    | <a id="check_hostname_crit"></a>[crit](../common-options.md#crit)                                              |                                         |
    | <a id="check_hostname_ok"></a>[ok](../common-options.md#ok)                                                    |                                         |
    | <a id="check_hostname_debug"></a>[debug](../common-options.md#debug)                                           | false                                   |
    | <a id="check_hostname_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                   |
    | <a id="check_hostname_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                 |
    | <a id="check_hostname_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                         |
    | <a id="check_hostname_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                   |
    | <a id="check_hostname_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                       |
    | <a id="check_hostname_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                      |
    | <a id="check_hostname_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                         |
    | <a id="check_hostname_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                         |
    | <a id="check_hostname_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${hostname} (${fqdn}), domain=${domain} |
    | <a id="check_hostname_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | hostname                                |
    | <a id="check_hostname_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                         |
    | <a id="check_hostname_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                         |
    | <a id="check_hostname_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                      |
    | <a id="check_hostname_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                         |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_hostname_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option              | Description                                                                                                      |
    |---------------------|------------------------------------------------------------------------------------------------------------------|
    | dns_hostname        | DNS hostname (the local label)                                                                                   |
    | domain              | Primary DNS suffix (empty when none is configured)                                                               |
    | fqdn                | Fully qualified DNS name                                                                                         |
    | fqdn_consistent     | True when fqdn == dns_hostname[.domain] (case-insensitive); false flags DNS-suffix drift                         |
    | hostname            | NetBIOS computer name (max 15 characters)                                                                        |
    | join                | Join state: domain, workgroup, standalone or unknown                                                             |
    | join_name           | The joined domain or workgroup name                                                                              |
    | netbios_matches_dns | True when the NetBIOS name matches the first 15 characters of the DNS hostname; false flags rename/imaging drift |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option          | Description                                                                                                                                             |
    |-----------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
    | domain          | DNS domain (the FQDN with the first label removed; empty when none)                                                                                     |
    | fqdn            | Canonical fully qualified name from the resolver (hostname when unresolvable)                                                                           |
    | fqdn_consistent | True when the FQDN equals, or starts with, the configured hostname; false flags DNS drift (the resolver canonicalises this host under a different name) |
    | hostname        | Configured hostname (gethostname)                                                                                                                       |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_installed_software

=== "Windows"

    Check installed software from the registry Uninstall hives (64-bit, 32-bit and per-user views): inventory, unwanted/EOL software policy and recent-install detection.

    #### About `check_installed_software`

    `check_installed_software` inventories installed software and answers three
    operator questions:

    - **"Is unwanted or EOL software present?"** — `crit=name like 'BitTorrent'`
      (an empty match set is OK, so an absence probe is cheap).
    - **"What was installed recently?"** — `warn=install_date > -7d` correlates
      incidents with fresh installs.
    - **"What is installed at all?"** — a bare call is an OK inventory with the
      package count as perf data.

    Each installed product is one row. There are no default thresholds (a bare call
    is an inventory), an empty match set returns OK, and the matched package count
    is emitted as `count` perf data. The shared keywords (`name`, `version`,
    `publisher`, `install_date`, `size`, `architecture`) carry the same meaning on
    both platforms; `version` comparisons are plain string comparisons everywhere,
    so pin patterns (e.g. `version like '7.'`) rather than relying on numeric
    ordering across multi-digit components.

    ##### Windows

    Reads the registry Uninstall hives —
    `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` in **both** the
    64-bit and 32-bit (Wow6432Node) views, plus every loaded per-user hive under
    `HKEY_USERS` (which covers per-user installs such as VS Code, JetBrains IDEs
    and Electron apps, regardless of the account the service runs as). The `hive`
    keyword says which view a row came from.

    The default filter is `system_component = 0`, matching what Add/Remove Programs
    shows; pass `filter=none` to include runtime/driver components.

    **Caveats:** `InstallDate` is best-effort — many installers never write it (the
    `install_date` expressions simply never match such entries), and MSI stamps it
    on every repair/modify, not only the original install. Legacy patch entries
    (children with `ParentKeyName`) and entries without a `DisplayName` are skipped.

    ##### Linux

    Reads the system package manager — `dpkg-query` on Debian/Ubuntu, `rpm -qa` on
    RHEL/Fedora/SUSE, `pacman -Q` on Arch (detected in that order, dpkg first
    because Debian-family hosts frequently carry an rpm binary too). The `manager`
    keyword says which one was used.

    Only packages whose dpkg state is exactly `installed` are listed: removed
    (`not-installed`), `config-files` leftovers and broken (`half-installed`,
    `unpacked`) packages are skipped, while held packages (`hold ok installed`)
    are kept. If the package-manager query itself fails, the check returns UNKNOWN
    rather than an empty "no installed software found" inventory, so a broken
    package database can never read as a clean OK.

    **Caveats:** install dates are exact on rpm (`INSTALLTIME`); dpkg does not
    record them, so they are approximated from the mtime of the package's
    `/var/lib/dpkg/info/<name>[:<arch>].list` file (rewritten on upgrade — treat as
    "last installed/upgraded"). `pacman -Q` exposes only name and version, so
    `publisher`, `size` and `install_date` stay unset there.

=== "Linux"

    Check installed software packages via the system package manager (dpkg/rpm/pacman): inventory, unwanted/EOL software policy and recent-install detection.

    #### About `check_installed_software`

    `check_installed_software` inventories installed software and answers three
    operator questions:

    - **"Is unwanted or EOL software present?"** — `crit=name like 'BitTorrent'`
      (an empty match set is OK, so an absence probe is cheap).
    - **"What was installed recently?"** — `warn=install_date > -7d` correlates
      incidents with fresh installs.
    - **"What is installed at all?"** — a bare call is an OK inventory with the
      package count as perf data.

    Each installed product is one row. There are no default thresholds (a bare call
    is an inventory), an empty match set returns OK, and the matched package count
    is emitted as `count` perf data. The shared keywords (`name`, `version`,
    `publisher`, `install_date`, `size`, `architecture`) carry the same meaning on
    both platforms; `version` comparisons are plain string comparisons everywhere,
    so pin patterns (e.g. `version like '7.'`) rather than relying on numeric
    ordering across multi-digit components.

    ##### Windows

    Reads the registry Uninstall hives —
    `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` in **both** the
    64-bit and 32-bit (Wow6432Node) views, plus every loaded per-user hive under
    `HKEY_USERS` (which covers per-user installs such as VS Code, JetBrains IDEs
    and Electron apps, regardless of the account the service runs as). The `hive`
    keyword says which view a row came from.

    The default filter is `system_component = 0`, matching what Add/Remove Programs
    shows; pass `filter=none` to include runtime/driver components.

    **Caveats:** `InstallDate` is best-effort — many installers never write it (the
    `install_date` expressions simply never match such entries), and MSI stamps it
    on every repair/modify, not only the original install. Legacy patch entries
    (children with `ParentKeyName`) and entries without a `DisplayName` are skipped.

    ##### Linux

    Reads the system package manager — `dpkg-query` on Debian/Ubuntu, `rpm -qa` on
    RHEL/Fedora/SUSE, `pacman -Q` on Arch (detected in that order, dpkg first
    because Debian-family hosts frequently carry an rpm binary too). The `manager`
    keyword says which one was used.

    Only packages whose dpkg state is exactly `installed` are listed: removed
    (`not-installed`), `config-files` leftovers and broken (`half-installed`,
    `unpacked`) packages are skipped, while held packages (`hold ok installed`)
    are kept. If the package-manager query itself fails, the check returns UNKNOWN
    rather than an empty "no installed software found" inventory, so a broken
    package database can never read as a clean OK.

    **Caveats:** install dates are exact on rpm (`INSTALLTIME`); dpkg does not
    record them, so they are approximated from the mtime of the package's
    `/var/lib/dpkg/info/<name>[:<arch>].list` file (rewritten on upgrade — treat as
    "last installed/upgraded"). `pacman -Q` exposes only name and version, so
    `publisher`, `size` and `install_date` stay unset there.

**Jump to section:**

* [Sample Commands](#check_installed_software_samples)
* [Command-line Arguments](#check_installed_software_options)
* [Filter keywords](#check_installed_software_filter_keys)


<a id="check_installed_software_samples"></a>
#### Sample Commands

**Default check (inventory: package count as status and perf):**

```
check_installed_software
OK: 101 software packages installed.|'count'=101;0;0
```

**Alert when unwanted software is present (an absent product is OK):**

```
check_installed_software "crit=name like 'Notepad++'"
CRITICAL: Notepad++ (64-bit x64) 1.0.0 (Notepad++ Team)|'count'=101;0;0

check_installed_software "crit=name like 'BitTorrent'"
OK: 101 software packages installed.|'count'=101;0;0
```

**Detect recent installs (correlate incidents with software changes):**

```
check_installed_software "warn=install_date > -30d" "top-syntax=${status}: ${warn_count} recent installs: ${warn_list}"
WARNING: 2 recent installs: PowerToys (Preview) 0.100.2 (Microsoft Corporation), Microsoft Edge 151.0.4129.72 (Microsoft Corporation)|'count'=101;0;0
```

**Threshold on installed size (large packages):**

```
check_installed_software "warn=size > 500M" "top-syntax=${status}: ${warn_count} packages over 500M"
WARNING: 3 packages over 500M|'count'=428;0;0
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_installed_software --arguments "crit=name like 'TeamViewer'"
OK: 101 software packages installed.
```

##### Windows

**Flag EOL software by version (string comparison — pin the major with like):**

```
check_installed_software "filter=name like 'Java 7'" "crit=version like '7.'"
CRITICAL: Java 7 Update 51 7.0.510 (Oracle)|'count'=1;0;0
```

**List per-user installs (software outside the machine-wide hives):**

```
check_installed_software "filter=hive = 'user'" "top-syntax=${status}: ${count} per-user packages: ${list}"
OK: 18 per-user packages: GitHub Desktop 3.6.3 (GitHub, Inc.), CLion 2026.2 (JetBrains s.r.o.), Microsoft Visual Studio Code (User) 1.115.0 (Microsoft Corporation), ...|'count'=18;0;0
```

**Only 32-bit software installed on a 64-bit host:**

```
check_installed_software "filter=architecture = 'x86'" "top-syntax=${status}: ${count} 32-bit packages"
OK: 51 32-bit packages|'count'=51;0;0
```

**Include SystemComponent entries (hidden from Add/Remove Programs):**

```
check_installed_software filter=none
OK: 233 software packages installed.|'count'=233;0;0
```

##### Linux

**Alert when unwanted software is present:**

```
check_installed_software "crit=name like 'telnetd'"
CRITICAL: telnetd 0.17-41 (Debian telnet maintainers)|'count'=428;0;0
```

**Flag EOL software (pin the version prefix with like):**

```
check_installed_software "filter=name like 'openjdk-7'" "crit=version like '7u'"
CRITICAL: openjdk-7-jre 7u51-2.4.6-1 (Debian Java Maintainers)|'count'=1;0;0
```

**Custom output showing the detected package manager:**

```
check_installed_software "filter=name = 'bash'" "top-syntax=${status}: ${list}" "detail-syntax=${name} ${version} via ${manager}"
OK: bash 5.2.21-2 via dpkg|'count'=1;0;0
```



<a id="check_installed_software_options"></a>
#### Command-line Arguments

=== "Windows"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                                   | Default Value                                    |
    |--------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------|
    | <a id="check_installed_software_filter"></a>[filter](../common-options.md#filter)                                        | system_component = 0                             |
    | <a id="check_installed_software_warning"></a>[warning](../common-options.md#warning)                                     |                                                  |
    | <a id="check_installed_software_warn"></a>[warn](../common-options.md#warn)                                              |                                                  |
    | <a id="check_installed_software_critical"></a>[critical](../common-options.md#critical)                                  |                                                  |
    | <a id="check_installed_software_crit"></a>[crit](../common-options.md#crit)                                              |                                                  |
    | <a id="check_installed_software_ok"></a>[ok](../common-options.md#ok)                                                    |                                                  |
    | <a id="check_installed_software_debug"></a>[debug](../common-options.md#debug)                                           | false                                            |
    | <a id="check_installed_software_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                            |
    | <a id="check_installed_software_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                               |
    | <a id="check_installed_software_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                  |
    | <a id="check_installed_software_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                            |
    | <a id="check_installed_software_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                |
    | <a id="check_installed_software_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                       |
    | <a id="check_installed_software_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(count) software packages installed. |
    | <a id="check_installed_software_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No installed software found           |
    | <a id="check_installed_software_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name} ${version} (${publisher})                |
    | <a id="check_installed_software_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                          |
    | <a id="check_installed_software_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                  |
    | <a id="check_installed_software_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                  |
    | <a id="check_installed_software_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                               |
    | <a id="check_installed_software_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                  |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                                   | Default Value                                    |
    |--------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------|
    | <a id="check_installed_software_filter"></a>[filter](../common-options.md#filter)                                        |                                                  |
    | <a id="check_installed_software_warning"></a>[warning](../common-options.md#warning)                                     |                                                  |
    | <a id="check_installed_software_warn"></a>[warn](../common-options.md#warn)                                              |                                                  |
    | <a id="check_installed_software_critical"></a>[critical](../common-options.md#critical)                                  |                                                  |
    | <a id="check_installed_software_crit"></a>[crit](../common-options.md#crit)                                              |                                                  |
    | <a id="check_installed_software_ok"></a>[ok](../common-options.md#ok)                                                    |                                                  |
    | <a id="check_installed_software_debug"></a>[debug](../common-options.md#debug)                                           | false                                            |
    | <a id="check_installed_software_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                            |
    | <a id="check_installed_software_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                               |
    | <a id="check_installed_software_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                  |
    | <a id="check_installed_software_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                            |
    | <a id="check_installed_software_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                |
    | <a id="check_installed_software_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                       |
    | <a id="check_installed_software_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(count) software packages installed. |
    | <a id="check_installed_software_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No installed software found           |
    | <a id="check_installed_software_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name} ${version} (${publisher})                |
    | <a id="check_installed_software_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                          |
    | <a id="check_installed_software_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                  |
    | <a id="check_installed_software_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                  |
    | <a id="check_installed_software_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                               |
    | <a id="check_installed_software_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                  |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_installed_software_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option            | Description                                                                                                   |
    |-------------------|---------------------------------------------------------------------------------------------------------------|
    | architecture      | 'x64' or 'x86' (registry view); empty for per-user installs                                                   |
    | hive              | 'machine' (HKLM) or 'user' (per-user install)                                                                 |
    | install_date      | Install date (supports date expressions such as 'install_date > -30d'); unset when Windows did not record one |
    | install_date_s    | Raw InstallDate string as recorded (usually YYYYMMDD; often empty)                                            |
    | install_location  | Install folder (InstallLocation)                                                                              |
    | key               | Uninstall registry sub-key name (product GUID or slug)                                                        |
    | name              | Product display name                                                                                          |
    | publisher         | Publisher / vendor                                                                                            |
    | size              | Estimated install size (from EstimatedSize); 0 when not recorded                                              |
    | system_component  | True for entries flagged SystemComponent (hidden from Add/Remove Programs); excluded by the default filter    |
    | uninstall_string  | Uninstall command line (UninstallString)                                                                      |
    | user              | Account ('DOMAIN\name' or SID) owning a per-user install; empty for machine-wide                              |
    | version           | Display version string (comparisons are lexical, not semver-aware)                                            |
    | windows_installer | True when the product was installed via Windows Installer (MSI)                                               |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option         | Description                                                                                                        |
    |----------------|--------------------------------------------------------------------------------------------------------------------|
    | architecture   | Package architecture (amd64, x86_64, noarch, ...)                                                                  |
    | install_date   | Install date (supports date expressions such as 'install_date > -30d'); unset when the manager does not record one |
    | install_date_s | Install date as YYYY-MM-DD; empty when unknown                                                                     |
    | manager        | Package manager the entry came from (dpkg, rpm, pacman)                                                            |
    | name           | Package name                                                                                                       |
    | package_status | Package state; always 'installed' for listed packages                                                              |
    | publisher      | Maintainer (dpkg, email stripped) / vendor (rpm); empty for pacman                                                 |
    | size           | Installed size in bytes; 0 when not recorded                                                                       |
    | version        | Version string (rpm: version-release); comparisons are lexical, not version-aware                                  |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_kernel_memory

=== "Windows"

    Check kernel memory-manager health: paged/nonpaged pool bytes, file-cache bytes and page-fault rates â€” the pool-exhaustion and hard-fault-storm signals free-RAM thresholds miss.

    #### About `check_kernel_memory`

    `check_kernel_memory` reports kernel memory-manager health: kernel-allocation
    gauges, cache bytes and page-fault rates. It complements `check_memory`
    (used/free/size of physical/committed/virtual) — **kernel-side leaks and
    hard-fault storms are the classic server failure modes that free-RAM thresholds
    do not catch.** The fault counters are rates, so the check samples a 1-second
    window (like `check_swap_io`).

    The check returns a single aggregate row. All gauges and rates are always
    emitted as perf data (`kernel_cache`, `kernel_page_faults_per_sec`, ...), which
    is what makes a slow kernel-allocation leak visible: it is inherently a trend
    signal, so let the backend graph it. There are no default thresholds.

    `cache` and `page_faults_per_sec` are shared; the kernel-allocation gauges and
    the hard-fault rate keep their platform-native names (`pool_*` /
    `hard_faults_per_sec` on Windows, `slab_*` / `major_faults_per_sec` on Linux),
    the same convention as `hive` vs `manager` in `check_installed_software`.

    ##### Windows

    Sourced from the PDH `Memory` counter set. Keywords: `pool_paged`,
    `pool_nonpaged`, `cache`, `page_faults_per_sec`, `transition_faults_per_sec`
    and `hard_faults_per_sec`.

    `hard_faults_per_sec` counts hard-fault *events* (`Page Reads/sec`), not the
    pages they bring in: `check_swap_io` reports the latter as `swap_in`
    (`Pages Input/sec`), and a read that pages in a whole cluster makes `swap_in`
    several times larger than the fault rate. Read the two side by side to tell a
    fault storm from a paging storm.

    ##### Linux

    Sourced from `/proc/meminfo` and `/proc/vmstat`. Keywords: `slab`,
    `slab_reclaimable`, `slab_unreclaimable`, `cache`, `page_faults_per_sec` and
    `major_faults_per_sec`. `slab_unreclaimable` is the gauge that exposes a slow
    kernel-side leak — reclaimable slab grows and shrinks with cache pressure and
    is not by itself a problem.

=== "Linux"

    Check kernel memory-manager health: slab bytes (reclaimable/unreclaimable), page-cache bytes and page-fault rates — the kernel-leak and fault-storm signals free-RAM thresholds miss.

    #### About `check_kernel_memory`

    `check_kernel_memory` reports kernel memory-manager health: kernel-allocation
    gauges, cache bytes and page-fault rates. It complements `check_memory`
    (used/free/size of physical/committed/virtual) — **kernel-side leaks and
    hard-fault storms are the classic server failure modes that free-RAM thresholds
    do not catch.** The fault counters are rates, so the check samples a 1-second
    window (like `check_swap_io`).

    The check returns a single aggregate row. All gauges and rates are always
    emitted as perf data (`kernel_cache`, `kernel_page_faults_per_sec`, ...), which
    is what makes a slow kernel-allocation leak visible: it is inherently a trend
    signal, so let the backend graph it. There are no default thresholds.

    `cache` and `page_faults_per_sec` are shared; the kernel-allocation gauges and
    the hard-fault rate keep their platform-native names (`pool_*` /
    `hard_faults_per_sec` on Windows, `slab_*` / `major_faults_per_sec` on Linux),
    the same convention as `hive` vs `manager` in `check_installed_software`.

    ##### Windows

    Sourced from the PDH `Memory` counter set. Keywords: `pool_paged`,
    `pool_nonpaged`, `cache`, `page_faults_per_sec`, `transition_faults_per_sec`
    and `hard_faults_per_sec`.

    `hard_faults_per_sec` counts hard-fault *events* (`Page Reads/sec`), not the
    pages they bring in: `check_swap_io` reports the latter as `swap_in`
    (`Pages Input/sec`), and a read that pages in a whole cluster makes `swap_in`
    several times larger than the fault rate. Read the two side by side to tell a
    fault storm from a paging storm.

    ##### Linux

    Sourced from `/proc/meminfo` and `/proc/vmstat`. Keywords: `slab`,
    `slab_reclaimable`, `slab_unreclaimable`, `cache`, `page_faults_per_sec` and
    `major_faults_per_sec`. `slab_unreclaimable` is the gauge that exposes a slow
    kernel-side leak — reclaimable slab grows and shrinks with cache pressure and
    is not by itself a problem.

**Jump to section:**

* [Sample Commands](#check_kernel_memory_samples)
* [Command-line Arguments](#check_kernel_memory_options)
* [Filter keywords](#check_kernel_memory_filter_keys)


<a id="check_kernel_memory_samples"></a>
#### Sample Commands

##### Windows

**Default check (inventory of the kernel memory gauges and fault rates):**

```
check_kernel_memory
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s|'kernel_cache'=298504192;0;0 'kernel_hard_faults_per_sec'=57.2;0;0 'kernel_page_faults_per_sec'=16617.16;0;0 'kernel_pool_nonpaged'=2760646656;0;0 'kernel_pool_paged'=1809305600;0;0 'kernel_transition_faults_per_sec'=4624.68;0;0
```

Note the shape of a healthy host: five-digit total faults/s (soft) but only a
handful of hard faults/s.

**Detect a nonpaged-pool leak (baseline the host, then pin absolute bytes):**

```
check_kernel_memory "warn=pool_nonpaged > 3G" "crit=pool_nonpaged > 4G"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

**Alert on a hard-fault storm (memory pressure forcing disk reads):**

```
check_kernel_memory "warn=hard_faults_per_sec > 200" "crit=hard_faults_per_sec > 1000"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

**Combine pool and fault policy in one check:**

```
check_kernel_memory "warn=pool_paged > 4G or pool_nonpaged > 3G" "crit=hard_faults_per_sec > 1000"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

**Inspect the fault breakdown (soft vs hard):**

```
check_kernel_memory "detail-syntax=faults=${page_faults_per_sec}/s (soft ${transition_faults_per_sec}/s, hard ${hard_faults_per_sec}/s)"
OK: faults=16617.16/s (soft 4624.68/s, hard 57.2/s)
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_kernel_memory --arguments "crit=hard_faults_per_sec > 1000"
OK: paged pool 1.685GB, nonpaged pool 2.571GB, cache 284.676MB, 57.2 hard faults/s
```

##### Linux

**Default check (inventory of the kernel memory gauges and fault rates):**

```
check_kernel_memory
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s|'kernel_cache'=4294967296;0;0 'kernel_major_faults_per_sec'=2;0;0 'kernel_page_faults_per_sec'=25000;0;0 'kernel_slab'=536870912;0;0 'kernel_slab_reclaimable'=402653184;0;0 'kernel_slab_unreclaimable'=134217728;0;0
```

**Detect an unreclaimable-slab leak (baseline the host, then pin absolute bytes):**

```
check_kernel_memory "warn=slab_unreclaimable > 1G" "crit=slab_unreclaimable > 2G"
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s
```

**Alert on a major-fault storm (memory pressure forcing disk reads):**

```
check_kernel_memory "warn=major_faults_per_sec > 200" "crit=major_faults_per_sec > 1000"
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s
```

**Inspect the fault breakdown (total vs major):**

```
check_kernel_memory "detail-syntax=faults=${page_faults_per_sec}/s (major ${major_faults_per_sec}/s), slab=${slab}"
OK: faults=25000/s (major 2/s), slab=512MB
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_kernel_memory --arguments "crit=major_faults_per_sec > 1000"
OK: slab 512MB (128MB unreclaimable), cache 4GB, 2 major faults/s
```



<a id="check_kernel_memory_options"></a>
#### Command-line Arguments

=== "Windows"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                              | Default Value                                                                                                  |
    |---------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|
    | <a id="check_kernel_memory_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                |
    | <a id="check_kernel_memory_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                |
    | <a id="check_kernel_memory_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                |
    | <a id="check_kernel_memory_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                                |
    | <a id="check_kernel_memory_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                |
    | <a id="check_kernel_memory_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                |
    | <a id="check_kernel_memory_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                          |
    | <a id="check_kernel_memory_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                          |
    | <a id="check_kernel_memory_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                                                                        |
    | <a id="check_kernel_memory_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                |
    | <a id="check_kernel_memory_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                          |
    | <a id="check_kernel_memory_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                              |
    | <a id="check_kernel_memory_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                             |
    | <a id="check_kernel_memory_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                |
    | <a id="check_kernel_memory_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                                                                |
    | <a id="check_kernel_memory_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | paged pool ${pool_paged}, nonpaged pool ${pool_nonpaged}, cache ${cache}, ${hard_faults_per_sec} hard faults/s |
    | <a id="check_kernel_memory_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | kernel                                                                                                         |
    | <a id="check_kernel_memory_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                |
    | <a id="check_kernel_memory_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                |
    | <a id="check_kernel_memory_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                             |
    | <a id="check_kernel_memory_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                              | Default Value                                                                                              |
    |---------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
    | <a id="check_kernel_memory_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                            |
    | <a id="check_kernel_memory_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                            |
    | <a id="check_kernel_memory_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                            |
    | <a id="check_kernel_memory_critical"></a>[critical](../common-options.md#critical)                                  |                                                                                                            |
    | <a id="check_kernel_memory_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                            |
    | <a id="check_kernel_memory_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                            |
    | <a id="check_kernel_memory_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                      |
    | <a id="check_kernel_memory_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                      |
    | <a id="check_kernel_memory_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                                                                    |
    | <a id="check_kernel_memory_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                            |
    | <a id="check_kernel_memory_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                      |
    | <a id="check_kernel_memory_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                          |
    | <a id="check_kernel_memory_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                         |
    | <a id="check_kernel_memory_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                            |
    | <a id="check_kernel_memory_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                                                            |
    | <a id="check_kernel_memory_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | slab ${slab} (${slab_unreclaimable} unreclaimable), cache ${cache}, ${major_faults_per_sec} major faults/s |
    | <a id="check_kernel_memory_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | kernel                                                                                                     |
    | <a id="check_kernel_memory_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                            |
    | <a id="check_kernel_memory_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                            |
    | <a id="check_kernel_memory_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                         |
    | <a id="check_kernel_memory_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                            |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_kernel_memory_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option                    | Description                                                                                                                                                                             |
    |---------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | cache                     | System file-cache working set in bytes (counter 'Cache Bytes')                                                                                                                          |
    | hard_faults_per_sec       | Hard faults per second (Page Reads/sec): faults that had to read from disk — the fault-storm signal                                                                                     |
    | page_faults_per_sec       | Total page faults per second (counter 'Page Faults/sec', soft + hard). Dominated by cheap soft faults and routinely very large on a healthy host — alert on hard_faults_per_sec instead |
    | pool_nonpaged             | Nonpaged pool bytes (counter 'Pool Nonpaged Bytes') — steady growth here is the classic driver-leak signal                                                                              |
    | pool_paged                | Paged pool bytes (counter 'Pool Paged Bytes'; supports size units, e.g. 'pool_paged > 2G'); renders human-readable                                                                      |
    | transition_faults_per_sec | Transition (soft) faults per second (counter 'Transition Faults/sec'), resolved without disk I/O — the dominant soft-fault kind                                                         |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option               | Description                                                                                                                                                                            |
    |----------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | cache                | Page-cache bytes (Cached in /proc/meminfo)                                                                                                                                             |
    | major_faults_per_sec | Major (hard) faults per second (pgmajfault in /proc/vmstat): faults that had to read from disk — the fault-storm signal                                                                |
    | page_faults_per_sec  | Total page faults per second, soft + hard (pgfault in /proc/vmstat). Dominated by cheap soft faults and routinely very large on a healthy host — alert on major_faults_per_sec instead |
    | slab                 | Total kernel slab allocator bytes (Slab in /proc/meminfo; supports size units, e.g. 'slab > 2G')                                                                                       |
    | slab_reclaimable     | Reclaimable slab bytes the kernel can drop under pressure, e.g. dentry/inode caches (SReclaimable in /proc/meminfo)                                                                    |
    | slab_unreclaimable   | Unreclaimable (pinned) slab bytes (SUnreclaim in /proc/meminfo) — steady growth here is the classic kernel/driver leak signal                                                          |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_kernel_stats

=== "Windows"

    Check system-wide kernel activity: context-switch and system-call rates plus live process and thread counts.

    #### About `check_kernel_stats`

    `check_kernel_stats` reports system-wide kernel activity as one row per metric.
    The rate counters are sampled over a 1-second window. Use `type=` (repeatable)
    to restrict which rows are returned; the default is all of them.

    Row keywords are the same on both platforms — `name`, `label`, `human`, `rate`
    (perf; `0` for the gauge rows) and `current` (perf) — but **`current` does not
    mean the same thing on both.** On Windows it is the gauge value, or the rounded
    per-second rate for a rate row; on Linux it is the raw cumulative counter read
    from `/proc/stat` for a rate row, which only ever grows.

    Threshold on `rate` when you mean a per-second value: a threshold written
    against `current` from the Windows reading fires permanently on Linux, where the
    same keyword is a counter in the millions.

    The default thresholds are thread-count guardrails on both platforms:
    `warn = name = 'threads' and current > 8000`,
    `crit = name = 'threads' and current > 10000`. Override them (`warn=none`) or
    threshold the rates explicitly, e.g. `crit=name = 'ctxt' and rate > 500000` —
    context-switch storms are workload-relative, so baseline before pinning.

    ##### Windows

    Sourced from the PDH `System` counter set.

    | Row (`name`) | Counter              | Kind  | Description                                      |
    |--------------|----------------------|-------|--------------------------------------------------|
    | `ctxt`       | Context Switches/sec | rate  | Scheduler churn; storms indicate lock contention |
    | `syscalls`   | System Calls/sec     | rate  | Kernel-transition rate (Windows only)            |
    | `processes`  | Processes            | gauge | Current process count                            |
    | `threads`    | Threads              | gauge | Current thread count                             |

    Windows exposes no cumulative counter here, so `current` on the rate rows is
    the rounded rate rather than a running total. `Processor Queue Length` and
    `System Up Time` from the same counter set are deliberately not duplicated —
    `check_load` and `check_uptime` own those.

    ##### Linux

    | Row (`name`) | Source                | Kind  | Description                            |
    |--------------|-----------------------|-------|----------------------------------------|
    | `ctxt`       | `/proc/stat`          | rate  | Context switches per second            |
    | `processes`  | `/proc/stat`          | rate  | Process/fork creations per second      |
    | `threads`    | `/proc/*/task`        | gauge | Live thread count (instantaneous)      |

    ##### Platform differences

    Linux's `processes` row is a fork *rate*; Windows has no process-creation-rate
    counter in this set, so its `processes` row is a *gauge* (current count).
    Windows adds the `syscalls` row, which Linux does not have.

=== "Linux"

    Check kernel activity: context-switch rate, fork rate and live thread count.

    #### About `check_kernel_stats`

    `check_kernel_stats` reports system-wide kernel activity as one row per metric.
    The rate counters are sampled over a 1-second window. Use `type=` (repeatable)
    to restrict which rows are returned; the default is all of them.

    Row keywords are the same on both platforms — `name`, `label`, `human`, `rate`
    (perf; `0` for the gauge rows) and `current` (perf) — but **`current` does not
    mean the same thing on both.** On Windows it is the gauge value, or the rounded
    per-second rate for a rate row; on Linux it is the raw cumulative counter read
    from `/proc/stat` for a rate row, which only ever grows.

    Threshold on `rate` when you mean a per-second value: a threshold written
    against `current` from the Windows reading fires permanently on Linux, where the
    same keyword is a counter in the millions.

    The default thresholds are thread-count guardrails on both platforms:
    `warn = name = 'threads' and current > 8000`,
    `crit = name = 'threads' and current > 10000`. Override them (`warn=none`) or
    threshold the rates explicitly, e.g. `crit=name = 'ctxt' and rate > 500000` —
    context-switch storms are workload-relative, so baseline before pinning.

    ##### Windows

    Sourced from the PDH `System` counter set.

    | Row (`name`) | Counter              | Kind  | Description                                      |
    |--------------|----------------------|-------|--------------------------------------------------|
    | `ctxt`       | Context Switches/sec | rate  | Scheduler churn; storms indicate lock contention |
    | `syscalls`   | System Calls/sec     | rate  | Kernel-transition rate (Windows only)            |
    | `processes`  | Processes            | gauge | Current process count                            |
    | `threads`    | Threads              | gauge | Current thread count                             |

    Windows exposes no cumulative counter here, so `current` on the rate rows is
    the rounded rate rather than a running total. `Processor Queue Length` and
    `System Up Time` from the same counter set are deliberately not duplicated —
    `check_load` and `check_uptime` own those.

    ##### Linux

    | Row (`name`) | Source                | Kind  | Description                            |
    |--------------|-----------------------|-------|----------------------------------------|
    | `ctxt`       | `/proc/stat`          | rate  | Context switches per second            |
    | `processes`  | `/proc/stat`          | rate  | Process/fork creations per second      |
    | `threads`    | `/proc/*/task`        | gauge | Live thread count (instantaneous)      |

    ##### Platform differences

    Linux's `processes` row is a fork *rate*; Windows has no process-creation-rate
    counter in this set, so its `processes` row is a *gauge* (current count).
    Windows adds the `syscalls` row, which Linux does not have.

**Jump to section:**

* [Sample Commands](#check_kernel_stats_samples)
* [Command-line Arguments](#check_kernel_stats_options)
* [Filter keywords](#check_kernel_stats_filter_keys)


<a id="check_kernel_stats_samples"></a>
#### Sample Commands

**Watch only the thread count with custom limits:**

```
check_kernel_stats type=threads "warn=current > 5000" "crit=current > 8000"
OK - Threads 3417|'threads'=3417;5000;8000
```

**Alert on a runaway context-switch rate (baseline the host first):**

```
check_kernel_stats "warn=name = 'ctxt' and rate > 100000" "crit=name = 'ctxt' and rate > 500000"
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363
```

**Select several rows and render the raw values:**

```
check_kernel_stats type=ctxt type=processes "detail-syntax=${name}=${current}"
OK - ctxt=119059, processes=628
```

##### Windows

**Default check (all four rows; thread-count guardrails apply):**

```
check_kernel_stats
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417|'ctxt'=119059;8000;10000 'syscalls'=268703;8000;10000 'processes'=628;8000;10000 'threads'=3417;8000;10000
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_kernel_stats --arguments "warn=none" --arguments "crit=name = 'threads' and current > 20000"
OK - Context Switches 119058.5/s, System Calls 268702.6/s, Processes 628, Threads 3417
```

##### Linux

**Default check (context-switch rate, fork rate and live thread count):**

```
check_kernel_stats
OK - Context Switches 57111.0/s, Process Creations 317.0/s, Threads 363|'ctxt'=2747325827;8000;10000 'processes'=2888772;8000;10000 'threads'=363;8000;10000
```

Note that on Linux `current` for the rate rows is the *cumulative* counter read
from `/proc/stat`, which is why `ctxt` shows a very large number in perf data
while the message shows the per-second rate.

**Only context switches and forks (repeat `type=`):**

```
check_kernel_stats type=ctxt type=processes
OK - Context Switches 57111.0/s, Process Creations 317.0/s
```



<a id="check_kernel_stats_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_kernel_stats_type"></a>

    | Option | Default Value | Description                                                                                    |
    |--------|---------------|------------------------------------------------------------------------------------------------|
    | type   |               | Select metric type(s) to show: ctxt, syscalls, processes or threads (repeatable; default: all) |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                             | Default Value                        |
    |--------------------------------------------------------------------------------------------------------------------|--------------------------------------|
    | <a id="check_kernel_stats_filter"></a>[filter](../common-options.md#filter)                                        |                                      |
    | <a id="check_kernel_stats_warning"></a>[warning](../common-options.md#warning)                                     | name = 'threads' and current > 8000  |
    | <a id="check_kernel_stats_warn"></a>[warn](../common-options.md#warn)                                              |                                      |
    | <a id="check_kernel_stats_critical"></a>[critical](../common-options.md#critical)                                  | name = 'threads' and current > 10000 |
    | <a id="check_kernel_stats_crit"></a>[crit](../common-options.md#crit)                                              |                                      |
    | <a id="check_kernel_stats_ok"></a>[ok](../common-options.md#ok)                                                    |                                      |
    | <a id="check_kernel_stats_debug"></a>[debug](../common-options.md#debug)                                           | false                                |
    | <a id="check_kernel_stats_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                |
    | <a id="check_kernel_stats_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                              |
    | <a id="check_kernel_stats_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                      |
    | <a id="check_kernel_stats_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                |
    | <a id="check_kernel_stats_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                    |
    | <a id="check_kernel_stats_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status} - ${list}                  |
    | <a id="check_kernel_stats_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                      |
    | <a id="check_kernel_stats_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                      |
    | <a id="check_kernel_stats_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${label} ${human}                    |
    | <a id="check_kernel_stats_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                              |
    | <a id="check_kernel_stats_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                      |
    | <a id="check_kernel_stats_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                      |
    | <a id="check_kernel_stats_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                   |
    | <a id="check_kernel_stats_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                      |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_kernel_stats_type"></a>

    | Option | Default Value | Description                                                                          |
    |--------|---------------|--------------------------------------------------------------------------------------|
    | type   |               | Select metric type(s) to show: ctxt, processes or threads (repeatable; default: all) |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                             | Default Value                        |
    |--------------------------------------------------------------------------------------------------------------------|--------------------------------------|
    | <a id="check_kernel_stats_filter"></a>[filter](../common-options.md#filter)                                        |                                      |
    | <a id="check_kernel_stats_warning"></a>[warning](../common-options.md#warning)                                     | name = 'threads' and current > 8000  |
    | <a id="check_kernel_stats_warn"></a>[warn](../common-options.md#warn)                                              |                                      |
    | <a id="check_kernel_stats_critical"></a>[critical](../common-options.md#critical)                                  | name = 'threads' and current > 10000 |
    | <a id="check_kernel_stats_crit"></a>[crit](../common-options.md#crit)                                              |                                      |
    | <a id="check_kernel_stats_ok"></a>[ok](../common-options.md#ok)                                                    |                                      |
    | <a id="check_kernel_stats_debug"></a>[debug](../common-options.md#debug)                                           | false                                |
    | <a id="check_kernel_stats_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                |
    | <a id="check_kernel_stats_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                              |
    | <a id="check_kernel_stats_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                      |
    | <a id="check_kernel_stats_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                |
    | <a id="check_kernel_stats_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                    |
    | <a id="check_kernel_stats_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status} - ${list}                  |
    | <a id="check_kernel_stats_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                      |
    | <a id="check_kernel_stats_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                      |
    | <a id="check_kernel_stats_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${label} ${human}                    |
    | <a id="check_kernel_stats_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                              |
    | <a id="check_kernel_stats_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                      |
    | <a id="check_kernel_stats_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                      |
    | <a id="check_kernel_stats_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                   |
    | <a id="check_kernel_stats_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                      |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_kernel_stats_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option  | Description                                                                       |
    |---------|-----------------------------------------------------------------------------------|
    | current | Gauge value (process/thread count); for the rate rows the rounded per-second rate |
    | human   | Human-readable value                                                              |
    | label   | Human-friendly metric label                                                       |
    | name    | Metric name: ctxt, syscalls, processes or threads                                 |
    | rate    | Per-second rate (0 for the processes/threads gauge rows)                          |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option  | Description                                                  |
    |---------|--------------------------------------------------------------|
    | current | Current raw value (cumulative counter, or thread count)      |
    | human   | Human-readable value                                         |
    | label   | Human-friendly metric label (Context Switches, Threads, ...) |
    | name    | Metric name: ctxt, processes or threads                      |
    | rate    | Per-second rate (0 for the threads row)                      |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_load

=== "Windows"

    Check the system load average (1/5/15 minutes), synthesised from the processor queue length plus busy cores.

    #### About `check_load`

    `check_load` reports 1/5/15-minute load averages —
    utilization tells you how busy the CPUs are, load tells you how much work is
    *queued for* them, which is the saturation signal utilization alone cannot
    give (100% CPU with an empty queue is a busy box; 100% with a deep queue is an
    overloaded one).

    ##### Linux

    The averages come straight from `/proc/loadavg` — the kernel's own 1-, 5-
    and 15-minute run-queue averages.

    ##### Windows

    Windows has no kernel-maintained load average, so the CheckSystem background
    collector synthesises one: every second it folds the instantaneous value

    ```
    load = processor queue length + busy cores
    ```

    into three exponential moving averages (the Linux loadavg formula sampled at
    1 Hz). Each fold decays over the interval actually measured rather than an
    assumed second, so the averages stay correct when a collector tick overruns
    the 1-second cadence — which is exactly what happens on the loaded hosts this
    check exists for. The queue length is the PDH counter `\System\Processor Queue Length`
    (threads ready to run but not running, system-wide) and busy cores is
    `cores x CPU busy%` from the same tick. This reproduces Linux semantics —
    running + runnable tasks — so a fully-busy 8-core box reads ~8.0 and a
    saturated one reads above it, and the familiar threshold conventions
    (`warn=load > <cores>`, or `percpu=true` with `warn=load > 1`) transfer as-is.

    ##### Common behaviour

    The check returns a single aggregate row and the keyword vocabulary is identical
    on both platforms, so warning/critical expressions and detail-syntax port
    between them. With `percpu=true` each figure is divided by the number of CPUs so
    thresholds port across hosts with different core counts (the row's `type` then
    reads `scaled` instead of `total`).

    There are no default thresholds; the three averages are always emitted as perf
    data (`total_load1` etc., `scaled_*` with `percpu=true`). `queue` is never
    divided by `percpu` — it is an absolute thread count.

    **Windows caveats:** the averages live in the collector, so the check reports
    *"Load average data is not available yet"* right after service start. 
    If the `\System\Processor Queue Length` counter is unavailable (corrupt perflib), 
    the load degrades to the CPU-utilization component and a warning is logged. 
    Some hypervisors report a small nonzero queue on idle guests — the smoothing 
    absorbs the noise, but baseline before alerting tightly on `queue`. 
    Load sampling can be turned off with `disable = load` in 
    `/settings/system/windows` (the check then reports data-unavailable rather 
    than zeros).

=== "Linux"

    Check the system load average (1/5/15 minutes).

    #### About `check_load`

    `check_load` reports 1/5/15-minute load averages —
    utilization tells you how busy the CPUs are, load tells you how much work is
    *queued for* them, which is the saturation signal utilization alone cannot
    give (100% CPU with an empty queue is a busy box; 100% with a deep queue is an
    overloaded one).

    ##### Linux

    The averages come straight from `/proc/loadavg` — the kernel's own 1-, 5-
    and 15-minute run-queue averages.

    ##### Windows

    Windows has no kernel-maintained load average, so the CheckSystem background
    collector synthesises one: every second it folds the instantaneous value

    ```
    load = processor queue length + busy cores
    ```

    into three exponential moving averages (the Linux loadavg formula sampled at
    1 Hz). Each fold decays over the interval actually measured rather than an
    assumed second, so the averages stay correct when a collector tick overruns
    the 1-second cadence — which is exactly what happens on the loaded hosts this
    check exists for. The queue length is the PDH counter `\System\Processor Queue Length`
    (threads ready to run but not running, system-wide) and busy cores is
    `cores x CPU busy%` from the same tick. This reproduces Linux semantics —
    running + runnable tasks — so a fully-busy 8-core box reads ~8.0 and a
    saturated one reads above it, and the familiar threshold conventions
    (`warn=load > <cores>`, or `percpu=true` with `warn=load > 1`) transfer as-is.

    ##### Common behaviour

    The check returns a single aggregate row and the keyword vocabulary is identical
    on both platforms, so warning/critical expressions and detail-syntax port
    between them. With `percpu=true` each figure is divided by the number of CPUs so
    thresholds port across hosts with different core counts (the row's `type` then
    reads `scaled` instead of `total`).

    There are no default thresholds; the three averages are always emitted as perf
    data (`total_load1` etc., `scaled_*` with `percpu=true`). `queue` is never
    divided by `percpu` — it is an absolute thread count.

    **Windows caveats:** the averages live in the collector, so the check reports
    *"Load average data is not available yet"* right after service start. 
    If the `\System\Processor Queue Length` counter is unavailable (corrupt perflib), 
    the load degrades to the CPU-utilization component and a warning is logged. 
    Some hypervisors report a small nonzero queue on idle guests — the smoothing 
    absorbs the noise, but baseline before alerting tightly on `queue`. 
    Load sampling can be turned off with `disable = load` in 
    `/settings/system/windows` (the check then reports data-unavailable rather 
    than zeros).

**Jump to section:**

* [Sample Commands](#check_load_samples)
* [Command-line Arguments](#check_load_options)
* [Filter keywords](#check_load_filter_keys)


<a id="check_load_samples"></a>
#### Sample Commands

**Show the system load average (1 / 5 / 15 minutes):**

```
check_load
OK: total load average: 2.33528, 1.84625, 1.74261|'total_load1'=2.33528;0;0 'total_load5'=1.84625;0;0 'total_load15'=1.74261;0;0
```

**Normalise the load per CPU (divide by the core count):**

```
check_load percpu=true
OK: scaled load average: 0.145955, 0.115391, 0.108913|'scaled_load1'=0.14595;0;0 'scaled_load5'=0.11539;0;0 'scaled_load15'=0.10891;0;0
```

**Warn / critical on any load window (`load` is the max of the three):**

```
check_load "warn=load > 20" "crit=load > 40"
OK: total load average: 2.33528, 1.84625, 1.74261|'total_load'=2.33528;20;40 'total_load1'=2.33528;0;0 'total_load5'=1.84625;0;0 'total_load15'=1.74261;0;0
```

**Threshold on a specific window, e.g. the 1-minute average:**

```
check_load "warn=load1 > 4" "crit=load1 > 8"
OK: total load average: 2.33528, 1.84625, 1.74261
```

**Per-CPU thresholds (portable across differently-sized hosts):**

```
check_load percpu=true "warn=load > 1" "crit=load > 2"
OK: scaled load average: 0.145955, 0.115391, 0.108913
```

**Inspect the run-queue counters:**

```
check_load "detail-syntax=run=${procs_running} total=${procs_total}"
OK: run=1 total=11221
```

##### Windows

The synthesised collector exposes three extra keywords — `queue` (the raw
`\System\Processor Queue Length` saturation signal), `cores` and `samples`
(collector ticks folded into the averages so far).

**Inspect the raw saturation signal and the collector state:**

```
check_load "detail-syntax=q=${queue} run=${procs_running} total=${procs_total} cores=${cores} samples=${samples}"
OK: q=0.0294169 run=1 total=11221 cores=16 samples=31
```

**Alert on sustained queueing regardless of utilization (USE-method saturation):**

```
check_load "warn=queue > 16" "crit=queue > 32"
OK: total load average: 2.33528, 1.84625, 1.74261
```



<a id="check_load_options"></a>
#### Command-line Arguments

| Option                       | Default Value | Description                                                                         |
|------------------------------|---------------|-------------------------------------------------------------------------------------|
| [percpu](#check_load_percpu) | false         | Divide the load averages by the number of CPUs (reports the 'scaled' per-core load) |



<h5 id="check_load_percpu">percpu:</h5>

Divide the load averages by the number of CPUs (reports the 'scaled' per-core load)

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                     | Default Value                                       |
|------------------------------------------------------------------------------------------------------------|-----------------------------------------------------|
| <a id="check_load_filter"></a>[filter](../common-options.md#filter)                                        |                                                     |
| <a id="check_load_warning"></a>[warning](../common-options.md#warning)                                     |                                                     |
| <a id="check_load_warn"></a>[warn](../common-options.md#warn)                                              |                                                     |
| <a id="check_load_critical"></a>[critical](../common-options.md#critical)                                  |                                                     |
| <a id="check_load_crit"></a>[crit](../common-options.md#crit)                                              |                                                     |
| <a id="check_load_ok"></a>[ok](../common-options.md#ok)                                                    |                                                     |
| <a id="check_load_debug"></a>[debug](../common-options.md#debug)                                           | false                                               |
| <a id="check_load_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                               |
| <a id="check_load_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                             |
| <a id="check_load_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                     |
| <a id="check_load_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                               |
| <a id="check_load_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                   |
| <a id="check_load_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                  |
| <a id="check_load_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                     |
| <a id="check_load_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                     |
| <a id="check_load_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${type} load average: ${load1}, ${load5}, ${load15} |
| <a id="check_load_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${type}                                             |
| <a id="check_load_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                     |
| <a id="check_load_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                     |
| <a id="check_load_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                  |
| <a id="check_load_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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

    <a id="check_memory_type"></a>

    | Option | Default Value | Description                                                                                        |
    |--------|---------------|----------------------------------------------------------------------------------------------------|
    | type   |               | The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE) |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                       | Default Value            |
    |--------------------------------------------------------------------------------------------------------------|--------------------------|
    | <a id="check_memory_filter"></a>[filter](../common-options.md#filter)                                        |                          |
    | <a id="check_memory_warning"></a>[warning](../common-options.md#warning)                                     | used > 80%               |
    | <a id="check_memory_warn"></a>[warn](../common-options.md#warn)                                              |                          |
    | <a id="check_memory_critical"></a>[critical](../common-options.md#critical)                                  | used > 90%               |
    | <a id="check_memory_crit"></a>[crit](../common-options.md#crit)                                              |                          |
    | <a id="check_memory_ok"></a>[ok](../common-options.md#ok)                                                    |                          |
    | <a id="check_memory_debug"></a>[debug](../common-options.md#debug)                                           | false                    |
    | <a id="check_memory_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                    |
    | <a id="check_memory_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                  |
    | <a id="check_memory_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                          |
    | <a id="check_memory_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                    |
    | <a id="check_memory_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                        |
    | <a id="check_memory_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}       |
    | <a id="check_memory_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                          |
    | <a id="check_memory_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                          |
    | <a id="check_memory_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${type}: ${used}/${size} |
    | <a id="check_memory_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${type}                  |
    | <a id="check_memory_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                          |
    | <a id="check_memory_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                          |
    | <a id="check_memory_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                       |
    | <a id="check_memory_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                          |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_memory_type"></a>

    | Option | Default Value | Description                                                                                        |
    |--------|---------------|----------------------------------------------------------------------------------------------------|
    | type   |               | The type of memory to check (physical = Physical memory (RAM), committed = total memory (RAM+PAGE) |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                       | Default Value      |
    |--------------------------------------------------------------------------------------------------------------|--------------------|
    | <a id="check_memory_filter"></a>[filter](../common-options.md#filter)                                        |                    |
    | <a id="check_memory_warning"></a>[warning](../common-options.md#warning)                                     | used > 80%         |
    | <a id="check_memory_warn"></a>[warn](../common-options.md#warn)                                              |                    |
    | <a id="check_memory_critical"></a>[critical](../common-options.md#critical)                                  | used > 90%         |
    | <a id="check_memory_crit"></a>[crit](../common-options.md#crit)                                              |                    |
    | <a id="check_memory_ok"></a>[ok](../common-options.md#ok)                                                    |                    |
    | <a id="check_memory_debug"></a>[debug](../common-options.md#debug)                                           | false              |
    | <a id="check_memory_show-all"></a>[show-all](../common-options.md#show-all)                                  | false              |
    | <a id="check_memory_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored            |
    | <a id="check_memory_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                    |
    | <a id="check_memory_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false              |
    | <a id="check_memory_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                  |
    | <a id="check_memory_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list} |
    | <a id="check_memory_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                    |
    | <a id="check_memory_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                    |
    | <a id="check_memory_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${type} = ${used}  |
    | <a id="check_memory_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${type}            |
    | <a id="check_memory_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                    |
    | <a id="check_memory_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                    |
    | <a id="check_memory_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                 |
    | <a id="check_memory_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                    |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_memory_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option          | Description                                                                                                        |
    |-----------------|--------------------------------------------------------------------------------------------------------------------|
    | convert_bytes() | Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.           |
    | format_bytes()  | Format a number as a human-readable byte string.                                                                   |
    | format_number() | Render a number with a fixed number of decimals, using the check's decimal and thousands separators.               |
    | free            | Free memory in bytes (g,m,k,b) or percentages %                                                                    |
    | free_pct        | % free memory                                                                                                      |
    | scale()         | Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)). |
    | size            | Total size of memory                                                                                               |
    | type            | The type of memory to check                                                                                        |
    | used            | Used memory in bytes (g,m,k,b) or percentages %                                                                    |
    | used_pct        | % used memory                                                                                                      |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option | Description                                     |
    |--------|-------------------------------------------------|
    | free   | Free memory in bytes (g,m,k,b) or percentages % |
    | size   | Total size of memory                            |
    | type   | The type of memory to check                     |
    | used   | Used memory in bytes (g,m,k,b) or percentages % |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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
    `source = 'adapter' and throughput > 100000000` to scope thresholds to a particular
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

    The byte-rate variables (`received`, `sent`, `throughput`) and their
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
    thresholds against `received`/`sent`/`throughput`, scoped to specific
    interfaces by name:

    ```
    check_network "filter=name = 'Ethernet 1'" \
                  "warning=throughput > 800000000" \
                  "critical=throughput > 950000000"
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
    `source = 'adapter' and throughput > 100000000` to scope thresholds to a particular
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

    The byte-rate variables (`received`, `sent`, `throughput`) and their
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
    thresholds against `received`/`sent`/`throughput`, scoped to specific
    interfaces by name:

    ```
    check_network "filter=name = 'Ethernet 1'" \
                  "warning=throughput > 800000000" \
                  "critical=throughput > 950000000"
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

* [Sample Commands](#check_network_samples)
* [Command-line Arguments](#check_network_options)
* [Filter keywords](#check_network_filter_keys)


<a id="check_network_samples"></a>
#### Sample Commands

**Default check (one record per interface):**

The defaults threshold total throughput: `throughput > 10000` warns and
`> 100000` is critical, in bytes per second.

```
check_network
OK: eth0 >659B/s <659B/s, ifb0 >0B/s <0B/s, ifb1 >0B/s <0B/s, lo >0B/s <0B/s|'eth0'=1318Bps;10000;100000 'ifb0'=0Bps;10000;100000 'ifb1'=0Bps;10000;100000 'lo'=0Bps;10000;100000
```

The message renders the human-readable `sent_human` / `received_human`; the
performance data carries the raw `throughput` in bytes per second.

**Exclude loopback and virtual interfaces:**

```
check_network "filter=name not like 'lo' and name not like 'ifb'"
OK: eth0 >659B/s <659B/s|'eth0'=1318Bps;10000;100000
```

Do this before setting any fleet-wide threshold — a container host has a lot of
`veth`/`docker` interfaces that will otherwise each become a record and a
performance-data series.

**Alert when a link is not up:**

Note that a loopback interface reports `unknown` rather than `up`, so filter it
out or this will fire on every host.

```
check_network "crit=link_status != 'up'" "warn=none" "detail-syntax=${name}: ${link_status}"
CRITICAL: eth0: up, ifb0: down, ifb1: down, lo: unknown
```

**Alert on interface errors rather than volume:**

Errors and drops are a cabling, driver or duplex-mismatch signal, and are
invisible to a throughput threshold. They are cumulative counters since boot, so
alert on any increase from your own baseline rather than on an absolute number.

```
check_network "crit=rx_errors > 0 or tx_errors > 0" "warn=none" "detail-syntax=${name}: rx_err=${rx_errors} tx_err=${tx_errors}"
OK: eth0: rx_err=0 tx_err=0, ifb0: rx_err=0 tx_err=0, ifb1: rx_err=0 tx_err=0, lo: rx_err=0 tx_err=0|'eth0_rx_errors'=0;0;0 'eth0_tx_errors'=0;0;0 'ifb0_rx_errors'=0;0;0 'ifb0_tx_errors'=0;0;0
```

**Threshold on link utilisation instead of raw bytes:**

`usage_in` / `usage_out` / `usage_total` are percentages of the link speed, which
ports across differently-sized links. They are `0` whenever `speed_bps` is
unknown — as it is on virtual interfaces and in many VMs — so pair the threshold
with a `speed_bps > 0` guard rather than trusting a 0% reading.

```
check_network "warn=usage_total > 60" "crit=usage_total > 85" "detail-syntax=${name}: ${usage_total}% of ${speed_bps}bps"
OK: eth0: 0% of 0bps, ifb0: 0% of 0bps, ifb1: 0% of 0bps, lo: 0% of 0bps|'eth0_usage_total'=0%;60;85 'ifb0_usage_total'=0%;60;85
```

**Inspect the raw fields:**

```
check_network "detail-syntax=${name} link=${link_status} rx=${received} tx=${sent} speed=${speed_bps}"
OK: eth0 link=up rx=659 tx=659 speed=0, ifb0 link=down rx=0 tx=0 speed=0, ifb1 link=down rx=0 tx=0 speed=0, lo link=unknown rx=0 tx=0 speed=0
```

**Right after the agent starts:**

The rates come from the 1 Hz background collector, so the first seconds after a
restart report no data rather than zeros.

```
check_network
UNKNOWN: No network data available yet (collector still initializing)
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_network --arguments "crit=rx_errors > 100"
OK: eth0 >659B/s <659B/s, lo >0B/s <0B/s
```



<a id="check_network_options"></a>
#### Command-line Arguments

=== "Windows"

    | Option                      | Default Value | Description                                                                                                                                                                                                                                                         |
    |-----------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | [mode](#check_network_mode) | interface     | Which WMI source to report from: 'interface' (default; Win32_PerfRawData_Tcpip_NetworkInterface, physical adapters only), 'adapter' (Win32_PerfRawData_Tcpip_NetworkAdapter, includes NIC team aggregates), or 'both' (every interface reported under both sources) |



    <h5 id="check_network_mode">mode:</h5>

    Which WMI source to report from: 'interface' (default; Win32_PerfRawData_Tcpip_NetworkInterface, physical adapters only), 'adapter' (Win32_PerfRawData_Tcpip_NetworkAdapter, includes NIC team aggregates), or 'both' (every interface reported under both sources)

    *Default Value:* `interface`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                                 |
    |---------------------------------------------------------------------------------------------------------------|-----------------------------------------------|
    | <a id="check_network_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
    | <a id="check_network_warning"></a>[warning](../common-options.md#warning)                                     | throughput > 10000                            |
    | <a id="check_network_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
    | <a id="check_network_critical"></a>[critical](../common-options.md#critical)                                  | throughput > 100000                           |
    | <a id="check_network_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
    | <a id="check_network_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
    | <a id="check_network_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
    | <a id="check_network_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
    | <a id="check_network_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | critical                                      |
    | <a id="check_network_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
    | <a id="check_network_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
    | <a id="check_network_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
    | <a id="check_network_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
    | <a id="check_network_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): Network interfaces seem ok.        |
    | <a id="check_network_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                               |
    | <a id="check_network_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name} >${sent_human}/s <${received_human}/s |
    | <a id="check_network_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                       |
    | <a id="check_network_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
    | <a id="check_network_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
    | <a id="check_network_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
    | <a id="check_network_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                                 |
    |---------------------------------------------------------------------------------------------------------------|-----------------------------------------------|
    | <a id="check_network_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
    | <a id="check_network_warning"></a>[warning](../common-options.md#warning)                                     | throughput > 10000                            |
    | <a id="check_network_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
    | <a id="check_network_critical"></a>[critical](../common-options.md#critical)                                  | throughput > 100000                           |
    | <a id="check_network_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
    | <a id="check_network_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
    | <a id="check_network_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
    | <a id="check_network_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
    | <a id="check_network_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | critical                                      |
    | <a id="check_network_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
    | <a id="check_network_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
    | <a id="check_network_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
    | <a id="check_network_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
    | <a id="check_network_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): Network interfaces seem ok.        |
    | <a id="check_network_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                               |
    | <a id="check_network_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name} >${sent_human}/s <${received_human}/s |
    | <a id="check_network_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                       |
    | <a id="check_network_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
    | <a id="check_network_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
    | <a id="check_network_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
    | <a id="check_network_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_network_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option            | Description                                                                                                                                                                                                         |
    |-------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | MAC               | The MAC address                                                                                                                                                                                                     |
    | convert_bytes()   | Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.                                                                                                            |
    | discards_in       | Inbound packets discarded per second                                                                                                                                                                                |
    | discards_out      | Outbound packets discarded per second                                                                                                                                                                               |
    | enabled           | True if the network interface is enabled                                                                                                                                                                            |
    | errors_in         | Inbound packet errors per second                                                                                                                                                                                    |
    | errors_out        | Outbound packet errors per second                                                                                                                                                                                   |
    | format_bytes()    | Format a number as a human-readable byte string.                                                                                                                                                                    |
    | format_number()   | Render a number with a fixed number of decimals, using the check's decimal and thousands separators.                                                                                                                |
    | link_status       | Network connection status                                                                                                                                                                                           |
    | name              | Network interface name                                                                                                                                                                                              |
    | net_connection_id | Network connection id                                                                                                                                                                                               |
    | packets_in        | Packets received per second                                                                                                                                                                                         |
    | packets_out       | Packets sent per second                                                                                                                                                                                             |
    | received          | Bytes received per second                                                                                                                                                                                           |
    | received_human    | Bytes received per second, formatted as a human-readable string (auto-scaled).                                                                                                                                      |
    | scale()           | Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)).                                                                                                  |
    | sent              | Bytes sent per second                                                                                                                                                                                               |
    | sent_human        | Bytes sent per second, formatted as a human-readable string (auto-scaled).                                                                                                                                          |
    | source            | WMI source: 'interface' or 'adapter'                                                                                                                                                                                |
    | speed             | The network interface speed (raw WMI value, e.g. "1000000000" or "Unknown")                                                                                                                                         |
    | speed_bps         | Negotiated link speed in bits/sec, parsed from the WMI Speed property. BEST-EFFORT: 0 when the speed is Unknown/empty (virtual adapters, some teams). Filter on speed_bps > 0 before relying on usage_in/out/total. |
    | team              | NIC team this adapter belongs to (empty if not a team member / LBFO unavailable)                                                                                                                                    |
    | team_status       | Raw MSFT_NetLbfoTeamMember.OperationalStatus of this team member (empty if not a team member)                                                                                                                       |
    | throughput        | Bytes total per second                                                                                                                                                                                              |
    | total_human       | Bytes total per second, formatted as a human-readable string (auto-scaled).                                                                                                                                         |
    | usage_in          | Percent of negotiated link speed used by received traffic. BEST-EFFORT: reads as 0 when speed is unknown - filter on speed_bps > 0 to distinguish idle from unknown.                                                |
    | usage_out         | Percent of negotiated link speed used by sent traffic. BEST-EFFORT: reads as 0 when speed is unknown - filter on speed_bps > 0 to distinguish idle from unknown.                                                    |
    | usage_total       | Percent of negotiated link speed used by total traffic. BEST-EFFORT: reads as 0 when speed is unknown - filter on speed_bps > 0 to distinguish idle from unknown.                                                   |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option           | Description                                                                                                        |
    |------------------|--------------------------------------------------------------------------------------------------------------------|
    | MAC              | The hardware (MAC) address                                                                                         |
    | convert_bytes()  | Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.           |
    | enabled          | True if the interface link is up                                                                                   |
    | format_bytes()   | Format a number as a human-readable byte string.                                                                   |
    | format_number()  | Render a number with a fixed number of decimals, using the check's decimal and thousands separators.               |
    | link_status      | Link operational state (up/down/unknown)                                                                           |
    | name             | Network interface name                                                                                             |
    | received         | Bytes received per second                                                                                          |
    | received_human   | Bytes received per second (human readable, auto-scaled)                                                            |
    | received_packets | Packets received per second                                                                                        |
    | rx_errors        | Cumulative receive errors since boot                                                                               |
    | scale()          | Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)). |
    | sent             | Bytes sent per second                                                                                              |
    | sent_human       | Bytes sent per second (human readable, auto-scaled)                                                                |
    | sent_packets     | Packets sent per second                                                                                            |
    | speed_bps        | Link speed in bits/sec (0 when unknown, e.g. virtual interfaces)                                                   |
    | throughput       | Bytes total (received + sent) per second                                                                           |
    | total_human      | Bytes total per second (human readable, auto-scaled)                                                               |
    | tx_errors        | Cumulative transmit errors since boot                                                                              |
    | usage_in         | Percent of link speed used by received traffic (0 when speed unknown)                                              |
    | usage_out        | Percent of link speed used by sent traffic (0 when speed unknown)                                                  |
    | usage_total      | Percent of link speed used by total traffic (0 when speed unknown)                                                 |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_os_updates

=== "Windows"

    Check for available Windows updates via the Windows Update Agent (WUA) API.

    #### Checking for pending OS updates

    `check_os_updates` reports the updates the system is waiting to install. The
    counters (`updates`, `security`, …) are **record keywords**: reference them from
    `detail-syntax` (rendered per record and included in `${list}`), not from
    `top-syntax`, where they read as 0. On both platforms the default `warning`
    filter is `updates > 0`, so a bare call warns whenever anything is pending:

    ```
    check_os_updates
    ```

    ##### Windows

    Sourced from the Windows Update Agent (WUA) API. Results can be filtered by
    severity, reboot requirements and other attributes.

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
    check_os_updates "warning=updates - defender > 0" "detail-syntax=${updates} total, ${defender} defender, ${rollups} rollups"
    ```

    **Filtering by title**

    `update-filter=<substring>` restricts the check to updates whose title contains
    the (case-insensitive) substring; all counters (`updates`, `security`, …) are then
    recomputed over just the matching subset:

    ```
    check_os_updates update-filter=".NET" "detail-syntax=${updates} .NET updates: ${titles}"
    ```

    > **Note:** the WUA search criteria is `Type='Software'`, so **driver updates are
    > excluded** by design. This keeps the count focused on OS/application patches.

    ##### Linux

    Sourced from the system package manager — `apt`, `dnf`, `yum`, `zypper` or
    `pacman`, whichever the host uses. The `manager` keyword names the one that was
    queried, and `count` remains a deprecated alias for `updates`.

    **Checking for security updates only**

    Often, you only want to be alerted for *security* updates. You can configure this using the `warning` and `critical` filters:

    ```
    check_os_updates "warning=none" "critical=security > 0"
    ```

    This will return `CRITICAL` if any security updates are pending and otherwise `OK` regardless of the number of ordinary updates.

    ##### Customizing the output

    You can use the syntax options to format the output string:

    ```
    check_os_updates "top-syntax=${status}: ${list}" "detail-syntax=Found ${updates} missing updates. Security: ${security}, Critical: ${critical} - ${titles}"
    ```

    On Linux, list the pending package names and the package manager that reported
    them:

    ```
    check_os_updates "detail-syntax=${updates} updates via ${manager}: ${packages}" show-all
    ```

=== "Linux"

    Check for available OS package updates via the system package manager (apt/dnf/yum/zypper/pacman).

    #### Checking for pending OS updates

    `check_os_updates` reports the updates the system is waiting to install. The
    counters (`updates`, `security`, …) are **record keywords**: reference them from
    `detail-syntax` (rendered per record and included in `${list}`), not from
    `top-syntax`, where they read as 0. On both platforms the default `warning`
    filter is `updates > 0`, so a bare call warns whenever anything is pending:

    ```
    check_os_updates
    ```

    ##### Windows

    Sourced from the Windows Update Agent (WUA) API. Results can be filtered by
    severity, reboot requirements and other attributes.

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
    check_os_updates "warning=updates - defender > 0" "detail-syntax=${updates} total, ${defender} defender, ${rollups} rollups"
    ```

    **Filtering by title**

    `update-filter=<substring>` restricts the check to updates whose title contains
    the (case-insensitive) substring; all counters (`updates`, `security`, …) are then
    recomputed over just the matching subset:

    ```
    check_os_updates update-filter=".NET" "detail-syntax=${updates} .NET updates: ${titles}"
    ```

    > **Note:** the WUA search criteria is `Type='Software'`, so **driver updates are
    > excluded** by design. This keeps the count focused on OS/application patches.

    ##### Linux

    Sourced from the system package manager — `apt`, `dnf`, `yum`, `zypper` or
    `pacman`, whichever the host uses. The `manager` keyword names the one that was
    queried, and `count` remains a deprecated alias for `updates`.

    **Checking for security updates only**

    Often, you only want to be alerted for *security* updates. You can configure this using the `warning` and `critical` filters:

    ```
    check_os_updates "warning=none" "critical=security > 0"
    ```

    This will return `CRITICAL` if any security updates are pending and otherwise `OK` regardless of the number of ordinary updates.

    ##### Customizing the output

    You can use the syntax options to format the output string:

    ```
    check_os_updates "top-syntax=${status}: ${list}" "detail-syntax=Found ${updates} missing updates. Security: ${security}, Critical: ${critical} - ${titles}"
    ```

    On Linux, list the pending package names and the package manager that reported
    them:

    ```
    check_os_updates "detail-syntax=${updates} updates via ${manager}: ${packages}" show-all
    ```

**Jump to section:**

* [Sample Commands](#check_os_updates_samples)
* [Command-line Arguments](#check_os_updates_options)
* [Filter keywords](#check_os_updates_filter_keys)


<a id="check_os_updates_samples"></a>
#### Sample Commands

##### Linux

**Default check (any pending update warns):**

```
check_os_updates
CRITICAL: 176 updates available (152 security) via apt|'updates_security'=152;0;0 'updates'=176;0;0
```

The default critical threshold is `security > 0`, so a host with pending
security updates goes critical rather than merely warning.

**Only care about security updates:**

```
check_os_updates "warning=none" "critical=security > 0"
CRITICAL: 176 updates available (152 security) via apt|'updates_security'=152;0;0
```

**Tolerate a backlog of ordinary updates:**

```
check_os_updates "warning=updates > 200" "critical=updates > 500"
OK: 176 updates available (152 security) via apt|'updates'=176;200;500
```

**Show which package manager answered:**

```
check_os_updates "detail-syntax=${updates} updates via ${manager}"
CRITICAL: 176 updates via apt|'updates_security'=152;0;0 'updates'=176;0;0
```

**List the pending package names:**

```
check_os_updates "detail-syntax=${updates}: ${packages}" show-all
CRITICAL: 176: bsdutils, bzip2, ca-certificates, containerd.io, coreutils, curl, diffutils, dirmngr, distro-info-data, docker-buildx-plugin, ...
```

##### Windows

**Default check:**

```
check_os_updates
WARNING: 7 updates available (2 security)|'updates'=7;0;0 'updates_security'=2;0;0
```

**Alert only on security and critical updates:**

```
check_os_updates "warning=important > 0" "critical=security > 0 or critical > 0"
CRITICAL: 7 updates available (2 security)
```

**Detect a pending reboot:**

`reboot_required` counts updates that would need a reboot once installed;
`reboot_pending` reports a reboot that is *already* queued system-wide.

```
check_os_updates "crit=reboot_pending = 1" "detail-syntax=reboot pending: ${reboot_pending}"
CRITICAL: reboot pending: 1
```

**Threshold OS patches separately from Defender definitions:**

```
check_os_updates "warning=updates - defender > 0" "detail-syntax=${updates} total, ${defender} defender, ${rollups} rollups"
WARNING: 7 total, 3 defender, 1 rollups
```

**Restrict to updates whose title matches a substring:**

All the counters are recomputed over just the matching subset.

```
check_os_updates update-filter=".NET" "detail-syntax=${updates} .NET updates: ${titles}"
WARNING: 2 .NET updates: 2026-08 Cumulative Update for .NET Framework 4.8, Update for .NET 8.0.14
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_os_updates --arguments "critical=security > 0"
OK: 0 updates available (0 security)
```



<a id="check_os_updates_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_os_updates_update-filter"></a>

    | Option        | Default Value | Description                                                                                                                                 |
    |---------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------|
    | update-filter |               | Only count updates whose title contains this (case-insensitive) substring. The counters and titles are recomputed over the matching subset. |




    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                           | Default Value                                                             |
    |------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------|
    | <a id="check_os_updates_filter"></a>[filter](../common-options.md#filter)                                        |                                                                           |
    | <a id="check_os_updates_warning"></a>[warning](../common-options.md#warning)                                     | updates > 0                                                               |
    | <a id="check_os_updates_warn"></a>[warn](../common-options.md#warn)                                              |                                                                           |
    | <a id="check_os_updates_critical"></a>[critical](../common-options.md#critical)                                  | security > 0 or critical > 0                                              |
    | <a id="check_os_updates_crit"></a>[crit](../common-options.md#crit)                                              |                                                                           |
    | <a id="check_os_updates_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                           |
    | <a id="check_os_updates_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                     |
    | <a id="check_os_updates_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                     |
    | <a id="check_os_updates_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                        |
    | <a id="check_os_updates_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                           |
    | <a id="check_os_updates_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                     |
    | <a id="check_os_updates_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                         |
    | <a id="check_os_updates_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                        |
    | <a id="check_os_updates_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): No updates available.                                          |
    | <a id="check_os_updates_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                           |
    | <a id="check_os_updates_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${updates} updates available (${security} security, ${critical} critical) |
    | <a id="check_os_updates_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | updates                                                                   |
    | <a id="check_os_updates_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                           |
    | <a id="check_os_updates_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                           |
    | <a id="check_os_updates_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                        |
    | <a id="check_os_updates_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                           |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                           | Default Value                                                      |
    |------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------|
    | <a id="check_os_updates_filter"></a>[filter](../common-options.md#filter)                                        |                                                                    |
    | <a id="check_os_updates_warning"></a>[warning](../common-options.md#warning)                                     | updates > 0                                                        |
    | <a id="check_os_updates_warn"></a>[warn](../common-options.md#warn)                                              |                                                                    |
    | <a id="check_os_updates_critical"></a>[critical](../common-options.md#critical)                                  | security > 0                                                       |
    | <a id="check_os_updates_crit"></a>[crit](../common-options.md#crit)                                              |                                                                    |
    | <a id="check_os_updates_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                    |
    | <a id="check_os_updates_debug"></a>[debug](../common-options.md#debug)                                           | false                                                              |
    | <a id="check_os_updates_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                              |
    | <a id="check_os_updates_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                 |
    | <a id="check_os_updates_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                    |
    | <a id="check_os_updates_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                              |
    | <a id="check_os_updates_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                  |
    | <a id="check_os_updates_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                 |
    | <a id="check_os_updates_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): No updates available.                                   |
    | <a id="check_os_updates_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                    |
    | <a id="check_os_updates_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${updates} updates available (${security} security) via ${manager} |
    | <a id="check_os_updates_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | updates                                                            |
    | <a id="check_os_updates_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                    |
    | <a id="check_os_updates_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                    |
    | <a id="check_os_updates_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                 |
    | <a id="check_os_updates_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                    |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_os_updates_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option          | Description                                                                                                |
    |-----------------|------------------------------------------------------------------------------------------------------------|
    | critical        | Number of critical updates                                                                                 |
    | defender        | Number of Defender/definition updates (churn daily; threshold separately)                                  |
    | error           | Last error message from the WUA search (if any)                                                            |
    | important       | Number of updates with MSRC severity 'Important'                                                           |
    | reboot_pending  | 1 if the system has a pending reboot queued (registry RebootRequired), even from already-installed updates |
    | reboot_required | Number of updates requiring a reboot                                                                       |
    | rollups         | Number of update-rollup updates                                                                            |
    | security        | Number of security updates                                                                                 |
    | titles          | Semicolon separated list of available update titles                                                        |
    | update_status   | Aggregated status: ok, warning, critical, pending, error                                                   |
    | updates         | Total number of available updates                                                                          |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option   | Description                                       |
    |----------|---------------------------------------------------|
    | manager  | Package manager used to query updates             |
    | packages | Comma separated list of available package updates |
    | security | Number of available security updates              |
    | updates  | Total number of available updates                 |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_os_version

Check the version of the underlying OS.

Reports the version of the underlying Windows OS, sourced from the OS version
information, the registry (UBR), `GetNativeSystemInfo` for the processor
architecture, and `Win32_BIOS` (WMI) for the inventory fields.

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

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                           | Default Value                          |
    |------------------------------------------------------------------------------------------------------------------|----------------------------------------|
    | <a id="check_os_version_filter"></a>[filter](../common-options.md#filter)                                        |                                        |
    | <a id="check_os_version_warning"></a>[warning](../common-options.md#warning)                                     | version <= 50                          |
    | <a id="check_os_version_warn"></a>[warn](../common-options.md#warn)                                              |                                        |
    | <a id="check_os_version_critical"></a>[critical](../common-options.md#critical)                                  | version <= 50                          |
    | <a id="check_os_version_crit"></a>[crit](../common-options.md#crit)                                              |                                        |
    | <a id="check_os_version_ok"></a>[ok](../common-options.md#ok)                                                    |                                        |
    | <a id="check_os_version_debug"></a>[debug](../common-options.md#debug)                                           | false                                  |
    | <a id="check_os_version_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                  |
    | <a id="check_os_version_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                |
    | <a id="check_os_version_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                        |
    | <a id="check_os_version_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                  |
    | <a id="check_os_version_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                      |
    | <a id="check_os_version_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                     |
    | <a id="check_os_version_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                        |
    | <a id="check_os_version_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                        |
    | <a id="check_os_version_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${version} (${kernel_version}) ${arch} |
    | <a id="check_os_version_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | version                                |
    | <a id="check_os_version_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                        |
    | <a id="check_os_version_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                        |
    | <a id="check_os_version_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                     |
    | <a id="check_os_version_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                        |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                           | Default Value                    |
    |------------------------------------------------------------------------------------------------------------------|----------------------------------|
    | <a id="check_os_version_filter"></a>[filter](../common-options.md#filter)                                        |                                  |
    | <a id="check_os_version_warning"></a>[warning](../common-options.md#warning)                                     |                                  |
    | <a id="check_os_version_warn"></a>[warn](../common-options.md#warn)                                              |                                  |
    | <a id="check_os_version_critical"></a>[critical](../common-options.md#critical)                                  |                                  |
    | <a id="check_os_version_crit"></a>[crit](../common-options.md#crit)                                              |                                  |
    | <a id="check_os_version_ok"></a>[ok](../common-options.md#ok)                                                    |                                  |
    | <a id="check_os_version_debug"></a>[debug](../common-options.md#debug)                                           | false                            |
    | <a id="check_os_version_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                            |
    | <a id="check_os_version_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                          |
    | <a id="check_os_version_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                  |
    | <a id="check_os_version_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                            |
    | <a id="check_os_version_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                |
    | <a id="check_os_version_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}               |
    | <a id="check_os_version_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                  |
    | <a id="check_os_version_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                  |
    | <a id="check_os_version_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${os} (kernel ${kernel_release}) |
    | <a id="check_os_version_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | kernel_release                   |
    | <a id="check_os_version_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                  |
    | <a id="check_os_version_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                  |
    | <a id="check_os_version_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                               |
    | <a id="check_os_version_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                  |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_os_version_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option         | Description                                                                                                                                                                                                                                                           |
    |----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | arch           | Native processor architecture: x64, x86, arm64, arm, ia64 or unknown (via GetNativeSystemInfo, so a 32-bit agent under WOW64 still reports the true hardware architecture)                                                                                            |
    | bios_version   | BIOS version (Win32_BIOS.SMBIOSBIOSVersion); inventory only, empty when WMI is unavailable                                                                                                                                                                            |
    | build          | Build version number (perfdata)                                                                                                                                                                                                                                       |
    | kernel_version | NT kernel version as major.minor.build.ubr (on Windows the kernel version tracks the OS version)                                                                                                                                                                      |
    | major          | Major version number (perfdata)                                                                                                                                                                                                                                       |
    | manufacturer   | BIOS manufacturer / vendor (Win32_BIOS.Manufacturer); inventory only, empty when WMI is unavailable                                                                                                                                                                   |
    | minor          | Minor version number (perfdata)                                                                                                                                                                                                                                       |
    | serial         | BIOS/system serial number (Win32_BIOS.SerialNumber); inventory only, empty when WMI is unavailable                                                                                                                                                                    |
    | suite          | Which suites are installed on the machine (Microsoft BackOffice, Web Edition, Compute Cluster Edition, Datacenter Edition, Enterprise Edition, Embedded, Home Edition, Remote Desktop Support, Small Business Server, Storage Server, Terminal Services, Home Server) |
    | ubr            | Update Build Revision — the patch level within a build (the .3803 in 10.0.19045.3803), read from the registry; 0 when unavailable (e.g. pre-Windows 10)                                                                                                               |
    | version        | System version: numeric for thresholds (major*10+minor, e.g. 'version <= 50'), rendered as the friendly product name (e.g. 'Windows 11 23H2')                                                                                                                         |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

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

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_pagefile

Check the size of the system pagefile(s).

#### About `check_pagefile`

`check_pagefile` reports the size and usage of the system's paging space. It
returns one record per paging file (Windows) or swap device (Linux), plus a
synthetic **`total`** record aggregating all of them — which is what you almost
always want to threshold on, since a host with several pagefiles will otherwise
alert per file.

The defaults are `used > 60%` for warning and `used > 80%` for critical.
Thresholds accept both absolute sizes and percentages, so
`crit=used > 8G` and `crit=used > 80%` are both valid; `free_pct` / `used_pct`
are available when you want the percentage as a plain number.

##### What it does and does not tell you

This is a **capacity** check: how much paging space is committed, not how hard
the machine is paging. A box can sit with swap 90% full and be perfectly
healthy — pages written out long ago and never needed again — while a box with
5% swap used can be thrashing badly. For the pressure signal, use
[`check_swap_io`](#check_swap_io), which reports the paging *rate*, and read the
two together.

##### Windows

`peak_used` reports the high-water mark of commit charge for each pagefile since
boot. That is often the more useful alert than instantaneous usage: it catches
the nightly job that briefly exhausted the pagefile hours before the check ran.

```
check_pagefile "crit=peak_used > 90%"
```

##### Linux

Each swap device (or swap file) is one record, and `name` is its path. A host
with swap disabled entirely reports only the `total` record with a size of zero;
guard against that with `filter=size > 0` if a zero-sized total would otherwise
read as 100% used in your dashboards.

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

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                         | Default Value             |
|----------------------------------------------------------------------------------------------------------------|---------------------------|
| <a id="check_pagefile_filter"></a>[filter](../common-options.md#filter)                                        |                           |
| <a id="check_pagefile_warning"></a>[warning](../common-options.md#warning)                                     | used > 60%                |
| <a id="check_pagefile_warn"></a>[warn](../common-options.md#warn)                                              |                           |
| <a id="check_pagefile_critical"></a>[critical](../common-options.md#critical)                                  | used > 80%                |
| <a id="check_pagefile_crit"></a>[crit](../common-options.md#crit)                                              |                           |
| <a id="check_pagefile_ok"></a>[ok](../common-options.md#ok)                                                    |                           |
| <a id="check_pagefile_debug"></a>[debug](../common-options.md#debug)                                           | false                     |
| <a id="check_pagefile_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                     |
| <a id="check_pagefile_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                   |
| <a id="check_pagefile_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                           |
| <a id="check_pagefile_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                     |
| <a id="check_pagefile_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                         |
| <a id="check_pagefile_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}        |
| <a id="check_pagefile_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                           |
| <a id="check_pagefile_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                           |
| <a id="check_pagefile_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name} ${used} (${size}) |
| <a id="check_pagefile_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                   |
| <a id="check_pagefile_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                           |
| <a id="check_pagefile_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                           |
| <a id="check_pagefile_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                        |
| <a id="check_pagefile_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                           |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_pagefile_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option        | Description                                     |
    |---------------|-------------------------------------------------|
    | free          | Free memory in bytes (g,m,k,b) or percentages % |
    | free_pct      | % free memory                                   |
    | name          | The name of the page file (location)            |
    | peak_used     | Peak used memory in bytes (g,m,k,b) since boot  |
    | peak_used_pct | % peak used memory since boot                   |
    | size          | Total size of pagefile                          |
    | used          | Used memory in bytes (g,m,k,b) or percentages % |
    | used_pct      | % used memory                                   |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option | Description                                     |
    |--------|-------------------------------------------------|
    | free   | Free memory in bytes (g,m,k,b) or percentages % |
    | name   | The name of the page file (swap)                |
    | size   | Total size of pagefile/swap                     |
    | used   | Used memory in bytes (g,m,k,b) or percentages % |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_patch_age

*Available on Windows only.*

Check installed-hotfix hygiene: how long since the newest hotfix was installed and whether specific required hotfixes are present.

#### About `check_patch_age`

`check_patch_age` reports the **installed** side of Windows patching — the
counterpart to `check_os_updates`, which reports what is still *pending*. It
enumerates installed hotfixes from `Win32_QuickFixEngineering` and answers the
two questions operators actually ask:

- **"When was this box last patched?"** — via `age`, the number of days since
  the newest hotfix was installed.
- **"Is KB\<n\> installed?"** — via the `hotfix=` option (vulnerability-response
  patch verification), or by testing the `ids` list directly.

The default threshold is `crit=missing > 0`, which is inert unless you pass one
or more `hotfix=` options (a bare number is matched with an implicit `KB`
prefix, so `hotfix=5034441` == `hotfix=KB5034441`). Age alerting is opt-in via
`warn=age > N` / `crit=age > N`.

**Caveat:** `Win32_QuickFixEngineering` reports only servicing-stack /
Component-Based-Servicing hotfixes (the `KB` list), not every cumulative-update
component, and its `InstalledOn` field is frequently blank or locale-formatted.
The check parses the common `M/D/YYYY` and `YYYYMMDD` forms; hotfixes whose date
cannot be parsed are excluded from the `age` calculation (and `age` is `-1` only
when *no* hotfix has a parseable date). Treat `age` as "days since the newest
*dated* hotfix", not an exact patch SLA clock.

**Jump to section:**

* [Sample Commands](#check_patch_age_samples)
* [Command-line Arguments](#check_patch_age_options)
* [Filter keywords](#check_patch_age_filter_keys)


<a id="check_patch_age_samples"></a>
#### Sample Commands

**Default check (reports install count and how long since the newest hotfix):**

```
check_patch_age
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```

**Warn if the box has not been patched in 40 days, critical after 90:**

```
check_patch_age "warn=age > 40" "crit=age > 90"
WARNING: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (57d ago)
```

**Verify a specific hotfix is installed (vulnerability response) — CRITICAL if missing:**

```
check_patch_age hotfix=KB5034441
CRITICAL: 42 hotfixes installed, newest KB5030211 on 1/9/2024 (94d ago); missing: KB5034441
```

**Verify several required hotfixes at once (bare numbers get an implicit KB prefix):**

```
check_patch_age hotfix=KB5034441 hotfix=5030211
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```

**Test presence via the `ids` list instead of the `hotfix=` option:**

```
check_patch_age "crit=ids not like 'KB5034441'"
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```

**Custom output listing the newest hotfix only:**

```
check_patch_age "top-syntax=%(status): %(list)" "detail-syntax=newest %(newest_id) (%(age)d ago), %(patches) installed"
OK: newest KB5034441 (18d ago), 42 installed
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_patch_age --argument "warn=age > 40"
OK: 42 hotfixes installed, newest KB5034441 on 3/12/2024 (18d ago)
```



<a id="check_patch_age_options"></a>
#### Command-line Arguments

<a id="check_patch_age_hotfix"></a>

| Option | Default Value | Description                                                                                                                                                                                   |
|--------|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| hotfix |               | A required HotFixID (repeatable). The check is CRITICAL when a requested hotfix is not installed. A bare number is matched with an implicit 'KB' prefix (hotfix=5034441 == hotfix=KB5034441). |




**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                          | Default Value      |
|-----------------------------------------------------------------------------------------------------------------|--------------------|
| <a id="check_patch_age_filter"></a>[filter](../common-options.md#filter)                                        |                    |
| <a id="check_patch_age_warning"></a>[warning](../common-options.md#warning)                                     |                    |
| <a id="check_patch_age_warn"></a>[warn](../common-options.md#warn)                                              |                    |
| <a id="check_patch_age_critical"></a>[critical](../common-options.md#critical)                                  | missing > 0        |
| <a id="check_patch_age_crit"></a>[crit](../common-options.md#crit)                                              |                    |
| <a id="check_patch_age_ok"></a>[ok](../common-options.md#ok)                                                    |                    |
| <a id="check_patch_age_debug"></a>[debug](../common-options.md#debug)                                           | false              |
| <a id="check_patch_age_show-all"></a>[show-all](../common-options.md#show-all)                                  | false              |
| <a id="check_patch_age_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored            |
| <a id="check_patch_age_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                    |
| <a id="check_patch_age_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false              |
| <a id="check_patch_age_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                  |
| <a id="check_patch_age_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list} |
| <a id="check_patch_age_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                    |
| <a id="check_patch_age_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                    |
| <a id="check_patch_age_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${message}         |
| <a id="check_patch_age_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | patch              |
| <a id="check_patch_age_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                    |
| <a id="check_patch_age_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                    |
| <a id="check_patch_age_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                 |
| <a id="check_patch_age_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                    |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_patch_age_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                     |
|------------------|-------------------------------------------------------------------------------------------------|
| age              | Days since the newest hotfix was installed (-1 if the install date is unknown)                  |
| ids              | Semicolon-separated list of all installed HotFixIDs (use 'ids like KBxxxxxxx' to test presence) |
| message          | Full status sentence used as the default detail line                                            |
| missing          | Number of requested hotfixes that are not installed                                             |
| missing_ids      | Semicolon-separated list of the requested hotfixes that are missing                             |
| newest_id        | HotFixID of the most recently installed hotfix                                                  |
| newest_installed | Install date of the newest hotfix (as reported by Windows)                                      |
| patches          | Total number of installed hotfixes                                                              |
| required         | Number of hotfixes requested via the hotfix= option                                             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_pdh

*Available on Windows only.*

Check the value of a performance (PDH) counter on the local or remote system.
The counters can also be added and polled periodically to get average values. Performance Log Users group membership is required to check performance counters.

#### About `check_pdh`

`check_pdh` reads Windows performance (PDH) counters and turns each one into a
filter record. It is the general-purpose escape hatch for anything the
purpose-built checks do not cover — if it shows up in Performance Monitor, this
can alert on it.

Counters are named with `counter=` (repeatable), and can also be passed
positionally. There are no default thresholds, so a bare call reports the values
and returns OK; a call with no counter at all is an error rather than an empty OK.

The alias `check_counter` is accepted for backwards compatibility.

##### Instantaneous versus averaged counters

Many PDH counters are *rates* and are meaningless from a single sample — a
single read of `\Processor(_Total)\% Processor Time` returns whatever the last
interval happened to be, or zero. `averages=true` takes two samples a second
apart and reports the difference, which is what you want for any `/sec` or `%`
counter.

For anything you check often, prefer the **configured collection** path instead:
add the counter under `[/settings/system/windows/counters]` so the background
collector samples it continuously, then reference it by its configured name.
That makes the check itself instant, and lets `time=` ask for an average over a
window (`time=5m`) rather than a one-second snapshot. `time=` may be repeated to
report several windows at once, in which case the default perf and detail syntax
automatically grow a `${time}` component so the series stay distinct.

##### Localized counter names

Counter names are localized, so `\Processor(_Total)\% Processor Time` does not
exist on a German or French Windows. `resolution=` decides how the name is
looked up:

- `auto` (default) — try the localized name, then the English API, then index
  expansion. This is what makes a single portable configuration work across
  language variants.
- `english` — force English names regardless of the system language.
- `index` — expand numeric counter indexes to their localized names, which is
  what `expand-index=true` does explicitly.

##### Instances, types and error handling

`instances=true` expands a wildcard instance (`\Process(*)\...`) into one record
per instance. `type=` picks the value format (`double`, `long`, `large`, default
`large`) and `flags=` passes PDH format flags (`nocap100`, `1000`, `noscale`) —
`nocap100` is the one you want for a counter that legitimately exceeds 100%,
such as multi-core `% Processor Time`.

`reload=true` re-reads the counter list on error, which helps with counters
registered after boot by a service that starts late. `ignore-errors=true` makes a
missing or invalid counter report `0` instead of failing the check — convenient
for a configuration shared across differently-provisioned hosts, but be aware
that it turns "the counter is gone" into a silent zero, which is exactly the
kind of thing you may want to alert on.

Note that reading performance counters requires membership of the
**Performance Log Users** group (or equivalent rights); a check that returns
access-denied errors is usually a permissions problem, not a missing counter.

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

<a id="check_pdh_counter"></a>
<a id="check_pdh_time"></a>
<a id="check_pdh_flags"></a>

| Option                                    | Default Value | Description                                                                                                                                                                                                                                                                                 |
|-------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| counter                                   |               | Performance counter to check                                                                                                                                                                                                                                                                |
| [expand-index](#check_pdh_expand-index)   | false         | Expand indexes in counter strings                                                                                                                                                                                                                                                           |
| [resolution](#check_pdh_resolution)       | auto          | How to resolve counter names against the system locale: auto (try the localized name, then the English API, then index expansion - the default), english (force English counter names regardless of the system language) or index (expand numeric counter indexes to their localized names) |
| [instances](#check_pdh_instances)         | false         | Expand wildcards and fetch all instances                                                                                                                                                                                                                                                    |
| [reload](#check_pdh_reload)               | false         | Reload counters on errors (useful to check counters which are not added at boot)                                                                                                                                                                                                            |
| [averages](#check_pdh_averages)           | false         | Check average values (ie. wait for 1 second to collecting two samples)                                                                                                                                                                                                                      |
| time                                      |               | Timeframe to use for named rrd counters                                                                                                                                                                                                                                                     |
| flags                                     |               | Extra flags to configure the counter (nocap100, 1000, noscale)                                                                                                                                                                                                                              |
| [type](#check_pdh_type)                   | large         | Format of value (double, long, large)                                                                                                                                                                                                                                                       |
| [ignore-errors](#check_pdh_ignore-errors) | false         | If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors                                                                                                                                                        |



<h5 id="check_pdh_expand-index">expand-index:</h5>

Expand indexes in counter strings

*Default Value:* `false`

<h5 id="check_pdh_resolution">resolution:</h5>

How to resolve counter names against the system locale: auto (try the localized name, then the English API, then index expansion - the default), english (force English counter names regardless of the system language) or index (expand numeric counter indexes to their localized names)

*Default Value:* `auto`

<h5 id="check_pdh_instances">instances:</h5>

Expand wildcards and fetch all instances

*Default Value:* `false`

<h5 id="check_pdh_reload">reload:</h5>

Reload counters on errors (useful to check counters which are not added at boot)

*Default Value:* `false`

<h5 id="check_pdh_averages">averages:</h5>

Check average values (ie. wait for 1 second to collecting two samples)

*Default Value:* `false`

<h5 id="check_pdh_type">type:</h5>

Format of value (double, long, large)

*Default Value:* `large`

<h5 id="check_pdh_ignore-errors">ignore-errors:</h5>

If we should ignore errors when checking counters, for instance missing counters or invalid counters will return 0 instead of errors

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                    | Default Value       |
|-----------------------------------------------------------------------------------------------------------|---------------------|
| <a id="check_pdh_filter"></a>[filter](../common-options.md#filter)                                        |                     |
| <a id="check_pdh_warning"></a>[warning](../common-options.md#warning)                                     |                     |
| <a id="check_pdh_warn"></a>[warn](../common-options.md#warn)                                              |                     |
| <a id="check_pdh_critical"></a>[critical](../common-options.md#critical)                                  |                     |
| <a id="check_pdh_crit"></a>[crit](../common-options.md#crit)                                              |                     |
| <a id="check_pdh_ok"></a>[ok](../common-options.md#ok)                                                    |                     |
| <a id="check_pdh_debug"></a>[debug](../common-options.md#debug)                                           | false               |
| <a id="check_pdh_show-all"></a>[show-all](../common-options.md#show-all)                                  | false               |
| <a id="check_pdh_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown             |
| <a id="check_pdh_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                     |
| <a id="check_pdh_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false               |
| <a id="check_pdh_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                   |
| <a id="check_pdh_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}  |
| <a id="check_pdh_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                     |
| <a id="check_pdh_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                     |
| <a id="check_pdh_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${alias} = ${value} |
| <a id="check_pdh_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${alias}            |
| <a id="check_pdh_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                     |
| <a id="check_pdh_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                     |
| <a id="check_pdh_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                  |
| <a id="check_pdh_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_pdh_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                        |
|-----------------|--------------------------------------------------------------------------------------------------------------------|
| alias           | The counter alias                                                                                                  |
| convert_bytes() | Convert a byte count to a specific unit and return the numeric value (1024-based). Useful in thresholds.           |
| counter         | The counter name                                                                                                   |
| format_bytes()  | Format a number as a human-readable byte string.                                                                   |
| format_number() | Render a number with a fixed number of decimals, using the check's decimal and thousands separators.               |
| scale()         | Divide a value by a divisor. Useful for arbitrary unit conversions (e.g. decimal Mbps with scale(value, 1000000)). |
| time            | The time for rrd checks                                                                                            |
| value           | The counter value (either float or int)                                                                            |
| value_f         | The counter value (force float value)                                                                              |
| value_gb        | Counter value in GB (1024-based).                                                                                  |
| value_human     | Counter value formatted as a human-readable byte string, auto-scaled to B/KB/MB/GB/...                             |
| value_i         | The counter value (force int value)                                                                                |
| value_kb        | Counter value in KB (1024-based).                                                                                  |
| value_mb        | Counter value in MB (1024-based).                                                                                  |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_pending_reboot

*Available on Windows only.*

Check whether the system is waiting for a reboot, aggregating the servicing, Windows Update, file-rename, computer-rename and domain-join signals.

#### About `check_pending_reboot`

`check_pending_reboot` answers a question no single Windows API does: **is this
machine waiting for a reboot, and why?** A pending reboot is signalled
independently by several subsystems, so the check reads each one and reports the
union. This is the reliable way to catch servers that have applied updates but
will not finish patching until they restart.

All signals are read from the 64-bit registry view, so a 32-bit agent under
WOW64 still reads the native keys.

The default threshold is `warn=pending = 1` (WARNING whenever a reboot is
pending, no critical). Override it to escalate, to alert only on specific causes
(e.g. `crit=servicing = 1`), or to suppress the default with `warn=none`. The
check always returns a single aggregate row, so there is no empty state.

##### How long has the reboot been pending?

A reboot queued minutes ago by an update is expected; one still queued days
later is usually the actionable case. The `age` and `written` keywords expose
how long the reboot has been pending, e.g.
`crit=pending = 1 and age > 7d`. The time comes from the last-write time of the
Component Based Servicing / Windows Update registry key (each exists only while
its reboot is queued); when both are set the oldest one wins. Three caveats:

- Only those two signals carry a timestamp. A reboot signalled solely by a
  pending file rename, computer rename or domain join reports
  `age`/`written` as `unknown`, which never trips a numeric threshold
  (test for it explicitly with `written = 'unknown'`).
- Threshold staleness with a duration on `age` (`age > 7d`) or a *relative*
  date on `written` (`written < -7d` — the signal appeared more than seven
  days ago). A quoted date string (`written < '2026-08-01 00:00:00'`) is
  **not** parsed as a date: it is compared as text against the raw timestamp
  and will not order correctly. The only quoted comparison with a defined
  meaning is the `written = 'unknown'` probe.
- The registry records the key's *last* write, not its creation: if servicing
  re-touches the key while the reboot is still queued, `age` restarts. Treat it
  as "pending at least this long since the last signal update".

**Jump to section:**

* [Sample Commands](#check_pending_reboot_samples)
* [Command-line Arguments](#check_pending_reboot_options)
* [Filter keywords](#check_pending_reboot_filter_keys)


<a id="check_pending_reboot_samples"></a>
#### Sample Commands

**Default check on a clean system:**

```
check_pending_reboot
OK: No reboot pending
```

**Default check when a reboot is queued (default `warn=pending = 1`):**

```
check_pending_reboot
WARNING: Reboot required: Windows Update (pending since 2026-08-16 09:41:12)
```

**Warn on any pending reboot but escalate one that has been pending for over a week:**

```
check_pending_reboot "warn=pending = 1" "crit=pending = 1 and age > 7d"
CRITICAL: Reboot required: Windows Update (pending since 2026-08-10 03:12:45)
```

The since-time is the last-write time of the Component Based Servicing /
Windows Update registry key, which exists only while that reboot is queued.
The other signals (file rename, computer rename, domain join) carry no
timestamp, so `age` and `written` report `unknown` for them and never trip a
numeric threshold (test for it with `written = 'unknown'`).

**Escalate a pending reboot to CRITICAL:**

```
check_pending_reboot "crit=pending = 1"
CRITICAL: Reboot required: Component Based Servicing, Windows Update
```

**Only alert on specific causes (ignore Windows Update, alert on servicing or a pending file rename):**

```
check_pending_reboot "warn=none" "crit=servicing = 1 or file_rename = 1"
OK: No reboot pending
```

**Custom output showing the number of signals and the reasons:**

```
check_pending_reboot "top-syntax=%(status): %(list)" "detail-syntax=%(signals) signal(s): %(reasons)"
WARNING: 1 signal(s): pending file rename
```

**Over NRPE against a remote host:**

```
check_nscp_client --host 192.168.56.103 --command check_pending_reboot
OK: No reboot pending
```



<a id="check_pending_reboot_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                |
|----------------------------------------------------------------------------------------------------------------------|------------------------------|
| <a id="check_pending_reboot_filter"></a>[filter](../common-options.md#filter)                                        |                              |
| <a id="check_pending_reboot_warning"></a>[warning](../common-options.md#warning)                                     | pending = 1                  |
| <a id="check_pending_reboot_warn"></a>[warn](../common-options.md#warn)                                              |                              |
| <a id="check_pending_reboot_critical"></a>[critical](../common-options.md#critical)                                  |                              |
| <a id="check_pending_reboot_crit"></a>[crit](../common-options.md#crit)                                              |                              |
| <a id="check_pending_reboot_ok"></a>[ok](../common-options.md#ok)                                                    |                              |
| <a id="check_pending_reboot_debug"></a>[debug](../common-options.md#debug)                                           | false                        |
| <a id="check_pending_reboot_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                        |
| <a id="check_pending_reboot_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                      |
| <a id="check_pending_reboot_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                              |
| <a id="check_pending_reboot_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                        |
| <a id="check_pending_reboot_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                            |
| <a id="check_pending_reboot_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}           |
| <a id="check_pending_reboot_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): No reboot pending |
| <a id="check_pending_reboot_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                              |
| <a id="check_pending_reboot_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${message}                   |
| <a id="check_pending_reboot_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | reboot                       |
| <a id="check_pending_reboot_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                              |
| <a id="check_pending_reboot_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                              |
| <a id="check_pending_reboot_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                           |
| <a id="check_pending_reboot_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                              |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_pending_reboot_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                                                                                                                                                                                                                                      |
|-----------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| age             | Seconds the reboot has been pending (since the oldest timestamped signal appeared); threshold with durations, e.g. pending = 1 and age > 7d                                                                                                                                                                                                      |
| computer_rename | 1 if the computer has been renamed but not yet rebooted (ActiveComputerName differs from the pending ComputerName)                                                                                                                                                                                                                               |
| domain_join     | 1 if a domain join / SPN update is pending in Netlogon (JoinDomain / AvoidSpnSet present)                                                                                                                                                                                                                                                        |
| file_rename     | 1 if 'Session Manager\PendingFileRenameOperations' is present and non-empty (a file replacement awaits reboot)                                                                                                                                                                                                                                   |
| message         | Full status sentence, e.g. 'Reboot required: Windows Update (pending since 2026-08-16 09:00:00)'                                                                                                                                                                                                                                                 |
| pending         | 1 if any pending-reboot signal is set (the aggregate flag most checks threshold on)                                                                                                                                                                                                                                                              |
| reasons         | Comma-separated human-readable list of pending-reboot causes ('none' if clear)                                                                                                                                                                                                                                                                   |
| servicing       | 1 if Component Based Servicing (CBS) has queued a reboot (the 'Component Based Servicing\RebootPending' key exists)                                                                                                                                                                                                                              |
| signals         | Number of distinct pending-reboot signals currently set                                                                                                                                                                                                                                                                                          |
| windows_update  | 1 if Windows Update has queued a reboot (WindowsUpdate\Auto Update\RebootRequired)                                                                                                                                                                                                                                                               |
| written         | When the oldest timestamped pending-reboot signal appeared (last-write time of the CBS/Windows Update key), as epoch seconds. Threshold staleness with `age` or a relative date (written < -7d); a quoted date string is compared as text, not as a date. 'unknown' when only untimestamped signals are set (`written = 'unknown'` tests for it) |
| written_s       | Signal-appearance time as a human-readable string ('unknown' if no timestamped signal)                                                                                                                                                                                                                                                           |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_printjobs

*Available on Windows only.*

Check individual Windows print jobs: document, owner, size, pages, age and spooler status of every queued job.

#### About `check_printjobs`

`check_printjobs` reports the **individual jobs** sitting in the Windows
spooler — one row per job — from `Win32_PrintJob`. Where
[`check_printqueue`](#check_printqueue) tells you *that* a queue is backed up,
this tells you *what* is stuck in it: which document, whose it is, how big it
is, how long it has been waiting and what the spooler says about it.

Units in thresholds:

- `age` takes durations — `age > 30m`, `age > 2h` — and a bare number still
  means seconds.
- `size` takes byte units — `size > 500M`, `size > 2G`. A **bare number is
  rejected** for size keywords, so write `size > 1K` rather than `size > 1024`.

Defaults: **CRITICAL** when `error = 1 or blocked = 1 or user_intervention = 1`
— the three states the spooler cannot get out of by itself — and **WARNING**
when `age > 600` (ten minutes). A paused job is deliberately *not* critical:
someone paused it on purpose. empty-state is **OK**, because an empty spooler is
the normal state; the check then reports "No print jobs queued" and still emits
`count` perfdata so queue depth can be graphed.

Perfdata is keyed `<printer>_<job id>`, so labels change as jobs come and go.
That is fine for alerting; for graphing prefer the always-present `count`, or
`check_printqueue`'s per-printer `jobs` series. **Windows only.**

**Jump to section:**

* [Sample Commands](#check_printjobs_samples)
* [Command-line Arguments](#check_printjobs_options)
* [Filter keywords](#check_printjobs_filter_keys)


<a id="check_printjobs_samples"></a>
#### Sample Commands

**Default check (stuck and failing jobs):**

The default is critical on a job the spooler cannot clear on its own and warning
on one that has been waiting more than ten minutes.

```
check_printjobs
OK: No print jobs queued|'count'=0;0;0
```

```
check_printjobs
OK: OneNote (Desktop): 'document' by micha (queued, 13s)|'OneNote (Desktop)_2_age'=13s;600;0 'count'=1;0;0
```

```
check_printjobs
CRITICAL: HP LaserJet: 'quarterly.pdf' by CORP\ann (error, 240s)|'HP LaserJet_42_age'=240s;600;0 'count'=1;0;0
```

**Alert earlier on a queue that is not moving:**

```
check_printjobs "warning=age > 1"
WARNING: OneNote (Desktop): 'document' by micha (queued, 23s)|'OneNote (Desktop)_2_age'=23s;1;0 'count'=1;0;0
```

**Full per-job detail:**

```
check_printjobs warning=none critical=none "top-syntax=${list}" "detail-syntax=printer=${printer} id=${id} doc='${document}' owner=${owner} status=${job_status} size=${size} pages=${pages}/${pages_printed} prio=${priority} age=${age} sub=${submitted}"
printer=OneNote (Desktop) id=2 doc='document' owner=micha status=queued size=53620 pages=1/0 prio=1 age=18 sub=2026-08-16 12:10:02|'count'=1;0;0
```

`submitted` is rendered in UTC; threshold on `age` (seconds) rather than on the
timestamp.

**Find who is filling the queue:**

```
check_printjobs "filter=owner = 'CORP\\ann'" "warning=count > 20" "critical=none" "top-syntax=${count} job(s) from ann" "ok-syntax=${count} job(s) from ann"
3 job(s) from ann
```

**Alert on a single very large job:**

Size thresholds take byte units; a bare number is rejected, so write `500M`
rather than `524288000`.

```
check_printjobs "warning=none" "critical=size > 500M"
CRITICAL: HP LaserJet: 'plot.ps' by CORP\bob (spooling, 45s)|'HP LaserJet_51_size'=734003200B;0;524288000 'count'=1;0;0
```

**Only the jobs needing a person at the printer:**

```
check_printjobs "filter=user_intervention = 1 or paper_out = 1" "critical=count > 0"
CRITICAL: HP LaserJet: 'invoice.pdf' by CORP\eve (user_intervention, paper_out, 512s)
```

**Watch one queue on a print server, over NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_printjobs --argument "filter=printer = 'HP LaserJet'" --argument "warning=age > 30m"
OK: All 2 job(s) ok.
```



<a id="check_printjobs_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                          | Default Value                                                  |
|-----------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------|
| <a id="check_printjobs_filter"></a>[filter](../common-options.md#filter)                                        |                                                                |
| <a id="check_printjobs_warning"></a>[warning](../common-options.md#warning)                                     | age > 600                                                      |
| <a id="check_printjobs_warn"></a>[warn](../common-options.md#warn)                                              |                                                                |
| <a id="check_printjobs_critical"></a>[critical](../common-options.md#critical)                                  | error = 1 or blocked = 1 or user_intervention = 1              |
| <a id="check_printjobs_crit"></a>[crit](../common-options.md#crit)                                              |                                                                |
| <a id="check_printjobs_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                |
| <a id="check_printjobs_debug"></a>[debug](../common-options.md#debug)                                           | false                                                          |
| <a id="check_printjobs_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                          |
| <a id="check_printjobs_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                             |
| <a id="check_printjobs_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                |
| <a id="check_printjobs_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                          |
| <a id="check_printjobs_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                              |
| <a id="check_printjobs_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                             |
| <a id="check_printjobs_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) job(s) ok.                             |
| <a id="check_printjobs_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No print jobs queued                                |
| <a id="check_printjobs_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${printer}: '${document}' by ${owner} (${job_status}, ${age}s) |
| <a id="check_printjobs_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${printer}_${id}                                               |
| <a id="check_printjobs_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                |
| <a id="check_printjobs_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                |
| <a id="check_printjobs_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                             |
| <a id="check_printjobs_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_printjobs_filter_keys"></a>
#### Filter keywords

| Option            | Description                                                                                                                      |
|-------------------|----------------------------------------------------------------------------------------------------------------------------------|
| age               | Seconds since the job was submitted (-1 when the spooler did not report a submit time); threshold with durations, e.g. age > 30m |
| blocked           | 1 when the job is blocked on the device queue                                                                                    |
| document          | Document name as the application submitted it                                                                                    |
| error             | 1 when the job is in an error state                                                                                              |
| id                | Spooler job id                                                                                                                   |
| job_status        | Job status words from the spooler: queued, printing, spooling, error, paused, blocked, ...                                       |
| offline           | 1 when the job's printer is offline                                                                                              |
| owner             | User who submitted the job                                                                                                       |
| pages             | Total pages in the job (0 when the driver does not report it)                                                                    |
| pages_printed     | Pages printed so far                                                                                                             |
| paper_out         | 1 when the job is waiting for paper                                                                                              |
| paused            | 1 when the job is paused                                                                                                         |
| printer           | Printer / queue the job is waiting on                                                                                            |
| printing          | 1 when the job is printing                                                                                                       |
| priority          | Job priority                                                                                                                     |
| size              | Job size in bytes; threshold with byte units, e.g. size > 500M                                                                   |
| spooling          | 1 when the job is still spooling                                                                                                 |
| status_mask       | Raw StatusMask bit field, for statuses without their own keyword                                                                 |
| submitted         | When the job was submitted, in UTC, or 'unknown'; threshold on age instead                                                       |
| user_intervention | 1 when the job needs someone at the printer                                                                                      |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_printqueue

*Available on Windows only.*

Check Windows print queues: queue depth, oldest-job age, offline and error states plus the driver, port and sharing of each printer.

#### About `check_printqueue`

`check_printqueue` monitors Windows **print queues** — the classic "the print
server is stuck" incident. It reads `Win32_Printer` (status, error state and the
device inventory) and `Win32_PrintJob` (queued jobs), producing one row per
printer with its queue depth and the age of the oldest waiting job.

For the individual jobs behind those counts — who submitted what, how big it is
and how long it has been waiting — use [`check_printjobs`](#check_printjobs),
which reports one row per job.

The device keywords (`driver`, `port`, `location`, `share`, `server`, `default`,
`shared`, `network`) are the inventory half of the check: they answer "is this
queue still pointing at the driver and port it is supposed to", which is the
other common cause of "printing is broken" once the queue itself looks healthy.
They are also useful as a filter — `filter=shared = 1` to watch only what a
print server actually publishes.

`oldest_job_age` is seconds and takes durations: `oldest_job_age > 30m`,
`oldest_job_age > 2h`. A bare number still means seconds. An empty queue reports
`-1`, which is below every threshold, so it cannot raise a stuck-queue alert.

Defaults: **WARNING** when `jobs > 10`, **CRITICAL** when `error = 1`.
Offline printers are **not** alerted by default — virtual printers (Print to
PDF, OneNote) and disconnected USB printers are routinely offline — so opt in
with the `offline` keyword where it matters (e.g. a print server). empty-state is
**OK** (a host with no printers is fine).

**Jump to section:**

* [Sample Commands](#check_printqueue_samples)
* [Command-line Arguments](#check_printqueue_options)
* [Filter keywords](#check_printqueue_filter_keys)


<a id="check_printqueue_samples"></a>
#### Sample Commands

**Default check (queue depth + printer errors):**

```
check_printqueue
OK: All 6 printer(s) ok.
```

**Default check with a backed-up or errored queue:**

```
check_printqueue
CRITICAL: HP LaserJet: printing, 3 job(s)
```

**Alert on offline printers too (typical for a print server):**

```
check_printqueue "crit=error = 1 or offline = 1"
CRITICAL: HP LaserJet: offline, 0 job(s)
```

**Alert on a stuck queue — a job waiting more than 30 minutes:**

```
check_printqueue "warn=jobs > 10 or oldest_job_age > 30m"
WARNING: HP LaserJet: printing, 2 job(s)
```

**Check one specific printer:**

```
check_printqueue "filter=printer = 'HP LaserJet'" "crit=offline = 1 or error = 1"
OK: All 1 printer(s) ok.
```

**Custom output with full per-printer detail:**

```
check_printqueue "top-syntax=%(status): %(list)" "detail-syntax=%(printer): %(printer_status)/%(error_state) jobs=%(jobs) oldest=%(oldest_job_age)s offline=%(offline)"
OK: HP LaserJet: idle/no_error jobs=0 oldest=-1s offline=0, Microsoft Print to PDF: idle/no_error jobs=0 oldest=-1s offline=0
```

**Over NRPE against a print server:**

```
check_nscp_client --host 192.168.56.103 --command check_printqueue --argument "crit=error = 1 or offline = 1"
OK: All 4 printer(s) ok.
```

**Show the device behind each queue (driver, port, sharing):**

```
check_printqueue warning=none critical=none "top-syntax=${list}" "detail-syntax=${printer} [drv=${driver}] [port=${port}] def=${default} shared=${shared} net=${network}"
Microsoft Print to PDF [drv=Microsoft Print To PDF] [port=PORTPROMPT:] def=0 shared=0 net=0, HP Color LaserJet Pro MFP 4302 [drv=Microsoft IPP Class Driver] [port=WSD-7f7ab05a-2fe9-4ca8-84cb-2f4b45e3bc9a] def=1 shared=0 net=0
```

**Alert when a queue moves to an unexpected driver or port:**

```
check_printqueue "filter=printer = 'HP LaserJet'" "crit=driver != 'HP Universal Printing PCL 6'"
CRITICAL: HP LaserJet: idle, 0 job(s)
```

**Only look at the shared queues on a print server:**

```
check_printqueue "filter=shared = 1" "crit=error = 1 or offline = 1"
OK: All 4 printer(s) ok.
```



<a id="check_printqueue_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                                 |
|------------------------------------------------------------------------------------------------------------------|-----------------------------------------------|
| <a id="check_printqueue_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
| <a id="check_printqueue_warning"></a>[warning](../common-options.md#warning)                                     | jobs > 10                                     |
| <a id="check_printqueue_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
| <a id="check_printqueue_critical"></a>[critical](../common-options.md#critical)                                  | error = 1                                     |
| <a id="check_printqueue_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
| <a id="check_printqueue_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
| <a id="check_printqueue_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
| <a id="check_printqueue_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
| <a id="check_printqueue_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                            |
| <a id="check_printqueue_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
| <a id="check_printqueue_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
| <a id="check_printqueue_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
| <a id="check_printqueue_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
| <a id="check_printqueue_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) printer(s) ok.        |
| <a id="check_printqueue_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No printers found                  |
| <a id="check_printqueue_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${printer}: ${printer_status}, ${jobs} job(s) |
| <a id="check_printqueue_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${printer}                                    |
| <a id="check_printqueue_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
| <a id="check_printqueue_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
| <a id="check_printqueue_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
| <a id="check_printqueue_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_printqueue_filter_keys"></a>
#### Filter keywords

| Option         | Description                                                                                                         |
|----------------|---------------------------------------------------------------------------------------------------------------------|
| default        | 1 if this is the default printer                                                                                    |
| driver         | Print driver the queue uses                                                                                         |
| error          | 1 if the printer is in a real error state (paper/toner/door/jam/service)                                            |
| error_jobs     | Number of queued jobs in an error state                                                                             |
| error_state    | Detected error state: no_error, no_paper, jammed, door_open, ...                                                    |
| jobs           | Number of queued print jobs                                                                                         |
| location       | Location as configured on the queue (empty when unset)                                                              |
| network        | 1 if the queue is a network (rather than local) printer                                                             |
| offline        | 1 if the printer is offline                                                                                         |
| oldest_job_age | Seconds since the oldest queued job (-1 if the queue is empty); threshold with durations, e.g. oldest_job_age > 30m |
| port           | Port the queue prints through (IP_x.x.x.x, USB001, PORTPROMPT:, ...)                                                |
| printer        | Printer / queue name                                                                                                |
| printer_status | Printer status: idle, printing, offline, stopped_printing, warmup, ...                                              |
| server         | Print server hosting the queue (empty for a local queue)                                                            |
| share          | Share name (empty when the queue is not shared)                                                                     |
| shared         | 1 if the queue is shared                                                                                            |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_process

Check state/metrics of one or more of the processes running on the computer.

#### Cross-agent portability keywords

`check_process` keeps a shared keyword vocabulary across the Windows and Linux
agents: `rss` is a straight alias for `working_set` (same bytes and human
value), and `state` accepts `running` as a synonym for `started` (the rendered
value stays `started`), so the same expressions work on both platforms.

#### Process owner (`username` / `uid`)

##### Windows

**`resolve-owner`** (default `false`) turns on owner resolution: it reads each
matching process's token to populate `username`/`uid`. It is opt-in because
`LookupAccountSid` can block for seconds on domain / Azure-AD accounts. Scope the
check to specific processes when using it on a busy host.

```
check_process process=sqlservr.exe resolve-owner=true "crit=username not like 'NT SERVICE'" "detail-syntax=%(exe) owner=%(username)"
check_process process=nginx.exe "warn=state != 'running'" "crit=rss > 2G"
```

##### Linux

`uid` is **always** populated — it is read out of a file the check already
opens, so it costs nothing — and it is numeric, so it can be thresholded
directly:

```
check_process process=* "crit=uid = 0 and working_set > 1G" "detail-syntax=%(exe) uid=%(uid) ws=%(working_set)"
```

```
check_process process=* "filter=uid >= 1000" "warn=count > 200" "top-syntax=%(status): %(count) user processes"
```

**`resolve-owner`** (default `false`) additionally turns each uid into a user
name. It is opt-in because the lookup goes through NSS, which can block for
seconds when it is backed by a remote directory (LDAP/SSSD) — the same reason
the Windows check gates owner resolution. Names are cached per uid for the
lifetime of the agent, so the cost is one lookup per *distinct* owner, not per
process.

```
check_process process=postgres resolve-owner=true "crit=username != 'postgres'" "detail-syntax=%(exe) owner=%(username)"
```

#### Process state: `state` vs `proc_state` (Linux)

Two different questions, two keywords: `state` is the cross-platform
started/stopped verdict (also available on Windows), `proc_state` the raw Linux
scheduler state.

`state` answers "is this process there and alive"; a zombie reports `stopped`
there. `proc_state` answers "what is it *doing*", which is what the two classic
Linux alerts need:

```
check_process process=* "crit=proc_state = 'zombie'" "detail-syntax=%(exe) (pid %(pid)) is a zombie"
```

```
check_process process=* "warn=proc_state = 'disk_sleep'" "detail-syntax=%(exe) is blocked in uninterruptible I/O"
```

`disk_sleep` (`D`) is the useful I/O-hang signal: a process stuck there cannot
be killed and usually means a wedged disk or an unresponsive NFS mount.

For readability the parser also accepts `uninterruptible` for `disk_sleep`,
`defunct` for `zombie` and `traced` for `tracing_stop`.

Note that `proc_state = 'stopped'` means SIGSTOP'd / job-control stopped (`T`),
which is **not** the same as `state = 'stopped'` (process not running).

#### `ppid` (Linux)

The parent process id, so process trees can be expressed:

```
check_process process=* "filter=ppid = 1" "warn=count < 10" "top-syntax=%(status): %(count) processes reparented to init"
```

It is also how kernel threads are excluded: on a standard Linux kernel every
kernel thread is a child of `kthreadd`, which is pid 2, so

```
check_process process=* "filter=ppid != 2 and pid != 2" "warn=count > 500"
```

drops them all. (Note that some environments — WSL, and some container
runtimes — do not run a `kthreadd` at pid 2, so check what pid 2 is on the host
before relying on this.)

#### `elapsed` and `rss` (Linux)

`elapsed` is the "has this been running long enough / too long" check, which
`creation` (an absolute timestamp) makes awkward:

```
check_process process=my-batch-job "crit=elapsed > 3600" "detail-syntax=%(exe) has been running for %(elapsed)s"
```

```
check_process process=nginx "crit=elapsed < 300" "top-syntax=%(status): nginx restarted recently"
```

```
check_process process=* "crit=rss > 2G" "detail-syntax=%(exe) rss=%(rss)"
```

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

The same approach works for CPU usage. Pass `delta=true` so that `%(time)` (and
`%(kernel)` / `%(user)`) report CPU usage over a one second window as a whole
percentage of total CPU, instead of the cumulative CPU seconds since the process
started, for example the top 10 processes by CPU:

```
filter_perf sort=normal limit=10 command=check_process arguments delta=true "warn=time > 50" "crit=time > 90" "detail-syntax=%(exe) cpu=%(time)%"
```

Note that `limit` only trims the performance data; the warning/critical status is still
evaluated against every matching process, so an alert is raised even if the offending
process is not among the items shown.

#### `delta=true` and the per-process CPU collector (Windows)

Unlike earlier releases, `delta=true` no longer samples, sleeps a second, then
samples again inside the check. Instead the CPU percentage is taken from a
background collector that diffs the system process table once a second, so the
check returns immediately with a always-fresh rolling one-second reading (and
memory/handle fields report their real absolute values, not a one-second change).

Because that collector is off by default, you must enable it once:

```ini
[/settings/system/windows]
process cpu = true
```

Until it is enabled, `check_process delta=true` returns `UNKNOWN` with a message
naming the setting (it fails fast on the flag, whether or not `time`/`kernel`/
`user` appear in the syntax) rather than reporting misleading numbers.
Cumulative CPU seconds (`delta` omitted) need no collector and are unaffected.

**Jump to section:**

* [Sample Commands](#check_process_samples)
* [Command-line Arguments](#check_process_options)
* [Filter keywords](#check_process_filter_keys)


<a id="check_process_samples"></a>
#### Sample Commands

##### Windows

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

##### Linux

**Show the owner, parent and state of every matching process:**

```
check_process process=bash "top-syntax=${list}" "detail-syntax=${exe} uid=${uid} ppid=${ppid} proc_state=${proc_state} elapsed=${elapsed}s"
bash uid=1000 ppid=385 proc_state=sleeping elapsed=961739s, bash uid=1000 ppid=381 proc_state=sleeping elapsed=961715s, bash uid=0 ppid=12728 proc_state=sleeping elapsed=961693s
```

**Resolve uids to user names (opt-in, `resolve-owner=true`):**

```
check_process process=bash resolve-owner=true "top-syntax=${list}" "detail-syntax=${exe} uid=${uid} owner=${username}"
bash uid=1000 owner=mickem, bash uid=1000 owner=mickem, bash uid=0 owner=root
```

Without the flag `uid` is still populated; only `username` stays empty.

**Count the processes owned by root (`uid` is numeric, so it thresholds
directly):**

```
check_process process=* "filter=uid = 0" "warn=count > 1000" "ok-syntax=%(status): %(count) processes owned by root"
OK: 27 processes owned by root|'count'=27;1000;0 ...
```

**Alert on zombie processes:**

```
check_process process=* "crit=proc_state = 'zombie'" "ok-syntax=%(status): no zombie processes (%(count) checked)" "detail-syntax=%(exe) (pid %(pid)) is a zombie"
OK: no zombie processes (49 checked)
```

**Alert on processes blocked in uninterruptible I/O (a wedged disk or a hung
NFS mount):**

```
check_process process=* "warn=proc_state = 'disk_sleep'" "ok-syntax=%(status): no processes blocked in uninterruptible I/O"
OK: no processes blocked in uninterruptible I/O
```

**Select by parent process id:**

```
check_process process=* "filter=ppid = 1" "ok-syntax=%(status): %(count) processes reparented to init"
OK: 22 processes reparented to init
```

**Threshold on how long a process has been running (`elapsed`, in seconds):**

```
check_process process=bash "crit=elapsed > 31536000" "top-syntax=${list}" "detail-syntax=${exe} up ${elapsed}s"
bash up 961756s, bash up 961732s, bash up 6183s, bash up 6s|'bash elapsed'=961756s;0;31536000 ...
```

**`rss` is an alias for `working_set` (same value, portable with the Windows
check):**

```
check_process process=bash "crit=rss > 2G" "top-syntax=${list}" "detail-syntax=${exe} rss=${rss} ws=${working_set}"
bash rss=8.594MB ws=8.594MB, bash rss=9.219MB ws=9.219MB, bash rss=4.688MB ws=4.688MB|'bash rss'=0.00839GB;0;2 ...
```



<a id="check_process_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_process_process"></a>
    <a id="check_process_scan-info"></a>
    <a id="check_process_scan-16bit"></a>
    <a id="check_process_scan-unreadable"></a>

    | Option                                        | Default Value | Description                                                                                                                                              |
    |-----------------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------------------------------------|
    | process                                       |               | The service to check, set this to * to check all services                                                                                                |
    | scan-info                                     |               | If all process metrics should be fetched (otherwise only status is fetched)                                                                              |
    | scan-16bit                                    |               | If 16bit processes should be included                                                                                                                    |
    | [delta](#check_process_delta)                 | false         | Report CPU usage as a percentage of total CPU instead of cumulative seconds.                                                                             |
    | scan-unreadable                               |               | If unreadable processes should be included (will not have information)                                                                                   |
    | [total](#check_process_total)                 | false         | Include the total of all matching files                                                                                                                  |
    | [resolve-owner](#check_process_resolve-owner) | false         | Populate the username/uid keywords with the process owner. Off by default: resolving the owner name can block for seconds on domain / Azure-AD accounts. |



    <h5 id="check_process_delta">delta:</h5>

    Report CPU usage as a percentage of total CPU instead of cumulative seconds.
    With delta=true the 'time' (and 'kernel'/'user') fields report the process CPU usage over a one second window as a whole percentage of total CPU. The reading is taken from the CheckSystem background collector (no per-check sleep), so it requires 'process cpu = true' under [/settings/system/windows]; without that the check returns UNKNOWN telling you to enable it.

    *Default Value:* `false`

    <h5 id="check_process_total">total:</h5>

    Include the total of all matching files

    *Default Value:* `false`

    <h5 id="check_process_resolve-owner">resolve-owner:</h5>

    Populate the username/uid keywords with the process owner. Off by default: resolving the owner name can block for seconds on domain / Azure-AD accounts.

    *Default Value:* `false`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                    |
    |---------------------------------------------------------------------------------------------------------------|----------------------------------|
    | <a id="check_process_filter"></a>[filter](../common-options.md#filter)                                        | state != 'unreadable'            |
    | <a id="check_process_warning"></a>[warning](../common-options.md#warning)                                     | state not in ('started')         |
    | <a id="check_process_warn"></a>[warn](../common-options.md#warn)                                              |                                  |
    | <a id="check_process_critical"></a>[critical](../common-options.md#critical)                                  | state = 'stopped', count = 0     |
    | <a id="check_process_crit"></a>[crit](../common-options.md#crit)                                              |                                  |
    | <a id="check_process_ok"></a>[ok](../common-options.md#ok)                                                    |                                  |
    | <a id="check_process_debug"></a>[debug](../common-options.md#debug)                                           | false                            |
    | <a id="check_process_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                            |
    | <a id="check_process_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                          |
    | <a id="check_process_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                  |
    | <a id="check_process_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                            |
    | <a id="check_process_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                |
    | <a id="check_process_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}       |
    | <a id="check_process_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): all processes are ok. |
    | <a id="check_process_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | UNKNOWN: No processes found      |
    | <a id="check_process_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${exe}=${state}                  |
    | <a id="check_process_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${exe}                           |
    | <a id="check_process_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                  |
    | <a id="check_process_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                  |
    | <a id="check_process_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                               |
    | <a id="check_process_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                  |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_process_process"></a>

    | Option                                        | Default Value | Description                                                                                                                                                                                                                                                 |
    |-----------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | process                                       |               | The process to check, set this to * to check all processes                                                                                                                                                                                                  |
    | [delta](#check_process_delta)                 |               | Measure CPU usage as a delta over a one second interval.                                                                                                                                                                                                    |
    | [total](#check_process_total)                 | false         | Include the total of all matching processes                                                                                                                                                                                                                 |
    | [resolve-owner](#check_process_resolve-owner) | false         | Populate the username keyword with the process owner's user name. Off by default: the lookup goes through NSS and can block for seconds when it is backed by a remote directory (LDAP/SSSD). The numeric uid keyword is always populated and needs no flag. |



    <h5 id="check_process_delta">delta:</h5>

    Measure CPU usage as a delta over a one second interval.
    The check samples process and system CPU times, sleeps for one second, then samples again. With delta=true the 'time' (and 'kernel'/'user') fields report the process CPU usage during that second as a whole percentage of total CPU, instead of cumulative CPU seconds.


    <h5 id="check_process_total">total:</h5>

    Include the total of all matching processes

    *Default Value:* `false`

    <h5 id="check_process_resolve-owner">resolve-owner:</h5>

    Populate the username keyword with the process owner's user name. Off by default: the lookup goes through NSS and can block for seconds when it is backed by a remote directory (LDAP/SSSD). The numeric uid keyword is always populated and needs no flag.

    *Default Value:* `false`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                    |
    |---------------------------------------------------------------------------------------------------------------|----------------------------------|
    | <a id="check_process_filter"></a>[filter](../common-options.md#filter)                                        | state != 'unreadable'            |
    | <a id="check_process_warning"></a>[warning](../common-options.md#warning)                                     | state not in ('started')         |
    | <a id="check_process_warn"></a>[warn](../common-options.md#warn)                                              |                                  |
    | <a id="check_process_critical"></a>[critical](../common-options.md#critical)                                  | state = 'stopped', count = 0     |
    | <a id="check_process_crit"></a>[crit](../common-options.md#crit)                                              |                                  |
    | <a id="check_process_ok"></a>[ok](../common-options.md#ok)                                                    |                                  |
    | <a id="check_process_debug"></a>[debug](../common-options.md#debug)                                           | false                            |
    | <a id="check_process_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                            |
    | <a id="check_process_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                          |
    | <a id="check_process_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                  |
    | <a id="check_process_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                            |
    | <a id="check_process_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                |
    | <a id="check_process_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}       |
    | <a id="check_process_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): all processes are ok. |
    | <a id="check_process_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | UNKNOWN: No processes found      |
    | <a id="check_process_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${exe}=${state}                  |
    | <a id="check_process_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${exe}                           |
    | <a id="check_process_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                  |
    | <a id="check_process_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                  |
    | <a id="check_process_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                               |
    | <a id="check_process_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                  |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_process_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option           | Description                                                                                      |
    |------------------|--------------------------------------------------------------------------------------------------|
    | command_line     | Command line of process (not always available)                                                   |
    | creation         | Creation time                                                                                    |
    | error            | Any error messages associated with fetching info                                                 |
    | exe              | The name of the executable                                                                       |
    | filename         | Name of process (with path)                                                                      |
    | gdi_handles      | Number of handles                                                                                |
    | handles          | Number of handles                                                                                |
    | hung             | Process is hung                                                                                  |
    | kernel           | Kernel CPU time: cumulative seconds, or % of total CPU with delta=true                           |
    | legacy_state     | Get process status (for legacy use via check_nt only)                                            |
    | new              | Process is new (can inly be used for real-time filters)                                          |
    | page_fault       | Page fault count                                                                                 |
    | pagefile         | Peak page file use in bytes (g,m,k,b)                                                            |
    | pagefile_pct     | Page file usage as a percentage of the system commit limit                                       |
    | peak_pagefile    | Page file usage in bytes (g,m,k,b)                                                               |
    | peak_virtual     | Peak virtual size in bytes (g,m,k,b)                                                             |
    | peak_working_set | Peak working set in bytes (g,m,k,b)                                                              |
    | pid              | Process id                                                                                       |
    | rss              | Resident set size; alias for working_set (g,m,k,b)                                               |
    | started          | Process is started                                                                               |
    | state            | The current state (started, stopped, hung); 'running' is accepted as a synonym for started       |
    | stopped          | Process is stopped                                                                               |
    | thread_count     | Number of threads                                                                                |
    | time             | User+kernel CPU time: cumulative seconds, or % of total CPU with delta=true                      |
    | uid              | Process owner SID, the Windows analogue of a Unix uid (empty unless resolve-owner=true)          |
    | user             | User CPU time: cumulative seconds, or % of total CPU with delta=true                             |
    | user_handles     | Number of handles                                                                                |
    | username         | Process owner as DOMAIN\name (empty unless resolve-owner=true, or when the token cannot be read) |
    | virtual          | Virtual size in bytes (g,m,k,b)                                                                  |
    | working_set      | Working set in bytes (g,m,k,b)                                                                   |
    | working_set_pct  | Working set as a percentage of total physical RAM                                                |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option           | Description                                                                                                                                                      |
    |------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | command_line     | Command line of process                                                                                                                                          |
    | creation         | Creation time                                                                                                                                                    |
    | elapsed          | Wall-clock seconds since the process started (0 when not known)                                                                                                  |
    | error            | Any error messages associated with fetching info                                                                                                                 |
    | exe              | The name of the executable                                                                                                                                       |
    | filename         | Name of process (with path)                                                                                                                                      |
    | kernel           | Kernel time in seconds                                                                                                                                           |
    | page_fault       | Page fault count                                                                                                                                                 |
    | page_faults      | Page fault count                                                                                                                                                 |
    | peak_virtual     | Peak virtual size in bytes                                                                                                                                       |
    | peak_working_set | Peak working set in bytes                                                                                                                                        |
    | pid              | Process id                                                                                                                                                       |
    | ppid             | Parent process id                                                                                                                                                |
    | proc_state       | Raw Linux scheduler state (the letter ps prints in its STAT column): running, sleeping, disk_sleep, zombie, stopped, tracing_stop, dead, idle, parked or unknown |
    | rss              | Resident set size in bytes; alias for working_set, matching the Windows keyword set (g,m,k,b)                                                                    |
    | started          | Process is started                                                                                                                                               |
    | state            | Cross-platform state verdict: started or stopped ('running' is accepted as a synonym for started in expressions; the rendered value stays 'started')             |
    | stopped          | Process is stopped                                                                                                                                               |
    | time             | User-kernel time in seconds                                                                                                                                      |
    | uid              | Real uid of the process owner from /proc/<pid>/status; -1 when not known (the synthetic 'not found' and total rows)                                              |
    | user             | User time in seconds                                                                                                                                             |
    | username         | Process owner user name (empty unless resolve-owner=true)                                                                                                        |
    | virtual          | Virtual size in bytes                                                                                                                                            |
    | working_set      | Working set (RSS) in bytes                                                                                                                                       |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_process_history

=== "Windows"

    Check the history of processes that have been running since NSClient++ started. Useful for verifying if certain applications have been executed.

    #### About `check_process_history`

    `check_process_history` reports the processes NSClient++ has seen running since
    the agent started — including ones that have long since exited. It answers the
    question a point-in-time `check_process` cannot: *did this ever run?*

    One record is returned per distinct executable, carrying `exe`, whether it is
    `running` right now, `first_seen` / `last_seen` timestamps and `times_seen`
    (how many collector ticks observed it).

    ##### It must be turned on first

    The history is kept by the CheckSystem background collector, which does **not**
    track it by default. Enable it once:

    ```ini
    [/settings/system/windows]
    process history = true
    ```

    (`[/settings/system/unix]` on Linux.) Until it is enabled the check returns
    UNKNOWN and names the setting, rather than reporting an empty history that would
    be easy to mistake for "nothing ran":

    ```
    check_process_history
    UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/windows)
    ```

    The path in that message is the module's own settings path, so it reads
    `/settings/system/unix` on Linux.

    ##### The window is the agent's uptime

    History lives in memory and starts empty when the agent starts. A restarted
    agent has no history, and there is no persistence across restarts — so
    `first_seen` means "first seen since this agent process started", not "first
    seen on this machine". Read the results alongside
    [`check_uptime`](#check_uptime) or `check_nscp`'s `uptime` when the window
    matters.

    ##### What it is good for

    The typical uses are verifying that scheduled work actually ran, and catching
    things that ran when they should not have:

    ```
    check_process_history process=backup.exe "crit=times_seen = 0"
    check_process_history "crit=exe like 'psexec'" "top-syntax=${status}: ${problem_list}"
    ```

    `process=` restricts the check to specific executable names (repeatable,
    case-insensitive); with no `process=` every process in the history is reported.
    There are no default thresholds, and the default empty state is OK.

    For the narrower question "what appeared *recently*", use
    [`check_process_history_new`](#check_process_history_new), which takes a `time=`
    window and reports only processes first seen inside it.

=== "Linux"

    Check the history of processes seen since NSClient++ started (requires 'process history = true').

    #### About `check_process_history`

    `check_process_history` reports the processes NSClient++ has seen running since
    the agent started — including ones that have long since exited. It answers the
    question a point-in-time `check_process` cannot: *did this ever run?*

    One record is returned per distinct executable, carrying `exe`, whether it is
    `running` right now, `first_seen` / `last_seen` timestamps and `times_seen`
    (how many collector ticks observed it).

    ##### It must be turned on first

    The history is kept by the CheckSystem background collector, which does **not**
    track it by default. Enable it once:

    ```ini
    [/settings/system/windows]
    process history = true
    ```

    (`[/settings/system/unix]` on Linux.) Until it is enabled the check returns
    UNKNOWN and names the setting, rather than reporting an empty history that would
    be easy to mistake for "nothing ran":

    ```
    check_process_history
    UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/windows)
    ```

    The path in that message is the module's own settings path, so it reads
    `/settings/system/unix` on Linux.

    ##### The window is the agent's uptime

    History lives in memory and starts empty when the agent starts. A restarted
    agent has no history, and there is no persistence across restarts — so
    `first_seen` means "first seen since this agent process started", not "first
    seen on this machine". Read the results alongside
    [`check_uptime`](#check_uptime) or `check_nscp`'s `uptime` when the window
    matters.

    ##### What it is good for

    The typical uses are verifying that scheduled work actually ran, and catching
    things that ran when they should not have:

    ```
    check_process_history process=backup.exe "crit=times_seen = 0"
    check_process_history "crit=exe like 'psexec'" "top-syntax=${status}: ${problem_list}"
    ```

    `process=` restricts the check to specific executable names (repeatable,
    case-insensitive); with no `process=` every process in the history is reported.
    There are no default thresholds, and the default empty state is OK.

    For the narrower question "what appeared *recently*", use
    [`check_process_history_new`](#check_process_history_new), which takes a `time=`
    window and reports only processes first seen inside it.

**Jump to section:**

* [Sample Commands](#check_process_history_samples)
* [Command-line Arguments](#check_process_history_options)
* [Filter keywords](#check_process_history_filter_keys)


<a id="check_process_history_samples"></a>
#### Sample Commands

**Before the history is turned on:**

The history is kept by the CheckSystem background collector, which is off by
default.

```
check_process_history
UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/unix)
```

Enable it once (`[/settings/system/windows]` on Windows):

```ini
[/settings/system/unix]
process history = true
```

**Default check (an inventory of everything seen since the agent started):**

There are no default thresholds, so a bare call is always OK and reports the
count.

```
check_process_history
OK: 91 processes in history.
```

**Restrict to specific executables (`process=`, repeatable, case-insensitive):**

```
check_process_history process=nscp process=make
OK: 2 processes in history.
```

**Show the detail for a process:**

The default `top-syntax` renders only the *problem* list, so with no threshold
set you get the OK summary. Ask for the full list to see the per-process detail.

```
check_process_history process=nscp "top-syntax=${status}: ${list}" "detail-syntax=${exe} running=${running} seen=${times_seen}"
OK: nscp running=true seen=1
```

`first_seen` / `last_seen` are timestamps and support date comparisons:

```
check_process_history process=nscp "detail-syntax=${exe} running=${running} seen=${times_seen} first=${first_seen}" show-all
OK: nscp running=true seen=1 first=2026-09-04 12:56:43
```

**Count only what is still running:**

```
check_process_history "filter=running = 'true'" "top-syntax=${status}: ${count} still running"
OK: 79 still running
```

**Alert on something that should never have run:**

Note that `like` is a substring match, so a short pattern matches more than you
expect — here `'nc'` matches a kernel worker thread.

```
check_process_history "crit=exe like 'nc'" "top-syntax=${status}: ${problem_list}"
CRITICAL: kworker/R-sync_wq (true)
```

Anchor the pattern or use `=` for an exact name when you mean one binary.

**Verify that a scheduled job actually ran:**

```
check_process_history process=backup.exe "crit=times_seen = 0"
CRITICAL: backup.exe (false)
```

**The window is the agent's uptime:**

History lives in memory and starts empty at agent start, so `first_seen` means
"first seen since this agent process started", not "first seen on this machine".
A restarted agent reports an empty history:

```
check_process_history
OK: 0 processes in history.
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_process_history --arguments "process=backup.exe" --arguments "crit=times_seen = 0"
OK: 1 processes in history.
```



<a id="check_process_history_options"></a>
#### Command-line Arguments

<a id="check_process_history_process"></a>

| Option  | Default Value | Description                                                                                                              |
|---------|---------------|--------------------------------------------------------------------------------------------------------------------------|
| process |               | Filter to specific process names. Can be specified multiple times. If not specified, all processes in history are shown. |




**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                | Default Value                             |
|-----------------------------------------------------------------------------------------------------------------------|-------------------------------------------|
| <a id="check_process_history_filter"></a>[filter](../common-options.md#filter)                                        |                                           |
| <a id="check_process_history_warning"></a>[warning](../common-options.md#warning)                                     |                                           |
| <a id="check_process_history_warn"></a>[warn](../common-options.md#warn)                                              |                                           |
| <a id="check_process_history_critical"></a>[critical](../common-options.md#critical)                                  |                                           |
| <a id="check_process_history_crit"></a>[crit](../common-options.md#crit)                                              |                                           |
| <a id="check_process_history_ok"></a>[ok](../common-options.md#ok)                                                    |                                           |
| <a id="check_process_history_debug"></a>[debug](../common-options.md#debug)                                           | false                                     |
| <a id="check_process_history_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                     |
| <a id="check_process_history_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                        |
| <a id="check_process_history_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                           |
| <a id="check_process_history_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                     |
| <a id="check_process_history_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                         |
| <a id="check_process_history_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                |
| <a id="check_process_history_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): ${count} processes in history. |
| <a id="check_process_history_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                           |
| <a id="check_process_history_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${exe} (${running})                       |
| <a id="check_process_history_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${exe}                                    |
| <a id="check_process_history_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                           |
| <a id="check_process_history_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                           |
| <a id="check_process_history_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                        |
| <a id="check_process_history_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                           |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_process_history_new

=== "Windows"

    Check for new processes that appeared within a specified time window. Useful for detecting unexpected or unauthorized applications.

    #### About `check_process_history_new`

    `check_process_history_new` reports processes that were **first seen within a
    recent time window** — i.e. processes that started (or first appeared to the
    agent) recently. It is useful for spotting unexpected launches, flapping
    services that keep restarting, or confirming that a scheduled job actually ran.

    It relies on the background **process-history collector**, which must be enabled:

    ```ini
    [/settings/system/windows]
    process history = true
    ```

    (`[/settings/system/unix]` on Linux.) Until that is set the check returns
    **UNKNOWN** and names the setting, quoting the module's own settings path — so
    the message reads `/settings/system/windows` on Windows and
    `/settings/system/unix` on Linux:

    ```
    check_process_history_new
    UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/windows)
    ```

    `time=` sets how far back "new" reaches — `30s`, `5m`, `1h` — and defaults to
    `5m`.

    There are no default thresholds; the empty result is `OK: No new processes
    found.` Threshold on `count` to alert on *any* new process, or filter by `exe`
    to watch for a specific program starting. See also the companion
    `check_process_history` (full history rather than just recently-new).

=== "Linux"

    Check for processes first seen within a recent time window (requires 'process history = true').

    #### About `check_process_history_new`

    `check_process_history_new` reports processes that were **first seen within a
    recent time window** — i.e. processes that started (or first appeared to the
    agent) recently. It is useful for spotting unexpected launches, flapping
    services that keep restarting, or confirming that a scheduled job actually ran.

    It relies on the background **process-history collector**, which must be enabled:

    ```ini
    [/settings/system/windows]
    process history = true
    ```

    (`[/settings/system/unix]` on Linux.) Until that is set the check returns
    **UNKNOWN** and names the setting, quoting the module's own settings path — so
    the message reads `/settings/system/windows` on Windows and
    `/settings/system/unix` on Linux:

    ```
    check_process_history_new
    UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/windows)
    ```

    `time=` sets how far back "new" reaches — `30s`, `5m`, `1h` — and defaults to
    `5m`.

    There are no default thresholds; the empty result is `OK: No new processes
    found.` Threshold on `count` to alert on *any* new process, or filter by `exe`
    to watch for a specific program starting. See also the companion
    `check_process_history` (full history rather than just recently-new).

**Jump to section:**

* [Sample Commands](#check_process_history_new_samples)
* [Command-line Arguments](#check_process_history_new_options)
* [Filter keywords](#check_process_history_new_filter_keys)


<a id="check_process_history_new_samples"></a>
#### Sample Commands

**List processes first seen in the last few minutes (needs the collector):**

```
check_process_history_new
OK: No new processes found.
```

**When the collector is not enabled it tells you exactly what to set:**

The path quoted is the module's own settings path, so it reads
`/settings/system/unix` on Linux and `/settings/system/windows` on Windows:

```
check_process_history_new
UNKNOWN: Process history is not enabled (set 'process history = true' under /settings/system/unix)
```

**Widen the "recently started" window to one hour:**

```
check_process_history_new time=1h
OK: No new processes found.
```

**Alert when any new process appears (e.g. detect unexpected launches):**

```
check_process_history_new time=10m "warn=count > 0"
WARNING: /usr/bin/rogue (first seen: 1720000000)
```

**Watch for a specific executable starting:**

```
check_process_history_new "crit=exe = '/usr/bin/nmap'"
OK: No new processes found.
```



<a id="check_process_history_new_options"></a>
#### Command-line Arguments

| Option                                  | Default Value | Description                                                                                                             |
|-----------------------------------------|---------------|-------------------------------------------------------------------------------------------------------------------------|
| [time](#check_process_history_new_time) | 5m            | Time window to check for new processes (e.g., 5m, 1h, 30s). Processes first seen within this window are considered new. |



<h5 id="check_process_history_new_time">time:</h5>

Time window to check for new processes (e.g., 5m, 1h, 30s). Processes first seen within this window are considered new.

*Default Value:* `5m`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                    | Default Value                      |
|---------------------------------------------------------------------------------------------------------------------------|------------------------------------|
| <a id="check_process_history_new_filter"></a>[filter](../common-options.md#filter)                                        |                                    |
| <a id="check_process_history_new_warning"></a>[warning](../common-options.md#warning)                                     |                                    |
| <a id="check_process_history_new_warn"></a>[warn](../common-options.md#warn)                                              |                                    |
| <a id="check_process_history_new_critical"></a>[critical](../common-options.md#critical)                                  |                                    |
| <a id="check_process_history_new_crit"></a>[crit](../common-options.md#crit)                                              |                                    |
| <a id="check_process_history_new_ok"></a>[ok](../common-options.md#ok)                                                    |                                    |
| <a id="check_process_history_new_debug"></a>[debug](../common-options.md#debug)                                           | false                              |
| <a id="check_process_history_new_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                              |
| <a id="check_process_history_new_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                 |
| <a id="check_process_history_new_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                    |
| <a id="check_process_history_new_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                              |
| <a id="check_process_history_new_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                  |
| <a id="check_process_history_new_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                 |
| <a id="check_process_history_new_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): No new processes found. |
| <a id="check_process_history_new_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                    |
| <a id="check_process_history_new_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${exe} (first seen: ${first_seen}) |
| <a id="check_process_history_new_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${exe}                             |
| <a id="check_process_history_new_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                    |
| <a id="check_process_history_new_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                    |
| <a id="check_process_history_new_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                 |
| <a id="check_process_history_new_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                    |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_registry_key

*Available on Windows only.*

Check existence, last-write time, and child counts of one or more Windows registry keys.

#### About `check_registry_key`

`check_registry_key` inspects registry **keys** — whether they exist, when they
were last written, and how many values and sub-keys they hold. Use
[`check_registry_value`](#check_registry_value) when you care about the contents
of a specific value instead.

At least one `key=` is required (repeatable), given as a full path including the
hive, e.g. `HKLM\Software\MyApp`. The default critical threshold is
`not exists`, so a bare call is an existence probe: name a key and the check
goes critical if it is missing.

##### Existence as a policy check

The `exists` keyword is the reason this check earns its place: a great deal of
Windows configuration is "this key is present" or "this key is absent". Both
directions are one expression:

```
check_registry_key key=HKLM\SOFTWARE\Policies\Microsoft\Windows\WindowsUpdate
check_registry_key key=HKLM\SOFTWARE\SomeVendor "crit=exists = 1"
```

Note that `empty-state` is `unknown`, so a check that matches nothing at all
reports UNKNOWN rather than a misleading OK.

##### Change detection

`written` is the key's last-write timestamp (epoch seconds, comparable as a
date) and `age` the seconds since. Together they turn the check into a
tamper/drift probe for keys that are supposed to be stable:

```
check_registry_key key=HKLM\SYSTEM\CurrentControlSet\Services\MyService "crit=age < 1d"
```

Be aware that Windows updates a key's last-write time for changes to its
*values* as well as its sub-keys, and that the timestamp is not maintained for
every hive with the same fidelity — treat it as a strong hint rather than an
audit record.

##### Enumeration, views and remote hosts

`recursive=true` walks the sub-keys below each starting key, bounded by
`max-depth=` (`-1`, the default under `recursive`, means unlimited). Without
`recursive` only the named key itself is examined. `exclude=` drops sub-keys by
name during enumeration.

`view=` selects the registry view on 64-bit Windows: `default`, `32`
(`KEY_WOW64_32KEY`) or `64` (`KEY_WOW64_64KEY`). This matters more often than it
looks — a 32-bit installer writes under `Wow6432Node`, and a check that does not
pin the view can report "missing" for a key that is plainly there in regedit.

`computer=` connects to a remote machine's registry, which requires the Remote
Registry service to be running there and appropriate rights; running the check
locally on the monitored host is both faster and easier to secure.

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

<a id="check_registry_key_key"></a>
<a id="check_registry_key_exclude"></a>
<a id="check_registry_key_computer"></a>
<a id="check_registry_key_max-depth"></a>

| Option                                     | Default Value | Description                                                                 |
|--------------------------------------------|---------------|-----------------------------------------------------------------------------|
| key                                        |               | One or more registry key paths to check (e.g. HKLM\Software\MyApp).         |
| exclude                                    |               | Registry key names to exclude from enumeration                              |
| computer                                   |               | Remote computer to connect to (empty = local)                               |
| [view](#check_registry_key_view)           | default       | Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY) |
| [recursive](#check_registry_key_recursive) | false         | Recursively enumerate all sub-keys below each starting key                  |
| max-depth                                  |               | Maximum recursion depth (requires --recursive; -1 = unlimited)              |



<h5 id="check_registry_key_view">view:</h5>

Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)

*Default Value:* `default`

<h5 id="check_registry_key_recursive">recursive:</h5>

Recursively enumerate all sub-keys below each starting key

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                             |
|--------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------|
| <a id="check_registry_key_filter"></a>[filter](../common-options.md#filter)                                        |                                                                           |
| <a id="check_registry_key_warning"></a>[warning](../common-options.md#warning)                                     |                                                                           |
| <a id="check_registry_key_warn"></a>[warn](../common-options.md#warn)                                              |                                                                           |
| <a id="check_registry_key_critical"></a>[critical](../common-options.md#critical)                                  | not exists                                                                |
| <a id="check_registry_key_crit"></a>[crit](../common-options.md#crit)                                              |                                                                           |
| <a id="check_registry_key_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                           |
| <a id="check_registry_key_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                     |
| <a id="check_registry_key_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                     |
| <a id="check_registry_key_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                   |
| <a id="check_registry_key_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                           |
| <a id="check_registry_key_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                     |
| <a id="check_registry_key_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                         |
| <a id="check_registry_key_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                                                |
| <a id="check_registry_key_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | ${status}: All %(count) registry key(s) are ok.                           |
| <a id="check_registry_key_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | ${status}: No registry keys found                                         |
| <a id="check_registry_key_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${path}: exists=${exists}, subkeys=${subkey_count}, values=${value_count} |
| <a id="check_registry_key_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${path}                                                                   |
| <a id="check_registry_key_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                           |
| <a id="check_registry_key_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                           |
| <a id="check_registry_key_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                        |
| <a id="check_registry_key_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                           |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_registry_value

*Available on Windows only.*

Check the type, content, and size of one or more Windows registry values.

#### About `check_registry_value`

`check_registry_value` inspects registry **values** — their type, contents and
size. Use [`check_registry_key`](#check_registry_key) when you care about the
key itself rather than what is in it.

At least one `key=` is required (repeatable). `value=` restricts the check to
specific value names; omit it, or pass `value=*`, to enumerate every value in
the key. The unnamed default value is reported as `(default)`. The default
critical threshold is `not exists`, so naming a value and running the check bare
is an existence probe.

##### Reading the value

Two keywords carry the contents, and picking the right one matters:

- **`string_value`** is the rendered form and works for every type. Use it for
  `REG_SZ`, `REG_EXPAND_SZ` and `REG_MULTI_SZ`, and for matching with `like`.
- **`int_value`** is the numeric value of a `REG_DWORD` or `REG_QWORD`, and is
  `0` for every other type. That zero is a trap: a threshold like
  `crit=int_value = 0` fires on any string value too, so pair it with a `type=`
  guard when the key might hold something unexpected.

`type` compares against the registry type names (`REG_SZ`, `REG_DWORD`, …), and
`size` is the raw byte size of the data.

This is the check for verifying that a policy or product setting actually holds
the value it is supposed to:

```
check_registry_value key=HKLM\SYSTEM\CurrentControlSet\Control\Lsa value=RunAsPPL "crit=int_value != 1"
check_registry_value key=HKLM\SOFTWARE\MyApp value=LogLevel "crit=string_value != 'INFO'"
```

##### Enumeration, views and remote hosts

`recursive=true` walks values in sub-keys as well, bounded by `max-depth=`
(unlimited by default under `recursive`); `exclude=` drops value names during
enumeration. `view=` selects the 32-bit or 64-bit registry view — the usual
cause of a value that is "missing" from the check but visible in regedit — and
`computer=` reads a remote machine's registry, which needs the Remote Registry
service and rights on the target.

`empty-state` is `unknown`, so an enumeration that matches nothing reports
UNKNOWN rather than OK.

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

<a id="check_registry_value_key"></a>
<a id="check_registry_value_value"></a>
<a id="check_registry_value_exclude"></a>
<a id="check_registry_value_computer"></a>
<a id="check_registry_value_max-depth"></a>

| Option                                       | Default Value | Description                                                                            |
|----------------------------------------------|---------------|----------------------------------------------------------------------------------------|
| key                                          |               | One or more registry key paths whose values to check (e.g. HKLM\Software\MyApp)        |
| value                                        |               | Restrict to specific value names (default: all values). Supports '*' to enumerate all. |
| exclude                                      |               | Value names to exclude from enumeration                                                |
| computer                                     |               | Remote computer to connect to (empty = local)                                          |
| [view](#check_registry_value_view)           | default       | Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)            |
| [recursive](#check_registry_value_recursive) | false         | Recursively enumerate values in all sub-keys                                           |
| max-depth                                    |               | Maximum recursion depth for --recursive (-1 = unlimited)                               |



<h5 id="check_registry_value_view">view:</h5>

Registry view: 'default', '32' (KEY_WOW64_32KEY), or '64' (KEY_WOW64_64KEY)

*Default Value:* `default`

<h5 id="check_registry_value_recursive">recursive:</h5>

Recursively enumerate values in all sub-keys

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                           |
|----------------------------------------------------------------------------------------------------------------------|-----------------------------------------|
| <a id="check_registry_value_filter"></a>[filter](../common-options.md#filter)                                        |                                         |
| <a id="check_registry_value_warning"></a>[warning](../common-options.md#warning)                                     |                                         |
| <a id="check_registry_value_warn"></a>[warn](../common-options.md#warn)                                              |                                         |
| <a id="check_registry_value_critical"></a>[critical](../common-options.md#critical)                                  | not exists                              |
| <a id="check_registry_value_crit"></a>[crit](../common-options.md#crit)                                              |                                         |
| <a id="check_registry_value_ok"></a>[ok](../common-options.md#ok)                                                    |                                         |
| <a id="check_registry_value_debug"></a>[debug](../common-options.md#debug)                                           | false                                   |
| <a id="check_registry_value_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                   |
| <a id="check_registry_value_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                 |
| <a id="check_registry_value_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                         |
| <a id="check_registry_value_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                   |
| <a id="check_registry_value_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                       |
| <a id="check_registry_value_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}              |
| <a id="check_registry_value_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | ${status}: %(list).                     |
| <a id="check_registry_value_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | ${status}: No registry values found     |
| <a id="check_registry_value_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${path}: ${string_value} (type=${type}) |
| <a id="check_registry_value_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${path}                                 |
| <a id="check_registry_value_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                         |
| <a id="check_registry_value_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                         |
| <a id="check_registry_value_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                      |
| <a id="check_registry_value_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                         |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_service

Check the state of one or more of the computer services.

#### About `check_service`

`check_service` reports the state of the machine's services. `state` is
normalised across platforms so the same `warning=` / `critical=` expressions
read the same way on Windows and Linux; the platform-native fields are exposed
alongside it.

##### Windows

Enumerates the Service Control Manager. Two helper functions make the
"is this service actually fine" question expressible in a filter:

##### `state_is_ok`

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

##### `state_is_perfect`

Helper function that checks if the state of a service is "perfect". It returns `True` if the state is "perfect" and `False` otherwise.
This can be used in filter expressions to warn about services that are not running perfectly.

| Configured            | State     | Result of `state_is_perfect` |
|-----------------------|-----------|------------------------------|
| auto-start            | running   | ✅ perfect                    |
| auto-start            | stopped   | ❌ not perfect                |
| auto-start + triggers | stopped   | ✅ perfect                    |
| demand-start          | any state | ✅ perfect                    |
| disabled              | stopped   | ✅ perfect                    |

##### Linux

`check_service` inspects **systemd** units (via `systemctl show`). It
maps each unit's raw systemd state to a normalised `state` keyword so thresholds
read the same way as on Windows, and also exposes the raw systemd fields and the
main process's resource usage.

By default it looks at units that are *not* inactive
(`filter = active != 'inactive'`) and treats a unit as **critical** when it is
not in a healthy state and is not deliberately disabled:

```
critical = ( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled'
```

An `enabled` unit that has **failed** is therefore CRITICAL.

A unit that is merely **stopped**, however, never reaches that threshold: the
default filter `active != 'inactive'` excludes it before the critical expression
is evaluated. With nothing left to match, the check falls to its empty state,
which is `unknown`:

```
check_service service=nginx
UNKNOWN: No services found
```

`service=<name>` (repeatable) narrows which units are *enumerated*; it does not
bypass the filter. To alert on a unit being stopped rather than failed, widen
the filter so inactive units are considered:

```
check_service service=nginx filter=none "crit=state != 'running'"
```

`exclude=` drops units by name, and `state=` (`all`, `active`, `inactive`,
`failed`) restricts the enumeration before filtering.

**Jump to section:**

* [Sample Commands](#check_service_samples)
* [Command-line Arguments](#check_service_options)
* [Filter keywords](#check_service_filter_keys)


<a id="check_service_samples"></a>
#### Sample Commands

##### Windows

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
CRITICAL: CRITICAL: nfoo=stopped (auto), nscp=stopped (auto), nscp2=stopped (auto), ...
```

Excluding nfoo service with exclude:
```
check_service exclude=nfoo
CRITICAL: CRITICAL: nscp=stopped (auto), nscp2=stopped (auto), ...
```

Excluding nscp2 with substring like matching filter:
```
check_service exclude=nfoo "filter=name not like 'nscp'"
CRITICAL: CRITICAL: ...
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

##### Linux

**Check all services (the default watches enabled units for failures):**

```
check_service
OK: All 42 service(s) are ok.
```

**Check one service by name:**

```
check_service service=cron
OK: All 1 service(s) are ok.
```

**Show the mapped state, raw systemd state and vendor preset:**

```
check_service service=cron "top-syntax=${list}" "detail-syntax=${name}=${state} active=${active} preset=${preset}"
cron=running active=active preset=enabled
```

**A failed enabled service is CRITICAL:**

```
check_service service=nginx
CRITICAL: nginx=failed
```

**A merely stopped service is filtered out, not reported:**

The default filter is `active != 'inactive'`, so a cleanly stopped unit never
reaches the critical expression and the check falls to its empty state:

```
check_service service=nginx
UNKNOWN: No services found
```

`service=` narrows which units are enumerated; it does not bypass the filter.

**Alert on a specific service not running (stopped included):**

Widen the filter so inactive units are considered:

```
check_service service=ssh filter=none "crit=state != 'running'"
OK: All 1 service(s) are ok.
```

**Alert on a service using too much memory (process metrics):**

```
check_service service=mysql "warn=rss > 1G" "crit=rss > 2G" "detail-syntax=${name} rss=${rss} cpu=${cpu}%"
OK: All 1 service(s) are ok.
```

**Check via NRPE:**

```
check_nrpe --host 192.168.56.103 --command check_service --arguments "service=docker"
OK: All 1 service(s) are ok.
```



<a id="check_service_options"></a>
#### Command-line Arguments

=== "Windows"

    <a id="check_service_computer"></a>
    <a id="check_service_service"></a>
    <a id="check_service_exclude"></a>

    | Option                                            | Default Value | Description                                                                                                                                                                           |
    |---------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
    | computer                                          |               | The name of the remote computer to check                                                                                                                                              |
    | service                                           |               | The service to check, set this to * to check all services                                                                                                                             |
    | exclude                                           |               | A list of services to ignore (mainly useful in combination with service=*)                                                                                                            |
    | [type](#check_service_type)                       | service       | The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process                                 |
    | [state](#check_service_state)                     | all           | The types of services to enumerate available states are active, inactive or all                                                                                                       |
    | [only-essential](#check_service_only-essential)   | false         | Set filter to classification = 'essential'                                                                                                                                            |
    | [only-ignored](#check_service_only-ignored)       | false         | Set filter to classification = 'ignored'                                                                                                                                              |
    | [only-role](#check_service_only-role)             | false         | Set filter to classification = 'role'                                                                                                                                                 |
    | [only-supporting](#check_service_only-supporting) | false         | Set filter to classification = 'supporting'                                                                                                                                           |
    | [only-system](#check_service_only-system)         | false         | Set filter to classification = 'system'                                                                                                                                               |
    | [only-user](#check_service_only-user)             | false         | Set filter to classification = 'user'                                                                                                                                                 |
    | [summary](#check_service_summary)                 | false         | Emit aggregate state-count performance data (running_services/stopped_services/paused_services/pending_services/service_count) across all enumerated services, for dashboard rollups. |



    <h5 id="check_service_type">type:</h5>

    The types of services to enumerate available types are driver, file-system-driver, kernel-driver, service, service-own-process, service-share-process

    *Default Value:* `service`

    <h5 id="check_service_state">state:</h5>

    The types of services to enumerate available states are active, inactive or all

    *Default Value:* `all`

    <h5 id="check_service_only-essential">only-essential:</h5>

    Set filter to classification = 'essential'

    *Default Value:* `false`

    <h5 id="check_service_only-ignored">only-ignored:</h5>

    Set filter to classification = 'ignored'

    *Default Value:* `false`

    <h5 id="check_service_only-role">only-role:</h5>

    Set filter to classification = 'role'

    *Default Value:* `false`

    <h5 id="check_service_only-supporting">only-supporting:</h5>

    Set filter to classification = 'supporting'

    *Default Value:* `false`

    <h5 id="check_service_only-system">only-system:</h5>

    Set filter to classification = 'system'

    *Default Value:* `false`

    <h5 id="check_service_only-user">only-user:</h5>

    Set filter to classification = 'user'

    *Default Value:* `false`

    <h5 id="check_service_summary">summary:</h5>

    Emit aggregate state-count performance data (running_services/stopped_services/paused_services/pending_services/service_count) across all enumerated services, for dashboard rollups.

    *Default Value:* `false`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                                           |
    |---------------------------------------------------------------------------------------------------------------|---------------------------------------------------------|
    | <a id="check_service_filter"></a>[filter](../common-options.md#filter)                                        |                                                         |
    | <a id="check_service_warning"></a>[warning](../common-options.md#warning)                                     | not state_is_perfect()                                  |
    | <a id="check_service_warn"></a>[warn](../common-options.md#warn)                                              |                                                         |
    | <a id="check_service_critical"></a>[critical](../common-options.md#critical)                                  | not state_is_ok()                                       |
    | <a id="check_service_crit"></a>[crit](../common-options.md#crit)                                              |                                                         |
    | <a id="check_service_ok"></a>[ok](../common-options.md#ok)                                                    |                                                         |
    | <a id="check_service_debug"></a>[debug](../common-options.md#debug)                                           | false                                                   |
    | <a id="check_service_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                   |
    | <a id="check_service_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                 |
    | <a id="check_service_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                         |
    | <a id="check_service_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                   |
    | <a id="check_service_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                       |
    | <a id="check_service_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${crit_list}, delayed (${warn_list})         |
    | <a id="check_service_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) service(s) are ok.              |
    | <a id="check_service_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No services found                            |
    | <a id="check_service_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}=${state}, exit=%(exit_code), type=%(start_type) |
    | <a id="check_service_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                 |
    | <a id="check_service_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                         |
    | <a id="check_service_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                         |
    | <a id="check_service_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                      |
    | <a id="check_service_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                         |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    <a id="check_service_service"></a>
    <a id="check_service_exclude"></a>

    | Option                        | Default Value | Description                                                                |
    |-------------------------------|---------------|----------------------------------------------------------------------------|
    | service                       |               | The service to check, set this to * to check all services                  |
    | exclude                       |               | A list of services to ignore (mainly useful in combination with service=*) |
    | [state](#check_service_state) | all           | The state of services to enumerate: active, inactive, failed, or all       |



    <h5 id="check_service_state">state:</h5>

    The state of services to enumerate: active, inactive, failed, or all

    *Default Value:* `all`


    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                                                                                   |
    |---------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------|
    | <a id="check_service_filter"></a>[filter](../common-options.md#filter)                                        | active != 'inactive'                                                                            |
    | <a id="check_service_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                 |
    | <a id="check_service_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                 |
    | <a id="check_service_critical"></a>[critical](../common-options.md#critical)                                  | ( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled' |
    | <a id="check_service_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                 |
    | <a id="check_service_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                 |
    | <a id="check_service_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                           |
    | <a id="check_service_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                           |
    | <a id="check_service_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                         |
    | <a id="check_service_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                 |
    | <a id="check_service_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                           |
    | <a id="check_service_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                               |
    | <a id="check_service_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${crit_list}                                                                         |
    | <a id="check_service_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) service(s) are ok.                                                      |
    | <a id="check_service_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No services found                                                                    |
    | <a id="check_service_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}=${state}                                                                                |
    | <a id="check_service_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                                         |
    | <a id="check_service_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                 |
    | <a id="check_service_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                 |
    | <a id="check_service_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                              |
    | <a id="check_service_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                 |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


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

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option             | Description                                                                                         |
    |--------------------|-----------------------------------------------------------------------------------------------------|
    | active             | Raw systemd ActiveState (active, inactive, failed)                                                  |
    | age                | Seconds since the main process started                                                              |
    | cpu                | CPU usage of the main process in percent (lifetime average)                                         |
    | created            | Unix timestamp when the main process started                                                        |
    | desc               | Unit description                                                                                    |
    | name               | Unit (service) name                                                                                 |
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

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_swap_io

=== "Windows"

    Check system paging (swap) I/O rates: pages/bytes paged in and out per second.

    #### About `check_swap_io`

    `check_swap_io` measures how fast the system is paging to and from swap. It
    samples the underlying counters over a ~1 second window and reports the rate.
    Sustained non-zero swap I/O is a strong signal of memory pressure — often more
    actionable than swap *usage*, since a box can sit with swap full but idle, or
    with little swap used yet thrashing hard.

    The keyword vocabulary is identical on both platforms, so warning/critical
    expressions and detail-syntax port between them. There are **no default
    thresholds**: sustained paging is workload dependent, and a default would warn
    on legitimately busy hosts. Set a threshold on `swap_in` / `swap_out` (pages/s)
    or `swap_in_bytes` / `swap_out_bytes` (bytes/s) for the host in question.

    ##### Windows

    Sourced from the memory performance counters `\Memory\Pages Input/sec` and
    `\Memory\Pages Output/sec`. Windows has no per-pagefile I/O counter, so this is
    a single system-wide aggregate row.

    > Note: on Windows these are system-wide paging rates (pages moved between disk
    > and physical memory) — the correct analogue of Linux swap-in/out — not literal
    > per-pagefile read/write bytes.

    ##### Linux

    Reads `pswpin` / `pswpout` from `/proc/vmstat`. On a host with no swap
    configured the rates are simply `0`.

=== "Linux"

    Check the swap in/out paging rate.

    #### About `check_swap_io`

    `check_swap_io` measures how fast the system is paging to and from swap. It
    samples the underlying counters over a ~1 second window and reports the rate.
    Sustained non-zero swap I/O is a strong signal of memory pressure — often more
    actionable than swap *usage*, since a box can sit with swap full but idle, or
    with little swap used yet thrashing hard.

    The keyword vocabulary is identical on both platforms, so warning/critical
    expressions and detail-syntax port between them. There are **no default
    thresholds**: sustained paging is workload dependent, and a default would warn
    on legitimately busy hosts. Set a threshold on `swap_in` / `swap_out` (pages/s)
    or `swap_in_bytes` / `swap_out_bytes` (bytes/s) for the host in question.

    ##### Windows

    Sourced from the memory performance counters `\Memory\Pages Input/sec` and
    `\Memory\Pages Output/sec`. Windows has no per-pagefile I/O counter, so this is
    a single system-wide aggregate row.

    > Note: on Windows these are system-wide paging rates (pages moved between disk
    > and physical memory) — the correct analogue of Linux swap-in/out — not literal
    > per-pagefile read/write bytes.

    ##### Linux

    Reads `pswpin` / `pswpout` from `/proc/vmstat`. On a host with no swap
    configured the rates are simply `0`.

**Jump to section:**

* [Sample Commands](#check_swap_io_samples)
* [Command-line Arguments](#check_swap_io_options)
* [Filter keywords](#check_swap_io_filter_keys)


<a id="check_swap_io_samples"></a>
#### Sample Commands

**Default check (current paging rate):**

```
check_swap_io
OK: 1 page file(s), in 0 pages/s, out 0 pages/s|'io_swap_in'=0;;; 'io_swap_out'=0;;; 'io_swap_in_bytes'=0B;;; 'io_swap_out_bytes'=0B;;;
```

On Linux the same call names swap devices rather than page files:

```
check_swap_io
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s|'io_swap_in'=0;0;0 'io_swap_out'=0;0;0 'io_swap_in_bytes'=0;0;0 'io_swap_out_bytes'=0;0;0
```

**Alert on sustained paging (pages/s):**

```
check_swap_io "warn=swap_in > 1000" "crit=swap_in > 5000"
OK: 1 page file(s), in 42 pages/s, out 7 pages/s|'io_swap_in'=42;1000;5000; 'io_swap_out'=7;;; 'io_swap_in_bytes'=172032B;;; 'io_swap_out_bytes'=28672B;;;
```

**Alert in either direction:**

```
check_swap_io "warn=swap_in > 100 or swap_out > 100" "crit=swap_in > 1000 or swap_out > 1000"
OK: 1 swap device(s) in 0 pages/s, out 0 pages/s
```

**Threshold on throughput (bytes/s) with a custom output line:**

```
check_swap_io "crit=swap_out_bytes > 10485760" "detail-syntax=in ${swap_in_bytes}B/s, out ${swap_out_bytes}B/s"
OK: in 172032B/s, out 28672B/s|'io_swap_in_bytes'=172032B;;; 'io_swap_out_bytes'=28672B;;10485760;
```



<a id="check_swap_io_options"></a>
#### Command-line Arguments

=== "Windows"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                                                              |
    |---------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|
    | <a id="check_swap_io_filter"></a>[filter](../common-options.md#filter)                                        |                                                                            |
    | <a id="check_swap_io_warning"></a>[warning](../common-options.md#warning)                                     |                                                                            |
    | <a id="check_swap_io_warn"></a>[warn](../common-options.md#warn)                                              |                                                                            |
    | <a id="check_swap_io_critical"></a>[critical](../common-options.md#critical)                                  |                                                                            |
    | <a id="check_swap_io_crit"></a>[crit](../common-options.md#crit)                                              |                                                                            |
    | <a id="check_swap_io_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                            |
    | <a id="check_swap_io_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                      |
    | <a id="check_swap_io_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                      |
    | <a id="check_swap_io_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                                    |
    | <a id="check_swap_io_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                            |
    | <a id="check_swap_io_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                      |
    | <a id="check_swap_io_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                          |
    | <a id="check_swap_io_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                         |
    | <a id="check_swap_io_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                            |
    | <a id="check_swap_io_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                            |
    | <a id="check_swap_io_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${swap_count} page file(s), in ${swap_in} pages/s, out ${swap_out} pages/s |
    | <a id="check_swap_io_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | io                                                                         |
    | <a id="check_swap_io_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                            |
    | <a id="check_swap_io_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                            |
    | <a id="check_swap_io_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                         |
    | <a id="check_swap_io_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                            |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                        | Default Value                                                               |
    |---------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------|
    | <a id="check_swap_io_filter"></a>[filter](../common-options.md#filter)                                        |                                                                             |
    | <a id="check_swap_io_warning"></a>[warning](../common-options.md#warning)                                     |                                                                             |
    | <a id="check_swap_io_warn"></a>[warn](../common-options.md#warn)                                              |                                                                             |
    | <a id="check_swap_io_critical"></a>[critical](../common-options.md#critical)                                  |                                                                             |
    | <a id="check_swap_io_crit"></a>[crit](../common-options.md#crit)                                              |                                                                             |
    | <a id="check_swap_io_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                             |
    | <a id="check_swap_io_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                       |
    | <a id="check_swap_io_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                       |
    | <a id="check_swap_io_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                                     |
    | <a id="check_swap_io_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                             |
    | <a id="check_swap_io_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                       |
    | <a id="check_swap_io_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                           |
    | <a id="check_swap_io_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                          |
    | <a id="check_swap_io_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                             |
    | <a id="check_swap_io_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                                                             |
    | <a id="check_swap_io_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${swap_count} swap device(s) in ${swap_in} pages/s, out ${swap_out} pages/s |
    | <a id="check_swap_io_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | io                                                                          |
    | <a id="check_swap_io_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                             |
    | <a id="check_swap_io_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                             |
    | <a id="check_swap_io_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                          |
    | <a id="check_swap_io_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                             |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_swap_io_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option         | Description                                                                                        |
    |----------------|----------------------------------------------------------------------------------------------------|
    | name           | Always 'swap' (single aggregate row)                                                               |
    | swap_count     | Number of page files on the system                                                                 |
    | swap_in        | Pages paged in from disk per second (perfdata io_swap_in)                                          |
    | swap_in_bytes  | Bytes paged in per second — swap_in multiplied by the system page size (perfdata io_swap_in_bytes) |
    | swap_out       | Pages paged out to disk per second (perfdata io_swap_out)                                          |
    | swap_out_bytes | Bytes paged out per second (perfdata io_swap_out_bytes)                                            |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option         | Description                                      |
    |----------------|--------------------------------------------------|
    | name           | Always 'swap' (single aggregate row)             |
    | swap_count     | Number of active swap devices                    |
    | swap_in        | Pages swapped in per second                      |
    | swap_in_bytes  | Bytes swapped in per second (pages x page size)  |
    | swap_out       | Pages swapped out per second                     |
    | swap_out_bytes | Bytes swapped out per second (pages x page size) |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_temperature

=== "Windows"

    Check ACPI thermal zone temperatures.

    #### About `check_temperature`

    `check_temperature` reports thermal sensor readings, one record per zone or
    sensor, in degrees Celsius. The defaults are `temperature > 70` (warning) and
    `temperature > 90` (critical).

    ##### Sensor availability is the main caveat

    Thermal sensors are hardware- and driver-dependent, and a great deal of the
    infrastructure this agent runs on does not expose any:

    - **Virtual machines** almost never present thermal zones — the hypervisor owns
      the hardware.
    - **Cloud instances** likewise.
    - On Linux, readings come from the kernel's thermal zones and hwmon sensors
      (`/sys/class/thermal`, `/sys/class/hwmon`), so a sensor needs a loaded driver
      to appear. `sensors-detect` from `lm-sensors` is the usual way to find out
      what a given box can report.
    - On Windows, ACPI thermal zones are read through WMI, and many vendors expose
      either nothing or a single coarse zone rather than per-component sensors.

    A host with no readable sensors does not fall through to the filter's empty
    state at all: the check returns **`UNKNOWN: No temperature sensors found`**
    before filtering. So on a VM this check is permanently UNKNOWN rather than
    quietly OK — which is honest, but means it should only be enabled where the
    hardware actually reports something.

    ##### Naming is not portable

    `name` is whatever the platform calls the zone — `thermal_zone0`,
    `coretemp Package id 0`, `TZ00` — and it differs between machines, vendors and
    kernel versions. Do not hard-code a sensor name in a fleet-wide check; threshold
    across all of them and use `detail-syntax` to identify the offender in the
    message.

    A sensible fleet-wide shape is a generous threshold on everything, since the
    absolute numbers vary a lot between a CPU package sensor and a chassis sensor:

    ```
    check_temperature "warn=temperature > 75" "crit=temperature > 90" "detail-syntax=${name}=${temperature}C"
    ```

    `active` reports whether the zone is currently active; on Windows
    `throttle_reasons` carries the ACPI throttle bitmask, which is a more direct
    signal that thermal limits are actually biting than the temperature alone.

=== "Linux"

    Check temperature sensors (thermal zones / hwmon).

    #### About `check_temperature`

    `check_temperature` reports thermal sensor readings, one record per zone or
    sensor, in degrees Celsius. The defaults are `temperature > 70` (warning) and
    `temperature > 90` (critical).

    ##### Sensor availability is the main caveat

    Thermal sensors are hardware- and driver-dependent, and a great deal of the
    infrastructure this agent runs on does not expose any:

    - **Virtual machines** almost never present thermal zones — the hypervisor owns
      the hardware.
    - **Cloud instances** likewise.
    - On Linux, readings come from the kernel's thermal zones and hwmon sensors
      (`/sys/class/thermal`, `/sys/class/hwmon`), so a sensor needs a loaded driver
      to appear. `sensors-detect` from `lm-sensors` is the usual way to find out
      what a given box can report.
    - On Windows, ACPI thermal zones are read through WMI, and many vendors expose
      either nothing or a single coarse zone rather than per-component sensors.

    A host with no readable sensors does not fall through to the filter's empty
    state at all: the check returns **`UNKNOWN: No temperature sensors found`**
    before filtering. So on a VM this check is permanently UNKNOWN rather than
    quietly OK — which is honest, but means it should only be enabled where the
    hardware actually reports something.

    ##### Naming is not portable

    `name` is whatever the platform calls the zone — `thermal_zone0`,
    `coretemp Package id 0`, `TZ00` — and it differs between machines, vendors and
    kernel versions. Do not hard-code a sensor name in a fleet-wide check; threshold
    across all of them and use `detail-syntax` to identify the offender in the
    message.

    A sensible fleet-wide shape is a generous threshold on everything, since the
    absolute numbers vary a lot between a CPU package sensor and a chassis sensor:

    ```
    check_temperature "warn=temperature > 75" "crit=temperature > 90" "detail-syntax=${name}=${temperature}C"
    ```

    `active` reports whether the zone is currently active; on Windows
    `throttle_reasons` carries the ACPI throttle bitmask, which is a more direct
    signal that thermal limits are actually biting than the temperature alone.

**Jump to section:**

* [Sample Commands](#check_temperature_samples)
* [Command-line Arguments](#check_temperature_options)
* [Filter keywords](#check_temperature_filter_keys)


<a id="check_temperature_samples"></a>
#### Sample Commands

**A host with no readable sensors:**

Most VMs and cloud instances expose no thermal zones at all. The check returns
UNKNOWN before the filter runs, rather than a misleading OK.

```
check_temperature
UNKNOWN: No temperature sensors found
```

**Default check on hardware that does report (`> 70` warns, `> 90` is critical):**

```
check_temperature
OK: thermal_zone0: 42 C, thermal_zone1: 38 C|'thermal_zone0'=42;70;90 'thermal_zone1'=38;70;90
```

**Fleet-wide thresholds:**

Sensor names differ between machines, vendors and kernel versions, so threshold
across all of them and let `detail-syntax` name the offender rather than
hard-coding a zone.

```
check_temperature "warn=temperature > 75" "crit=temperature > 90" "detail-syntax=${name}=${temperature}C"
CRITICAL: coretemp Package id 0=94C
```

**Watch one specific sensor:**

```
check_temperature "filter=name like 'Package'" "crit=temperature > 85"
OK: coretemp Package id 0: 61 C
```

**Only the zones that are currently active:**

```
check_temperature "filter=active = 1" "detail-syntax=${name}=${temperature}C"
OK: thermal_zone0=42C
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_temperature --arguments "crit=temperature > 90"
OK: thermal_zone0: 42 C, thermal_zone1: 38 C
```



<a id="check_temperature_options"></a>
#### Command-line Arguments

=== "Windows"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                            | Default Value                         |
    |-------------------------------------------------------------------------------------------------------------------|---------------------------------------|
    | <a id="check_temperature_filter"></a>[filter](../common-options.md#filter)                                        |                                       |
    | <a id="check_temperature_warning"></a>[warning](../common-options.md#warning)                                     | temperature > 70                      |
    | <a id="check_temperature_warn"></a>[warn](../common-options.md#warn)                                              |                                       |
    | <a id="check_temperature_critical"></a>[critical](../common-options.md#critical)                                  | temperature > 90                      |
    | <a id="check_temperature_crit"></a>[crit](../common-options.md#crit)                                              |                                       |
    | <a id="check_temperature_ok"></a>[ok](../common-options.md#ok)                                                    |                                       |
    | <a id="check_temperature_debug"></a>[debug](../common-options.md#debug)                                           | false                                 |
    | <a id="check_temperature_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                 |
    | <a id="check_temperature_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | critical                              |
    | <a id="check_temperature_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                       |
    | <a id="check_temperature_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                 |
    | <a id="check_temperature_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                     |
    | <a id="check_temperature_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                    |
    | <a id="check_temperature_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All thermal zones seem ok. |
    | <a id="check_temperature_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                       |
    | <a id="check_temperature_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${temperature} C             |
    | <a id="check_temperature_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                               |
    | <a id="check_temperature_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                       |
    | <a id="check_temperature_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                       |
    | <a id="check_temperature_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                    |
    | <a id="check_temperature_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                       |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.

=== "Linux"

    **Common options:**

    These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


    | Option                                                                                                            | Default Value                 |
    |-------------------------------------------------------------------------------------------------------------------|-------------------------------|
    | <a id="check_temperature_filter"></a>[filter](../common-options.md#filter)                                        |                               |
    | <a id="check_temperature_warning"></a>[warning](../common-options.md#warning)                                     | temperature > 70              |
    | <a id="check_temperature_warn"></a>[warn](../common-options.md#warn)                                              |                               |
    | <a id="check_temperature_critical"></a>[critical](../common-options.md#critical)                                  | temperature > 90              |
    | <a id="check_temperature_crit"></a>[crit](../common-options.md#crit)                                              |                               |
    | <a id="check_temperature_ok"></a>[ok](../common-options.md#ok)                                                    |                               |
    | <a id="check_temperature_debug"></a>[debug](../common-options.md#debug)                                           | false                         |
    | <a id="check_temperature_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                         |
    | <a id="check_temperature_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | critical                      |
    | <a id="check_temperature_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                               |
    | <a id="check_temperature_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                         |
    | <a id="check_temperature_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                             |
    | <a id="check_temperature_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}            |
    | <a id="check_temperature_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): Temperature is ok. |
    | <a id="check_temperature_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                               |
    | <a id="check_temperature_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${temperature}C      |
    | <a id="check_temperature_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                       |
    | <a id="check_temperature_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                               |
    | <a id="check_temperature_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                               |
    | <a id="check_temperature_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                            |
    | <a id="check_temperature_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                               |


    This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_temperature_filter_keys"></a>
#### Filter keywords

=== "Windows"

    | Option           | Description                        |
    |------------------|------------------------------------|
    | active           | True if the thermal zone is active |
    | name             | Thermal zone name                  |
    | temperature      | Temperature in degrees Celsius     |
    | throttle_reasons | Throttle reasons bitmask           |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

=== "Linux"

    | Option      | Description                    |
    |-------------|--------------------------------|
    | active      | Whether the sensor is active   |
    | name        | Thermal zone / sensor name     |
    | temperature | Temperature in degrees Celsius |

    This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_uptime

Check time since last server re-boot.

#### About `check_uptime`

`check_uptime` reports how long the machine has been running since its last
boot, as a single aggregate row.

The defaults invert the usual reading of "uptime": the check warns when uptime
is **less** than 2 days and goes critical below 1 day. That is deliberate — a
low uptime means the machine has just rebooted, which is the event worth
alerting on. A high uptime is only a problem if your patching policy makes it
one, in which case invert the comparison:

```
check_uptime "warn=uptime > 90d" "crit=uptime > 180d"
```

`uptime` accepts units, so thresholds are written the way you think about them
(`2d`, `12h`, `90d`) rather than in raw seconds.

##### Rendering the duration

`max-unit=` controls the largest unit `${uptime}` is rendered in — `s`, `m`,
`h`, `d` or `w`, defaulting to `w`. For a six-week uptime, `w` renders
`6w 0d 00:00`, `d` renders `42d 00:00` and `h` renders `1008:00`. Pick whichever
reads best for the audience; it affects only the rendered string, never the
comparisons.

##### Boot time and timezone

`boot` is the wall-clock time the machine came up, derived as *now minus
uptime*, and `${tz}` renders the timezone label it is expressed in. Both follow
the module's configured timezone (default `local`), so the boot time in the
message matches the clock an operator is reading it against.

The same duration formatting and unit handling is shared with `check_nscp`'s
`uptime` and `crash_age` keywords, so thresholds written for one read the same
way in the other.

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

| Option                             | Default Value | Description                                                                                                                              |
|------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------|
| [max-unit](#check_uptime_max-unit) | w             | Largest time unit used to render ${uptime}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'. |



<h5 id="check_uptime_max-unit">max-unit:</h5>

Largest time unit used to render ${uptime}: s|m|h|d|w (default: w). For a 6-week uptime, w=>'6w 0d 00:00', d=>'42d 00:00', h=>'1008:00'.

*Default Value:* `w`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                       | Default Value                             |
|--------------------------------------------------------------------------------------------------------------|-------------------------------------------|
| <a id="check_uptime_filter"></a>[filter](../common-options.md#filter)                                        |                                           |
| <a id="check_uptime_warning"></a>[warning](../common-options.md#warning)                                     | uptime < 2d                               |
| <a id="check_uptime_warn"></a>[warn](../common-options.md#warn)                                              |                                           |
| <a id="check_uptime_critical"></a>[critical](../common-options.md#critical)                                  | uptime < 1d                               |
| <a id="check_uptime_crit"></a>[crit](../common-options.md#crit)                                              |                                           |
| <a id="check_uptime_ok"></a>[ok](../common-options.md#ok)                                                    |                                           |
| <a id="check_uptime_debug"></a>[debug](../common-options.md#debug)                                           | false                                     |
| <a id="check_uptime_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                     |
| <a id="check_uptime_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                   |
| <a id="check_uptime_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                           |
| <a id="check_uptime_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                     |
| <a id="check_uptime_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                         |
| <a id="check_uptime_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                        |
| <a id="check_uptime_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                           |
| <a id="check_uptime_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                           |
| <a id="check_uptime_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | uptime: ${uptime}h, boot: ${boot} (${tz}) |
| <a id="check_uptime_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | uptime                                    |
| <a id="check_uptime_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                           |
| <a id="check_uptime_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                           |
| <a id="check_uptime_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                        |
| <a id="check_uptime_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                           |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_uptime_filter_keys"></a>
#### Filter keywords

| Option | Description          |
|--------|----------------------|
| boot   | System boot time     |
| uptime | Time since last boot |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_w32time

*Available on Windows only.*

Check the Windows Time service: whether the machine is following a time source at all, which one, the computed clock offset and the configured peers.

#### About `check_w32time`

`check_w32time` reports what the Windows Time service (W32Time) itself thinks:
whether the machine is following a time source at all, which source that is, how
far the clock was last computed to be off and which peers are configured. This
is the inside-out counterpart to CheckNet's `check_ntp_offset`, which probes an
NTP server from the outside: a domain member whose time hierarchy has broken
keeps answering with a plausible clock for hours while Kerberos ticket
validation is already on its way to failing, and only the service's own view
shows it.

The data is assembled from four places:

| Source | What it gives |
|---|---|
| Service control manager | Whether W32Time exists, is running, and how it starts. |
| `HKLM\SYSTEM\CurrentControlSet\Services\W32Time\Parameters` | `Type` (the synchronization mode) and `NtpServer` (the configured peers). |
| `HKLM\SYSTEM\CurrentControlSet\Services\W32Time\Config\LastKnownGoodTime` | When the service last recorded the clock as good. |
| `W32TimeQuerySource` (w32time.dll) | The source the running service is actually following. Like `w32tm /query /source`, this needs privilege: the agent has it running as a service, an unprivileged caller gets access denied and the check falls back to the configured peers. |
| "Windows Time Service" PDH counters | Computed time offset, NTP round trip delay, clock frequency adjustment and the number of time sources in use. |

The counter-backed keywords — `offset`, `delay`, `frequency_adjustment`,
`time_sources` and `last_sync_age` — come from counters the service only
maintains while it runs. When
there is no measurement they render as `unknown`, compare false against every
number (so a threshold like `offset > 1000` cannot fire on a missing value) and
emit no perfdata. Test for the absence explicitly with `offset = 'unknown'`.

`synchronized` ranks its evidence rather than guessing. The service not running
or `Type=NoSync` settles it on its own. Otherwise, when the service could be
asked what it follows, that answer decides — the local clock means
unsynchronized, anything else means synchronized. When it could not be asked,
`time_sources = 0` (no time source in use) decides instead. With neither piece
of evidence the check reports the configured intent and does not raise an alarm,
so a host where the counters are unavailable does not alert forever.

Default thresholds: **critical** when `synchronized = 0 or offset > 30000` and
**warning** when `offset > 1000`. The critical is the important one — it fires
when the machine follows no time source at all, whether because the service is
not running, because `Type` is `NoSync`, or because it has fallen back to its
own clock. Kerberos rejects tickets once the clock is five minutes out, so the
30-second critical leaves room to act.

On a **workgroup** machine Windows trigger-starts W32Time and stops it again
between synchronizations, so `running` is 0 most of the time and the default
critical fires by design. Check the configuration and the age of the last good
synchronization there instead, e.g.
`check_w32time "critical=sync_type = 'NoSync'" "warning=last_sync_age > 604800"`.
On a server or domain member the service is expected to run continuously and the
defaults apply as they are. **Windows only.**

**Jump to section:**

* [Sample Commands](#check_w32time_samples)
* [Command-line Arguments](#check_w32time_options)
* [Filter keywords](#check_w32time_filter_keys)


<a id="check_w32time_samples"></a>
#### Sample Commands

**Check that the machine is following a time source (Windows)**

The default is critical when the machine is not synchronizing at all and warning
once the computed offset passes one second.

```
check_w32time
L        cli OK: synchronizing with dc01.corp.example.com (offset 3ms)|'w32time_offset'=3ms;1000;30000
```

```
check_w32time
L        cli CRITICAL: the Windows Time service is stopped (start type demand)
```

```
check_w32time
L        cli CRITICAL: not synchronizing: falling back to Local CMOS Clock
```

```
check_w32time
L        cli CRITICAL: not synchronizing: no time source in use (configured: time.windows.com)|'w32time_offset'=0ms;1000;30000
```

**Show the service state, configuration and source**

```
check_w32time warning=none critical=none "top-syntax=${status}: ${list}" "detail-syntax=svc=${service_state}/${start_type} type=${sync_type} src=${source} (${source_from}) peers=${peers}"
L        cli OK: svc=stopped/demand type=NTP src=time.windows.com (configuration) peers=time.windows.com
```

`source_from` says where the source came from: `service` when the running
service was asked what it is actually following, `configuration` when it could
not be asked and the configured peers are shown instead. The verdict is worded
to match — "synchronizing with X" only when the service confirmed it, and
"configured to synchronize with X" when that is all we know.

```
check_w32time warning=none critical=none "top-syntax=${list}" "detail-syntax=src=[${source}] from=${source_from} local=${local_clock} sync=${synchronized} srcs=${time_sources} off=${offset} delay=${delay}"
L        cli src=[time.windows.com] from=configuration local=0 sync=0 srcs=0 off=0 delay=31
```

**Watch a domain member's time hierarchy**

`local_clock` is the signal that a domain member has lost its hierarchy and is
free-running: it keeps answering, but its clock is no longer anchored to
anything, which breaks Kerberos once it drifts past five minutes.

```
check_w32time "critical=local_clock = 1 or sync_type = 'NoSync' or running = 0"
L        cli OK: synchronizing with dc01.corp.example.com (offset 12ms)
```

**Alert on drift only**

```
check_w32time "warning=offset > 500" "critical=offset > 5000"
L        cli WARNING: synchronizing with time.windows.com (offset 812ms)|'w32time_offset'=812ms;500;5000
```

**Report how long ago the clock was last validated**

```
check_w32time "warning=last_sync_age > 86400" "critical=none" "top-syntax=${status}: ${list}" "detail-syntax=last sync ${last_sync} (${last_sync_age}s ago)"
L        cli OK: last sync 2026-08-15 21:28:41 (49654s ago)|'w32time_last_sync'=49654s;86400;0
```

**Values the service has not measured read as `unknown`**

The "Windows Time Service" counters only carry data while the service is
running; until then `offset`, `delay`, `frequency_adjustment` and `time_sources`
render as `unknown`, compare false against every number and emit no perfdata.

```
check_w32time "warning=none" "critical=none" "top-syntax=${list}" "detail-syntax=off=${offset} delay=${delay} freq=${frequency_adjustment} srcs=${time_sources}"
L        cli off=unknown delay=unknown freq=unknown srcs=unknown
```

```
check_w32time "critical=offset = 'unknown'"
L        cli CRITICAL: the Windows Time service is stopped (start type demand)
```

**A workgroup client, where W32Time is trigger-started**

Windows starts the time service on demand on a machine that is not domain
joined, so it is stopped most of the time. Check the configuration and the age
of the last good synchronization there instead of the service state.

```
check_w32time "critical=sync_type = 'NoSync'" "warning=last_sync_age > 604800"
L        cli OK: the Windows Time service is stopped (start type demand)|'w32time_last_sync'=50036s;604800;0
```



<a id="check_w32time_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                        | Default Value                      |
|---------------------------------------------------------------------------------------------------------------|------------------------------------|
| <a id="check_w32time_filter"></a>[filter](../common-options.md#filter)                                        |                                    |
| <a id="check_w32time_warning"></a>[warning](../common-options.md#warning)                                     | offset > 1000                      |
| <a id="check_w32time_warn"></a>[warn](../common-options.md#warn)                                              |                                    |
| <a id="check_w32time_critical"></a>[critical](../common-options.md#critical)                                  | synchronized = 0 or offset > 30000 |
| <a id="check_w32time_crit"></a>[crit](../common-options.md#crit)                                              |                                    |
| <a id="check_w32time_ok"></a>[ok](../common-options.md#ok)                                                    |                                    |
| <a id="check_w32time_debug"></a>[debug](../common-options.md#debug)                                           | false                              |
| <a id="check_w32time_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                              |
| <a id="check_w32time_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                            |
| <a id="check_w32time_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                    |
| <a id="check_w32time_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                              |
| <a id="check_w32time_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                  |
| <a id="check_w32time_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                 |
| <a id="check_w32time_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                    |
| <a id="check_w32time_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      |                                    |
| <a id="check_w32time_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${state}                           |
| <a id="check_w32time_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | w32time                            |
| <a id="check_w32time_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                    |
| <a id="check_w32time_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                    |
| <a id="check_w32time_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                 |
| <a id="check_w32time_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                    |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_w32time_filter_keys"></a>
#### Filter keywords

| Option               | Description                                                                                                                                                                                                           |
|----------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| delay                | NTP round trip delay to the time source in milliseconds                                                                                                                                                               |
| frequency_adjustment | Correction the service applies to the clock frequency, in parts per billion (negative slows the clock down)                                                                                                           |
| installed            | True when the W32Time service exists on this host                                                                                                                                                                     |
| last_sync            | Time of the last known good synchronization, in UTC, or 'unknown'                                                                                                                                                     |
| last_sync_age        | Seconds since the last synchronization W32Time recorded as good; threshold with durations, e.g. last_sync_age > 24h                                                                                                   |
| local_clock          | True when the source is the machine's own clock (Local CMOS Clock / free-running)                                                                                                                                     |
| offset               | Absolute clock offset against the time source in milliseconds, as last computed by the service; 'unknown' until it has measured one (`offset = 'unknown'` tests for it)                                               |
| peer_count           | Number of configured NTP peers                                                                                                                                                                                        |
| peers                | Configured NTP peers, comma separated (empty on a domain member, which discovers its source)                                                                                                                          |
| running              | True when the W32Time service is running                                                                                                                                                                              |
| service_state        | State of the W32Time service: running, stopped, starting, ... or 'not installed'                                                                                                                                      |
| source               | The time source in use; the configured peers when the service could not be asked (see source_from)                                                                                                                    |
| source_from          | Where source came from: 'service' (live), 'configuration' or 'unknown'                                                                                                                                                |
| start_type           | Start type of the W32Time service: auto, delayed, demand, disabled, ...                                                                                                                                               |
| state                | One line verdict: not installed, not running, NoSync, falling back to the local clock or synchronizing with a source                                                                                                  |
| sync_type            | Configured synchronization type: NT5DS (domain hierarchy), NTP, AllSync or NoSync                                                                                                                                     |
| synchronized         | True when the machine is following a time source: the service runs, synchronization is not turned off, the source is not the local clock and - when the source could not be read - at least one time source is in use |
| time_sources         | Number of NTP time sources the client is currently using                                                                                                                                                              |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                                                          | Description              |
|-------------------------------------------------------------------------|--------------------------|
| [/settings/default](#default-values)                                    | Default values           |
| [/settings/system/unix](#unix-system)                                   | Unix system              |
| [/settings/system/unix/real-time/cpu](#realtime-cpu-filters)            | Realtime cpu filters     |
| [/settings/system/unix/real-time/memory](#realtime-memory-filters)      | Realtime memory filters  |
| [/settings/system/unix/real-time/process](#realtime-process-filters)    | Realtime process filters |
| [/settings/system/unix/service-tags](#service-tags)                     | Service tags             |
| [/settings/system/windows](#windows-system)                             | Windows system           |
| [/settings/system/windows/counters](#pdh-counters)                      | PDH Counters             |
| [/settings/system/windows/real-time/checks](#legacy-generic-filters)    | Legacy generic filters   |
| [/settings/system/windows/real-time/cpu](#realtime-cpu-filters)         | Realtime cpu filters     |
| [/settings/system/windows/real-time/memory](#realtime-memory-filters)   | Realtime memory filters  |
| [/settings/system/windows/real-time/process](#realtime-process-filters) | Realtime process filters |
| [/settings/system/windows/service-tags](#service-tags)                  | Service tags             |


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


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| time                |                           | TIME                |
| times               |                           | TIMES               |
| top syntax          |                           | SYNTAX              |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Realtime cpu filters section
[/settings/system/unix/real-time/cpu/sample]
#byte unit=...
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
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


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| top syntax          |                           | SYNTAX              |
| type                |                           | MEMORY TYPE         |
| types               |                           | MEMORY TYPES        |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Realtime memory filters section
[/settings/system/unix/real-time/memory/sample]
#byte unit=...
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
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


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| process             |                           | PROCESS             |
| processes           |                           | PROCESSES           |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| top syntax          |                           | SYNTAX              |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Realtime process filters section
[/settings/system/unix/real-time/process/sample]
#byte unit=...
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#process=...
#processes=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
#top syntax=...
#warning=...

```






### Service tags <a id="/settings/system/unix/service-tags"></a>

*Available on Linux only.*


Systemd units to surface as host tags: each key is a unit name and each value the tag to publish. When the unit exists and is active the tag is published as <tag>=enabled (removed otherwise). Example: postgresql=postgres


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### Windows system <a id="/settings/system/windows"></a>

*Available on Windows only.*


Section for system checks and system settings

| Key                                           | Default Value | Description               |
|-----------------------------------------------|---------------|---------------------------|
| [default buffer length](#default-buffer-time) | 1h            | Default buffer time       |
| [disable](#disable-automatic-checks)          |               | Disable automatic checks  |
| [fetch core loads](#fetch-core-load)          | true          | Fetch core load           |
| [process cpu](#sample-per-process-cpu)        | false         | Sample per-process CPU    |
| [process history](#track-process-history)     | false         | Track process history     |
| [subsystem](#pdh-subsystem)                   | default       | PDH subsystem             |
| [timezone](#timezone)                         | local         | Timezone                  |
| [use pdh for cpu](#use-pdh-to-fetch-cpu-load) | false         | Use PDH to fetch CPU load |


```ini
# Section for system checks and system settings
[/settings/system/windows]
default buffer length=1h
fetch core loads=true
process cpu=false
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

A comma separated list of checks to disable in the collector: battery,cpu,handles,load,network,temperature,cpu_frequency,os_updates,metrics,pdh. Please note disabling these will mean part of NSClient++ will no longer function as expected.


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

#### Sample per-process CPU <a id="/settings/system/windows/process cpu"></a>

Sample per-process CPU usage once a second in the background so that 'check_process delta=true' can report CPU% without stalling the check for a second. Off by default (adds one system-process-table query per second); required for the delta=true CPU fields.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/system/windows](#/settings/system/windows) |
| Key:           | process cpu                                           |
| Default value: | `false`                                               |


**Sample:**

```
[/settings/system/windows]
# Sample per-process CPU
process cpu=false
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
| resolution          |               | COUNTER RESOLUTION  |
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
#resolution=...
#type=...

```



**Known instances:**

*  disk_queue_length
*  memory_pages_sec







### Legacy generic filters <a id="/settings/system/windows/real-time/checks"></a>

*Available on Windows only.*


A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| check               | cpu                       | TYPE OF CHECK       |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| time                |                           | TIME                |
| times               |                           | FILES               |
| top syntax          |                           | SYNTAX              |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Legacy generic filters section
[/settings/system/windows/real-time/checks/sample]
#byte unit=...
check=cpu
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
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


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| time                |                           | TIME                |
| top syntax          |                           | SYNTAX              |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Realtime cpu filters section
[/settings/system/windows/real-time/cpu/sample]
#byte unit=...
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
#time=...
#top syntax=...
#warning=...

```






### Realtime memory filters <a id="/settings/system/windows/real-time/memory"></a>

*Available on Windows only.*


A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| top syntax          |                           | SYNTAX              |
| type                |                           | MEMORY TYPE         |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Realtime memory filters section
[/settings/system/windows/real-time/memory/sample]
#byte unit=...
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
#top syntax=...
#type=...
#warning=...

```






### Realtime process filters <a id="/settings/system/windows/real-time/process"></a>

*Available on Windows only.*


A set of filters to use in real-time mode


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value             | Description         |
|---------------------|---------------------------|---------------------|
| byte unit           |                           | BYTE UNIT           |
| command             |                           | COMMAND NAME        |
| critical            |                           | CRITICAL FILTER     |
| debug               |                           | DEBUG               |
| decimal separator   |                           | DECIMAL SEPARATOR   |
| decimals            | -1                        | DECIMALS            |
| destination         |                           | DESTINATION         |
| detail syntax       |                           | SYNTAX              |
| empty message       | eventlog found no records | EMPTY MESSAGE       |
| escape html         |                           | ESCAPE HTML         |
| filter              |                           | FILTER              |
| list separator      |                           | LIST SEPARATOR      |
| maximum age         | 5m                        | MAXIMUM AGE         |
| ok                  |                           | OK FILTER           |
| ok syntax           |                           | SYNTAX              |
| perf config         |                           | PERF CONFIG         |
| process             |                           | PROCESS             |
| run on startup      |                           | RUN ON STARTUP      |
| severity            |                           | SEVERITY            |
| silent period       | false                     | Silent period       |
| source id           |                           | SOURCE ID           |
| target              |                           | DESTINATION         |
| target id           |                           | TARGET ID           |
| thousands separator |                           | THOUSANDS SEPARATOR |
| top syntax          |                           | SYNTAX              |
| warning             |                           | WARNING FILTER      |


**Sample:**

```ini
# An example of a Realtime process filters section
[/settings/system/windows/real-time/process/sample]
#byte unit=...
#command=...
#critical=...
#debug=...
#decimal separator=...
decimals=-1
#destination=...
#detail syntax=...
empty message=eventlog found no records
#escape html=...
#filter=...
#list separator=...
maximum age=5m
#ok=...
#ok syntax=...
#perf config=...
#process=...
#run on startup=...
#severity=...
silent period=false
#source id=...
#target=...
#target id=...
#thousands separator=...
#top syntax=...
#warning=...

```






### Service tags <a id="/settings/system/windows/service-tags"></a>

*Available on Windows only.*


Windows services to surface as host tags: each key is a service name and each value the tag to publish. When the service exists and is running the tag is published as <tag>=enabled (removed otherwise). Example: MSSQLSERVER=sql-server


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.





