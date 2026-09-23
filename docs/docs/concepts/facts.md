# Tags and Facts

NSClient++ can describe the host it runs on in two shapes. They look similar
and are used for very different things.

| | Tags | Facts |
|---|---|---|
| Shape | flat `key=value` strings | a JSON tree: sections, scalars, lists of records |
| Size | 256 entries, 1 KiB per value | one document per host, 1 MiB by default |
| Purpose | fleet **group selectors** | **inventory**: what the machine is |
| Default | modules publish a handful unconditionally | **nothing until a fact set is enabled** |
| Read on | `/api/v2/tags` | [`/api/v2/facts`](../api/rest/facts.md) |
| Sent to the fleet server | in full, on every state report | a hash on every report; the document only when it changes |

A selector needs a value that is flat, cheap and always there — `sqlserver =
"detected"`, `drives = "c:,d:"`. An inventory needs structure: which volumes
exist, with their sizes and filesystems; which interfaces, with their
addresses. Neither replaces the other, and enabling facts changes nothing
about tags.

## Nothing is collected until you ask

This is the part worth knowing before anything else: **a fresh install
collects no facts at all.** Loading `CheckSystem` does not enable `os`.
Enrolling with a fleet server does not enable anything. The agent reports the
hash of an empty document and nothing else.

Inventory is data about your machines that you did not necessarily agree to
ship, so you — or a fleet bundle you accepted — turn it on, per fact set:

```ini
[/settings/facts]
os = true
storage.volumes = true
```

## Fact sets

A **fact set** is the unit that is enabled, produced and documented. It is
named by a dotted id of one or two parts: `os`, `hardware`,
`software.installed`. Two parts exist where one module splits enablement
finer than its top-level key — the installed-software list is the expensive
one, and you should be able to collect `software.hotfixes` without it.

To see what this agent can collect, with what each set holds and what it costs:

```
$ nscp test
…
facts list
ID                  STATE     PRODUCER     DESCRIPTION
agent               disabled  core         What this agent is: its version, the host name it reports, …
hardware            disabled  CheckSystem  What this machine is made of: vendor, model, serial, …
identity            disabled  CheckSystem  What this machine calls itself: host name, canonical FQDN, …
network.interfaces  disabled  CheckSystem  Every network interface: name, MAC address, IP addresses, …
os                  disabled  CheckSystem  What this machine runs: family, distribution name and version, …
storage.volumes     disabled  CheckDisk    Every mounted filesystem this host has: mount point …
```

`facts` prints the document and `facts refresh` collects now. The same three
are on the REST API, and the web UI's **Inventory** page renders them.

### The sets available today

| Id | Produced by | Holds |
|---|---|---|
| `agent` | the core | agent version, reported host name, loaded modules, whether the host is enrolled |
| `os` | CheckSystem | family, name, version, kernel, architecture, boot time; on Windows also whether a reboot is pending and the BIOS version |
| `identity` | CheckSystem | host name, FQDN, DNS domain; on Windows also the NetBIOS name and the domain-join state |
| `hardware` | CheckSystem | vendor, model, serial, asset tag, chassis, CPU and memory; on Windows also the populated memory modules |
| `network.interfaces` | CheckSystem | one record per interface: MAC, addresses, link speed, link state |
| `storage.volumes` | CheckDisk | one record per mounted filesystem: device, filesystem, size, type |

The same id means the same thing on both platforms. Where a platform has
nothing to report the key is simply absent rather than filled with a
placeholder — `identity.domain_joined` exists on Windows and not on Linux,
because Linux has no such state to report.

## What the document looks like

```json
{
  "agent":    { "version": "0.20.0", "hostname": "web-01", "modules": ["CheckDisk", "CheckSystem"], "enrolled": true },
  "os":       { "family": "linux", "name": "Ubuntu 24.04.1 LTS", "version": "24.04",
                "kernel": "6.8.0-45-generic", "arch": "x86_64", "boot_time": "2026-09-01T04:12:09Z" },
  "storage":  { "volumes": [ { "id": "/", "fs": "ext4", "size_bytes": 255000000000, "type": "fixed" } ] },
  "network":  { "interfaces": [ { "id": "eth0", "mac": "02:fc:00:00:00:01",
                                  "addresses": ["10.0.0.5"], "speed_bps": 10000000000, "state": "up" } ] }
}
```

The rules the agent enforces on every set, so that a server can rely on them:

* **Keys** are `snake_case` ASCII, at most 64 characters. A key that carries a
  unit ends with it (`size_bytes`, `speed_bps`, `speed_mhz`).
* **Unknown values are omitted**, never written as `null` or `""`. An absent
  key means the host did not report one; it never means zero or empty.
* **Every record in a list has an `id`** that is unique in the list and stable
  across runs — the mount point, the interface name, the package name. This is
  what lets a server diff a list instead of seeing every refresh as a full
  replacement.
* **Timestamps** are ISO 8601 UTC; dates without a time are `YYYY-MM-DD`.
* **Bounds**: depth 6, 5000 records per list, 1 MiB per document.

A set that breaks a rule is rejected whole and its previous value is kept, with
the reason logged and reported under `errors` on the API — a half-applied set
would describe a host state that never existed.

### Record ids are the check's instance names

A record's `id` is deliberately the same string the corresponding check uses
for that instance: `storage.volumes[].id` is `check_drivesize`'s `drive`,
`network.interfaces[].id` is `check_network`'s interface name. So a failing
check and the inventory record it concerns name the same thing, without a
lookup table.

## Refreshing

Facts are collected once at startup, then every `[/settings/facts] interval`
(default `1h`), and again after a settings reload — which is what makes a
fleet bundle that enables a set take effect without a restart. `facts refresh`
and `POST /api/v2/facts/refresh` collect on demand.

Inventory changes slowly, so the interval is long by design. A producer whose
data is expensive paces itself further on top of it.

| Key | Default | Meaning |
|---|---|---|
| `interval` | `1h` | How often every enabled fact set is refreshed. |
| `max size` | `1048576` | Largest document, in bytes. A set that would push it past this is rejected. |

There is deliberately no global `enabled` switch: an empty section is "off",
which keeps the reading of the configuration unambiguous.

## Fleet-managed inventory

Because enablement is plain configuration, a fleet bundle turns inventory on
for a whole group with the INI it already renders:

```ini
[/settings/facts]
os = true
software.installed = true
```

No new protocol is involved. Bundles are applied as a merge patch, per key, so
one bundle can enable `os` and another `storage.volumes` without either
clobbering the other.

## What facts never contain

Facts are inventory, not configuration. They never carry credentials, command
lines, environment variables or the agent's configuration, and a producer that
could leak one — a task or process list — strips the arguments. The one
configuration-adjacent item, the loaded-module list in `agent`, is opt-in like
everything else.

The state report the agent sends to a fleet server still carries only whether
a local configuration file is present, never its contents; the facts document
does not change that. What the report gains is the document's hash, so the
server can tell that inventory changed without being sent it.
