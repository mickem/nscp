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
