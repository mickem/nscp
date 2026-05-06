# External Script Integration

**Goal:** Run your own scripts (PowerShell, batch, VBScript, or any executable) as monitoring checks, so NSClient++ can call them and return their result to your monitoring server.

---

## Prerequisites

Enable the `CheckExternalScripts` module in `nsclient.ini`:

```ini
[/modules]
CheckExternalScripts = enabled
NRPEServer           = enabled   ; if using NRPE
```

---

## How External Scripts Work

NSClient++ runs your script as a child process, captures its output, and passes the exit code and text back to the monitoring server.

**Exit codes:**

| Exit code | Status |
|---|---|
| 0 | OK |
| 1 | WARNING |
| 2 | CRITICAL |
| 3 | UNKNOWN |

**Output format:**

```
Status message text|'metric_name'=value;warn;crit;min;max
```

The part after `|` is optional performance data. If you do not need graphs, output only the status text.

---

## Adding a Script

### Short format (most common)

```ini
[/settings/external scripts/scripts]
check_my_app  = scripts\check_my_app.bat
check_my_app2 = scripts\check_my_app.ps1
```

Each key becomes a new check command. The script path is relative to the NSClient++ installation directory.

### Long format (for advanced options)

```ini
[/settings/external scripts/scripts/check_my_app]
command = scripts\check_my_app.bat
```

Use the long format when you need to set additional options such as running the script as a specific user.

---

## Script Examples

### Batch file (check_my_app.bat)

```batch
@echo off
rem Check if a file exists
if exist "C:\AppData\lockfile.lock" (
    echo WARNING: Lock file exists - application may be hung
    exit /b 1
)
echo OK: Application is running normally
exit /b 0
```

### PowerShell script (check_memory_ps.ps1)

```powershell
$mem = Get-CimInstance Win32_OperatingSystem
$usedPct = [math]::Round((($mem.TotalVisibleMemorySize - $mem.FreePhysicalMemory) / $mem.TotalVisibleMemorySize) * 100)

if ($usedPct -ge 90) {
    Write-Host "CRITICAL: Memory usage at $usedPct%"
    exit 2
} elseif ($usedPct -ge 80) {
    Write-Host "WARNING: Memory usage at $usedPct%"
    exit 1
} else {
    Write-Host "OK: Memory usage at $usedPct%|'memory_used_pct'=$usedPct%;80;90;0;100"
    exit 0
}
```

Register it:

```ini
[/settings/external scripts/scripts]
check_memory_ps = scripts\check_memory_ps.ps1
```

---

## Script Wrappers

NSClient++ includes wrappers to run different script types. These are pre-configured in the `[/settings/external scripts/wrappings]` section:

```ini
[/settings/external scripts/wrappings]
bat = scripts\%SCRIPT% %ARGS%
ps1 = cmd /c echo scripts\%SCRIPT% %ARGS%; exit($lastexitcode) | powershell.exe -command -
vbs = cscript.exe //T:90 //NoLogo scripts\lib\wrapper.vbs %SCRIPT% %ARGS%
exe = cmd /c %SCRIPT% %ARGS%
```

**Using a wrapped script:**

```ini
[/settings/external scripts/wrapped scripts]
check_updates = check_updates.vbs $ARG1$ $ARG2$
```

---

## Using Arguments

### Hard-coded arguments (recommended for security)

```ini
[/settings/external scripts/scripts]
check_disk_c = scripts\check_disk.bat C: 20 10
```

The arguments `C:`, `20`, and `10` are always passed to the script — they cannot be changed from the monitoring server.

### Arguments from the monitoring server (allow arguments)

There are actually **two** independent `allow arguments` flags — one on the
NRPE server, one on `CheckExternalScripts` — and the combination determines
how exposed your scripts are. Pick the strategy that matches your threat
model:

| Strategy                                | NRPE `allow arguments` | External-scripts `allow arguments` | Trade-off                                                                                            |
|-----------------------------------------|------------------------|------------------------------------|------------------------------------------------------------------------------------------------------|
| **None — locked down (most secure)**    | `false`                | `false`                            | All thresholds hard-coded in `nsclient.ini`. Most secure; least flexible.                            |
| **Built-ins only**                      | `true`                 | `false`                            | The monitoring server can drive thresholds for built-in commands (`check_cpu`, ...) but cannot pass arbitrary arguments to your scripts. Good middle ground. |
| **Both — fully remote-controlled**      | `true`                 | `true`                             | Maximum flexibility; biggest blast radius. A compromised NRPE port can run your scripts with any arguments. Combine with strict `allowed hosts` and a firewall. |

Built-ins-only configuration:

```ini
[/settings/NRPE/server]
allow arguments = true

[/settings/external scripts]
allow arguments = false
```

Both-allowed configuration (only with strict network restrictions):

```ini
[/settings/NRPE/server]
allow arguments = true

[/settings/external scripts]
allow arguments = true

[/settings/external scripts/scripts]
foo = scripts\foo.bat $ARG1$ $ARG2$
```

Arguments are accessed in scripts as `$ARG1$`, `$ARG2$`, etc.

!!! danger
    Enabling argument pass-through (especially the second flag) is a security
    risk: any host that can reach the NRPE port can pass arbitrary arguments
    to whatever script you've defined. Combine with `allowed hosts` and a
    firewall.

### Protocol payload limits

Each transport has its own hard-coded payload size limit. If your script
output exceeds the limit, the result will be truncated or the protocol will
reject it outright:

| Protocol | Limit       |
|----------|-------------|
| NRPE v2  | 1024 bytes  |
| NRPE v3+ | configurable, but practical limit is still small |
| NSCA     | 512 bytes   |

For long output, summarise in the message and put detail in the
performance-data section, or split the check into multiple smaller checks.

---

## Running a Script as a Different User

```ini
[/settings/external scripts/scripts/check_as_admin]
command  = scripts\check_admin_resource.bat
user     = Administrator
password = s3cr3t_p@ssword
```

---

## Programs That Should Keep Running

If a script starts a long-running process (e.g., a remediation action), it must not block NSClient++.  
Set `capture output = false` to launch and immediately return:

```ini
[/settings/external scripts/scripts/fix_problem]
command        = scripts\fix_problem.bat
capture output = false
```

!!! danger
    Do **not** use `start` or similar shell tricks to background a process inside a regular script. This causes handle inheritance issues that block NSClient++'s port until it is restarted. Use `capture output = false` instead.

---

## Ignoring Performance Data

NSClient++ parses anything after `|` in your script's output as Nagios
performance data. If your script's pipe character isn't perfdata — or it
emits non-conforming text that the parser then mangles — disable parsing.

Per-script:

```ini
[/settings/external scripts/scripts/check_foo]
command         = scripts\check_foo.bat
ignore perfdata = true
```

Globally for every external script:

```ini
[/settings/external scripts/scripts/default]
ignore perfdata = true
```

---

## Testing Your Script

Use the NSClient++ test shell to verify your script works before connecting it to a monitoring server:

```
nscp test
check_my_app
```

You will see the exit code and output immediately.

---

## Configuration Example

```ini
[/modules]
CheckExternalScripts = enabled
NRPEServer           = enabled

[/settings/external scripts]
allow arguments = false   ; keep this false unless you specifically need it

[/settings/external scripts/scripts]
check_my_app   = scripts\check_my_app.bat
check_my_svc   = scripts\check_service_status.ps1

[/settings/NRPE/server]
allowed hosts = 10.0.0.1
port          = 5666
```

On the monitoring server:

```
check_nrpe -H <agent-ip> -c check_my_app
check_nrpe -H <agent-ip> -c check_my_svc
```

---

## Running a Script on a Schedule (Locally)

The Scheduler module is the same machinery used for [passive
monitoring](passive-monitoring-nsca.md), but its `channel` knob doesn't have
to point at a monitoring server. Two local-only channels are useful when you
want NSClient++ to run a script periodically for reasons other than reporting
to Nagios/Icinga:

| Channel | Effect                                                                          | Use case                                                              |
|---------|---------------------------------------------------------------------------------|-----------------------------------------------------------------------|
| `noop`  | Discard the result entirely                                                     | Periodic remediation/maintenance scripts; fire-and-forget actions     |
| `file`  | Append the result to a file via the `SimpleFileWriter` module                   | Local audit trail; feeding output to log shippers or other tools      |

### Fire and forget (`channel = noop`)

Run a remediation script every five minutes; do not push the result anywhere:

```ini
[/modules]
CheckExternalScripts = enabled
Scheduler            = enabled

[/settings/external scripts/scripts/cleanup_temp]
command = scripts\cleanup_temp.bat

[/settings/scheduler/schedules/cleanup_loop]
command  = cleanup_temp
interval = 5m
channel  = noop
```

### Write results to a file (`channel = file`)

Append every run's output to a local file — useful as an audit trail or to
feed a log shipper:

```ini
[/modules]
CheckExternalScripts = enabled
Scheduler            = enabled
SimpleFileWriter     = enabled    ; provides the `file` channel

[/settings/external scripts/scripts/collect_inventory]
command = scripts\collect_inventory.ps1

[/settings/scheduler/schedules/inventory_hourly]
command  = collect_inventory
schedule = 18 * * * *             ; 18 minutes past every hour
channel  = file
```

By default `SimpleFileWriter` appends to `output.txt` next to `nsclient.ini`;
see the module's reference for redirecting elsewhere or rotating the file.

### When to use which channel

| Goal                                                | Channel  | See also                                                              |
|-----------------------------------------------------|----------|-----------------------------------------------------------------------|
| Push results to Nagios / Icinga / NRDP              | `NSCA` / `NRDP` / `Icinga` | [Passive Monitoring (NSCA/NRDP)](passive-monitoring-nsca.md), [Passive Monitoring (Icinga 2)](passive-monitoring-icinga.md) |
| Run a script periodically; don't care about output  | `noop`   | (this section)                                                        |
| Run a script periodically; capture output locally   | `file`   | (this section)                                                        |

For the full scheduler syntax — `interval` vs cron-style `schedule`, per-job
overrides, real-time channels — see [Passive Monitoring → Configure the
Scheduler](passive-monitoring-nsca.md#step-2-configure-the-scheduler).

---

## Where to Find Scripts

You don't have to write everything yourself. Community-maintained Nagios-
compatible plugins live at:

- [Nagios Exchange](https://exchange.nagios.org/)
- [Icinga Exchange](https://exchange.icinga.com/)

Both work with NSClient++ as long as the script honours the standard
exit-code convention (0 OK, 1 WARN, 2 CRIT, 3 UNKNOWN).

---

## Next Steps

- [Reference: CheckExternalScripts](../reference/check/CheckExternalScripts.md) — full configuration reference
- [Extending: Python Scripts](../extending/python.md) — write internal scripts in Python for deeper NSClient++ integration
- [Passive Monitoring](passive-monitoring-nsca.md) — have scripts push results on a schedule instead of being polled
