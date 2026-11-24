---
icon: "🐧 🔒"
modules: [CheckSystem]
action: none
---
**New on Linux: `check_pending_reboot` and `check_hardware`.** The two Windows
checks now exist in the Unix system module too, with the same aggregate
keywords, defaults and output, so a policy such as
`crit=pending = 1 and age > 7d` or `crit=serial != 'ABC1234'` ports across
platforms. `check_pending_reboot` reports the distribution's reboot marker
(`/run/reboot-required` with its package list on Debian/Ubuntu,
`/run/reboot-needed` on SUSE) as `marker`, and a newer installed kernel of the
running kernel's flavour as `kernel` — a real version comparison that covers
the RHEL family, which has no marker; neither runs a helper process, and a
container reports no reboot pending. `check_hardware` reads `/sys/class/dmi/id`
and parses the SMBIOS table for the per-DIMM inventory (`modules`, `slots`,
`memory_speed`, `module_list`). The serials, the UUID and the SMBIOS table are
root-only on Linux: an unprivileged agent gets empty serials, an `unknown`
module count that never trips a numeric threshold, and `memory` from the
kernel's memory blocks (see `memory_source`). A host without DMI (containers,
microVMs) returns UNKNOWN with a fixed message. Neither check is available on
macOS yet. Both are marked experimental. See the
[security notice](../security/notices.md#linux-posture-and-inventory-checks-expose-account-and-hardware-details)
for what the checks expose.
