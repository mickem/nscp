---
icon: "🖥️"
modules: [CheckHyperV]
action: conditional
---
**Hyper-V: over-commitment, virtual switches and duplicate VM names.**
`check_hyperv_host` gained `running_vms`, `vm_vcpus`, `cpu_overcommit`
(virtual processors of the running VMs as % of the logical processors),
`memory_assigned`, `physical_memory` and `memory_overcommit`, with the two
percentages added to its perfdata. The new `check_hyperv_switches` checks
every virtual switch (`type` external/internal/private, `state`, `health`,
`ports`), and `check_hyperv_vms` gained `name_count` so
`critical=name_count > 1` finds VMs that share a name. To compute the
over-commitment
`check_hyperv_host` now reads the virtual machines over WMI as well as the
performance counters, so it needs the same rights as `check_hyperv_vms`
(local administrator or Hyper-V Administrators; the default LocalSystem
service account has them). Nothing to do on a default install. If the agent
runs under an account that can read the counters but not see every VM, or the
VM list cannot be read in time (`vm-timeout`, 5 s by default), the check still
reports the health summary but leaves the guest keywords at 0 with the new
`guests_known` false and the reason in `guests_error`; add
`or guests_known = 0` to a threshold on the over-commitment keywords so that
does not read as an idle host, and grant the account Hyper-V Administrators
membership. `check_hyperv_vms` now reports `health` as `unknown`, `degraded`,
`minor_failure` or `non_recoverable_error` where it used to report
`health_0`, `health_10`, `health_15` or `health_30`; a filter matching the old
spellings must be updated.
