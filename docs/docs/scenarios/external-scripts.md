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

To allow the monitoring server to pass its own arguments:

```ini
[/settings/external scripts]
allow arguments = true

[/settings/NRPE/server]
allow arguments = true
```

!!! danger
    Enabling argument pass-through is a security risk. Anyone who can reach the NRPE port can pass arbitrary arguments to your scripts. Use hard-coded arguments where possible, or combine with `allowed hosts` restrictions.

Arguments are accessed in scripts as `$ARG1$`, `$ARG2$`, etc.

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

## Next Steps

- [Reference: CheckExternalScripts](../reference/check/CheckExternalScripts.md) — full configuration reference
- [Howto: External Scripts](../howto/external_scripts.md) — more advanced configuration options
- [Extending: Python Scripts](../extending/python.md) — write internal scripts in Python for deeper NSClient++ integration
- [Passive Monitoring](passive-monitoring-nsca.md) — have scripts push results on a schedule instead of being polled
