---
icon: "🐧 🔒"
modules: [CheckSystem]
action: none
---
**New on Linux: `check_hardware`.** The Windows check now exists in the Unix
system module too, with the same keywords, defaults and output, so a policy
such as `crit=serial != 'ABC1234'` ports across platforms. It reads
`/sys/class/dmi/id` and parses the SMBIOS table for the per-DIMM inventory
(`modules`, `slots`, `memory_speed`, `module_list`). The serials, the UUID and
the SMBIOS table are root-only on Linux: an unprivileged agent gets empty
serials, an `unknown` module count that never trips a numeric threshold, and
`memory` from the kernel's memory blocks (see `memory_source`). A host without
DMI (containers, microVMs) returns UNKNOWN with a fixed message, and so does
a damaged SMBIOS table: a partial module count is never reported. Perf data
is emitted only for a complete SMBIOS memory inventory, so not on an
unprivileged agent. The check is
not available on macOS yet and is marked experimental. See the
[security notice](../security/notices.md#linux-inventory-checks-expose-patch-level-and-hardware-identifiers)
for what the check exposes.
