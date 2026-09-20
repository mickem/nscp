---
icon: "🧪"
modules: [CheckHyperV]
action: none
---
**New `CheckHyperV` module (experimental).** Nothing to do on an upgrade: the
module is not enabled by default. On a Hyper-V host, `CheckHyperV = enabled`
under `[/modules]` adds `check_hyperv_host` (virtual machine health summary
and hypervisor capacity), `check_hyperv_cpu` (logical processor load, which
the ordinary CPU counters cannot see on a host) and `check_hyperv_vms` (one
record per virtual machine: state, heartbeat, health, uptime, memory,
processors, checkpoints and Hyper-V Replica status), plus a `hyperv` metrics
bundle. Reading the virtual machines needs local administrator or *Hyper-V
Administrators* rights for the service account. The module is marked
experimental: its options, keywords and output may still change.
