---
icon: "📤 🔒"
modules: [CheckMKServer]
action: conditional
---
**Checkmk agent: Linux CPU and kernel sections, full memory data and process owners.**
The agent dump `CheckMKServer` serves on port 6556 now matches what the
official Checkmk agent sends for the sections NSClient++ already had, and adds
the Linux CPU sections:

| Section | OS | Change |
|---|---|---|
| `<<<cpu>>>` | Linux | New: load averages, CPU count and thread limit (`CPU load`, `Number of threads`). |
| `<<<kernel>>>` | Linux | New: `/proc/vmstat` and `/proc/stat` (`CPU utilization`, `Kernel Performance`). |
| `<<<mem>>>` | Linux | The whole of `/proc/meminfo` instead of four derived lines. |
| `<<<mem>>>` | Windows | `PageTotal`/`PageFree` (commit limit) and `VirtualTotal`/`VirtualFree` added; `SwapTotal`/`SwapFree` are 0, as the official agent sends them. |
| `<<<df>>>` | Linux | Built by `check_drivesize fetch-only` instead of running `df`, and now carries inode counts. Requires `CheckDisk`. |
| `<<<ps>>>` | both | The user field holds the process owner instead of being empty. |
| `<<<ps>>>` | Windows | Every process has a command: one the agent cannot open (System, Registry, and other accounts' processes when not elevated) is named from a process snapshot, where it used to be sent without one and Checkmk dropped it. |
| `<<<services>>>` | Windows | Delayed and trigger-start services are sent as `auto` and spaces in a service name as `_`, as the official agent does; Checkmk's Service Summary counted only plain `auto` services and misparsed names with spaces. |
| `<<<uptime>>>`, `<<<systemtime>>>` | both | Read when the dump is finished rather than from the metrics store or at its start, so the System Time offset and the boot time no longer drift by the seconds it takes to build the dump. |
| `<<<check_mk>>>` | both | `OSType`, `OSName` and `OSVersion` (and `OSPlatform` on Linux), which Checkmk turns into `cmk/os_*` host labels. |
| `<<<mem>>>`, `<<<uptime>>>` | Windows | Exact values. They were rounded to six significant digits (32 GiB of RAM was off by ~30 kB, and an uptime past 11.5 days was sent in scientific notation, e.g. `1.03457e+06`). The same applies to every number a Lua script reads through `Metrics()`. |

If a Checkmk site already monitors hosts through NSClient++, run service
discovery on them after upgrading: the memory service moves to Checkmk's
Linux or Windows memory check, and the new CPU services appear. A process
discovery rule or check that matches on the user now sees the owner (an
account name, or the numeric uid / SID when it has no name) rather than an
empty field; see the
[security notice](../security/notices.md#check_process-fetch-only-and-the-checkmk-ps-section-now-name-each-processs-owner).
On Linux, enable `CheckDisk` to keep the `<<<df>>>` section.
