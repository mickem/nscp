# CheckHyperV

*Available on Windows only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

Checks for a Hyper-V host: hypervisor health and capacity, logical processor load and the state, heartbeat, memory and snapshots of every virtual machine.

## Enable module

To enable this module and allow using the commands you need to add `CheckHyperV = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckHyperV = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckHyperV module.

**List of commands:**

A list of all available queries (check commands)

| Command                                                  | Description                                                                                                              |
|----------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------|
| [check_hyperv_cpu](#check_hyperv_cpu) *(experimental)*   | Check hypervisor logical processor load (guest, hypervisor and total run time) on the host.                              |
| [check_hyperv_host](#check_hyperv_host) *(experimental)* | Check the Hyper-V host: virtual machine health summary and hypervisor capacity (logical/virtual processors, partitions). |
| [check_hyperv_vms](#check_hyperv_vms) *(experimental)*   | Check virtual machines (state, health, heartbeat, uptime, memory, processors, snapshots, replication).                   |

### check_hyperv_cpu

Check hypervisor logical processor load (guest, hypervisor and total run time) on the host.

#### About `check_hyperv_cpu`

On a Hyper-V host the ordinary CPU counters (and Task Manager) only see the
root partition: the time the logical processors spend running guests is
invisible to them, so a host that is saturated by its VMs can look idle.
`check_hyperv_cpu` reads the "Hyper-V Hypervisor Logical Processor" counters
instead, which account for every logical processor regardless of which
partition used it.

Each logical processor is one record (`Hv LP 0`, `Hv LP 1`, ...) and a
synthetic `total` record carries the average over all of them (the sum for
`context_switches`). The default filter keeps only `total`; use
`filter=processor != 'total'` to alert on individual logical processors, for
example to catch one VM pinning a single core.

The run-time counters are rates, so the check samples them twice, one second
apart (`averages`, on by default). `averages=false` skips the wait but then
every rate reads 0 — only useful to prove the counters exist.

The percentages are rounded to one decimal, in the detail line and in the
perfdata alike; the perfdata labels are the processor joined to the keyword
(`total_total_run_time`, `Hv LP 3_guest_run_time`).

Reading `total_run_time` against `guest_run_time` and `hypervisor_run_time`
tells the two kinds of load apart: a high hypervisor share with modest guest
time points at scheduling or intercept overhead (many small VMs, nested
virtualisation, storms of timer interrupts) rather than at busy guests.

**Jump to section:**

* [Sample Commands](#check_hyperv_cpu_samples)
* [Command-line Arguments](#check_hyperv_cpu_options)
* [Filter keywords](#check_hyperv_cpu_filter_keys)


<a id="check_hyperv_cpu_samples"></a>
#### Sample Commands

**Check the aggregate logical processor load with the default thresholds (80/90 %):**

```
check_hyperv_cpu
OK: total: 23.4% total (21.1% guest, 2.3% hypervisor)|'total_total_run_time'=23.4%;80;90 'total_guest_run_time'=21.1%;0;0 'total_hypervisor_run_time'=2.3%;0;0
```

**Alert on individual logical processors instead of the average:**

```
check_hyperv_cpu "filter=processor != 'total'" "warning=total_run_time > 90" "critical=total_run_time > 98"
WARNING: Hv LP 5: 94.2% total (93.1% guest, 1.1% hypervisor)|'Hv LP 0_total_run_time'=12.5%;90;98 'Hv LP 0_guest_run_time'=11.9%;0;0 'Hv LP 0_hypervisor_run_time'=0.6%;0;0 'Hv LP 1_total_run_time'=9.3%;90;98 ...
```

**Watch the hypervisor's own share of the time:**

```
check_hyperv_cpu "warning=hypervisor_run_time > 15" "critical=hypervisor_run_time > 30"
OK: total: 31.0% total (27.2% guest, 3.8% hypervisor)|'total_hypervisor_run_time'=3.8%;15;30 'total_total_run_time'=31%;0;0 'total_guest_run_time'=27.2%;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_cpu
Hyper-V counters (Hyper-V Hypervisor Logical Processor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```



<a id="check_hyperv_cpu_options"></a>
#### Command-line Arguments

        
| Option                                 | Default Value | Description                                                                                                                                                                   |
|----------------------------------------|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [averages](#check_hyperv_cpu_averages) | true          | Sample the counters twice, one second apart. The run-time counters are rates, so this is what gives them a value; averages=false skips the wait and reports 0 for every rate. |



<h5 id="check_hyperv_cpu_averages">averages:</h5>

Sample the counters twice, one second apart. The run-time counters are rates, so this is what gives them a value; averages=false skips the wait and reports 0 for every rate.

*Default Value:* `true`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                                                                                         |
|------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------|
| <a id="check_hyperv_cpu_filter"></a>[filter](../common-options.md#filter)                                        | processor = 'total'                                                                                   |
| <a id="check_hyperv_cpu_warning"></a>[warning](../common-options.md#warning)                                     | total_run_time > 80                                                                                   |
| <a id="check_hyperv_cpu_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                       |
| <a id="check_hyperv_cpu_critical"></a>[critical](../common-options.md#critical)                                  | total_run_time > 90                                                                                   |
| <a id="check_hyperv_cpu_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                       |
| <a id="check_hyperv_cpu_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                       |
| <a id="check_hyperv_cpu_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                 |
| <a id="check_hyperv_cpu_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                 |
| <a id="check_hyperv_cpu_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                               |
| <a id="check_hyperv_cpu_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                       |
| <a id="check_hyperv_cpu_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                 |
| <a id="check_hyperv_cpu_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                     |
| <a id="check_hyperv_cpu_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                    |
| <a id="check_hyperv_cpu_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                       |
| <a id="check_hyperv_cpu_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No logical processor counters found                                                                   |
| <a id="check_hyperv_cpu_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${processor}: ${total_run_time}% total (${guest_run_time}% guest, ${hypervisor_run_time}% hypervisor) |
| <a id="check_hyperv_cpu_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${processor}                                                                                          |
| <a id="check_hyperv_cpu_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                       |
| <a id="check_hyperv_cpu_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                       |
| <a id="check_hyperv_cpu_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                    |
| <a id="check_hyperv_cpu_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                       |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_hyperv_cpu_filter_keys"></a>
#### Filter keywords

| Option              | Description                                                                                                                            |
|---------------------|----------------------------------------------------------------------------------------------------------------------------------------|
| context_switches    | Virtual processor context switches per second on the logical processor                                                                 |
| guest_run_time      | % of time spent running guest (and root partition) code                                                                                |
| hypervisor_run_time | % of time spent in the hypervisor itself (scheduling, intercepts)                                                                      |
| idle_time           | % of time the logical processor was idle                                                                                               |
| processor           | Logical processor instance ('Hv LP 0', 'Hv LP 1', ...) or 'total' for the average over all of them                                     |
| total_run_time      | % of time the logical processor ran guest or hypervisor code (the host's real CPU usage, which Task Manager on the host under-reports) |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_hyperv_host

Check the Hyper-V host: virtual machine health summary and hypervisor capacity (logical/virtual processors, partitions).

#### About `check_hyperv_host`

`check_hyperv_host` reads two hypervisor performance counter objects and
reports the host picture in one record: the "Hyper-V Virtual Machine Health
Summary" (how many virtual machines the host considers ok and how many
critical) and the "Hyper-V Hypervisor" capacity counters (logical processors,
virtual processors handed to running partitions, running partitions).

The counters only exist when the Hyper-V role is installed **and** the
hypervisor is running; on any other host the check reports UNKNOWN with a
message saying so. It needs no special privileges beyond reading performance
counters.

A VM is *critical* in the health summary when the host cannot keep it running
as configured (a missing virtual disk, a lost virtual switch, a failed
save/restore), which is why `critical=health_critical > 0` is the default.
`partitions` counts the root (host) partition too, so it is the number of
running VMs plus one. All the capacity values are emitted as perfdata so the
ratio of virtual to logical processors can be graphed over time. For per-VM
detail see `check_hyperv_vms`, for the actual CPU load `check_hyperv_cpu`.

**Jump to section:**

* [Sample Commands](#check_hyperv_host_samples)
* [Command-line Arguments](#check_hyperv_host_options)
* [Filter keywords](#check_hyperv_host_filter_keys)


<a id="check_hyperv_host_samples"></a>
#### Sample Commands

**Check the host with the default thresholds (critical when any VM is unhealthy):**

```
check_hyperv_host
OK: 12 VMs ok, 0 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_health_ok'=12;0;0 'vms_health_critical'=0;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0
```

**Warn when the host is over-committed on processors:**

```
check_hyperv_host "warning=virtual_processors > 64" "critical=health_critical > 0"
OK: 12 VMs ok, 0 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_virtual_processors'=48;64;0 'vms_health_critical'=0;0;0 'vms_health_ok'=12;0;0 'vms_logical_processors'=32;0;0 'vms_partitions'=13;0;0
```

**A VM the host cannot keep running trips the default critical threshold:**

```
check_hyperv_host
CRITICAL: 11 VMs ok, 1 critical, 13 partitions on 32 logical processors (48 virtual)|'vms_health_critical'=1;0;0 'vms_health_ok'=11;0;0 'vms_logical_processors'=32;0;0 'vms_virtual_processors'=48;0;0 'vms_partitions'=13;0;0
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_host
Hyper-V counters (Hyper-V Virtual Machine Health Summary, Hyper-V Hypervisor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```



<a id="check_hyperv_host_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                            | Default Value                                                                                                                                          |
|-------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------|
| <a id="check_hyperv_host_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                                                        |
| <a id="check_hyperv_host_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                                                        |
| <a id="check_hyperv_host_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                                                        |
| <a id="check_hyperv_host_critical"></a>[critical](../common-options.md#critical)                                  | health_critical > 0                                                                                                                                    |
| <a id="check_hyperv_host_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                                                        |
| <a id="check_hyperv_host_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                                                        |
| <a id="check_hyperv_host_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                                                  |
| <a id="check_hyperv_host_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                                                  |
| <a id="check_hyperv_host_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                                                                |
| <a id="check_hyperv_host_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                                                        |
| <a id="check_hyperv_host_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                                                  |
| <a id="check_hyperv_host_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                                                      |
| <a id="check_hyperv_host_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                                                     |
| <a id="check_hyperv_host_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                                                        |
| <a id="check_hyperv_host_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No Hyper-V counters found                                                                                                                              |
| <a id="check_hyperv_host_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${health_ok} VMs ok, ${health_critical} critical, ${partitions} partitions on ${logical_processors} logical processors (${virtual_processors} virtual) |
| <a id="check_hyperv_host_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | vms                                                                                                                                                    |
| <a id="check_hyperv_host_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                                                        |
| <a id="check_hyperv_host_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                                                        |
| <a id="check_hyperv_host_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                                                     |
| <a id="check_hyperv_host_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_hyperv_host_filter_keys"></a>
#### Filter keywords

| Option             | Description                                                                                              |
|--------------------|----------------------------------------------------------------------------------------------------------|
| health_critical    | Virtual machines whose health is critical (the host could not keep them running as configured)           |
| health_ok          | Virtual machines whose health is ok                                                                      |
| logical_processors | Logical processors the hypervisor manages on this host                                                   |
| partitions         | Running partitions, including the root (host) partition: the number of running virtual machines plus one |
| total_pages        | Memory pages the hypervisor has allocated for its own use                                                |
| virtual_processors | Virtual processors currently allocated to running partitions (root and guests)                           |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_hyperv_vms

Check virtual machines (state, health, heartbeat, uptime, memory, processors, snapshots, replication).

#### About `check_hyperv_vms`

`check_hyperv_vms` lists every virtual machine on the host as one record,
built from the `root\virtualization\v2` WMI namespace: the VM itself
(`Msvm_ComputerSystem`: power state, health, uptime, replication), its
heartbeat integration component, the memory and processor settings, the
memory and virtual processors currently backing it, and its checkpoints.

The defaults look for the two things that go wrong most: a **running guest
that stops answering the heartbeat** (`warning`), which is what a hung or
blue-screened VM looks like from the outside, and a **VM the host itself
reports as unhealthy** (`critical`). A guest that cannot answer is left
alone: `heartbeat = 'disabled'` when its heartbeat integration service is
turned off in the VM settings, `heartbeat = 'none'` when the VM has no
heartbeat component for the check to read. A guest without integration
services at all still has the component and reads `no_contact`, which does
warn, so either install them or exclude the VM with a filter.

`state` covers the transitional states too (`starting`, `saving`,
`pausing`, ...), and `operation` names a long-running operation in progress
(a checkpoint being merged, a live migration, a backup) — useful to explain a
short heartbeat outage. `uptime`, `memory_assigned` and `cpu_load` are emitted
as perfdata for every VM; the configured limits (`memory_startup`,
`memory_minimum`, `memory_maximum`, `vcpus`) are keywords for thresholds and
filters.

Checkpoints are the classic disk eater: `snapshots` and `oldest_snapshot`
(a date, so `oldest_snapshot < -7d` means older than a week) let you alert
on forgotten ones. The Hyper-V Replica keywords (`replication_mode`,
`replication_state`, `replication_health`) come straight from the VM row;
they read `none` / `disabled` / `not_applicable` on a VM that is not
replicated.

Reading the namespace needs local administrator or *Hyper-V Administrators*
membership for the account the service runs as (the default LocalSystem
account has it). Hyper-V does not refuse a caller without it: it silently
returns only the host's own row, so every VM just seems to be missing. The
check compares that with the VM count from the health summary counters, which
anyone can read, and when WMI shows none of the VMs the counters count it
reports UNKNOWN (`... none are visible to this account`) instead of "No
virtual machines found". When the counters cannot be read either, an empty
list is only trusted from an account that may see every VM (an elevated
administrator, a member of Hyper-V Administrators, LocalSystem); anyone else
gets UNKNOWN (`... cannot tell that there are none`). The `hyperv.vms` facts
and the metrics do the same. This is what you see when you run `nscp test`
from a shell that is not elevated.

Without the Hyper-V role the namespace does not exist and the check reports
UNKNOWN with a message saying so. With the role installed but the *Hyper-V
Virtual Machine Management* service (vmms) stopped, the namespace is there
but its classes are not, and the message points at the service instead.

`snapshots` and `oldest_snapshot` count the checkpoints someone took. The
recovery points Hyper-V Replica keeps for a replicated VM are not
checkpoints and are not counted.

**Jump to section:**

* [Sample Commands](#check_hyperv_vms_samples)
* [Command-line Arguments](#check_hyperv_vms_options)
* [Filter keywords](#check_hyperv_vms_filter_keys)


<a id="check_hyperv_vms_samples"></a>
#### Sample Commands

**Check all virtual machines with the default thresholds:**

```
check_hyperv_vms
OK: all 3 virtual machine(s) ok|'web-01_uptime'=864000s;0;0 'web-01_memory_assigned'=4294967296B;0;0 'web-01_cpu_load'=12%;0;0 'db-01_uptime'=864012s;0;0 'db-01_memory_assigned'=17179869184B;0;0 'db-01_cpu_load'=41%;0;0 'test-01_uptime'=0s;0;0 'test-01_memory_assigned'=0B;0;0 'test-01_cpu_load'=0%;0;0
```

**A running guest that stops answering the heartbeat (a hung VM):**

```
check_hyperv_vms
WARNING: web-01: running, heartbeat lost_communication, health ok|'web-01_uptime'=864000s;0;0 'web-01_memory_assigned'=4294967296B;0;0 'web-01_cpu_load'=100%;0;0 ...
```

**Only the VMs that are supposed to be running, and alert when one is not:**

```
check_hyperv_vms "filter=vm like 'prod-'" "critical=state != 'running'"
CRITICAL: prod-db-02: off, heartbeat disabled, health ok|'prod-db-02_uptime'=0s;0;0 'prod-db-02_memory_assigned'=0B;0;0 'prod-db-02_cpu_load'=0%;0;0 ...
```

**Forgotten checkpoints older than a week:**

```
check_hyperv_vms "warning=oldest_snapshot < -7d" "detail-syntax=${vm}: ${snapshots} checkpoint(s), oldest ${oldest_snapshot}"
WARNING: db-01: 2 checkpoint(s), oldest 2026-08-30 14:02:11|...
```

**Hyper-V Replica health on a primary:**

```
check_hyperv_vms "filter=replication_mode != 'none'" "warning=replication_health = 'warning'" "critical=replication_health = 'critical'" "detail-syntax=${vm}: ${replication_state} (${replication_health})"
OK: all 2 virtual machine(s) ok|...
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_vms
Hyper-V virtual machine information not available: the Hyper-V role is not installed on this host (root\virtualization\v2 missing)
```

**Run by an account that may not see the virtual machines (here: `nscp test` in a shell that is not elevated):**

```
check_hyperv_vms
Hyper-V reports 1 virtual machine(s) on this host but none are visible to this account: run as an elevated administrator or a member of Hyper-V Administrators
```



<a id="check_hyperv_vms_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                                                                               |
|------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------|
| <a id="check_hyperv_vms_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                             |
| <a id="check_hyperv_vms_warning"></a>[warning](../common-options.md#warning)                                     | state = 'running' and heartbeat != 'ok' and heartbeat != 'disabled' and heartbeat != 'none' |
| <a id="check_hyperv_vms_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                             |
| <a id="check_hyperv_vms_critical"></a>[critical](../common-options.md#critical)                                  | health != 'ok'                                                                              |
| <a id="check_hyperv_vms_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                             |
| <a id="check_hyperv_vms_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                             |
| <a id="check_hyperv_vms_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                       |
| <a id="check_hyperv_vms_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                       |
| <a id="check_hyperv_vms_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                     |
| <a id="check_hyperv_vms_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                             |
| <a id="check_hyperv_vms_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                       |
| <a id="check_hyperv_vms_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                           |
| <a id="check_hyperv_vms_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                                                                  |
| <a id="check_hyperv_vms_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): all %(count) virtual machine(s) ok                                               |
| <a id="check_hyperv_vms_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No virtual machines found                                                                   |
| <a id="check_hyperv_vms_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${vm}: ${state}, heartbeat ${heartbeat}, health ${health}                                   |
| <a id="check_hyperv_vms_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${vm}                                                                                       |
| <a id="check_hyperv_vms_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                             |
| <a id="check_hyperv_vms_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                             |
| <a id="check_hyperv_vms_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                          |
| <a id="check_hyperv_vms_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                             |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_hyperv_vms_filter_keys"></a>
#### Filter keywords

| Option             | Description                                                                                                                                                                                                                                          |
|--------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| cpu_load           | Average load of the VM's virtual processors in % (0 when it is off)                                                                                                                                                                                  |
| dynamic_memory     | True when dynamic memory is enabled for the VM                                                                                                                                                                                                       |
| generation         | VM generation (1 or 2)                                                                                                                                                                                                                               |
| health             | Health as the host sees it: ok, major_failure or critical_failure                                                                                                                                                                                    |
| heartbeat          | What the guest's heartbeat integration service reports: ok, degraded, error, non_recoverable_error, no_contact, lost_communication, dormant; disabled when the service is turned off in the VM settings, none when the VM has no heartbeat component |
| id                 | GUID of the virtual machine                                                                                                                                                                                                                          |
| last_state_change  | When the VM last changed power state. Comparable to relative times, e.g. last_state_change > -1h.                                                                                                                                                    |
| memory_assigned    | Memory currently assigned to the VM in bytes (0 when it is off)                                                                                                                                                                                      |
| memory_maximum     | Configured maximum memory in bytes (dynamic memory only; 0 with static memory)                                                                                                                                                                       |
| memory_minimum     | Configured minimum memory in bytes (dynamic memory only; 0 with static memory)                                                                                                                                                                       |
| memory_startup     | Configured startup memory in bytes                                                                                                                                                                                                                   |
| oldest_snapshot    | Creation time of the oldest checkpoint (0 / 'none' without checkpoints). Comparable to relative times, e.g. oldest_snapshot < -7d.                                                                                                                   |
| operation          | Long-running operation in progress (creating_snapshot, merging_disks, migrating, backing_up, ...) or none                                                                                                                                            |
| operational_status | Operational status: ok, degraded, predictive_failure, stopped, in_service or dormant                                                                                                                                                                 |
| pid                | Process id of the VM's worker process (0 when it is off)                                                                                                                                                                                             |
| replication_health | Hyper-V Replica health: not_applicable, ok, warning or critical                                                                                                                                                                                      |
| replication_mode   | Hyper-V Replica role of the VM: none, primary, replica, test_replica or extended_replica                                                                                                                                                             |
| replication_state  | Hyper-V Replica state: disabled, replicating, suspended, critical, resynchronizing, failover_in_progress, ...                                                                                                                                        |
| snapshots          | Number of checkpoints (snapshots) the VM has                                                                                                                                                                                                         |
| state              | Power state: running, off, saved, paused, starting, stopping, saving, pausing, resuming, fast_saved, fast_saving or state_<n> for an unknown code                                                                                                    |
| state_code         | Raw EnabledState value behind `state`                                                                                                                                                                                                                |
| uptime             | Seconds since the VM was last started (0 when it is off)                                                                                                                                                                                             |
| vcpus              | Configured virtual processors                                                                                                                                                                                                                        |
| version            | Configuration version of the VM (e.g. 9.0)                                                                                                                                                                                                           |
| vm                 | Name of the virtual machine                                                                                                                                                                                                                          |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                                    | Description |
|---------------------------------------------------|-------------|
| [/settings/hyperv/facts](#/settings/hyperv/facts) |             |


### /settings/hyperv/facts <a id="/settings/hyperv/facts"></a>



| Key                              | Default Value | Description       |
|----------------------------------|---------------|-------------------|
| [hyperv.vms](#hyper-v-vms-facts) | false         | HYPER-V VMS FACTS |


```ini
# 
[/settings/hyperv/facts]
hyperv.vms=false
```

#### HYPER-V VMS FACTS <a id="/settings/hyperv/facts/hyperv.vms"></a>

Collect the \`hyperv.vms\` fact set: one record per virtual machine on this host - its name (the record id, the same value check_hyperv_vms calls \`vm\`), its GUID, generation and configuration version, the configured processors and memory, whether dynamic memory is on, how many checkpoints it has and its Hyper-V Replica role. Not its state, heartbeat, load or assigned memory: that is monitoring, and it lives in check_hyperv_vms. Re-read every facts round with the same WMI queries the check runs, because VMs are created, reconfigured and removed while the agent runs. Nothing is collected while this is off.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/hyperv/facts](#/settings/hyperv/facts) |
| Key:           | hyperv.vms                                        |
| Default value: | `false`                                           |


**Sample:**

```
[/settings/hyperv/facts]
# HYPER-V VMS FACTS
hyperv.vms=false
```
