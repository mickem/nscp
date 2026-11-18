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
service account has them). Nothing to do on a default install; if the agent
runs under an account that can read the counters but not see the VMs,
`check_hyperv_host` now reports UNKNOWN with the reason - grant the account
Hyper-V Administrators membership.
