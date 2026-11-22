# CheckHyperV

*Available on Windows only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

Checks for a Hyper-V host: hypervisor health, capacity and over-commitment, logical processor load, the state, heartbeat, memory and snapshots of every virtual machine, and the virtual switches.

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

| Command                                                          | Description                                                                                                                                          |
|------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| [check_hyperv_cpu](#check_hyperv_cpu) *(experimental)*           | Check hypervisor logical processor load (guest, hypervisor and total run time) on the host.                                                          |
| [check_hyperv_host](#check_hyperv_host) *(experimental)*         | Check the Hyper-V host: virtual machine health summary, hypervisor capacity (logical/virtual processors, partitions) and CPU/memory over-commitment. |
| [check_hyperv_switches](#check_hyperv_switches) *(experimental)* | Check virtual switches (type, state, health, ports).                                                                                                 |
| [check_hyperv_vms](#check_hyperv_vms) *(experimental)*           | Check virtual machines (state, health, heartbeat, uptime, memory, processors, snapshots, replication, duplicate names).                              |

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
apart. `single-sample` skips the wait but then every rate reads 0 — only
useful to prove the counters exist.

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

        
| Option                                           | Default Value | Description                                                                                                                                                                                     |
|--------------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [single-sample](#check_hyperv_cpu_single-sample) | false         | Sample the counters once instead of twice one second apart. The run-time counters are rates, so the second sample is what gives them a value; this skips the wait and reports 0 for every rate. |



<h5 id="check_hyperv_cpu_single-sample">single-sample:</h5>

Sample the counters once instead of twice one second apart. The run-time counters are rates, so the second sample is what gives them a value; this skips the wait and reports 0 for every rate.

*Default Value:* `false`


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

Check the Hyper-V host: virtual machine health summary, hypervisor capacity (logical/virtual processors, partitions) and CPU/memory over-commitment.

#### About `check_hyperv_host`

`check_hyperv_host` reads two hypervisor performance counter objects and
reports the host picture in one record: the "Hyper-V Virtual Machine Health
Summary" (how many virtual machines the host considers ok and how many
critical) and the "Hyper-V Hypervisor" capacity counters (logical processors,
virtual processors handed to running partitions, running partitions).

The counters only exist when the Hyper-V role is installed **and** the
hypervisor is running; on any other host the check reports UNKNOWN with a
message saying so.

A VM is *critical* in the health summary when the host cannot keep it running
as configured (a missing virtual disk, a lost virtual switch, a failed
save/restore), which is why `critical=health_critical > 0` is the default.
`partitions` counts the root (host) partition too, so it is the number of
running VMs plus one. All the capacity values are emitted as perfdata so the
ratio of virtual to logical processors can be graphed over time. For per-VM
detail see `check_hyperv_vms`, for the actual CPU load `check_hyperv_cpu`.

#### Over-commitment

How much of the host the running virtual machines have been handed is read
from the VMs themselves (the same `root\virtualization\v2` walk
`check_hyperv_vms` does), because `virtual_processors` counts the root
partition's processors too and no counter sums the guests' memory:

| Keyword             | Meaning |
|---------------------|---------|
| `running_vms`       | Virtual machines that are running. |
| `vm_vcpus`          | Virtual processors configured on the running VMs. |
| `cpu_overcommit`    | `vm_vcpus` as % of `logical_processors`. Above 100 the guests are promised more processors than the host has; how far that is safe depends on the workload, 200–400 % is common for general-purpose VMs. |
| `memory_assigned`   | Bytes currently backing the running VMs (with dynamic memory, what they have right now, not their maximum). |
| `physical_memory`   | Bytes of physical memory in the host. |
| `memory_overcommit` | `memory_assigned` as % of `physical_memory`. Hyper-V never hands out more memory than it has, so this stays below 100; the rest is what the host itself and dynamic memory have to work with. |

`cpu_overcommit` and `memory_overcommit` are emitted as perfdata. Because of
the VM walk the check needs the same rights as `check_hyperv_vms`: local
administrator or *Hyper-V Administrators* membership (the default LocalSystem
account has it).

When the health counters count no active virtual machines there is nothing to
sum, so the walk is skipped and any account gets an over-commitment of zero.
Otherwise a walk that cannot be trusted - WMI fails, it runs past
`vm-timeout` (5 seconds by default), or this account sees fewer active VMs
than the counters count - does not take the health summary down with it: the
guest keywords stay 0, `guests_known` is false and `guests_error` says why.
Make a threshold on over-commitment catch that case too:

```
check_hyperv_host "warning=cpu_overcommit > 300 or guests_known = 0" "critical=health_critical > 0 or memory_overcommit > 95"
```

#### Hosts as SCVMM sees them

For the fabric view - every host System Center Virtual Machine Manager
manages, whether VMM can reach it and whether it considers it healthy - the
agent ships a sample script, `scripts\check_scvmm_hosts.ps1`, rather than a
native check: VMM is only reachable through its PowerShell module, so the
script runs where the VMM console is installed (usually the VMM server).
Call `powershell.exe` with `-File`, so the script's exit code reaches the
agent and the arguments stay with the entry:

```ini
[/modules]
CheckExternalScripts = enabled

[/settings/external scripts/scripts]
check_scvmm_hosts = powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -File scripts\check_scvmm_hosts.ps1 -Server vmm01 -HostGroup "All Hosts\Production"
```

`-HostGroup` covers the group and every group below it. A host is CRITICAL
when VMM cannot communicate with it or its overall state is not OK;
`-Warning` lists the states that only warn (by default
`NeedsAttention,Updating`) and `-Ignore` the states that are not reported, reachable
or not (by default `MaintenanceMode`). Both take a comma separated list. A
missing VMM module or server is UNKNOWN.

**Jump to section:**

* [Sample Commands](#check_hyperv_host_samples)
* [Command-line Arguments](#check_hyperv_host_options)
* [Filter keywords](#check_hyperv_host_filter_keys)


<a id="check_hyperv_host_samples"></a>
#### Sample Commands

**Check the host with the default thresholds (critical when any VM is unhealthy):**

```
check_hyperv_host
OK: 1 VMs ok, 0 critical, 3 partitions on 16 logical processors (40 virtual)|'vms_cpu_overcommit'=50% 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9% 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Warn when the running VMs have more virtual processors than the host has logical ones, critical at four times:**

```
check_hyperv_host "warning=cpu_overcommit > 100" "critical=cpu_overcommit > 400 or health_critical > 0" "detail-syntax=${vm_vcpus} vCPUs on ${logical_processors} logical processors (${cpu_overcommit}%)"
OK: 8 vCPUs on 16 logical processors (50%)|'vms_cpu_overcommit'=50%;100;400 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9% 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Warn when the VMs hold more than 90 % of the host's memory:**

```
check_hyperv_host "warning=memory_overcommit > 90" "detail-syntax=${memory_assigned} of ${physical_memory} assigned to ${running_vms} VMs (${memory_overcommit}%)"
OK: 3686793216 of 33777467392 assigned to 1 VMs (10.9%)|'vms_cpu_overcommit'=50% 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9%;90 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Alert on over-commitment, and also when the virtual machines could not be read (`guests_known`):**

```
check_hyperv_host "warning=cpu_overcommit > 300 or guests_known = 0" "critical=health_critical > 0 or memory_overcommit > 95"
OK: 1 VMs ok, 0 critical, 3 partitions on 16 logical processors (40 virtual)|'vms_cpu_overcommit'=50%;300 'vms_guests_known'=1;0 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9%;;95 'vms_partitions'=3 'vms_virtual_processors'=40
```

**Show why the guests could not be read, here for an account without Hyper-V rights while a VM runs:**

```
check_hyperv_host "detail-syntax=${health_ok} VMs ok, guests known: ${guests_known} ${guests_error}"
OK: 1 VMs ok, guests known: 0 Hyper-V reports 1 virtual machine(s) on this host but none are visible to this account: run as an elevated administrator or a member of Hyper-V Administrators|'vms_cpu_overcommit'=0% 'vms_health_critical'=0;;0 'vms_health_ok'=1 'vms_logical_processors'=16 'vms_memory_overcommit'=0% 'vms_partitions'=3 'vms_virtual_processors'=40
```

**A VM the host cannot keep running trips the default critical threshold (illustrative output):**

```
check_hyperv_host
CRITICAL: 0 VMs ok, 1 critical, 2 partitions on 16 logical processors (40 virtual)|'vms_cpu_overcommit'=50% 'vms_health_critical'=1;;0 'vms_health_ok'=0 'vms_logical_processors'=16 'vms_memory_overcommit'=10.9% 'vms_partitions'=2 'vms_virtual_processors'=40
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_host
Hyper-V counters (Hyper-V Virtual Machine Health Summary, Hyper-V Hypervisor) not available - is the Hyper-V role installed and the hypervisor running on this host? (...)
```



<a id="check_hyperv_host_options"></a>
#### Command-line Arguments

        
| Option                                      | Default Value | Description                                                                                                                                                                    |
|---------------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [vm-timeout](#check_hyperv_host_vm-timeout) | 5             | Seconds to wait for the virtual machine list (WMI) behind the over-commitment keywords. When it runs out the check still reports the health counters, with guests_known false. |



<h5 id="check_hyperv_host_vm-timeout">vm-timeout:</h5>

Seconds to wait for the virtual machine list (WMI) behind the over-commitment keywords. When it runs out the check still reports the health counters, with guests_known false.

*Default Value:* `5`


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

| Option             | Description                                                                                                                                                                                                            |
|--------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| cpu_overcommit     | Virtual processors of the running virtual machines as % of the host's logical processors (vm_vcpus / logical_processors); above 100 the guests are promised more processors than the host has                          |
| guests_error       | Why the virtual machines could not be read (empty when they were)                                                                                                                                                      |
| guests_known       | False when the virtual machines could not be read (WMI failed or timed out, or this account cannot see all of them), so running_vms, vm_vcpus, memory_assigned and the over-commitment keywords are 0 and mean nothing |
| health_critical    | Virtual machines whose health is critical (the host could not keep them running as configured)                                                                                                                         |
| health_ok          | Virtual machines whose health is ok                                                                                                                                                                                    |
| logical_processors | Logical processors the hypervisor manages on this host                                                                                                                                                                 |
| memory_assigned    | Memory currently assigned to the running virtual machines in bytes                                                                                                                                                     |
| memory_overcommit  | Memory assigned to the running virtual machines as % of the host's physical memory (memory_assigned / physical_memory); what is left is all the host itself and dynamic memory have to work with                       |
| partitions         | Running partitions, including the root (host) partition: the number of running virtual machines plus one                                                                                                               |
| physical_memory    | Physical memory of the host in bytes                                                                                                                                                                                   |
| running_vms        | Virtual machines that are running                                                                                                                                                                                      |
| total_pages        | Memory pages the hypervisor has allocated for its own use                                                                                                                                                              |
| virtual_processors | Virtual processors currently allocated to running partitions (root and guests)                                                                                                                                         |
| vm_vcpus           | Virtual processors configured on the running virtual machines (the root partition's are not counted)                                                                                                                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_hyperv_switches

Check virtual switches (type, state, health, ports).

#### About `check_hyperv_switches`

`check_hyperv_switches` lists every Hyper-V virtual switch on the host as one
record, read from the `root\virtualization\v2` WMI namespace
(`Msvm_VirtualEthernetSwitch` and its ports). It is the native equivalent of
`Get-VMSwitch`.

`type` is worked out the way `Get-VMSwitch` reports `SwitchType`, from what
the switch's own ports connect to:

| `type`     | Meaning |
|------------|---------|
| `external` | Bound to a physical network adapter (wired or Wi-Fi). `management_os` says whether the host shares that adapter. |
| `internal` | No physical adapter, but the host has a virtual adapter on it: host and virtual machines can talk. |
| `private`  | Neither: only the virtual machines on this host can talk over it. |

The defaults warn on a switch whose `state` is anything but `enabled` or
`not_applicable` and go critical on one whose `health` is not `ok`. A switch
has no power state of its own, and Hyper-V reports most switches as
`not_applicable`, so a threshold written as `state != 'enabled'` would warn on
a healthy host. `ports` counts every port on the switch (each
connected virtual machine network adapter, plus the external and host ports)
and is emitted as perfdata.

A typical use is pinning the switches a host must have, so a switch deleted
or recreated by hand (which drops every VM connected to it) is noticed:
filter on the names, set `empty-state=critical`, and the check fails when
the switch disappears.

Without the Hyper-V role the namespace does not exist and the check reports
UNKNOWN with a message saying so; with the role installed but the *Hyper-V
Virtual Machine Management* service (vmms) stopped, the message points at
the service instead. A host with the role and no switches reports
`No virtual switches found` in the `empty-state` (UNKNOWN by default).

**Jump to section:**

* [Sample Commands](#check_hyperv_switches_samples)
* [Command-line Arguments](#check_hyperv_switches_options)
* [Filter keywords](#check_hyperv_switches_filter_keys)


<a id="check_hyperv_switches_samples"></a>
#### Sample Commands

**Check all virtual switches with the default thresholds:**

```
check_hyperv_switches
OK: all 2 virtual switch(es) ok|'Default Switch_ports'=2 'FSE Switch_ports'=6
```

**List every switch with its type and state, whether problems or not:**

```
check_hyperv_switches "top-syntax=${status}: ${list}" "ok-syntax=${status}: ${list}"
OK: Default Switch: internal, not_applicable, health ok, FSE Switch: internal, not_applicable, health ok|'Default Switch_ports'=2 'FSE Switch_ports'=6
```

**A switch the host must have; critical when it is missing:**

```
check_hyperv_switches "filter=switch = 'LAN'" "critical=type != 'external' or health != 'ok'" empty-state=critical "empty-syntax=CRITICAL: switch LAN is missing"
CRITICAL: switch LAN is missing
```

**Ports per switch, and whether the host has an adapter on it:**

```
check_hyperv_switches "warning=ports > 100" "ok-syntax=${status}: ${list}" "detail-syntax=${switch}: ${type}, ${ports} ports, host adapter ${management_os}"
OK: Default Switch: internal, 2 ports, host adapter 1, FSE Switch: internal, 6 ports, host adapter 1|'Default Switch_ports'=2;100 'FSE Switch_ports'=6;100
```

**On a host without the role the check reports UNKNOWN with a clear message:**

```
check_hyperv_switches
Hyper-V virtual switch information not available: the Hyper-V role is not installed on this host (root\virtualization\v2 missing)
```



<a id="check_hyperv_switches_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                | Default Value                                  |
|-----------------------------------------------------------------------------------------------------------------------|------------------------------------------------|
| <a id="check_hyperv_switches_filter"></a>[filter](../common-options.md#filter)                                        |                                                |
| <a id="check_hyperv_switches_warning"></a>[warning](../common-options.md#warning)                                     | state not in ('enabled', 'not_applicable')     |
| <a id="check_hyperv_switches_warn"></a>[warn](../common-options.md#warn)                                              |                                                |
| <a id="check_hyperv_switches_critical"></a>[critical](../common-options.md#critical)                                  | health != 'ok'                                 |
| <a id="check_hyperv_switches_crit"></a>[crit](../common-options.md#crit)                                              |                                                |
| <a id="check_hyperv_switches_ok"></a>[ok](../common-options.md#ok)                                                    |                                                |
| <a id="check_hyperv_switches_debug"></a>[debug](../common-options.md#debug)                                           | false                                          |
| <a id="check_hyperv_switches_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                          |
| <a id="check_hyperv_switches_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                        |
| <a id="check_hyperv_switches_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                |
| <a id="check_hyperv_switches_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                          |
| <a id="check_hyperv_switches_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                              |
| <a id="check_hyperv_switches_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                     |
| <a id="check_hyperv_switches_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): all %(count) virtual switch(es) ok  |
| <a id="check_hyperv_switches_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No virtual switches found                      |
| <a id="check_hyperv_switches_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${switch}: ${type}, ${state}, health ${health} |
| <a id="check_hyperv_switches_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${switch}                                      |
| <a id="check_hyperv_switches_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                |
| <a id="check_hyperv_switches_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                |
| <a id="check_hyperv_switches_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                             |
| <a id="check_hyperv_switches_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_hyperv_switches_filter_keys"></a>
#### Filter keywords

| Option             | Description                                                                                                                                                                                          |
|--------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| health             | Health as the host sees it: ok, degraded, minor_failure, major_failure, critical_failure, non_recoverable_error or unknown                                                                           |
| id                 | GUID of the virtual switch                                                                                                                                                                           |
| management_os      | True when the host itself has a virtual network adapter on the switch (always for internal, optional for external)                                                                                   |
| operational_status | Operational status: ok, degraded, predictive_failure, stopped, in_service or dormant                                                                                                                 |
| ports              | Ports on the switch: one per connected virtual machine network adapter plus the external and host ports                                                                                              |
| state              | Switch state: enabled or not_applicable (both normal; Hyper-V reports most switches as not_applicable), disabled, enabled_but_offline, starting, shutting_down, ... or state_<n> for an unknown code |
| state_code         | Raw EnabledState value behind `state`                                                                                                                                                                |
| switch             | Name of the virtual switch                                                                                                                                                                           |
| type               | Switch type: external (bound to a physical network adapter), internal (host and virtual machines only) or private (virtual machines only)                                                            |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_hyperv_vms

Check virtual machines (state, health, heartbeat, uptime, memory, processors, snapshots, replication, duplicate names).

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
virtual machines found". The counters only count running VMs, so when they
count none (or cannot be read) an empty list is only trusted from an account
that may see every VM (an elevated administrator, a member of Hyper-V
Administrators, LocalSystem); anyone else gets UNKNOWN (`... cannot tell that
there are none`). The `hyperv.vms` facts
and the metrics do the same. This is what you see when you run `nscp test`
from a shell that is not elevated.

Without the Hyper-V role the namespace does not exist and the check reports
UNKNOWN with a message saying so. With the role installed but the *Hyper-V
Virtual Machine Management* service (vmms) stopped, the namespace is there
but its classes are not, and the message points at the service instead.

`name_count` is the number of VMs on this host that share the VM's name,
itself included (compared case-insensitively, the way `Get-VM -Name` matches).
Hyper-V keys virtual machines by GUID and lets two of them carry the same
name - typically after an import that kept the original or a failed
migration - which breaks every script and tool that addresses VMs by name.
`critical=name_count > 1` catches it. The count is per host: two VMs with the
same name on different cluster nodes are not seen.

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

**Two virtual machines with the same name (an import that kept the original):**

```
check_hyperv_vms "critical=name_count > 1" "detail-syntax=${vm} (${id}): ${name_count} VMs share the name"
CRITICAL: web-01 (1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e): 2 VMs share the name, web-01 (9a8b7c6d-5e4f-4321-8765-0fedcba98765): 2 VMs share the name|...
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
| health             | Health as the host sees it: ok, degraded, minor_failure, major_failure, critical_failure, non_recoverable_error or unknown                                                                                                                           |
| heartbeat          | What the guest's heartbeat integration service reports: ok, degraded, error, non_recoverable_error, no_contact, lost_communication, dormant; disabled when the service is turned off in the VM settings, none when the VM has no heartbeat component |
| id                 | GUID of the virtual machine                                                                                                                                                                                                                          |
| last_state_change  | When the VM last changed power state. Comparable to relative times, e.g. last_state_change > -1h.                                                                                                                                                    |
| memory_assigned    | Memory currently assigned to the VM in bytes (0 when it is off)                                                                                                                                                                                      |
| memory_maximum     | Configured maximum memory in bytes (dynamic memory only; 0 with static memory)                                                                                                                                                                       |
| memory_minimum     | Configured minimum memory in bytes (dynamic memory only; 0 with static memory)                                                                                                                                                                       |
| memory_startup     | Configured startup memory in bytes                                                                                                                                                                                                                   |
| name_count         | Virtual machines on this host with the same name as this one, itself included (compared case-insensitively), so name_count > 1 finds duplicate names                                                                                                 |
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
