---
icon: "🏷️"
modules: [CheckSystem]
action: none
---
**CheckSystem publishes what the host *is* as host tags.** Nothing to do — ten
new tags appear on `/api/v2/tags`, in the web UI and, on an enrolled host, in
what the fleet server is told. Turn the whole set off with
`publish facts = false` under `[/settings/system/windows]` or
`[/settings/system/unix]`.

The module already published the Windows version and, where configured, a tag
per running service. It knew a good deal more — every fact below is read by
one of its checks — and published none of it, so a group selector could not
tell a 64-core Linux VM from a laptop without running a check against the host.
The unix module published no facts at all.

| Tag | Windows | Linux |
|---|---|---|
| `os_name` | `Windows Server 2022` | `Ubuntu 24.04.1 LTS` (`PRETTY_NAME`) |
| `os_version` | `10.0.20348` | `6.8.0-45-generic` — the kernel version on both |
| `os_family` | `windows` | `linux` |
| `arch` | `x86_64`, `arm64`, `x86`, … | same vocabulary |
| `cpu_cores` | logical processors | `_SC_NPROCESSORS_ONLN` |
| `memory_gb` | installed physical memory, whole GB | same |
| `virtualization` | see below | see below |
| `manufacturer` | `Dell Inc.` (SMBIOS) | `Dell Inc.` (`/sys/class/dmi/id/sys_vendor`) |
| `model` | `PowerEdge R650` (SMBIOS) | `PowerEdge R650` (`…/product_name`) |
| `domain` | primary DNS suffix | host name minus its first label |

A fact that could not be determined is **not published**: an absent tag is the
contract for unknown, rather than a tag reading `unknown` that every selector
would have to special-case. So a VM with no SMBIOS strings has no
`manufacturer`, and a host whose name is not qualified has no `domain`.

Three things worth knowing:

* **The values mean the same thing on every platform.** Windows spells 64-bit
  Intel `AMD64` and unix `x86_64`; both publish `x86_64`. That is the point of
  the facts — one expression selects a mixed fleet.
* **`virtualization` says `none` on a Windows host that runs Hyper-V, WSL2,
  Windows Sandbox or virtualization-based security.** Those move Windows into
  the hypervisor's root partition, where it reads its own hypervisor bit, so a
  naive check calls every modern desktop a VM. What the firmware says outranks
  the bit: a machine whose SMBIOS names an OEM and a product is the physical
  host. The vocabulary is `vmware`, `hyperv`, `kvm`, `xen`, `virtualbox`,
  `qemu`, `parallels`, `bhyve`, `acrn`, plus `virtual` (a hypervisor is
  present but neither the CPU nor SMBIOS names it — this is what WSL2 reports)
  and `none`.
* **They are gathered once, when the module starts.** A fact describes the
  machine, not its configuration, and nothing it reads can change without the
  host restarting. Reloading settings does not re-gather them; restarting the
  service does. Nothing in the gather forks a process, queries WMI or calls the
  resolver, so it cannot hold up the service start.

`domain` on unix comes from the configured host name rather than the resolver,
for that last reason. A Linux host with a short host name and its domain only
in DNS therefore publishes no `domain`, where a Windows host in the same
domain does.
