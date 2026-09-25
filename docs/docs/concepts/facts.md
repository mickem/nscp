# Host Facts

**Facts are the agent's inventory of the machine it runs on**: what OS it is,
what hardware it sits on, which volumes and network interfaces it has, what is
installed on it, and which NSClient++ is running on it. The core collects them
from the loaded modules, keeps them as one document per host, and serves that
document on [`/api/v2/facts`](../api/rest/facts.md), in the web UI and in
`nscp test`.

**Nothing is collected until you turn a fact set on.** An inventory is data an
operator did not necessarily agree to ship, so a fresh install reports an empty
document and does no work for it.

---

## Facts or tags?

Tags and facts both describe the host. They answer different questions, and
facts do not replace tags.

|                       | Tags                                          | Facts                                                  |
|-----------------------|-----------------------------------------------|--------------------------------------------------------|
| Shape                 | flat `key=value` strings                      | a document: sections, numbers, strings, lists of records |
| Question              | *which group is this host in*                 | *what is this host*                                    |
| Collected             | always, a handful per module                  | only the sets you enable                               |
| Used for              | fleet group selectors (`os_family = "linux"`) | inventory, and deciding what to monitor                |
| Sent to a fleet server | on every state report                        | not yet                                                |

A fleet selector matches a tag whole, which is why `os_family` and `arch` are
tags. "Which volumes does this host have" is a list of records, and a
comma-joined tag such as `drives=c:,d:` is exactly the shape facts exist to get
away from.

---

## Enabling a fact set

A fact set is enabled **in the module that produces it**, next to that module's
other settings. The switch cannot exist without the module, so there is no way
to enable a set and get nothing back because its module is not loaded.

```ini
[/modules]
CheckSystem = enabled
CheckDisk = enabled

; CheckSystem on Windows. On Linux: [/settings/system/unix/facts]
[/settings/system/windows/facts]
os = true
hardware = true
network.interfaces = true
software.installed = true

[/settings/disk/facts]
storage.volumes = true

; CheckHyperV, on a Hyper-V host.
[/settings/hyperv/facts]
hyperv.vms = true

; The one set the core produces itself.
[/settings/facts]
agent = true
```

Every switch is a bool defaulting to `false`, so each one is a separate key
that a [fleet bundle](../setup/fleet.md) can turn on without overwriting
another bundle's choices.

| Set                  | Produced by | Key, in section                                  | Cost |
|----------------------|-------------|--------------------------------------------------|------|
| `agent`              | the core    | `agent`, `[/settings/facts]`                     | none |
| `os`                 | CheckSystem | `os`, `[/settings/system/windows/facts]` or `[/settings/system/unix/facts]` | none: read once at start |
| `hardware`           | CheckSystem | `hardware`, same section                         | none: read once at start |
| `network.interfaces` | CheckSystem | `network.interfaces`, same section               | low: read every round, no WMI, nothing forked |
| `software.installed` | CheckSystem | `software.installed`, same section               | the highest here: every round, a walk of the registry's Uninstall hives or one forked package-manager query |
| `storage.volumes`    | CheckDisk   | `storage.volumes`, `[/settings/disk/facts]`      | low: the enumeration `check_drivesize drive=*` does |
| `hyperv.vms`         | CheckHyperV | `hyperv.vms`, `[/settings/hyperv/facts]`         | moderate: every round (never at startup), the seven WMI queries `check_hyperv_vms` runs, which grow with every VM and checkpoint and can stall while the Hyper-V management provider starts |

The full description of each switch is in the module's settings reference.
Turning a set off takes effect on the next settings reload: the module stops
returning the set, and the core drops it from the document. There is nothing
to clean up.

---

## What each set contains

A field that could not be determined is **omitted**, never written as an empty
string or zero. A consumer reads an absent key as unknown.

### `agent`

| Field      | Example                         | Meaning |
|------------|---------------------------------|---------|
| `version`  | `0.12.6`                        | the NSClient++ version |
| `modules`  | `["CheckDisk", "CheckSystem"]`  | the loaded modules, sorted, each once |
| `enrolled` | `true`                          | whether this host is enrolled with a fleet server: yes or no, never which server or with which identity |

### `os` and `hardware`

| Set        | Fields |
|------------|--------|
| `os`       | `family`, `name`, `version`, `arch`, `virtualization`, `domain` |
| `hardware` | `manufacturer`, `model`, `cpu_cores`, `memory_gb` |

Both are read once, when the module starts, because none of it can change
without a reboot. A manual refresh reads them again.

### `network.interfaces`

One record per network interface. The loopback interface is left out.

| Field          | Example                      | Meaning |
|----------------|------------------------------|---------|
| `id`           | `eth0`, `Intel(R) Ethernet Connection I219-V` | the interface: the kernel name on Linux, the adapter description on Windows |
| `display_name` | `Ethernet`                   | the connection name (Windows only) |
| `mac`          | `00:1a:2b:3c:4d:5e`          | the hardware address, lowercase and colon-separated on every platform |
| `status`       | `up`                         | the RFC 2863 link state: `up`, `down`, `dormant`, `lowerlayerdown`, `notpresent`, `testing`, `unknown` |
| `speed_bps`    | `1000000000`                 | the negotiated link speed in bits per second |
| `addresses`    | `["192.168.1.10", "fe80::1"]`| the IPv4 and IPv6 addresses, without an IPv6 zone suffix |

There are no traffic counters. They change every second, which would make
every round a change. [`check_network`](../reference/check/CheckSystem.md) is
where traffic is monitored. The Windows XP build reports no `speed_bps`.

### `storage.volumes`

One record per volume that `check_drivesize drive=*` would report.

| Field        | Example                  | Meaning |
|--------------|--------------------------|---------|
| `id`         | `/`, `C:\`               | the mount point: the value `check_drivesize` calls `drive` |
| `device`     | `/dev/sda1`, `\\?\Volume{…}\` | the block device on Linux, the volume GUID path on Windows |
| `filesystem` | `ext4`, `NTFS`           | the filesystem |
| `type`       | `fixed`                  | the `check_drivesize` type: `fixed`, `remote`, `removable`, `cdrom`, `ramdisk`, `unknown` |
| `label`      | `Data`                   | the filesystem label |
| `size_bytes` | `107374182400`           | the total size |

There is no free space, for the same reason there are no traffic counters.
A remote volume (NFS, SMB, a mapped drive) is listed without `size_bytes`.
Asking a dead server for a size would stall the round until it times out.

### `software.installed`

One record per installed program, from the same source
[`check_installed_software`](../reference/check/CheckSystem.md) reads: the
registry's `Uninstall` hives on Windows - the 64-bit and 32-bit machine views
and every loaded per-user hive, never `Win32_Product` - and the host's own
package manager (dpkg, rpm or pacman) on unix.

| Field          | Example                      | Meaning |
|----------------|------------------------------|---------|
| `id`           | `Google Chrome`, `bash`      | the name, with the version appended when the host has two installs of it |
| `name`         | `Google Chrome`              | what the platform calls it: the `name` of `check_installed_software` |
| `version`      | `129.0.6668.101`             | as the platform recorded it; never parsed or compared as a number |
| `publisher`    | `Google LLC`                 | the publisher (Windows), the maintainer or vendor (unix) |
| `architecture` | `x86_64`, `x86`, `noarch`    | the [`os`](#os-and-hardware) set's vocabulary, plus `noarch` for a package that has none (rpm's `noarch`, dpkg's `all`, pacman's `any`) |
| `source`       | `registry`, `dpkg`           | which database the record came from |
| `scope`        | `machine`                    | `machine` or `user`, by the hive it was installed into. Windows only: a unix package manager installs for the machine |
| `install_date` | `2026-09-09`                 | a date, not a time, and only where the platform records one |
| `size_bytes`   | `5904384`                    | the installed size, where it is recorded |

What it leaves out is as deliberate as what it carries. Entries Windows hides
from Programs and Features (`SystemComponent`) are not listed: they are the
runtimes and bookkeeping keys a component left behind, and they roughly double
the record count. One product installed into several user hives is one record,
because the host has that software on it once. And nothing here says whether
anything is *running* - that is a check, and it would change the document every
round.

This is the one set with a size limit of its own. A Linux desktop has a few
thousand packages, and the whole document has to fit `[/settings/facts] max
size`; a set that would not fit is rejected whole, which would leave the host
with no inventory at all. So the list stops at **2500 records**, and the set
then carries an error under `errors` saying how many were found.

### `hyperv.vms`

One record per virtual machine on a Hyper-V host, from the same
`root\virtualization\v2` classes
[`check_hyperv_vms`](../reference/windows/CheckHyperV.md) reads.

| Field                  | Example                                | Meaning |
|------------------------|----------------------------------------|---------|
| `id`                   | `web-01`                               | the VM name: the value `check_hyperv_vms` calls `vm`, with the VM's GUID appended when the host has two VMs of that name |
| `name`                 | `web-01`                               | the VM name as Hyper-V shows it |
| `vm_id`                | `1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e` | the VM's GUID |
| `generation`           | `2`                                    | the VM generation |
| `version`              | `9.0`                                  | the configuration version |
| `vcpus`                | `4`                                    | the configured virtual processors |
| `memory_startup_bytes` | `2147483648`                           | the configured startup memory |
| `dynamic_memory`       | `true`                                 | whether dynamic memory is on |
| `memory_minimum_bytes` | `536870912`                            | the dynamic memory floor (dynamic memory only) |
| `memory_maximum_bytes` | `8589934592`                           | the dynamic memory ceiling (dynamic memory only) |
| `checkpoints`          | `2`                                    | how many checkpoints (snapshots) the VM has |
| `replication_mode`     | `primary`                              | the Hyper-V Replica role: `none`, `primary`, `replica`, `test_replica`, `extended_replica` |

There is no power state, heartbeat, uptime, load or assigned memory: those
change from one round to the next and are what `check_hyperv_vms` monitors.
On a host without the Hyper-V role the set is enabled but cannot be
collected, and says so under `errors` every round. The same goes for a
stopped management service (vmms), and for an account that may not see the
virtual machines: Hyper-V hides them from it without an error, so an empty
list would claim the host has none. The set reports that under `errors` and
the last list collected is kept.

The set is not read on the startup round. That round runs on the thread
that starts the service, and the first query into the virtualization
namespace starts the Hyper-V management provider when it is not already
running, which can take long enough to hold up every producer after it. So
at startup the set says so under `errors`, and it is collected on the first
scheduled round (every `[/settings/facts] interval`), on a settings reload,
or right away with `facts refresh` / `POST /api/v2/facts/commands/refresh`.

### Record ids match check instance names

A list record's `id` is the same string that the corresponding check uses to
name the instance. `storage.volumes[].id` is the `drive` of `check_drivesize`.
`network.interfaces[].id` is the `name` of `check_network`.
`software.installed[].id` is the `name` of `check_installed_software` - with
the version appended in the one case where the host has two installs sharing a
name, because an id has to be unique in its list. `hyperv.vms[].id` is the `vm`
of `check_hyperv_vms`, with the GUID appended for the same reason when two VMs
share a name. The ids are stable across
rounds, so a consumer can diff two documents record by record.

---

## When facts are collected

| When                     | What happens |
|--------------------------|--------------|
| Start                    | one round, before the first fleet report |
| Every `[/settings/facts] interval` (default `1h`) | a round; a set whose values have not changed does not move the document's revision |
| A settings reload        | a round, because a set may have been switched on or off |
| On demand                | `facts refresh` in `nscp test`, or `POST /api/v2/facts/commands/refresh` |

Each set a module produces carries a *gathered* time: when its values were
read off the machine, which for `os` and `hardware` is when the module started.
The core builds `agent` on the round itself, so the round's time is its age. A set that is enabled
but could not be collected keeps its previous value, and the reason is
reported under `errors`, so a transient failure never blanks the inventory.

---

## Reading facts

```
nscp> facts
Revision: 4  Checked: 2026-09-24T13:19:40Z
Enabled: agent, network, os, storage
Gathered:
  network: 2026-09-24T13:19:40Z
  os: 2026-09-24T13:19:40Z
  storage: 2026-09-24T13:19:40Z

  agent:
    enrolled: false
    modules:
      - CheckDisk
      - CheckSystem
    version: 0.12.6
  network:
    interfaces:
      - eth0
          addresses:
            - 192.0.2.2
          mac: 02:fc:00:00:00:01
          status: up
  os:
    arch: x86_64
    family: linux
    name: Ubuntu 24.04.4 LTS
    version: 6.18.44
    virtualization: virtual
  storage:
    volumes:
      - /
          device: /dev/vda
          filesystem: ext4
          size_bytes: 270553174016
          type: fixed
```

`facts <path>` shows one subtree (`facts storage.volumes`), and `facts refresh`
collects now. Over REST the same document is `GET /api/v2/facts` (see
[Facts](../api/rest/facts.md)), and the web UI shows it on the Facts page.

---

## The document rules

The core checks every set against these rules before it accepts it. A set that
breaks a rule is rejected whole, and the previous value of that set is kept.

* **Keys** are `snake_case` ASCII, at most 64 characters. A key that carries a
  unit ends with it (`size_bytes`, `speed_bps`).
* **Values** are strings, numbers or booleans. There is no `null`: an unknown
  value is left out.
* **Lists** are lists of records, each with an `id` unique in its list and
  stable across rounds, or plain lists of strings (`addresses`, `modules`).
* **Size**: depth at most 6, at most 5000 records in a list, and the whole
  document within `[/settings/facts] max size` (1 MiB by default).

---

## What facts never contain

Facts describe the machine, not the configuration. They never contain
settings, credentials, command lines or environment variables. That holds
inside a set too: `software.installed` carries what is installed and which
version of it, never the install path or the uninstall command line
`check_installed_software` can also show. The loaded module list in `agent` is
the one configuration-adjacent value, and it is opt-in like everything else.
