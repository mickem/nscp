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

| Exit code | Status   |
|-----------|----------|
| 0         | OK       |
| 1         | WARNING  |
| 2         | CRITICAL |
| 3         | UNKNOWN  |

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

NSClient++ includes wrappers to run different script types. These are
pre-configured in the `[/settings/external scripts/wrappings]` section:

```ini
[/settings/external scripts/wrappings]
bat = "scripts\%SCRIPT%" $ARGS"$
ps1 = powershell.exe -NoProfile -ExecutionPolicy Bypass -NonInteractive -File "scripts\%SCRIPT%" $ARGS"$
vbs = cscript.exe //T:30 //NoLogo "scripts\lib\wrapper.vbs" %SCRIPT% $ARGS"$
```

**Using a wrapped script:**

```ini
[/settings/external scripts/wrapped scripts]
check_updates = check_updates.vbs $ARG1$ $ARG2$
```

!!! note
    Older versions of NSClient++ shipped a `ps1` wrapping that piped through
    `cmd.exe`:
    `cmd /c echo scripts\%SCRIPT% %ARGS% ; exit($lastexitcode) | powershell.exe -command -`.
    The current default invokes `powershell.exe -File` directly so that
    `$ARGn$` substitutions cannot pass through `cmd.exe`'s parser as
    statement-level metacharacters. Existing operator-supplied wrappings
    keep working unchanged; only the default templates were updated.

---

## How Arguments Reach Your Script

When `allow arguments = true`, NSClient++ tokenises your command template
once and substitutes `$ARGn$` at the **token** level, then launches the
script with the resulting argument vector directly:

* On Unix, `fork` + `execvp` — no `/bin/sh -c`.
* On Windows, `CreateProcess` with `lpApplicationName` set to `argv[0]` and a
  per-argument-quoted command line that round-trips through
  `CommandLineToArgvW`.

What this means in practice:

* A `$ARGn$` whose value contains spaces, `;`, `$`, `(`, `)`, newlines or
  any other shell metacharacter reaches your script as a **single argv
  element**. It is no longer interpreted as command separation, redirection
  or sub-shell.
* `$ARGS$` / `%ARGS%` and the quoted-equivalent forms, when used as a
  *standalone* template token, splat the supplied arguments as separate
  argv elements. Embedded inside a larger token (e.g.
  `prefix-$ARGS$-suffix`) they collapse to a space-joined single token.

### Quote command paths that contain spaces

Because the launcher now uses `argv[0]` to fix the executable, an unquoted
path with spaces splits across argv:

```ini
# WRONG — argv[0] becomes "C:\Program" and the launch fails
[/settings/external scripts/scripts/check_app]
command = C:\Program Files\nscp\check.exe --foo

# RIGHT — the whole path is one token
[/settings/external scripts/scripts/check_app]
command = "C:\Program Files\nscp\check.exe" --foo
```

The bundled wrappings (`scripts\%SCRIPT%`) already follow this rule.

---

## Common Gotchas

### PowerShell `exit 2` reported as WARNING (not CRITICAL)

If a PowerShell check script does `exit 2` and the result reaches your
monitoring server as **WARNING** instead of CRITICAL, the script's exit code
isn't reaching NSClient++ — `powershell.exe` itself is exiting with `1`.

This trips most people on first contact with PowerShell-based checks. It is
a `powershell.exe` behaviour, not an NSClient++ bug.

```ini
# Broken — `exit 2` from the script becomes WARNING
[/settings/external scripts/scripts]
check_crit = powershell scripts\check_crit.ps1
```

When `powershell.exe` is invoked **without `-File`**, it runs the path in
`-Command` mode and exits with **`0` on success / `1` on any error**,
**regardless of `$LASTEXITCODE` inside the script**. The script's `exit 2`
is silently dropped and NSClient++ sees `1` → WARNING.

Pick any of the three fixes:

**Option 1 — use the `.ps1` wrapping (recommended)**

The bundled `ps1` wrapping (in `[/settings/external scripts/wrappings]`)
invokes `powershell.exe -File` so the script's exit code is propagated
verbatim. Move the entry to **`[wrapped scripts]`** and reference the file
name:

```ini
[/settings/external scripts/wrapped scripts]
check_crit = check_crit.ps1
```

**Option 2 — use `-File`**

```ini
[/settings/external scripts/scripts]
check_crit = powershell -File scripts\check_crit.ps1
```

In `-File` mode `powershell.exe` returns the script's exit code verbatim.

**Option 3 — propagate `$LASTEXITCODE` explicitly**

```ini
[/settings/external scripts/scripts]
check_crit = powershell -Command "scripts\check_crit.ps1; exit $LASTEXITCODE"
```

Same idea as the wrapping — the trailing `exit $LASTEXITCODE` forwards the
inner script's code.

**Verify on the host before deploying:**

```cmd
powershell scripts\check_crit.ps1
echo %ERRORLEVEL%   :: prints 1 — the bug

powershell -File scripts\check_crit.ps1
echo %ERRORLEVEL%   :: prints 2 — correct
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
    Enabling argument pass-through is a security risk: any host that can
    reach the NRPE port can pass arbitrary arguments to whatever script you
    have defined. Combine with `allowed hosts` and a firewall.

    NSClient++ now isolates each `$ARGn$` substitution as a single argv
    element (no shell, no Windows command-line re-tokenisation), so an
    attacker cannot embed `;`, `$()`, redirection, or extra flags via
    argument injection. They **can** still call any registered command with
    any string value, which is enough to drive a script into doing
    expensive work, hitting external systems, or exposing whatever the
    script chooses to expose. Treat your scripts as the security boundary
    and validate `$ARGn$` inside the script itself.

!!! note
    The `allow nasty characters` flag remains as defence in depth. With it
    set to `false`, NSClient++ rejects requests whose arguments contain
    `|`, `` ` ``, `&`, `>`, `<`, `'`, `"`, `\`, `[`, `]`, `{`, `}`. It is
    no longer the only thing standing between the network and a shell
    interpreter — argv isolation is — but leaving it `false` continues to
    block the most obvious abuse patterns.

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
