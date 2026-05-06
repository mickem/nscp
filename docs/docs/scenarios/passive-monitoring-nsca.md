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

### NSCA server-side configuration

The NSCA daemon needs the matching cipher and password in its `nsca.cfg`:

```text
decryption_method = 14
password          = secret-password
```

If you're troubleshooting the connection, redirect the result file to a path
you can `tail` to see what arrives:

```text
command_file = /tmp/result.txt
```

!!! note
    The password is a shared secret — both sides must use the same value, and
    it isn't authenticated in the cryptographic sense (no challenge/response).
    Use a strong, long password to make brute-force impractical.

### NSCA encryption reference

NSClient++ uses Crypto++; NSCA uses libmcrypt. The two libraries don't share a
common identifier, so the matching is by table:

| NSClient++ value | NSCA method | Security                      |
|------------------|-------------|-------------------------------|
| `none`           | 0           | ⚠️ Not secure — avoid         |
| `xor`            | 1           | ⚠️ Not secure — avoid         |
| `des`            | 2           | ⚠️ Insecure                   |
| `3des`           | 3           | ⚠️ Legacy                     |
| `cast128`        | 4           | 🟡 Moderate                   |
| `xtea`           | 6           | 🟡 Moderate                   |
| `blowfish`       | 8           | 🟡 Moderate                   |
| `twofish`        | 9           | 🟢 Very secure                |
| `rc2`            | 11          | ⚠️ Insecure                   |
| `aes256`         | 14          | ✅ Industry standard (default) |
| `serpent`        | 20          | 🟢 Paranoid                   |
| `gost`           | 23          | ⚠️ Questionable               |

!!! warning "AES naming gotcha"
    NSCA names ciphers by **block size**, NSClient++ names them by **key
    size**. NSCA's `RIJNDAEL-128` (method `14`) is what NSClient++ calls
    `aes256` — same algorithm, different label. NSCA only supports AES with a
    128-bit block, so the other AES-named NSCA values are not valid choices.

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

### Schedule options reference

| Option     | Description                                                                                  |
|------------|----------------------------------------------------------------------------------------------|
| `interval` | How often the check runs (e.g. `30s`, `5m`, `1h`). Apply via the `default` template.         |
| `schedule` | Cron-style alternative to `interval` (see below). Use one or the other, not both.            |
| `command`  | The check command to execute. With short-form (`name = command`) this is the value.          |
| `alias`    | Name reported back to Nagios. Defaults to the schedule key, so usually omitted.              |
| `channel`  | Target channel for the result. Defaults to `NSCA`; set this to fan out to other channels.    |
| `report`   | Filter on which results are sent: `all`, `ok`, `warning`, `critical`, or comma-combinations. |

### Cron-style schedules

When you need finer control than "every N minutes" — e.g. "47 minutes past every
hour", "weekdays at 06:00", or "every Sunday at midnight" — use `schedule`
instead of `interval`. The five fields are the standard cron form:
**minute hour day-of-month month day-of-week**.

```ini
[/settings/scheduler/schedules/disk_c]
command  = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
schedule = 47 * * * *      ; 47 minutes past every hour
channel  = NSCA

[/settings/scheduler/schedules/nightly_full]
command  = check_drivesize
schedule = 0 2 * * *       ; every day at 02:00
channel  = NSCA

[/settings/scheduler/schedules/weekday_office]
command  = check_cpu
schedule = */15 8-18 * * 1-5  ; every 15 min, 08:00–18:00, Mon–Fri
channel  = NSCA
```

Use `interval` for the simple recurring case; reach for `schedule` when you
need a specific wall-clock time or weekday filter.

### Short-form vs. long-form schedules

The `[/settings/scheduler/schedules]` block supports two equivalent shapes:

```ini
; Short form — the key is the alias, the value is the command.
[/settings/scheduler/schedules]
cpu  = check_cpu
mem  = check_memory
```

```ini
; Long form — one section per schedule, more knobs available.
[/settings/scheduler/schedules/cpu]
command  = check_cpu
interval = 30s

[/settings/scheduler/schedules/mem]
command  = check_memory
interval = 5m
```

Short form is concise but only carries `command`. Switch to long form when
you need per-schedule overrides (interval, channel, alias, report).

### Real-time channels

The Scheduler isn't the only thing that can publish into the NSCA channel.
`CheckLogFile` and `CheckEventLog` can both publish results in real time —
useful for surfacing log/event hits without waiting for the next scheduler
tick. They share the same channel mechanism, so a result from `CheckEventLog`
is indistinguishable from a scheduled one on the receiving side.

```ini
[/settings/eventlog/real-time/filters/critical-app]
log     = Application
filter  = level = 'error' AND source = 'MyApp'
channel = NSCA
```

### Multiple targets

A single `NSCAClient` instance handles one target, but you can fan results
out to multiple servers (or channels) by combining target sections with the
`channel` knob on each schedule. The receiver matches by channel name.

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

Variables can be combined with literal text — for example to add a `win_`
prefix on Windows hosts that share a Linux-host naming convention:

```ini
[/settings/NSCA/client]
hostname = win_${host_lc}.${domain_lc}
```

Supported variables:

| Variable       | Meaning                            |
|----------------|------------------------------------|
| `${host}`      | Windows computer name (mixed case) |
| `${host_lc}`   | Lowercase                          |
| `${host_uc}`   | Uppercase                          |
| `${domain}`    | Windows domain name                |
| `${domain_lc}` | Domain lowercase                   |
| `${domain_uc}` | Domain uppercase                   |

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

## Troubleshooting

NSCA is fire-and-forget — the agent doesn't get a delivery acknowledgement,
so a misconfigured client will look fine from NSClient++'s side and silent
on the server side. The first place to look is the NSCA daemon log on the
monitoring server:

```
sudo tail -f /var/log/syslog
```

A successful submission shows up as:

```
nsca: Connection from 192.168.0.104 port 8198
nsca: Handling the connection...
nsca: SERVICE CHECK -> Host Name: 'win-server-01', Service Description: 'CPU Load',
      Return Code: '0', Output: 'OK CPU Load ok.|...'
nsca: End of connection...
```

A misconfigured submission usually surfaces as:

```
nsca: Connection from 192.168.0.104 port 26117
nsca: Handling the connection...
nsca: Received invalid packet type/version from client
```

That message means almost always one of two things:

- **Wrong password** — the shared secret on the agent doesn't match
  `nsca.cfg`'s `password`.
- **Wrong encryption** — the cipher mismatch (e.g. you set `encryption =
  aes256` but the daemon has `decryption_method = 9`).

If the daemon log is silent altogether, the connection isn't reaching it —
check `address`, `port`, firewalls, and `allowed_hosts` on the NSCA daemon.

To see what NSClient++ is sending, restart it in test mode:

```
net stop nscp
nscp test
```

…and watch the trace lines as the scheduler fires.

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

- [Event Log Monitoring](event-log.md) — add real-time event log alerts to passive monitoring
- [Reference: NSCAClient](../reference/client/NSCAClient.md) — full configuration reference
- [Reference: Scheduler](../reference/generic/Scheduler.md) — full scheduler reference
