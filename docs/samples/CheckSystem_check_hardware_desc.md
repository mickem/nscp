#### About `check_hardware`

`check_hardware` reports system/chassis/memory hardware inventory: vendor,
model, UUID, serial, chassis type, enclosure serial, asset tag and the memory
modules. It complements `check_os_version` — that check answers "what OS am I
running", this one answers "what box am I". The keywords (`vendor`, `model`,
`uuid`, `serial`, `chassis_type`, `chassis`, `chassis_serial`, `asset_tag`,
`slots`, `modules`, `memory`, `memory_speed`, `module_list`) carry the same
meaning on Windows and Linux, so a policy written for one ports to the other.

The useful alerts are **pinned expectations and changes**, not thresholds on a
moving value:

- **"Did the hardware change?"** — `crit=serial != 'ABC1234'` catches a
  re-imaged, replaced or cloned box.
- **"Did a DIMM drop?"** — `warn=modules < 8` / `crit=memory < 64G` catch a
  failed module long before the OS-level memory checks look abnormal.
- **"Is this the right kind of machine?"** — `crit=chassis like 'Laptop'` in a
  server fleet, or `warn=modules < slots`-style capacity planning via the
  `slots` count.

There are no default thresholds (a bare call is an inventory line); `memory`
and `modules` are always emitted as perf data (`hardware_memory`,
`hardware_modules`). Per-item keywords such as `module_list` belong in
`detail-syntax` (the default `top-syntax` embeds them via `${list}`).

**Caveats:** VMs and OEM boards frequently report blank or placeholder values —
serials like `To be filled by O.E.M.`, chassis `Other`, `slots` 0 — so baseline
a host before pinning expectations on it.

##### Windows

Reads WMI (`root\CIMV2`): `Win32_ComputerSystemProduct` (vendor, model, UUID,
serial), `Win32_SystemEnclosure` (chassis type, enclosure serial, asset tag),
`Win32_PhysicalMemory` (per-DIMM inventory) and `Win32_PhysicalMemoryArray`
(total sockets). Each WMI class is read best-effort: a class that is missing
(stripped-down VMs) leaves its fields empty rather than failing the check, and
the check only errors when *no* class answers (WMI down).
`ConfiguredClockSpeed` falls back to the raw `Speed` on older Windows.

##### Linux

The identity comes from `/sys/class/dmi/id` (the kernel's copy of the SMBIOS
system and chassis records: `sys_vendor`, `product_name`, `product_uuid`,
`product_serial`, `chassis_type`, `chassis_serial`, `chassis_asset_tag`). The
memory inventory comes from the raw SMBIOS table,
`/sys/firmware/dmi/tables/DMI`, parsed directly (memory arrays and memory
devices, types 16 and 17) — no `dmidecode` is needed. `memory_speed` is the
configured clock, falling back to the rated speed; `slots` counts the sockets
of the arrays used for system memory only (not video or flash arrays), and a
memory device that belongs to a video, flash, non-volatile or cache array (a
system ROM described as a flash device, for instance) is not counted as a
module. Devices of an array the firmware labels Other or Unknown are counted. The UUID is rendered
upper case, as Windows and `dmidecode` print it.

**What an unprivileged agent sees.** The kernel makes `product_uuid`,
`product_serial`, `chassis_serial` and the SMBIOS table readable by root only.
Without root:

- `uuid`, `serial` and `chassis_serial` are empty strings (not an error);
- `modules`, `slots` and `memory_speed` are `unknown` and `module_list` is
  empty. An `unknown` value compares false against every number, so
  `warn=modules < 8` never fires on an agent that simply may not read the
  table; test for the case with `modules = 'unknown'`;
- `memory` falls back to the memory the kernel manages, counted in hotplug
  memory blocks (`/sys/devices/system/memory`). It is close to, but not exactly,
  the installed total; `memory_source` says which one you got (`smbios`,
  `sysfs` or `none`).

**No DMI at all.** Containers, microVMs (Firecracker) and boards without
SMBIOS expose neither `/sys/class/dmi/id` nor the table. The check then returns
UNKNOWN with `No hardware inventory available: this host exposes no DMI/SMBIOS
data ...` rather than an inventory line of empty strings. A hypervisor that
publishes the system record but no memory devices (some QEMU/KVM
configurations) gives a normal inventory line with `modules` `unknown`.

The SMBIOS table is firmware-supplied data and is parsed defensively: every
structure, string and field offset is bounds-checked, and strings are reduced
to printable ASCII. The memory inventory is all or nothing. A table that
lists sockets but no populated module, or one whose walk stopped early (a
malformed or truncated structure, or an entry point that announces fewer
structures than the table holds) before every socket the system memory arrays
announce was read, gives the same result as an unreadable one: `modules`, `slots` and `memory_speed` are
`unknown`, `module_list` is empty and `memory` falls back to the kernel's
total, rather than a partial count that a threshold such as `modules < 8`
would act on. Likewise `memory` is the sum of the modules only when every
module reports its size; otherwise it is the kernel's total (`memory_source`
`sysfs`). The identity fields read before the damage are kept.

##### macOS

Not supported yet: the check returns `UNKNOWN`.
