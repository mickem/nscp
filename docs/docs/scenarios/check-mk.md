# Checkmk Agent Integration

**Goal:** Have a Checkmk site monitor a Windows host through NSClient++ — pulling an agent dump that looks and behaves like the official Checkmk agent, complete with system sections, local checks, MRPE entries, and scheduler-driven cached results.

<!-- @formatter:off -->
!!! tip
    This scenario is for connecting NSClient++ to an existing Checkmk site (open-source or commercial). If you already have nscp deployed for NRPE or NSCA, you can layer Checkmk on top without removing those — they're separate listeners.
<!-- @formatter:on -->

---

## How Checkmk Talks to the Agent

Checkmk's agent protocol is **active and pull-style**: the site opens a TCP connection to the agent on port 6556, the agent pushes a single text "agent dump", and the connection closes.

```mermaid
flowchart LR
    M[Checkmk site] -->|TCP/6556| A[NSClient++<br/>CheckMKServer]
    A -->|text dump, then closes| D[(Agent dump)]
```

A typical agent dump looks like:

```text
<<<check_mk>>>
Version: ...
<<<mem>>>
MemTotal: 16382336 kB
...
```

The dump is plain UTF-8 text divided into `<<<section_name>>>` blocks. Checkmk's parser routes each section to the appropriate "check plugin" on the server side (`mem`, `df`, `services`, `local`, `mrpe`, etc.).

NSClient++ produces this dump from a Lua script (`scripts/lua/default_check_mk.lua`) that mixes these sources:

| Source                         | Sections                                                                   |
|--------------------------------|----------------------------------------------------------------------------|
| Internal metrics store         | `<<<mem>>>` (Windows)                                                      |
| `/proc` files, sent as-is      | `<<<mem>>>`, `<<<cpu>>>`, `<<<kernel>>>`, `<<<uptime>>>` (Linux)           |
| Native checks                  | `<<<services>>>`, `<<<ps>>>`, `<<<df>>>` (`fetch-only`); `<<<uptime>>>` on Windows (`check_uptime`); OS lines of `<<<check_mk>>>` (`check_os_version`) |
| Settings-driven bridge         | `<<<local>>>`, `<<<mrpe>>>`                                                |
| Scheduler / passive submission | Cached entries inside `<<<local>>>` / `<<<mrpe>>>`                         |

You don't normally edit the Lua script — you control everything from `nsclient.ini`.

---

## Quick Setup with `nscp mk install`

One command enables the agent and the modules its sections need, sets the port and who may connect, and adds local checks and MRPE entries. Run it on the monitored host, in an elevated prompt on Windows (or with `sudo` on Linux), then restart NSClient++:

```commandline
$ nscp mk install --allowed-hosts 10.0.0.5 --local "CPU Load=check_cpu warn=load>80 crit=load>95"
Checkmk agent enabled on port 6556, for: 10.0.0.5
Modules enabled: CheckMKServer, CheckSystem, CheckDisk
Local check "CPU Load": check_cpu warn=load>80 crit=load>95
Restart NSClient++ to apply. `nscp mk show` lists the configuration.
```

Replace `10.0.0.5` with the address of your Checkmk server (see [Step 6](#step-6-register-the-host-in-checkmk) for the Checkmk side). On Windows, restart with `net stop nscp && net start nscp`; on Linux, `sudo systemctl restart nsclient`.

### Options

| Option | What it does |
|---|---|
| `--allowed-hosts <list>` | Who may fetch the agent dump: a comma separated list of addresses, host names or networks (`10.0.0.0/24`). Written to `/settings/check_mk/server` only, so NRPE and the other listeners keep their own lists. Defaults to what is configured, which on a fresh install is `127.0.0.1`. |
| `--port <port>` | The TCP port to listen on. Defaults to what is configured, `6556` on a fresh install, which is what Checkmk expects. |
| `--local "<name>=<command> [args]"` | Add a local check, or replace the one with that name. `<name>` becomes the Checkmk service name and may contain spaces. Repeat the option for more than one. |
| `--mrpe "<name>=<command> [args]"` | Add or replace an MRPE entry. The name cannot contain spaces (MRPE has no quoting): use `Uptime_Check`, not `Uptime Check`. Repeatable. |
| `--remove-local <name>` | Remove a local check. Repeatable. |
| `--remove-mrpe <name>` | Remove an MRPE entry. Repeatable. |
| `--help` | List the options and their current values. |

The name ends at the first `=`, so the command can carry `key=value` arguments as it would on the command line. Quote each `--local` / `--mrpe` value as a whole, as in the examples.

The command always enables `CheckMKServer`, `CheckSystem` and `CheckDisk`. It does not touch `Scheduler` or `CheckHelpers`, which only [Step 5](#step-5-scheduler-driven-cached-checks-optional) and some local-check expressions need, and it does not open the firewall (see [Troubleshooting](#agent-dump-is-empty-connection-refused)).

### Changing the setup later

`install` starts from what is already configured, so a later run only changes what you pass. Running it again is safe.

```commandline
$ nscp mk install --mrpe "Uptime_Check=check_uptime warn=uptime<2d" --remove-local "CPU Load"
Checkmk agent enabled on port 6556, for: 10.0.0.5
Modules enabled: CheckMKServer, CheckSystem, CheckDisk
MRPE entry "Uptime_Check": check_uptime warn=uptime<2d
Removed local check "CPU Load"
Restart NSClient++ to apply. `nscp mk show` lists the configuration.
```

To let a second Checkmk server (or a whole subnet) in, pass the complete list again. `--allowed-hosts` replaces the list, it does not add to it:

```commandline
$ nscp mk install --allowed-hosts "10.0.0.5,10.0.1.0/24"
```

If only this machine may connect, `install` says so, since a Checkmk site on another host could not fetch anything:

```commandline
$ nscp mk install
Checkmk agent enabled on port 6556, for: 127.0.0.1
Modules enabled: CheckMKServer, CheckSystem, CheckDisk
WARNING: only this machine may connect. Pass --allowed-hosts <address of the Checkmk server> so the site can fetch the agent.
Restart NSClient++ to apply. `nscp mk show` lists the configuration.
```

### Showing the configuration

`nscp mk show` prints what the agent will run with after the next restart:

```commandline
$ nscp mk show
Checkmk agent (CheckMKServer)
Modules:
  CheckMKServer: enabled
  CheckSystem: enabled
  CheckDisk: enabled
Port: 6556
Allowed hosts: 10.0.0.5
Local checks:
  CPU Load: check_cpu warn=load>80 crit=load>95
MRPE entries:
  Uptime_Check: check_uptime warn=uptime<2d
```

Everything the command writes is ordinary `nsclient.ini` configuration, the same keys the steps below describe, so you can also edit the file by hand or manage it with your configuration tooling.

---

## Prerequisites

`nscp mk install` enables the modules for you. To set them up by hand instead, enable these in `nsclient.ini`:

```ini
[/modules]
CheckMKServer = enabled   ; the agent listener (port 6556)
CheckSystem   = enabled   ; mem, ps, services, uptime, OS labels
CheckDisk     = enabled   ; df / drive sizes and inodes
CheckHelpers  = enabled   ; useful for local-check expressions
Scheduler     = enabled   ; only if you want passive cached checks (Step 5)
```

---

## Step 1 — Verify the Listener

Configure the listener and start NSClient++ (`nscp mk install --allowed-hosts <checkmk-site-ip>` writes exactly this):

```ini
[/settings/check_mk/server]
port          = 6556
allowed hosts = 127.0.0.1, <checkmk-site-ip>
```

Restart and verify with any TCP client:

```
nc -w 5 127.0.0.1 6556
```

Or from PowerShell:

```powershell
$tc = [System.Net.Sockets.TcpClient]::new("127.0.0.1", 6556)
$sr = [System.IO.StreamReader]::new($tc.GetStream())
$sr.ReadToEnd()
```

You should see something like:

```
<<<check_mk>>>
Version: nsclient++
AgentOS: windows
Hostname: WIN-DESKTOP
OSType: windows
OSName: Windows 11 23H2
OSVersion: 10.0.22631.3803
<<<uptime>>>
175140
<<<systemtime>>>
1715361234
<<<mem>>>
MemTotal:      16382336 kB
MemFree:       8000000 kB
SwapTotal:     0 kB
SwapFree:      0 kB
PageTotal:     32000000 kB
PageFree:      23000000 kB
VirtualTotal:  137438953344 kB
VirtualFree:   137438000000 kB
<<<df>>>
C: NTFS 1953511476 526123456 1427388020 27% C:
<<<ps>>>
(WIN-DESKTOP\jane,123456,67890,42,1234) explorer
...
<<<services>>>
Spooler running/auto Print Spooler
...
```

A Linux host sends the same header (with `OSType: linux`, `OSPlatform`,
`OSName` and `OSVersion` from `/etc/os-release`), its own `/proc/meminfo` as
`<<<mem>>>`, `<<<cpu>>>` and `<<<kernel>>>`, and a `<<<df>>>` that also carries
inode counts:

```
<<<cpu>>>
0.26 0.47 0.52 2/566 160426 8
126572
<<<kernel>>>
1715361234
nr_free_pages 312345
...
cpu  4705 356 584 3699176 23 23 0 0 0 0
...
<<<df>>>
/dev/sda1 ext4 30297152 11962620 17371776 41% /
[df_inodes_start]
/dev/sda1 ext4 1921360 391218 1530142 21% /
[df_inodes_end]
```

Empty output usually means the source IP isn't in `allowed hosts` (the Checkmk site connects from its own IP, not the docker host's).

---

## Step 2 — What Comes For Free

These sections are produced automatically with no extra configuration:

| Section            | OS      | Source                                                                                                     |
|--------------------|---------|------------------------------------------------------------------------------------------------------------|
| `<<<check_mk>>>`   | both    | Version, hostname and the `OSType` / `OSName` / `OSVersion` lines Checkmk turns into `cmk/os_*` host labels (`check_os_version`) |
| `<<<systemtime>>>` | Windows | Current Unix epoch (lets Checkmk warn on clock skew)                                                       |
| `<<<uptime>>>`     | both    | Seconds since boot: `/proc/uptime` as-is on Linux, `check_uptime` on Windows. Built after every other section, like `<<<systemtime>>>`: Checkmk reads both against its own clock when the fetch ends, so time spent building the dump would otherwise read as clock skew and a late boot time |
| `<<<mem>>>`        | both    | Linux: `/proc/meminfo` as-is. Windows: physical memory, commit limit (`PageTotal`/`PageFree`) and virtual address space from `GlobalMemoryStatusEx` |
| `<<<cpu>>>`        | Linux   | `/proc/loadavg` and the CPU count, then the kernel's thread limit                                          |
| `<<<kernel>>>`     | Linux   | Epoch, then `/proc/vmstat` and `/proc/stat` as-is (raw counters; Checkmk computes the rates)               |
| `<<<df>>>`         | both    | Per-filesystem size/used/free/mountpoint (`check_drivesize fetch-only`); on Linux also the inode counts     |
| `<<<services>>>`   | both    | `name state/start_type display_name` per service (Windows SCM; systemd units on Linux). On Windows, spaces in the name become `_` and every auto-start variant (delayed, trigger) is sent as `auto`, as the official agent does |
| `<<<ps>>>`         | both    | `(user,vsz_kb,rss_kb,cputime,pid) cmdline` per process; the user is the owner's account name              |

Once Checkmk has a host pointing at your nscp instance, these light up automatically as services in discovery: `Memory`, `Filesystem /` (or `Filesystem C:/`), `Uptime`, `Service Summary`, and on Linux also `CPU load`, `CPU utilization`, `Number of threads` and `Kernel Performance`.

!!! note "No CPU services on Windows yet"
    The agent dump carries no CPU section on Windows (`wmi_cpuload` /
    `winperf_processor`), so a Windows host gets no `CPU load` or
    `CPU utilization` service from discovery. Until it does, expose `check_cpu`
    as a local check, as shown in Step 3.

---

## Step 3 — Expose nscp Checks as Local Checks

The most powerful part of the integration: any nscp check command becomes a **Checkmk local check** by adding one line to `nsclient.ini`:

```ini
[/settings/check_mk/server/local]
; service-name = command=<nscp-check> [args...]
CPU Load        = command=check_cpu warn=load>80 crit=load>95
Disk C          = command=check_drivesize drive=C: "warn=free<20%" "crit=free<10%"
Mail Queue      = command=check_files path="C:\Mail\Queue" maxage=5m
```

From the command line, each line is one `--local` option:

```commandline
$ nscp mk install --local "CPU Load=check_cpu warn=load>80 crit=load>95" --local "Disk C=check_drivesize drive=C: \"warn=free<20%\" \"crit=free<10%\""
```

On every agent fetch, NSClient++ runs each command, formats the result as a Checkmk local-check line (`state "name" perfdata text`), and emits it inside `<<<local>>>`. Checkmk discovers each entry as a service named after the key.

<!-- @formatter:off -->
!!! note "Perfdata label gotcha"
    nscp emits Nagios-style single-quoted labels like `'load 5m'=10%;80;90`.
    Checkmk's local-check parser doesn't accept those, so the script
    automatically rewrites them to `load_5m=10%;80;90`. You don't need to do
    anything special — just be aware that metric names with spaces become
    underscored in the Checkmk view.
<!-- @formatter:on -->

---

## Step 4 — Add MRPE Entries

MRPE is the "Nagios plugin" relay format. Functionally equivalent to local checks for most purposes; useful if your Checkmk site already has MRPE rules wired up.

```ini
[/settings/check_mk/server/mrpe]
; service-name = command=<nscp-check> [args...]
Uptime_Check = command=check_uptime warn=uptime<2d
Commit       = command=check_memory type=committed warn=used>80% crit=used>90%
```

Or from the command line:

```commandline
$ nscp mk install --mrpe "Uptime_Check=check_uptime warn=uptime<2d" --mrpe "Commit=check_memory type=committed warn=used>80% crit=used>90%"
```

Same pattern as local checks; the output goes into `<<<mrpe>>>` instead. Two differences:

- **No spaces in the name.** An MRPE line cannot quote one, so Checkmk would read the second word as the return code. `nscp mk install` refuses such a name; use `_` instead.
- **Pick names the built-in services do not use.** Checkmk already discovers `Uptime` and `Memory` from the system sections, so an MRPE entry called `Uptime` shows up as a second service with the same name.

| Use `local` when…                               | Use `mrpe` when…                                   |
|-------------------------------------------------|----------------------------------------------------|
| Starting fresh, no preference                   | You already have MRPE-based service templates      |
| You want fast-discovery                         | The site uses MRPE rule packs                      |
| You care about Checkmk-style perfdata rendering | You need raw Nagios-format perfdata round-tripping |

---

## Step 5 — Scheduler-Driven Cached Checks (Optional)

For checks that are **slow** or **expensive** (large directory walks, WMI queries, calls to remote services), running them inline on every agent fetch is wasteful. Checkmk pulls roughly every minute, so a 30-second check runs 50% of the time.

The **submission channel** path solves this: have the Scheduler run the check on its own interval, submit the result to a CheckMKServer channel, and let it serve the *cached* result on every subsequent fetch with a `cached(generated, interval)` prefix that tells Checkmk how fresh the data is.

```ini
[/modules]
Scheduler = enabled

; Defaults applied to all entries below
[/settings/scheduler/schedules/default]
channel  = check_mk-mrpe   ; or check_mk-local
interval = 5m
report   = all

; A scheduled MRPE check (inherits channel = check_mk-mrpe)
[/settings/scheduler/schedules/Slow Inventory]
command = check_files path="\\fileserver\share\inventory" pattern=*.csv

; Override channel per-schedule for a local-formatted entry
[/settings/scheduler/schedules/AD Replication]
command = check_ad_replication
channel = check_mk-local
```

**Two channels are pre-registered** by `CheckMKServer`:

| Channel           | Result lands in       |
|-------------------|-----------------------|
| `check_mk-mrpe`   | `<<<mrpe>>>` section  |
| `check_mk-local`  | `<<<local>>>` section |

The schedule key (`Slow Inventory`, `AD Replication`) becomes the Checkmk service name. CheckMKServer keeps the latest result per service in memory; the next agent fetch emits it with a `cached(<gen_epoch>,<ttl>)` prefix:

```
<<<mrpe>>>
cached(1715361234,60) Slow Inventory 0 OK: 412 files|count=412
```

Checkmk's UI surfaces this as a freshness indicator — for example: *"Cache generated 4 seconds ago, cache interval: 1 minute, elapsed cache lifespan: 6.54%"*.

### Scheduler-channel TTL

The `cached(...)` prefix tells Checkmk how stale the data may be. Tune it under the listener:

```ini
[/settings/check_mk/server]
submission ttl = 60   ; seconds; should be ≥ scheduler interval
```

Keep the TTL **at least as long as the scheduler interval** — otherwise Checkmk will show the entry as stale between scheduler ticks.

---

## Step 6 — Register the Host in Checkmk

In the Checkmk site web UI:

1. **Setup → Hosts → Add host** (or use the REST API).
2. **Hostname**: anything you like (this is the WATO host name).
3. **IPv4 address**: the IP/DNS name of the Windows machine running NSClient++.
4. **Agent / data source**: leave at the default *"Checkmk agent"* (TCP).
5. **Save → Activate changes**.
6. **Run service discovery** on the new host — Checkmk pulls one agent dump and lists every section it found as a discoverable service.
7. **Accept all** (or pick the ones you want), then activate again.

Within one fetch interval (default ~1 minute) every monitored item turns green.

---

## Complete Configuration Example

The listener, modules, local checks and MRPE entries below come from one command; the Scheduler part is configured by hand:

```commandline
$ nscp mk install --allowed-hosts "127.0.0.1,10.0.0.5" ^
    --local "CPU Load=check_cpu warn=load>80 crit=load>95" ^
    --local "Disk C=check_drivesize drive=C: \"warn=free<20%\" \"crit=free<10%\"" ^
    --mrpe "Uptime_Check=check_uptime warn=uptime<2d" ^
    --mrpe "Commit=check_memory type=committed warn=used>80% crit=used>90%"
```

```ini
[/modules]
CheckMKServer = enabled
LUAScript     = enabled
CheckSystem   = enabled
CheckDisk     = enabled
CheckHelpers  = enabled
Scheduler     = enabled

[/settings/check_mk/server]
port           = 6556
allowed hosts  = 127.0.0.1, 10.0.0.5
submission ttl = 300

[/settings/check_mk/server/local]
CPU Load = command=check_cpu warn=load>80 crit=load>95
Disk C   = command=check_drivesize drive=C: "warn=free<20%" "crit=free<10%"

[/settings/check_mk/server/mrpe]
Uptime_Check = command=check_uptime warn=uptime<2d
Commit       = command=check_memory type=committed warn=used>80% crit=used>90%

[/settings/scheduler/schedules/default]
channel  = check_mk-mrpe
interval = 5m
report   = all

[/settings/scheduler/schedules/Slow Inventory]
command = check_files path="\\fileserver\share\inventory" pattern=*.csv

[/settings/scheduler/schedules/AD Replication]
command = check_ad_replication
channel = check_mk-local
```

---

## Troubleshooting

### Agent dump is empty / connection refused

Almost always one of:

- **`CheckMKServer` not enabled** — check `/modules` and module load logs, or run `nscp mk show`.
- **`allowed hosts` blocks the source IP** — Checkmk connects from the site's IP. The agent logs each refusal with the address it saw: `Rejected connection from: 172.17.0.2, not in allowed hosts (10.0.0.5)`. Add that address with `nscp mk install --allowed-hosts "10.0.0.5,172.17.0.2"`.
- **A Checkmk container on the same machine** — what address it connects from depends on the container runtime. With Docker Desktop or Rancher Desktop on WSL mirrored networking, connections from the container reach the agent as `127.0.0.1`, so keep `127.0.0.1` in the list; with plain Docker on Linux they come from the bridge network (`172.17.0.0/16`). The rejection line above tells you which.
- **Port 6556 not reachable** — Windows Firewall, corporate routing, or another process bound to the port. Verify with `Test-NetConnection -ComputerName <host> -Port 6556`. `nscp mk install` does not open the firewall; on Windows, allow the Checkmk server in with:

    ```powershell
    New-NetFirewallRule -DisplayName "NSClient++ Checkmk agent" -Direction Inbound -Protocol TCP -LocalPort 6556 -RemoteAddress 10.0.0.5 -Action Allow
    ```

### `Invalid data:` in Checkmk's `cmk -v` output

Means Checkmk's parser rejected one of our lines. Most common causes:

- A **local check** line with single-quoted perfdata labels (`'metric name'=...`). The script normalizes these automatically; if you see them, you may have an outdated `default_check_mk.lua` in your `scripts/lua/` directory.
- A **service name with spaces** in `<<<mrpe>>>` (MRPE protocol cannot quote spaces — use underscores).

### Service name shows as `check_xxx` instead of the schedule alias

The CheckMKServer in your build is older than the alias-aware fix. Rebuild
`CheckMKServer.dll` and redeploy.

### Checkmk shows "stale" / "no data" for cached entries

The `cached(...)` interval is shorter than the scheduler period, so Checkmk
considers the data expired before the next push. Either lengthen
`submission ttl` or shorten the schedule's `interval`.

### What's NOT supported (yet)

- **Symmetric encryption** (AES-256-CBC with pre-shared password). Checkmk site needs to be configured *without* encryption for nscp to work today, or routed through a TLS-terminating proxy.
- **Real-Time Check (UDP push)**.
- **`cmk-agent-ctl` mTLS**.
- **`<<<logwatch>>>`** / **`<<<winperf_*>>>`** / **`<<<wmi_cpuload>>>`** / **`<<<fileinfo>>>`** sections — so no CPU, network interface or disk I/O services on Windows, and no network interface or disk I/O services on Linux.

For these, fall back to the official Checkmk agent on hosts that need them, or expose the check via the local/mrpe bridge.

---

## Next Steps

- [Reference: CheckMKServer](../reference/client/CheckMKServer.md) — full settings reference for the listener
- [Reference: Scheduler](../reference/generic/Scheduler.md) — full scheduler reference for the submission channels
- [Passive Monitoring with NSCA](passive-monitoring-nsca.md) — same scheduler pattern targeting a Nagios-style server instead
