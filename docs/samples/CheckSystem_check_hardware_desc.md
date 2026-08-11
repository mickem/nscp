#### About `check_hardware`

`check_hardware` reports BIOS/chassis/memory hardware inventory from WMI
(`root\CIMV2`): `Win32_ComputerSystemProduct` (vendor, model, UUID, serial),
`Win32_SystemEnclosure` (chassis type, enclosure serial, asset tag),
`Win32_PhysicalMemory` (per-DIMM inventory) and `Win32_PhysicalMemoryArray`
(total sockets). It complements `check_os_version` — that check answers "what
OS am I running" (and carries the BIOS serial/version for back-compat), this
one answers "what box am I".

The useful alerts are **pinned expectations and changes**, not thresholds on a
moving value:

- **"Did the hardware change?"** — `crit=serial != 'ABC1234'` catches a
  re-imaged, replaced or cloned box.
- **"Did a DIMM drop?"** — `warn=modules < 8` / `crit=memory < 64G` catch a
  failed module long before the OS-level memory checks look abnormal.
- **"Is this the right kind of machine?"** — `crit=chassis like 'Laptop'` in a
  server fleet, or `warn=modules < slots`-style capacity planning via the
  `slots` count.

Keywords (a single aggregate row):

| Keyword          | Description                                                                       |
|------------------|-----------------------------------------------------------------------------------|
| `vendor`         | System vendor/manufacturer                                                        |
| `model`          | System model / product name                                                       |
| `uuid`           | SMBIOS system UUID                                                                |
| `serial`         | System serial number                                                              |
| `chassis`        | Chassis type name (`Desktop`, `Laptop`, `Rack Mount Chassis`, ...)                |
| `chassis_type`   | Raw SMBIOS chassis type number (0 when unknown)                                   |
| `chassis_serial` | Enclosure serial number                                                           |
| `asset_tag`      | SMBIOS asset tag                                                                  |
| `memory`         | Total installed memory (size units work: `memory < 64G`); renders human-readable  |
| `modules`        | Number of populated memory modules                                                |
| `slots`          | Total memory sockets on the board (0 when not reported)                           |
| `memory_speed`   | Slowest populated module's configured clock in MHz (0 when unknown)               |
| `module_list`    | Semicolon-separated per-DIMM inventory (`DIMM_A1: 32GB@4800MHz; ...`)             |

There are no default thresholds (a bare call is an inventory line); `memory`
and `modules` are always emitted as perf data (`hardware_memory`,
`hardware_modules`). Per-item keywords such as `module_list` belong in
`detail-syntax` (the default `top-syntax` embeds them via `${list}`).

**Caveats:** VMs and OEM boards frequently report blank or placeholder values —
serials like `To be filled by O.E.M.`, chassis `Other`, `slots` 0 — so baseline
a host before pinning expectations on it. Each WMI class is read best-effort: a
class that is missing (stripped-down VMs) leaves its fields empty rather than
failing the check, and the check only errors when *no* class answers (WMI
down). `ConfiguredClockSpeed` falls back to the raw `Speed` on older Windows.
