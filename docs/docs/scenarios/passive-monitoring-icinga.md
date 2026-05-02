# Passive Monitoring with Icinga 2

**Goal:** Have NSClient++ run checks on a schedule and submit the results to an [Icinga 2](https://icinga.com/) server
through its REST API — without Icinga 2 needing to poll each Windows host directly.

!!! tip
The Icinga 2 client uses HTTPS and the standard Icinga 2 REST API (`/v1/actions/process-check-result`). It is a good fit
when your monitoring server runs Icinga 2, the agents are behind a firewall, or you cannot install the Icinga agent on
the Windows hosts.

---

## How Passive Monitoring Works

In **active (NRPE / agent) monitoring**, the monitoring server initiates contact and polls the agent:

```
Icinga 2 Server ──check_nrpe──► NSClient++ → runs check → returns result
```

In **passive monitoring with the Icinga 2 REST API**, the agent runs checks on its own schedule and submits the results
over HTTPS:

```
NSClient++ (Scheduler) → runs check → IcingaClient ──HTTPS──► Icinga 2 (/v1/actions/process-check-result)
```

The Icinga 2 server simply waits for results. If no result arrives within the expected time window, Icinga 2 raises a
freshness alert.

---

## Prerequisites

Enable these modules in `nsclient.ini`:

```ini
[/modules]
CheckSystem = enabled   ; provides check_cpu, check_memory, etc.
CheckDisk = enabled     ; provides check_drivesize
Scheduler = enabled     ; runs checks on a timer
IcingaClient = enabled  ; submits results to the Icinga 2 REST API
CheckHelpers = enabled  ; provides the check_ok command

```

You also need an Icinga 2 user with the right permissions. On the Icinga 2 server, add an `ApiUser` that is allowed to
run the `process-check-result` action and (optionally) create objects:

```cfg
object ApiUser "nscp" {
    password = "secret-password"
    permissions = [
        "actions/process-check-result",
        "objects/query/Host",
        "objects/query/Service",
        "objects/create/Host",
        "objects/create/Service"
    ]
}
```

The last three `objects/*` permissions are only required if you want NSClient++ to auto-create missing host/service
objects (see [Step 5](#step-5--optional-auto-create-host-and-service-objects)).

---

## Step 1 — Verify the Icinga 2 Connection

Before configuring scheduled checks, verify that NSClient++ can submit a test result to the Icinga 2 REST API.

```
nscp client --module IcingaClient --query submit_icinga ^
    --address https://icinga2.example.com:5665/ ^
    --username nscp --password secret-password ^
    --command "service check" ^
    --result 2 ^
    --message "Hello from NSClient++"
```

!!! note
The `^` character is the Windows **Command Prompt** line-continuation character. In PowerShell use a backtick (`` ` ``)
instead, or write the command on a single line.

Expected output:

```
Successfully processed check result for object
```

In the Icinga 2 web UI you should immediately see the service `service check` on the host transition to **CRITICAL**
with the message `Hello from NSClient++`. If you do not, double-check:

- The `ApiUser` exists on the Icinga 2 server and has the `actions/process-check-result` permission.
- A host with the matching name (the Windows hostname unless you override it — see [Step 4](#step-4--set-the-hostname))
  and a service called `service check` already exist on the Icinga 2 side, or `ensure objects` is enabled (Step 5).
- Port 5665 is reachable from the Windows host.

### TLS verify modes

Icinga 2's REST API ships with a self-signed certificate by default. Choose the verify mode that matches your setup:

| `verify mode` value | Behaviour                               | When to use                |
|---------------------|-----------------------------------------|----------------------------|
| `none`              | Accept any certificate                  | 🚀 Quick start / lab       |
| `peer-cert`         | Validate against the supplied `ca` file | ✅ Production (recommended) |
| `peer`              | Validate but do not require a peer cert | ⚠️ Rare                    |

To validate the Icinga 2 self-signed certificate in production, copy the Icinga 2 CA (`/var/lib/icinga2/ca/ca.crt` on
the Icinga 2 master) to the Windows host and reference it via the `ca` setting.

---

## Step 2 — Configure the Scheduler

The `Scheduler` module runs your checks at regular intervals and sends results to a **channel** (`ICINGA` by default for
IcingaClient).

**Set default parameters for all scheduled jobs:**

```ini
[/settings/scheduler/schedules/default]
channel = ICINGA  ; which channel to send results to
interval = 5m      ; how often to run each check
report = all     ; send results regardless of status (OK, WARN, CRIT)
```

**Add scheduled checks:**

```ini
[/settings/scheduler/schedules]
; Format: check_name = check_command [arguments...]
host_check = check_ok
cpu = check_cpu
memory = check_memory
disk_c = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

Each schedule key becomes the **service name** on the Icinga 2 side, except for the special name `host_check` — that one
is submitted as a **host** check result rather than a service result. Use it for any check that should drive the host's
UP/DOWN state in Icinga 2.

!!! note
The `default` schedule section applies its settings to all schedules that do not override them. You can override any
setting per schedule by creating a dedicated section:

```ini
[/settings/scheduler/schedules/disk_c]
command  = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
interval = 15m   ; check disk less often than cpu/memory
channel  = ICINGA
report   = all
```

---

## Step 3 — Configure the Icinga 2 Target

```ini
[/settings/Icinga/client/targets/default]
address = https://icinga2.example.com:5665/
username = nscp
password = secret-password
verify mode = peer-cert
ca = c:\program files\nsclient++\security\icinga2-ca.crt
```

| Key           | Purpose                                                                                |
|---------------|----------------------------------------------------------------------------------------|
| `address`     | Base URL of the Icinga 2 REST API. Defaults to port 5665 and protocol `https`.         |
| `username`    | The Icinga 2 `ApiUser` name.                                                           |
| `password`    | The `ApiUser` password.                                                                |
| `verify mode` | TLS verification — see the table in [Step 1](#step-1--verify-the-icinga-2-connection). |
| `ca`          | CA bundle used when `verify mode = peer-cert`.                                         |
| `tls version` | Optional TLS version pin (e.g. `1.2`, `1.3`).                                          |
| `timeout`     | Per-request timeout in seconds (default `30`).                                         |
| `proxy`       | HTTP proxy URL — see the [proxy notes below](#using-an-http-proxy).                    |
| `no proxy`    | Comma-separated list of hosts that bypass the proxy.                                   |

---

## Step 4 — Set the Hostname

NSClient++ must submit results using the **hostname as it is known in Icinga 2**. By default it uses the Windows
computer name, which may not match.

**Auto-detect hostname:**

```ini
[/settings/Icinga/client]
hostname = auto
```

**Use a specific hostname:**

```ini
[/settings/Icinga/client]
hostname = win-server-01.example.com
```

**Build the hostname from system variables:**

```ini
[/settings/Icinga/client]
hostname = ${host_lc}.${domain_lc}
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

This name is used both as the Icinga 2 host name in the URL (`?host=…` / `?service=host!service`) and as the
`check_source` field on the resulting check result, unless you override `check source` on the target explicitly.

---

## Step 5 — (Optional) Auto-create Host and Service Objects

By default, the host and service objects must already exist in the Icinga 2 configuration. If they do not, the REST API
rejects the submission. Enable `ensure objects` to have NSClient++ create them on demand:

```ini
[/settings/Icinga/client/targets/default]
address = https://icinga2.example.com:5665/
username = nscp
password = secret-password
ensure objects = true
host template = generic-host
service template = generic-service
check command = dummy
```

When `ensure objects = true`, NSClient++ probes each host/service with a `GET` first and only `PUT`s a new object when
the probe returns 404. Existing objects are left untouched. The `ApiUser` needs the `objects/query/*` and
`objects/create/*` permissions listed in the [Prerequisites](#prerequisites).

| Key                | Purpose                                                                                                |
|--------------------|--------------------------------------------------------------------------------------------------------|
| `ensure objects`   | When `true`, auto-create missing host/service objects before submitting.                               |
| `host template`    | Comma-separated Icinga 2 templates applied to auto-created hosts (default: `generic-host`).            |
| `service template` | Comma-separated Icinga 2 templates applied to auto-created services (default: `generic-service`).      |
| `check command`    | The `check_command` to set on auto-created service objects (default: `dummy`).                         |
| `check source`     | Override for the `check_source` field reported to Icinga 2. Defaults to the local hostname when empty. |

!!! warning
Auto-creating objects through the REST API does **not** persist them in your Icinga 2 configuration files — they are
stored in Icinga 2's runtime config repository. If you reconfigure your monitoring through Director or text config, the
auto-created objects may end up duplicated or removed. For production deployments most operators prefer to define
objects explicitly and leave `ensure objects = false`.

---

## Step 6 — Restart NSClient++

```
net stop nscp
net start nscp
```

After restart, NSClient++ will start executing scheduled checks and submitting results to Icinga 2. Your monitoring tool
should begin receiving passive check results within one interval.

---

## Complete Configuration Example

```ini
[/modules]
CheckSystem = enabled
CheckDisk = enabled
Scheduler = enabled
IcingaClient = enabled
CheckHelpers = enabled

[/settings/Icinga/client]
hostname = ${host_lc}.${domain_lc}

[/settings/Icinga/client/targets/default]
address = https://icinga2.example.com:5665/
username = nscp
password = s3cr3t_p@ssw0rd
verify mode = peer-cert
ca = c:\program files\nsclient++\security\icinga2-ca.crt
ensure objects = true

[/settings/scheduler/schedules/default]
channel = ICINGA
interval = 5m
report = all

[/settings/scheduler/schedules]
host_check = check_ok
cpu = check_cpu
memory = check_memory
disk_c = check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

---

## Using an HTTP Proxy

If the Windows host can only reach the Icinga 2 master through a corporate HTTP proxy, set `proxy` (and optionally
`no proxy`) on the target. HTTPS is tunnelled to the master via an HTTP `CONNECT` request, so the same setting works for
`https://` Icinga 2 URLs.

```ini
[/settings/Icinga/client/targets/default]
address = https://icinga2.example.com:5665/
username = nscp
password = s3cr3t_p@ssw0rd
proxy = http://proxy.corp.example:3128/
no proxy = localhost,127.0.0.1,.internal
```

If the proxy itself needs credentials, embed them in the URL — `@` and `:` inside the username/password must be
percent-encoded:

```ini
proxy = http://alice:s%40cret@proxy.corp.example:3128/
```

---

## Monitoring Configuration on the Icinga 2 Side

For pre-defined services (no `ensure objects`), set them up as passive with a freshness threshold so Icinga 2 alerts you
when results stop arriving:

```cfg
apply Service "CPU Load" {
    import "generic-service"

    check_command          = "dummy"
    enable_active_checks   = false
    enable_passive_checks  = true
    check_freshness        = true
    freshness_threshold    = 600   /* seconds (10 minutes) */
    vars.dummy_state       = 3     /* UNKNOWN if no result */
    vars.dummy_text        = "No data received"

    assign where host.vars.os == "Windows"
}
```

Match `freshness_threshold` to your scheduler `interval` plus some headroom (e.g. `interval = 5m` →
`freshness_threshold = 600`).

---

## Next Steps

- [Event Log Monitoring](event-log.md) — add real-time event log alerts to passive monitoring
- [Reference: Scheduler](../reference/generic/Scheduler.md) — full scheduler reference
