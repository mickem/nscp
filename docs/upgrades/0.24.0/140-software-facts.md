---
icon: "📦"
modules: [CheckSystem]
action: none
---
**A software inventory: the `software.installed` fact set.** Nothing to do.
Like every other fact set it collects nothing until you turn it on.

```ini
; CheckSystem. On Linux: [/settings/system/unix/facts]
[/settings/system/windows/facts]
software.installed = true
```

One record per installed program, with the name as its id - the same value
`check_installed_software` calls `name`:

| Field | Example | Meaning |
|---|---|---|
| `id` / `name` | `Google Chrome`, `bash` | what the platform calls it |
| `version` | `129.0.6668.101` | as recorded, never parsed |
| `publisher` | `Google LLC` | publisher (Windows), maintainer or vendor (unix) |
| `architecture` | `x86_64`, `noarch` | one vocabulary on every platform |
| `source` | `registry`, `dpkg`, `rpm`, `pacman` | which database the record came from |
| `scope` | `machine`, `user` | Windows only: which hive it was installed into |
| `install_date` | `2026-09-09` | a date, where the platform records one |
| `size_bytes` | `5904384` | installed size, where it is recorded |

The list comes from the same source `check_installed_software` reads: the
registry's Uninstall hives on Windows (never `Win32_Product`), and the host's
own package manager on unix. Windows entries hidden from Programs and Features
(`SystemComponent`) are left out, and one product installed into several user
hives is one record.

This is the largest and most expensive set: a few hundred records on a Windows
host, thousands on a Linux one, re-read on every facts round because software
is installed and removed while the agent runs. A host with more packages than
fit the facts document reports the first 2500 and says so under `errors`,
rather than having the whole set rejected over the size budget. See
[Host Facts](../concepts/facts.md) for every set, its fields and its cost.
