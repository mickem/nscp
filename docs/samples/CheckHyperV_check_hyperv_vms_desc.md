#### About `check_hyperv_vms`

`check_hyperv_vms` lists every virtual machine on the host as one record,
built from the `root\virtualization\v2` WMI namespace: the VM itself
(`Msvm_ComputerSystem`: power state, health, uptime, replication), its
heartbeat integration component, the memory and processor settings, the
memory and virtual processors currently backing it, and its checkpoints.

The defaults look for the two things that go wrong most: a **running guest
that stops answering the heartbeat** (`warning`), which is what a hung or
blue-screened VM looks like from the outside, and a **VM the host itself
reports as unhealthy** (`critical`). A guest whose heartbeat integration
service is turned off cannot answer and is left alone (`heartbeat =
'disabled'`); a guest without integration services at all reads `no_contact`,
so either install them or exclude the VM with a filter.

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
virtual machines found". The `hyperv.vms` facts do the same. This is what you
see when you run `nscp test` from a shell that is not elevated. Without the Hyper-V role the
namespace does not exist and the check reports UNKNOWN with a message saying
so.
