# Service and Process Monitoring

**Goal:** Verify that critical services — Windows services or Linux `systemd`
units — are running, and that specific processes are alive and healthy.

`check_service` works on **both platforms**: on Windows it inspects the Service
Control Manager, and on Linux it inspects **systemd** units. Either way the raw
platform state is mapped to a normalised `state` keyword, so the same thresholds
read the same on both (see [Monitoring Windows Services](#monitoring-windows-services)
and [Monitoring Linux Services (systemd)](#monitoring-linux-services-systemd)).
`check_process` is also cross-platform — on Linux, match the executable name
without an extension (`process=sshd` instead of `process=explorer.exe`).

---

## Prerequisites

Enable the `CheckSystem` module in `nsclient.ini` (the module is enabled under
the same name on both Windows and Linux):

```ini
[/modules]
CheckSystem = enabled
NRPEServer  = enabled   ; if using NRPE
```

---

## Monitoring Windows Services

### Default service check

```
check_service
```

This checks that all auto-start services are in the `running` state.

**Expected output (healthy):**

```
OK: All services are ok.
```

**Expected output (alert):**

```
WARNING: WARNING: Spooler=stopped (auto), wuauserv=stopped (auto)
```

### Check a specific service

```
check_service service=Spooler
```

```
OK: All services are ok.
'Spooler'=4;4;0
```

### Alert if a service that should be stopped is running

Useful for verifying that a decommissioned or unwanted service stays off:

```
check_service service=Telnet "crit=state = 'started'" warn=none
```

### Exclude noisy auto-start services that are expected to be stopped

```
check_service "exclude=clr_optimization_v4.0.30319_32" "exclude=clr_optimization_v4.0.30319_64"
```

Or with a filter for substring matching (more flexible):

```
check_service "filter=start_type = 'auto' and name not like 'clr_optimization'"
```

<!-- @formatter:off -->
!!! note
    `exclude=` is faster but only matches the exact service name. `filter=` is more flexible and supports expressions, substring matching, and logical operators.
<!-- @formatter:on -->

### Show all services and their states

```
check_service "top-syntax=${list}" "detail-syntax=${name}: ${state}"
```

```
AdobeARMservice: running, Spooler: running, wuauserv: stopped, ...
```

### Via NRPE

```
check_nrpe -H <agent-ip> -c check_service
```

---

## Monitoring Linux Services (systemd)

The same `check_service` command works on Linux, where it inspects **systemd**
units. The raw systemd state is mapped to a normalised `state` keyword so
thresholds read the same as on Windows, and the raw fields are exposed too
(`active`, `sub_state`, `preset`).

By default it flags any *enabled* unit that has failed or stopped, and ignores
units that are deliberately `disabled`:

```
check_service
OK: All 42 service(s) are ok.
```

**Check a specific unit and show its state / preset:**

```
check_service service=cron "top-syntax=${list}" "detail-syntax=${name}=${state} active=${active} preset=${preset}"
cron=running active=active preset=enabled
```

**Only alert on one unit being down:**

```
check_service service=ssh "crit=state != 'running'"
```

**Alert on a unit's resource usage** — process metrics `rss`, `vms`, `cpu`,
`tasks`, `age` are available:

```
check_service service=mysql "warn=rss > 1G" "crit=rss > 2G"
```

The default critical expression is
`( state not in ('running', 'oneshot', 'static') or active = 'failed' ) and preset != 'disabled'`,
so a stopped-but-disabled unit stays OK while an enabled unit that failed is
CRITICAL.

---

## Monitoring Processes

### Check that specific processes are running

```
check_process process=explorer.exe
```

**Expected output (running):**

```
OK: All processes are ok.
'explorer.exe state'=1;1;0
```

**Expected output (not running):**

```
CRITICAL: CRITICAL: myapp.exe=stopped
'myapp.exe state'=0;1;0
```

On Linux the same check looks like:

```
check_process process=sshd
```

### Check multiple processes at once

```
check_process process=explorer.exe process=myapp.exe
```

### Alert on memory usage

Useful for detecting memory leaks in a process:

```
check_process process=explorer.exe "warn=working_set > 500m" "crit=working_set > 1g"
```

### Show process detail in the message

```
check_process process=explorer.exe "warn=working_set > 200m" \
  "detail-syntax=${exe} — memory: ${working_set}, handles: ${handles}, cpu time: ${user}s"
```

```
WARNING: Explorer.EXE — memory: 431.8MB, handles: 5639, cpu time: 2535s
```

### List all processes consuming more than 200 MB virtual memory

```
check_process "filter=virtual > 200m" "top-syntax=${list}" "detail-syntax=${exe}=${virtual}"
```

### Via NRPE

```
check_nrpe -H <agent-ip> -c check_process --argument "process=myapp.exe"
```

<!-- @formatter:off -->
!!! warning
    Passing arguments via NRPE requires `allow arguments = true` in the NRPE server config. See [NRPE security](nrpe.md) for the security implications.
<!-- @formatter:on -->

---

## Configuration Example

A minimal `nsclient.ini` for NRPE-based service and process monitoring:

```ini
[/modules]
CheckSystem = enabled
NRPEServer  = enabled

[/settings/NRPE/server]
allowed hosts = 10.0.0.1
port          = 5666
```

On the monitoring server:

```
check_nrpe -H <agent-ip> -c check_service
check_nrpe -H <agent-ip> -c check_process --argument "process=myapp.exe"
```

---

## Next Steps

- [Event Log Monitoring](event-log.md) — catch service crash events before they are noticed
- [Windows Server Health](windows-server-health.md) — add service checks to a full health baseline
- [Linux Server Health](linux-server-health.md) — the same for a Linux host (load, CPU, memory, disk, services)
- [Checks In Depth: Filters](../concepts/checks.md#3-filters-choosing-what-to-check) — understand how to write filter expressions
- [Reference: CheckSystem](../reference/windows/CheckSystem.md) — full command reference
