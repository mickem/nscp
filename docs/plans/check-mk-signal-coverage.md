# Checkmk signal coverage — plan and gap register

**Scope:** what NSClient++ must emit for a Checkmk site to get the same signal set
it gets from the official agent, what we can already build from the checks and
metrics added since the last review, and what is still missing.

**Reference points used:** Checkmk's own `agents/check_mk_agent.linux` (v3.0.0b1)
and `agents/wnx/install/resources/check_mk.yml` (the Windows agent's built-in
`sections:` list), plus the `wnx` providers for `mem`, `df` and `services`.

---

## 1. How the dump is produced today

`CheckMKServer` accepts a TCP connection on 6556 and hands a `check_mk::packet`
to `scripts/lua/default_check_mk.lua`, which fills it from three sources:

| Source                    | Lua handle       | Used for                                    |
|---------------------------|------------------|---------------------------------------------|
| Metrics store (1 Hz)      | `Metrics()`      | `uptime`, `mem`                             |
| Check command `fetch-only`| `Core()`         | `ps`, `services`, `df` (Windows)            |
| Settings + submissions    | `Settings()` / `Submissions()` | `local`, `mrpe` (sync + cached) |
| Shelling out              | `io.popen`       | `df` (Linux)                                |

The packet layer already supports more than the script uses: section separators
(`:sep(N)`), `:cached(gen,ttl)`, `:persist(epoch)` and piggyback blocks
(`<<<<host>>>>`), all exposed to Lua via `set_separator` / `set_cached` /
`set_persist` / `add_piggyback`. **None of the decorations except `cached` on
local/mrpe entries are currently used, and piggyback is entirely unused.**

`Metrics():get(prefix)` does substring matching and returns a key→value table, so
enumerating e.g. every `system.network.*` key from Lua already works.

---

## 2. Section inventory — what we emit today

| Section        | Platform | Source                        | State                                              |
|----------------|----------|-------------------------------|----------------------------------------------------|
| `check_mk`     | both     | Lua literal                   | Minimal: `Version`, `AgentOS`, `Hostname` only     |
| `systemtime`   | Windows  | `os.time()`                   | OK (should also be emitted on Linux — it is not)   |
| `uptime`       | both     | `system.uptime.boot.raw`      | OK, but silently absent until the collector warms  |
| `mem`          | both     | `system.mem.*`                | Incomplete on Windows, **badly** incomplete on Linux |
| `df`           | Windows  | `check_drivesize fetch-only`  | Space-separated (agent uses TAB)                   |
| `df`           | Linux    | `io.popen("df -PT …")`        | Legacy `df`, no inodes, shells out                 |
| `services`     | both     | `check_service fetch-only`    | **Wrong section on Linux** (see G1.3)              |
| `ps`           | both     | `check_process fetch-only`    | Empty user field, non-standard cputime             |
| `local`        | both     | settings + `check_mk-local`   | OK                                                 |
| `mrpe`         | both     | settings + `check_mk-mrpe`    | OK                                                 |

That is **10 sections**. A stock Linux agent emits ~25 built-ins before plugins;
a stock Windows agent has **24** built-in section providers.

---

## 3. Reference: what the real agent emits

### Linux (`check_mk_agent.linux` 3.0.0b1) — core built-ins

`check_mk`, `labels:sep(0)`, `df_v2`, `mounts`, `cpu`, `mem`, `uptime`,
`kernel`, `diskstat`, `lnx_if` (+ `:sep(58)`), `tcp_conn_stats`,
`systemd_units`, `ps_lnx`, `lnx_thermal:sep(124)`, `ntp` / `chrony` /
`timesyncd`, `fileinfo:sep(124)`, `job`, `md`, `multipath`,
`nfsmounts_v2:sep(0)`, `cifsmounts`, `local:sep(0)`, `mrpe`,
`cmk_agent_ctl_status:sep(0)`, `checkmk_agent_plugins_lnx:sep(0)`.

Plus opportunistic hardware/cluster sections (`megaraid_*`, `storcli_*`,
`3ware_*`, `zpool*`, `drbd`, `heartbeat_*`, `pvecm_*`, `vbox_guest`, `nvidia`,
`postfix_mailq`, `varnish`, `haproxy`, `omd_*`) that only fire when the
underlying tool is installed.

### Windows (`check_mk.yml` `sections:`) — all 24 built-ins

`check_mk`, `mrpe`, `skype`, `spool`, `plugins`, `local`, `winperf`
(→ `winperf_phydisk`, `winperf_if`, `winperf_processor`), `uptime`,
`systemtime`, `df`, `mem`, `services`, `msexch`, `dotnet_clrmemory`,
`wmi_webservices`, `wmi_cpuload`, `ps`, `fileinfo`, `logwatch`,
`openhardwaremonitor`, `agent_plugins`, `w32time_status`, `w32time_peers`.

### Exact formats worth pinning down

```text
# Windows mem (wnx/providers/mem.cpp) — 8 lines, not 4
MemTotal / MemFree          = ullTotalPhys / ullAvailPhys
SwapTotal / SwapFree        = (ullTotalPageFile - ullTotalPhys) / (ullAvailPageFile - ullAvailPhys)
PageTotal / PageFree        = ullTotalPageFile / ullAvailPageFile
VirtualTotal / VirtualFree  = ullTotalVirtual / ullAvailVirtual

# Windows df (wnx/providers/df.cpp) — TAB separated
{volume}\t{fstype}\t{size_kb}\t{used_kb}\t{avail_kb}\t{pct}%\t{mountpoint}

# Windows services (wnx/providers/services.cpp)
{name} {state}/{start_type} {display_name}

# Linux mem — /proc/meminfo verbatim minus '^Swap:|^Mem:|total:'
# Linux cpu  — "<loadavg line> <num_cpus>" then /proc/sys/kernel/threads-max
# Linux kernel — epoch, then /proc/vmstat and /proc/stat verbatim
# Linux diskstat — epoch, then filtered /proc/diskstats
```

Our Windows `CheckMemory::getMemoryStatus()` already maps these correctly:
`phys` → Mem\*, `page` → Swap\* (it already subtracts phys), `commited` →
Page\*, `virt` → Virtual\*. So the Windows `mem` fix is purely additive.

---

## 4. Gap register

### G1 — Defects in sections we already emit

| # | Gap | Impact | Effort |
|---|-----|--------|--------|
| G1.1 | **Linux `mem` has no swap at all.** The Lua reads `system.mem.page.total/avail`, which only exists on Windows; the Unix collector publishes `system.mem.{physical,cached,swap}.*`. So the Linux dump is two lines: `MemTotal`, `MemFree`. | Linux "Memory" service has no swap, no cache/buffer accounting; `mem.linux` cannot discover — only the crude `mem.used` fallback works. | S |
| G1.2 | **Windows `mem` is missing `PageTotal`/`PageFree`/`VirtualTotal`/`VirtualFree`.** Values are correct, the set is short. | No commit-charge levels on the Windows "Memory" service. | S |
| G1.3 | **Linux emits `<<<services>>>`**, a Windows-only section. Checkmk routes it to the `windows_services` plugin, which then invents a "Service Summary" out of systemd state words it doesn't model. The site test asserting `Service Summary` on Linux is asserting this bug. | Wrong service model on every Linux host; no systemd unit monitoring. | M |
| G1.4 | **Windows `df` is space-separated.** The real agent uses TAB precisely because volume labels and folder mount points contain spaces. | Volumes mounted in folders / labelled volumes produce corrupt filesystem items. | S |
| G1.5 | **Linux `df` shells out to `/bin/df`** and emits the legacy `df` section. | Per-fetch fork; no inode monitoring (`df_v2` `[df_inodes_start]` block); no `lsblk` UUID mapping. `check_drivesize --fetch-only` already exists on Linux and is unused here. | S |
| G1.6 | **`ps` has an empty user field and a raw-seconds cputime** (`(,vsz,rss,<secs>,pid) cmd`). The agent emits `user` and `[[dd-]hh:]mm:ss`. | Checkmk process rules that match on user can't work; CPU-time-based rules read wrong or drop entries. Needs validation against a live site. | M |
| G1.7 | **`uptime` and `mem` silently vanish on a cold start** because they depend on the 1 Hz collector having ticked. | First discovery after a restart can miss services. Both are cheap to read directly. | S |
| G1.8 | **`check_mk` header is three lines.** No `OSType`/`OSPlatform`/`OSName`/`OSVersion` (these drive Checkmk host labels), no `OnlyFrom`, no directory keys, and `Version: nsclient++` is not a parseable agent version. | No automatic OS labels; the "Check_MK Agent" service can't reason about agent version/updates. | S |
| G1.9 | **No `systemtime` on Linux.** Emitted only under `if IS_WIN`. | No clock-skew detection on Linux hosts. | XS |

### G2 — Core sections missing where we already have the data

| Section | Platform | Data we already have | Note |
|---------|----------|----------------------|------|
| `cpu` | Linux | `check_load` reads loadavg; core count in `system.cpu.*` | One line. Gives the standard "CPU load" service. |
| `kernel` | Linux | `check_kernel_stats` reads `/proc/stat`; `check_swap_io` reads `/proc/vmstat` | Section is literally those two files + epoch. Gives context switches, process creation, page-in/out. |
| `mounts` | Linux | `check_mount` | Mount-option drift detection (ro-remount alarms). |
| `systemd_units` | Linux | `check_service` (systemd backend) | Replaces G1.3. Needs the `[uptime]` + `[show]` block format. |
| `df_v2` | both | `check_drivesize --fetch-only` | Add fstype/inodes; switch Windows to TAB. |
| `lnx_thermal` | Linux | `check_temperature` (reads `/sys/class/thermal`) | `:sep(124)`, trip points included. |
| `tcp_conn_stats` | both | `check_connections` | Per-state connection counts. |
| `fileinfo` | both | `check_files` / `check_single_file` | `:sep(124)`, header/content blocks. Config-driven pattern list. |
| `openhardwaremonitor` | Windows | `check_temperature` | Only if the values line up with OHM's schema; otherwise ship as `local`. |
| `w32time_status`, `w32time_peers` | Windows | `check_w32time` | Direct 1:1 match with a check we already have. |
| `wmi_webservices` | Windows | `check_iis_sites` / `check_iis_app_pools` | IIS per-site counters. |

### G3 — Counter-based sections (blocked on E1)

These are the highest-value missing services and they all need **monotonic raw
counters**, which the metrics store does not carry (see E1).

| Section | Platform | Blocking issue |
|---------|----------|----------------|
| `lnx_if` | Linux | We publish `received`/`sent` as **bytes per second**; Checkmk wants raw `/proc/net/dev` counters plus an `ethtool`/sysfs speed+link block. |
| `winperf_if` | Windows | Same: `BytesReceivedPersec` etc. are rates. Checkmk wants the raw PDH counter 510 blob. |
| `diskstat` | Linux | `check_disk_io` publishes `read_bytes_per_sec`, `reads_per_sec`, latencies — all derived. Checkmk wants raw `/proc/diskstats`. |
| `winperf_phydisk` | Windows | Raw PDH counter 234. |
| `winperf_processor` / `wmi_cpuload` | Windows | Raw PDH counter 238 / WMI load. We have `system.cpu.*` idle/user/kernel deltas but not in the expected shape. |

### G4 — Sections needing new data acquisition

| Section | Platform | Nearest thing we have | Gap |
|---------|----------|------------------------|-----|
| `logwatch` | both | `check_eventlog`, `check_logfile` | Needs stateful "since last fetch" bookmarking per source + the `[[[source]]]` / `C W O` line classification. Biggest single feature here. |
| `windows_updates` | Windows | `check_os_updates` (already has count/security/critical/reboot_required) | Format conversion only, plus a cached-section TTL — this check is slow. |
| `apt` / `zypper` | Linux | `check_os_updates` | Same. |
| `ntp` / `chrony` / `timesyncd` | Linux | `check_ntp_offset` | Ours queries NTP over the wire; the section wants the local daemon's peer table. |
| `smart` / `smart_posix` | both | `check_disk_health` (free space + I/O only) | No SMART attribute source at all. |
| `md`, `multipath`, `nfsmounts_v2`, `cifsmounts` | Linux | — | New readers; all are cheap file/command reads. |
| `docker_node_info`, `docker_node_disk_usage`, `docker_container_*` | both | `check_docker`, `check_docker_info`, `check_docker_stats`, `check_docker_df` | Container sections belong in **piggyback blocks** — the packet layer supports it, nothing uses it yet. First real user for `add_piggyback`. |
| `mssql_*` | Windows | `CheckMSSQL` (`check_mssql_databases`, `_backup`, `_jobs`) | Format conversion; a genuine differentiator vs. the stock agent, which needs a Python plugin for this. |
| `win_printers` | Windows | `check_printqueue`, `check_printjobs` | Plugin-supplied section upstream; we can serve it natively. |
| `windows_tasks` | Windows | `check_tasksched` | Same. |
| `win_license` | Windows | `check_activation` | Same. |
| `dotnet_clrmemory`, `msexch_*`, `skype` | Windows | `check_pdh` | Reachable through PDH/WMI but no dedicated check today. |
| `winperf_ts_sessions` | Windows | `check_rds_sessions`, `check_rds_session_load` | Format conversion. |
| `job` | Linux | — | Reads `$MK_VARDIR/job/<user>/*`. Only relevant if we ship a `mk-job` equivalent. |

### G5 — Protocol and transport gaps

These are already listed under "What's NOT supported (yet)" in
`docs/docs/scenarios/check-mk.md`, restated here with the plan implications:

- **Symmetric encryption** (AES-256-CBC + pre-shared password). Sites configured
  with encryption cannot use us at all. Self-contained, well-specified, small.
- **Real-Time Checks** (UDP 6559, `mem` / `df` / `winperf_processor` at 1 Hz).
  Our 1 Hz collector is a natural fit; the encryption work is a prerequisite
  because RTC payloads are always encrypted.
- **`cmk-agent-ctl` mTLS / registration.** The modern default in Checkmk 2.x.
  Large: TLS handshake, registration state, and the `cmk_agent_ctl_status`
  section. Sites can still fall back to "legacy pull mode", which is what we
  rely on today — that fallback is what makes this deferrable, not optional
  forever.
- **Per-section enable/disable, `disabled_sections`, and section timeouts.**
  Every real agent has this; we have no section-level configuration at all.

### G6 — Labels and HW/SW inventory

- `labels:sep(0)` — one JSON line, trivially derivable (`cmk/device_type` from a
  hypervisor probe). Cheap win, improves host auto-classification.
- HW/SW inventory sections. `check_hardware`, `check_os_version` and
  `check_installed_software` collect most of what the inventory plugins want.
  This is a separate track with its own section family and should not be mixed
  into the monitoring rollout.

### G7 — Explicitly out of scope

`megaraid_*`, `storcli_*`, `3ware_*`, `zpool*`, `drbd`, `heartbeat_*`,
`pvecm_*`, `varnish`, `haproxy`, `omd_*`, `nvidia`, `postfix_mailq`,
`qmail_stats`, `openvpn_clients`, `libelle_*`. All are opportunistic vendor/
application probes; the `local`/`mrpe` bridge covers them adequately for anyone
who needs them.

---

## 5. Enabling work

### E1 — Raw counters, not rates *(blocks all of G3)*

Every collector we added publishes **derived per-second rates**
(`read_bytes_per_sec`, `BytesReceivedPersec`, `rx_bytes_per_sec`, …). Checkmk
computes its own rates from monotonic counters and needs the raw values plus a
timestamp. There is no way to reconstruct a counter from a rate.

Two options, and they should be mixed:

- **Linux — read at dump time.** `/proc/net/dev`, `/proc/diskstats`, `/proc/stat`,
  `/proc/vmstat`, `/proc/meminfo` are single cheap reads. Emitting them verbatim
  is both the fastest and the most faithful path, and it removes the cold-start
  dependency (G1.7). No collector change needed.
- **Windows — publish raw alongside rates.** PDH already hands us raw counter
  values before we difference them; add `…​.raw` metrics (or better, expose the
  raw blob through the section provider in E2) rather than reverse-engineering
  rates.

### E2 — A first-class section provider, beyond `fetch-only`

`fetch-only` was a pragmatic hack and it has hit its limits: it hijacks a check
command's argument list, returns one untyped blob in the message field, and
cannot express a section separator, a cache header, more than one section, or a
piggyback target. It also forces every section through a Lua round trip holding
the GIL — the contention that caused the fix in b3452dd.

Proposal: mirror the existing metrics pattern. A module declares
`"sections": "produce"` in `module.json` and implements `fetchSections()`,
returning `{name, separator, cached_ttl, piggyback_host, lines[]}`.
`CheckMKServer` collects these directly in C++ and merges them with the Lua
packet. Benefits:

- C++ data stays in C++; the Lua script keeps doing what it is good at
  (`local`/`mrpe`, user customisation) and stops being on the hot path for
  system sections.
- Section decorations become expressible without new Lua API surface.
- Per-section caching and enable/disable get one obvious home.
- `fetch-only` stays for backwards compatibility but stops growing.

### E3 — Section configuration and caching

Add `/settings/check_mk/server/sections` with per-section `enabled` and
`cache ttl`, and honour `cached(gen,ttl)` on system sections (already supported
by `packet::section`). Required before shipping anything expensive
(`windows_updates`, `wmi_webservices`, `msexch_*`, WMI-backed sections).

### E4 — Validation

`tests/check_mk-site.test.ts` already stands up a real Checkmk site and runs
`cmk -d` / `cmk -II` / `cmk -v`, and already asserts no `Invalid data:`. This is
the right harness — it just needs to grow from 9 substring assertions to a
per-section expectation table, and to be run in CI rather than behind
`RUN_CMK_SITE_TEST=1`. **Every section added below must land with a site-test
assertion naming the service Checkmk is expected to discover.** Note that the
current `expect(check).toContain("Service Summary")` assertion on Linux is
asserting G1.3 and must be replaced, not kept.

---

## 6. Phased plan

### P0 — Fix what we already ship *(no new infrastructure)*
G1.1 Linux swap in `mem`, G1.2 Windows `PageTotal`/`VirtualTotal`, G1.4 TAB in
Windows `df`, G1.5 Linux `df` via `check_drivesize --fetch-only`, G1.7 read
uptime/mem directly instead of via the collector, G1.8 fill out the `check_mk`
header (incl. a parseable `Version:` — note both check_mk tests assert
`Version: nsclient++` and must be updated), G1.9 `systemtime` on Linux.
Add G2's one-liners while in here: `cpu`, `mounts`, `labels`.

*Outcome: the sections we claim to support actually produce correct services.*

### P1 — Infrastructure
E2 section provider + E3 per-section config/caching, then E4 site-test
expectation table. Nothing user-visible; everything after this depends on it.

### P2 — Correct the service model
G1.3 `systemd_units` (retire `services` on Linux), G1.6 `ps` user + cputime
format, `df_v2` with inodes, `kernel`, `tcp_conn_stats`, `lnx_thermal`,
`fileinfo`.

### P3 — Counters *(needs E1)*
`lnx_if`, `winperf_if`, `diskstat`, `winperf_phydisk`, `winperf_processor` /
`wmi_cpuload`. This is where the interface and disk graphs come from and is the
most visible remaining gap for anyone comparing us to the stock agent.

### P4 — Leverage the new checks
`w32time_status`/`w32time_peers`, `windows_updates` / `apt`, `win_printers`,
`windows_tasks`, `win_license`, `wmi_webservices`, `winperf_ts_sessions`,
`mssql_*`, docker sections via piggyback, `ntp`/`chrony`, `smart`.
Order within the phase by "does the stock agent need a plugin for this?" —
those are where we win.

### P5 — Protocol
Encryption → Real-Time Checks → `cmk-agent-ctl` mTLS. Separately: `logwatch`
(G4), which is large enough to be its own project and is the single most
requested thing the stock Windows agent does that we do not.

---

## 7. Decisions needed

1. **`Version:` string.** Reporting a Checkmk-shaped version makes the
   "Check_MK Agent" service behave, but implies an agent-update contract we do
   not implement. Report `Version: 2.x.y` and lie, or report our own version and
   accept the degraded service? Recommendation: our own version, formatted
   `<major>.<minor>.<patch>`, plus `AgentDirectory`/`OnlyFrom` so the rest of the
   service works.
2. **E2 scope.** Is a new module capability (`"sections": "produce"`) acceptable,
   or should sections keep going through Lua? The GIL cost and the
   decoration-expressibility problem both argue for the capability.
3. **Windows counters.** Feed `winperf_*` from the existing PDH collector's raw
   values, or shell a dedicated PDH read at dump time as the real agent does
   (it forks a helper for exactly this reason)?
4. **`logwatch` state.** Where does the per-source bookmark live — the existing
   `SimpleCache`/state store, or a new one? This determines whether logwatch can
   ship before or after E3.
5. **Do we chase `df` or `df_v2`?** Both are parsed today; `df_v2` is where
   inode monitoring lives. Recommendation: `df_v2`, since we would otherwise
   ship inode support twice.
