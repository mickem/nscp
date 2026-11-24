---
icon: "🐧 🔒"
modules: [CheckSystem]
action: none
---
**New on Linux: `check_pending_reboot`.** The Windows check now exists in the
Unix system module too, with the same aggregate keywords, defaults and output,
so a policy such as `crit=pending = 1 and age > 7d` ports across platforms. It
reports the distribution's reboot marker (`/run/reboot-required` with its
package list on Debian/Ubuntu, `/run/reboot-needed` on SUSE) as `marker`, and a
newer installed kernel of the running kernel's flavour as `kernel` — a real
version comparison that covers the RHEL family, which has no marker. Neither
signal runs a helper process, and a container reports no reboot pending. The
check is not available on macOS yet and is marked experimental. See the
[security notice](../security/notices.md#linux-inventory-checks-expose-patch-level-and-hardware-identifiers)
for what the check exposes.
