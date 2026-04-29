# Passive Monitoring with NSCA/NRDP

**Goal:** Have NSClient++ run checks on a schedule and push the results to your monitoring server — without the monitoring server needing to poll each machine.

!!! tip
    Passive monitoring is ideal when the monitored machines are behind a firewall, are in a DMZ, or when you want to distribute the check workload across many agents rather than centralising it on the monitoring server.

---

## How Passive Monitoring Works

In **active (NRPE) monitoring**, the monitoring server initiates contact and polls the agent:

```
Monitoring Server ──check_nrpe──► NSClient++ → runs check → returns result
```

In **passive monitoring**, the agent runs checks on its own schedule and pushes results to the server:

```
NSClient++ (Scheduler) → runs check → NSCAClient ──────► Monitoring Server (NSCA daemon)
```

The monitoring server simply waits for results. If no result arrives within the expected time window, the monitoring server raises a "freshness" alert.

---

## Prerequisites

Enable these modules in `nsclient.ini`:

```ini
[/modules]
CheckSystem  = enabled   ; provides check_cpu, check_memory, etc.
CheckDisk    = enabled   ; provides check_drivesize
Scheduler    = enabled   ; runs checks on a timer
NSCAClient   = enabled   ; pushes results to the NSCA server
```

---

## Step 1 — Verify the NSCA Connection

Before configuring scheduled checks, verify that NSClient++ can send a test result to the NSCA server.

```
nscp nsca --host <nagios-server-ip> ^
    --password secret-password ^
    --encryption aes256 ^
    --command "service check" ^
    --result 2 ^
    --hostname "my-windows-host" ^
    --message "Hello from NSClient++"
```

!!! note
    The `^` character is the Windows **Command Prompt** line-continuation character. In PowerShell use a backtick (`` ` ``) instead, or write the command on a single line.

Expected output:

```
Submission successful
```

If you have set up a test result file on the NSCA server, you should see:

```
[timestamp] PROCESS_SERVICE_CHECK_RESULT;my-windows-host;service check;2;Hello from NSClient++
```

### NSCA encryption reference

| NSClient++ value | NSCA method number | Security |
|---|---|---|
| `aes256` | 14 | ✅ Industry standard (recommended) |
| `twofish` | 9 | 🟢 Very secure |
| `blowfish` | 8 | 🟡 Moderate |
| `xor` | 1 | ⚠️ Not secure — avoid |
| `none` | 0 | ⚠️ No encryption — avoid |

---

## Step 2 — Configure the Scheduler

The `Scheduler` module runs your checks at regular intervals and sends results to a **channel** (`NSCA` by default).

**Set default parameters for all scheduled jobs:**

```ini
[/settings/scheduler/schedules/default]
channel  = NSCA    ; which channel to send results to
interval = 5m      ; how often to run each check
report   = all     ; send results regardless of status (OK, WARN, CRIT)
```

**Add scheduled checks:**

```ini
[/settings/scheduler/schedules]
; Format: check_name = check_command [arguments...]
host_check = check_ok
cpu        = check_cpu
memory     = check_memory
disk_c     = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

Each key becomes the **service name** that Nagios/Icinga will see in the passive check result.

!!! note
    The `default` schedule section applies its settings to all schedules that do not override them. You can override any setting per schedule by creating a dedicated section:

    ```ini
    [/settings/scheduler/schedules/disk_c]
    command  = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
    interval = 15m   ; check disk less often than cpu/memory
    channel  = NSCA
    report   = all
    ```

---

## Step 3 — Configure the NSCA Client

```ini
[/settings/NSCA/client/targets/default]
address    = <nagios-server-ip>
encryption = aes256
password   = secret-password
```

The password must match the password in the NSCA server's `nsca.cfg`.

---

## Step 4 — Set the Hostname

NSClient++ must send results using the **hostname as it is known in Nagios/Icinga**. By default it uses the Windows computer name which may not match.

**Auto-detect hostname:**

```ini
[/settings/NSCA/client]
hostname = auto
```

**Use a specific hostname:**

```ini
[/settings/NSCA/client]
hostname = win-server-01.example.com
```

**Build the hostname from system variables:**

```ini
[/settings/NSCA/client]
hostname = ${host_lc}.${domain_lc}
```

Supported variables:

| Variable | Meaning |
|---|---|
| `${host}` | Windows computer name (mixed case) |
| `${host_lc}` | Lowercase |
| `${host_uc}` | Uppercase |
| `${domain}` | Windows domain name |
| `${domain_lc}` | Domain lowercase |
| `${domain_uc}` | Domain uppercase |

---

## Step 5 — Restart NSClient++

```
net stop nscp
net start nscp
```

After restart, NSClient++ will start executing scheduled checks and pushing results to the NSCA server. Your monitoring tool should begin receiving passive check results within one interval.

---

## Complete Configuration Example

```ini
[/modules]
CheckSystem  = enabled
CheckDisk    = enabled
Scheduler    = enabled
NSCAClient   = enabled

[/settings/NSCA/client]
hostname = ${host_lc}.${domain_lc}

[/settings/NSCA/client/targets/default]
address    = 10.0.0.1
encryption = aes256
password   = s3cr3t_p@ssw0rd

[/settings/scheduler/schedules/default]
channel  = NSCA
interval = 5m
report   = all

[/settings/scheduler/schedules]
host_check = check_ok
cpu        = check_cpu
memory     = check_memory
disk_c     = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

---

## Using NRDP Instead of NSCA

NRDP is a more modern HTTP-based alternative to NSCA. Replace `NSCAClient` with `NRDPClient` and update the configuration:

```ini
[/modules]
NRDPClient = enabled

[/settings/NRDP/client/targets/default]
address  = http://nagios-server/nrdp/
token    = my-nrdp-token
```

The scheduler configuration stays the same — just change `channel = NSCA` to `channel = NRDP`.

---

## Monitoring Configuration on the Server Side

In Nagios/Icinga, define your services as **passive** with a freshness threshold:

```cfg
define service {
    host_name              win-server-01.example.com
    service_description    CPU Load
    check_command          check_dummy!3!"No data received"
    passive_checks_enabled  1
    active_checks_enabled   0
    check_freshness         1
    freshness_threshold     600   ; seconds (10 minutes)
}
```

---

## Next Steps

- [Getting Started: NSCA](../getting-started/nsca.md) — detailed NSCA setup walkthrough
- [Event Log Monitoring](event-log.md) — add real-time event log alerts to passive monitoring
- [Reference: NSCAClient](../reference/client/NSCAClient.md) — full configuration reference
- [Reference: Scheduler](../reference/generic/Scheduler.md) — full scheduler reference
