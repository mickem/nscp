---
icon: "🏷️"
modules: [CheckSystem]
action: none
---
**CheckSystem describes the host it runs on: three more selector tags, and the
first two fact sets.** Nothing to do. `os_family`, `arch` and `virtualization`
join the tags CheckSystem already published, and the inventory — the vendor,
the model, the size and the DNS domain — is available as the `os` and
`hardware` fact sets, which collect nothing until you turn them on.

The module already published the OS version and, where configured, a tag per
running service. It knew a good deal more about the machine — every value below
is read by one of its checks — and published none of it, so nothing could tell
a 64-core Linux VM from a laptop without running a check against the host. The
unix module published no facts at all.

#### Tags: the three that select a group

| Tag | Windows | Linux |
|---|---|---|
| `os_family` | `windows` | `linux` |
| `arch` | `x86_64`, `arm64`, `x86`, … | the same vocabulary |
| `virtualization` | see below | see below |

They join the existing `os_name` and `os_version` and are published at start,
as those are. A tag is matched whole by a fleet group selector and is uploaded
on every state report, which is what these three are for and what the
inventory below is deliberately kept out of.

The values mean the same thing on every platform: Windows spells 64-bit Intel
`AMD64` and unix `x86_64`, and both publish `x86_64`. That is the point — one
expression selects a mixed fleet.

#### Fact sets: the inventory, opt-in

```ini
[/settings/system/windows/facts]
os = true
hardware = true
```

`[/settings/system/unix/facts]` on Linux, with the same two keys. Both default
to **false**: an inventory is data an operator did not necessarily agree to
ship, so nothing is collected until a set is enabled. `nscp test` then shows
what the core holds:

```
nscp> facts
  hardware:
    cpu_cores: 20
    manufacturer: Webhallen
    memory_gb: 32
    model: Gaming PC
  os:
    arch: x86_64
    family: windows
    name: Windows 11 24H2
    version: 10.0.26200
    virtualization: none
```

| Set | Fields |
|---|---|
| `os` | `family`, `name`, `version`, `arch`, `virtualization`, `domain` |
| `hardware` | `manufacturer`, `model`, `cpu_cores`, `memory_gb` |

A field that could not be determined is **omitted**, never written empty — a
consumer reads an absent key as unknown, where an empty string reads as an
answer. So a VM with no SMBIOS strings has no `manufacturer`, and a host whose
name is not qualified has no `domain`. `os_name` is the product name
(`Windows Server 2022`, `Ubuntu 24.04.1 LTS`) and `os_version` the kernel
version on both platforms (`10.0.20348`, `6.8.0-45-generic`), so "which hosts
still run the old kernel" is one query across the fleet.

#### Two things that surprise

* **`virtualization` reads `none` on a Windows host running Hyper-V, WSL2,
  Windows Sandbox or virtualization-based security.** Those move Windows into
  the hypervisor's root partition, where it reads its own hypervisor bit — so
  a naive check calls every modern desktop a VM. What the firmware says
  outranks the bit: a machine whose SMBIOS names an OEM and a product is the
  physical host. The vocabulary is `vmware`, `hyperv`, `kvm`, `xen`,
  `virtualbox`, `qemu`, `parallels`, `bhyve`, `acrn`, plus `virtual` (a
  hypervisor is present but neither the CPU nor SMBIOS names it — this is what
  WSL2 reports) and `none`.
* **`domain` on unix comes from the configured host name, not the resolver.**
  Nothing in the gather forks a process, queries WMI or calls the resolver, so
  it cannot hold up a service start on a host whose nameserver is unreachable.
  The cost is that a Linux host with a short name and its domain only in DNS
  publishes no `domain`, where a Windows host in the same domain does.

Turning a fact set off takes effect on the next settings reload — the core
drops a set its producer stops returning. The tags are gathered once, at start,
because nothing they read can change without the host restarting.
