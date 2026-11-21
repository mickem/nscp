# Migrating from the Icinga 2 Windows Agent

**Goal:** Replace the Icinga agent on a Windows host with NSClient++ - one host at a time, keeping the master, the
host and service objects, their history and the host's certificate - by letting NSClient++ take the agent's place on
the Icinga cluster protocol.

This page is the migration path. The [Native Icinga 2 Agent](icinga-agent.md) scenario describes the same module for
a host that never had an Icinga agent, and explains each setting in more depth.

<!-- @formatter:off -->
!!! warning "Experimental"
    The `IcingaAgent` module is new and marked experimental: its settings and behaviour may still change. Migrate a
    pilot host first and keep the rollback at the end of this page at hand.
<!-- @formatter:on -->

It covers both common starting points:

| You run today                                                                   | Checks are defined as                                               |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------|
| The **Icinga 2 agent for Windows** (the `icinga2` MSI and its Windows service)   | ITL commands: `disk-windows`, `load-windows`, ..., or `nscp-local-*` |
| **Icinga for Windows** (the PowerShell framework and plugins, managing an agent) | `Invoke-IcingaCheck*` commands, through `ifw-api` or PowerShell     |

---

## What stays and what changes

| Stays                                                                | Changes                                                                        |
|----------------------------------------------------------------------|--------------------------------------------------------------------------------|
| The master, its zones, the host's `Endpoint` and `Zone`              | The software on the host: NSClient++ instead of `icinga2` (and PowerShell)     |
| `Host` and `Service` objects, their names, history and downtimes     | Each service's `check_command`: an NSClient++ query, defined with `ifw-api`    |
| `command_endpoint`: the master still schedules, the host still runs  | Thresholds: NSClient++ filter expressions (`free < 10%`) instead of bare numbers |
| The host's certificate and the Icinga CA (copied over, see step 5)   | Performance data labels, so graphs start new series                            |
| Port `5665`, with the agent connecting to the master                 | The master can no longer connect *to* the agent; the agent always connects out |

---

## Before you start: does the host fit?

NSClient++ implements the part of the cluster protocol an agent needs to run checks for its parent. Check these
before you plan a host's migration:

| The Icinga agent on this host...                                               | With NSClient++                                                                                   |
|--------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------|
| Is connected **to** by the master (`host` set on its `Endpoint` on the master) | Not supported: the agent connects out. The host needs outbound TCP `5665` to its parent.          |
| Has **two parents** (a zone with two masters or two satellites)                | Not supported yet: the agent keeps one connection, to `parent host`. Leave these hosts for later. |
| Runs event commands or notifications, or the master's `execute-command`        | Refused: the module only runs check queries.                                                      |
| Keeps checking on its own schedule while the master is unreachable             | No: checks run while the parent is connected, and nothing is replayed afterwards.                 |
| Receives configuration from the master (`accept_config`)                       | Not needed: there is no Icinga configuration on the host. Check definitions live on the master.   |
| Runs your own plugins or scripts                                               | Keep them: run them through [External Scripts](external-scripts.md), see the mapping below.       |

On the master you need **Icinga 2.14** or later, for the `ifw-api` check command.

---

## Step 1 — Take stock of what the host runs

List the services the master runs on the agent and the commands behind them:

```
icinga2 object list --type Service --name 'win-host-01!*' | grep -E '^Object|check_command|command_endpoint'
```

Every `check_command` executed through `command_endpoint` needs an NSClient++ counterpart. Enable the module that
provides it on the agent.

### ITL Windows plugins

| Icinga command    | NSClient++ query                    | Module       | Notes                                                             |
|-------------------|-------------------------------------|--------------|-------------------------------------------------------------------|
| `disk-windows`    | `check_drivesize`                   | CheckDisk    | `drive=C:`; several drives or `drive=*` in one service            |
| `load-windows`    | `check_cpu`                         | CheckSystem  | `time=5m` picks the averaging window                              |
| `memory-windows`  | `check_memory`                      | CheckSystem  | `type=physical`                                                   |
| `swap-windows`    | `check_pagefile`                    | CheckSystem  | or `check_memory type=committed`                                  |
| `service-windows` | `check_service`                     | CheckSystem  | `service=Spooler`; without it, every auto-start service is checked |
| `procs-windows`   | `check_process`                     | CheckSystem  | `process=sqlservr.exe`                                            |
| `uptime-windows`  | `check_uptime`                      | CheckSystem  |                                                                   |
| `network-windows` | `check_network`                     | CheckSystem  |                                                                   |
| `perfmon-windows` | `check_pdh`                         | CheckSystem  | `counter=\Processor(_Total)\% Processor Time`                     |
| `update-windows`  | `check_os_updates`                  | CheckSystem  | `check_pending_reboot` covers the reboot flag                     |
| `users-windows`   | `check_users`                       | CheckSecurity |                                                                  |
| `ping-windows`    | `check_ping`                        | CheckNet     | `host=192.168.0.1`                                                |

### `nscp-local` commands

The Icinga 2 Windows installer could bundle an old NSClient++, which the ITL's `nscp-local-*` commands start once per
check. The queries are the same ones, now answered by a running agent:

| Icinga command                                              | NSClient++ query   |
|-------------------------------------------------------------|--------------------|
| `nscp-local-cpu`, `-memory`, `-pagefile`, `-uptime`         | `check_cpu`, `check_memory`, `check_pagefile`, `check_uptime` |
| `nscp-local-disk`                                           | `check_drivesize`  |
| `nscp-local-service`, `-process`                            | `check_service`, `check_process` |
| `nscp-local-counter`                                        | `check_pdh`        |
| `nscp-local-tasksched`                                      | `check_tasksched`  |
| `nscp-local-os-version`, `-version`                         | `check_os_version`, `check_version` |
| `nscp-local` with `nscp_query` and `nscp_arguments`         | the `nscp-query` command in step 2 |

The arguments you pass in `nscp_arguments` today carry over unchanged.

### Icinga for Windows plugins

| Icinga for Windows plugin                | NSClient++ query                    | Module        |
|------------------------------------------|-------------------------------------|---------------|
| `Invoke-IcingaCheckCPU`                  | `check_cpu`                         | CheckSystem   |
| `Invoke-IcingaCheckMemory`               | `check_memory`                      | CheckSystem   |
| `Invoke-IcingaCheckUsedPartitionSpace`   | `check_drivesize`                   | CheckDisk     |
| `Invoke-IcingaCheckDiskHealth`           | `check_disk_health`                 | CheckDisk     |
| `Invoke-IcingaCheckService`              | `check_service`                     | CheckSystem   |
| `Invoke-IcingaCheckProcess`              | `check_process`                     | CheckSystem   |
| `Invoke-IcingaCheckUptime`               | `check_uptime`                      | CheckSystem   |
| `Invoke-IcingaCheckNetworkInterface`     | `check_network`                     | CheckSystem   |
| `Invoke-IcingaCheckPerfCounter`          | `check_pdh`                         | CheckSystem   |
| `Invoke-IcingaCheckUpdates`              | `check_os_updates`                  | CheckSystem   |
| `Invoke-IcingaCheckTimeSync`             | `check_w32time`, `check_ntp_offset` | CheckSystem   |
| `Invoke-IcingaCheckEventlog`             | `check_eventlog`                    | CheckEventLog |
| `Invoke-IcingaCheckScheduledTask`        | `check_tasksched`                   | CheckTaskSched |
| `Invoke-IcingaCheckDirectory`            | `check_files`                       | CheckDisk     |
| `Invoke-IcingaCheckCertificate`          | `check_certificate`                 | CheckSecurity |
| `Invoke-IcingaCheckFirewall`             | `check_firewall`                    | CheckSecurity |
| `Invoke-IcingaCheckNLA`                  | `check_nla`                         | CheckSecurity |
| `Invoke-IcingaCheckUsers`                | `check_users`                       | CheckSecurity |

The existing `Invoke-IcingaCheck*` commands cannot simply be pointed at NSClient++: their arguments (`-Warning 80`)
are the PowerShell plugin's, not the query's.

### Your own plugins and scripts

A plugin or PowerShell script the Icinga agent runs today keeps working as an external script. Register it on the
agent and call it by its name, like any other query:

```ini
[/modules]
CheckExternalScripts = enabled

[/settings/external scripts/scripts]
check_backup = scripts\check_backup.ps1
```

See [External Scripts](external-scripts.md) for arguments and wrappings.

---

## Step 2 — Add the NSClient++ commands on the master

Add the new `CheckCommand` objects next to the existing ones. Nothing uses them yet, so this step is safe to deploy
at any time. Each one imports `ifw-api`; the master then sends the query name and its arguments to the agent:

```icinga2
// /etc/icinga2/zones.d/global-templates/nscp-commands.conf
object CheckCommand "nscp-disk" {
  import "ifw-api"
  vars.ifw_api_command = "check_drivesize"
  vars.ifw_api_arguments = {
    "drive"    = { value = "$nscp_disk_drive$" }
    "warning"  = { value = "$nscp_disk_warning$" }
    "critical" = { value = "$nscp_disk_critical$" }
  }
}

object CheckCommand "nscp-cpu" {
  import "ifw-api"
  vars.ifw_api_command = "check_cpu"
  vars.ifw_api_arguments = {
    "warning"  = { value = "$nscp_cpu_warning$" }
    "critical" = { value = "$nscp_cpu_critical$" }
    "time"     = { value = "$nscp_cpu_time$" }
  }
}

object CheckCommand "nscp-memory" {
  import "ifw-api"
  vars.ifw_api_command = "check_memory"
  vars.ifw_api_arguments = {
    "warning"  = { value = "$nscp_mem_warning$" }
    "critical" = { value = "$nscp_mem_critical$" }
    "type"     = { value = "$nscp_mem_type$" }
  }
}

object CheckCommand "nscp-service" {
  import "ifw-api"
  vars.ifw_api_command = "check_service"
  vars.ifw_api_arguments = {
    "service" = { value = "$nscp_service_name$" }
  }
}

// Any query, with its arguments written the NSClient++ way.
object CheckCommand "nscp-query" {
  import "ifw-api"
  vars.ifw_api_command = "$nscp_query$"
  vars.ifw_api_arguments = {
    "(arguments)" = { value = "$nscp_arguments$", skip_key = true }
  }
}
```

The `nscp-` prefix keeps the new commands apart from the ITL's and from the names of the queries themselves, so both
generations can exist while hosts move over. `nscp-query` covers everything you do not want a dedicated command for.
An argument whose variable is not set is left out, and the query's own default applies.

<!-- @formatter:off -->
!!! note "Icinga Director"
    In Director, create the same commands as check commands that import `ifw-api`, with the `nscp_*` variables as
    data fields. The cut-over in step 3 is then a field on the host template instead of a host variable in a file.
<!-- @formatter:on -->

---

## Step 3 — Make the services switchable per host

Both agents cannot serve the same host at the same time, so a host switches all its agent services at once. Drive
that from one host variable. For a set of `apply` rules like this:

```icinga2
apply Service "disk" {
  check_command = "disk-windows"
  command_endpoint = host.name
  vars.disk_win_warn = "20%"
  vars.disk_win_crit = "10%"
  assign where host.vars.os == "Windows"
}
```

exclude migrated hosts from the old rule and add the new one under the **same service name**, so the service keeps
its history, downtimes and notification settings when the host switches:

```icinga2
apply Service "disk" {
  check_command = "disk-windows"
  command_endpoint = host.name
  vars.disk_win_warn = "20%"
  vars.disk_win_crit = "10%"
  assign where host.vars.os == "Windows" && host.vars.agent != "nscp"
}

apply Service "disk" {
  check_command = "nscp-disk"
  command_endpoint = host.name
  vars.nscp_disk_drive = "*"
  vars.nscp_disk_warning = "free < 20%"
  vars.nscp_disk_critical = "free < 10%"
  assign where host.vars.os == "Windows" && host.vars.agent == "nscp"
}
```

### Translating thresholds

An Icinga plugin takes a number and knows by itself which direction is bad. An NSClient++ threshold is an expression
that says so, which is also what lets one check alert on several things at once:

| Icinga                                           | NSClient++                                                         |
|--------------------------------------------------|--------------------------------------------------------------------|
| `disk_win_warn = "20%"`, `disk_win_crit = "10%"`  | `warning=free < 20%`, `critical=free < 10%`                        |
| `load_win_warn = 80`, `load_win_crit = 95`       | `warning=load > 80`, `critical=load > 95`                          |
| `memory_win_warn = "10%"`                        | `warning=free < 10%`                                               |
| `service_win_service = "Spooler"`                | `service=Spooler` (alerts when the service is not running)         |
| `procs_win_user`, a minimum count                | `process=app.exe`, `critical=count < 1`                            |

Check the direction of every threshold you carry over against what the old service alerted on, rather than copying
the number. [Checks In Depth](../concepts/checks.md) describes the expression syntax, and each query's reference page
lists its keywords and defaults.

Validate and deploy (`icinga2 daemon -C && systemctl reload icinga2`). No host has `vars.agent = "nscp"` yet, so
nothing changes.

---

## Step 4 — Install NSClient++ next to the Icinga agent

Install NSClient++ on the host while the Icinga agent keeps running. The *Icinga 2 agent* feature of the installer is
the `IcingaAgent` module; it stays idle until it has a `parent host`.

```batch
msiexec /qn /i NSCP-<version>-x64.msi
```

Enable the modules behind the queries from step 1:

```ini
[/modules]
IcingaAgent = enabled
CheckSystem = enabled
CheckDisk = enabled
```

Now try the checks on the host itself, with the arguments the master will send, before anything depends on them:

```
nscp client --module CheckDisk --boot --query check_drivesize "drive=*" "warning=free < 20%" "critical=free < 10%"
```

Compare the result with what the service shows in Icinga today. This is the moment to adjust thresholds and filters:
the host is still monitored by the old agent.

---

## Step 5 — Cut over, keeping the host's identity

The Icinga agent already holds what NSClient++ would otherwise have to request: a key, a certificate the Icinga CA
signed for this endpoint, and the CA itself. NSClient++ uses the same three files under the same names, so copying
them lets it connect as the endpoint the master already knows - no ticket, no `icinga2 ca sign`, and the master is
verified from the first connection.

Look up three values in the Icinga agent's configuration (`C:\ProgramData\icinga2\etc\icinga2`):

| Value             | Where                                                                         | NSClient++ setting |
|-------------------|-------------------------------------------------------------------------------|--------------------|
| The agent's name  | `NodeName` in `constants.conf`; also the file name in the `certs` folder      | `endpoint name`    |
| The parent's name | The parent `Endpoint` in `zones.conf`                                         | `parent endpoint`  |
| The parent's address | `host` of that `Endpoint`                                                  | `parent host`      |

Write them to `nsclient.ini`. `endpoint name` must be spelled exactly as the Icinga agent's name, capitals included:
it selects the certificate files and has to match the `Endpoint` object on the master.

```ini
[/settings/Icinga/agent]
parent host = icinga-master.example.com
parent endpoint = icinga-master.example.com
endpoint name = win-host-01.example.com
trust on first use = false                 ; the CA is copied in below, so never connect unverified
```

Schedule a downtime for the host in Icinga, then swap the agents. In an elevated PowerShell:

```powershell
$cn  = 'win-host-01.example.com'                       # endpoint name
$src = 'C:\ProgramData\icinga2\var\lib\icinga2\certs'
$dst = 'C:\Program Files\NSClient++\security\icinga'   # the `certificate directory`

Stop-Service icinga2
Set-Service icinga2 -StartupType Disabled

New-Item -ItemType Directory -Force $dst | Out-Null
Copy-Item "$src\$cn.crt", "$src\$cn.key", "$src\ca.crt" $dst
# The key: SYSTEM and Administrators only.
icacls "$dst\$cn.key" /inheritance:r /grant:r '*S-1-5-18:F' '*S-1-5-32-544:F'

Restart-Service nscp
```

`certificate directory` defaults to `${certificate-path}/icinga`: `security\icinga` under the install folder, or
under `%ProgramData%\NSClient++` with the modern [file layout](../concepts/file-layout.md).

The NSClient++ log shows a verified connection straight away, without the bootstrap lines of a new agent:

```
Icinga agent: Connected to Icinga endpoint 'icinga-master.example.com' at icinga-master.example.com:5665
```

Then switch the host on the master and reload:

```icinga2
object Host "win-host-01.example.com" {
  // ...
  vars.agent = "nscp"
}
```

If the host's `Endpoint` on the master has a `host` (and `port`) attribute, remove it in the same change: the master
would otherwise keep trying to connect to an agent that no longer listens, and log each failure.

Between the two halves of the swap, services are `UNKNOWN` for a check interval or so - NSClient++ is asked for a
query called `disk-windows`, or the Icinga agent for a command it does not have. That is what the downtime is for.

<!-- @formatter:off -->
!!! tip "Starting with a fresh certificate instead"
    If you would rather not copy a private key, skip the copy: set `ticket` (from `icinga2 pki ticket --cn <endpoint
    name>` on the master), copy only `ca.crt`, and NSClient++ generates its own key and has it signed, as in
    [step 4 of the agent scenario](icinga-agent.md#step-4-configure-and-start-the-agent). The endpoint name stays
    the same, so nothing changes on the master.
<!-- @formatter:on -->

---

## Step 6 — Verify

In Icinga Web 2, or through the API, each migrated service now shows the NSClient++ output, with `check_source` still
the host's endpoint name:

```
curl -k -s -u <api user> 'https://localhost:5665/v1/objects/services?filter=host.name==%22win-host-01.example.com%22&attrs=last_check_result'
```

```
disk      OK        OK: All drives ok                      'C:\ free'=176GB;44;22;0;223 'C:\ free %'=79%;19;9;0;100
cpu       OK        OK: CPU load is ok.                    'total 5m'=3%;80;95
memory    OK        OK: physical = 6.2GB                   'physical'=6.2GB;14.1;15.3;0;15.7 'physical %'=39%;90;98;0;100
```

Check three things before you end the downtime:

- **Every service has a fresh result.** A service still on an old command, or on a query whose module is not
  enabled, reports `UNKNOWN` and names the query.
- **The states match what the old agent reported**, in particular for services that were not `OK` before.
- **The host is UP**: its `cluster-zone` check reports the zone as connected.

Performance data labels differ from the Icinga plugins' (`'C:\ free'` rather than `C:`), so a graphing backend starts
new series for the migrated services. The old ones remain; nothing is renamed for you.

---

## Step 7 — Restrict the master and remove the old agent

Once the host has run on NSClient++ for a while:

1. **Grant the master only what it monitors.** Every check runs as `IcingaAgent:<parent endpoint>`. The Icinga agent
   ran any command the master named; NSClient++ can be told which queries that is:

    ```ini
    [/settings/permissions]
    enabled = true

    [/settings/permissions/policies]
    IcingaAgent:icinga-master.example.com = CheckSystem.*, CheckDisk.check_drivesize
    ```

    See [Permissions](../concepts/permissions.md) for the observe-then-enforce rollout.

2. **Uninstall the Icinga agent** from *Apps & features* (Icinga for Windows has its own
   `Uninstall-IcingaForWindows`), and close the inbound firewall rule for port `5665` if the host had one.

3. **Delete the old copy of the key.** Uninstalling leaves `C:\ProgramData\icinga2` behind, including
   `var\lib\icinga2\certs\<name>.key` - the same key NSClient++ now uses. Remove the folder.

4. **Drop the old half of the configuration** when the last host has moved: the `host.vars.agent != "nscp"` rules,
   and then the variable itself.

Certificate renewal needs nothing from you: the master renews the certificate as it did for the Icinga agent, see
[Certificate renewal](icinga-agent.md#certificate-renewal).

---

## Rolling back

Until the old agent is uninstalled, a host goes back in a minute:

```powershell
Stop-Service nscp
Set-Service icinga2 -StartupType Automatic
Start-Service icinga2
```

and on the master, remove `vars.agent = "nscp"` from the host and reload. Both agents use the same certificate, so
the master accepts either; just never run both against it at once. Clear `parent host` in `nsclient.ini` if
NSClient++ is to keep running on the host for anything else.

---

## Rolling out to more hosts

Steps 1 to 3 are done once. For each further host it is step 4 (install, with the modules and the
`/settings/Icinga/agent` section delivered by your deployment tool), the swap in step 5 and the host variable.
`endpoint name` defaults to the lower-cased host name, so set it explicitly wherever the Icinga agent's name is a
fully qualified name or contains capitals. To hand out the same configuration to many hosts, see
[Automated installation](../setup/installing.md#automated-installation-windows-msi).

---

## Troubleshooting

| Symptom                                                                    | Cause and fix                                                                                              |
|----------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
| Services `UNKNOWN`, naming `disk-windows`, `Invoke-IcingaCheckCPU`, ...    | The host runs NSClient++ but its services still use the old commands: set `vars.agent = "nscp"` and reload the master. |
| Services `UNKNOWN: Check command 'nscp-disk' does not exist`               | The other way round: the host variable is set but the Icinga agent is still the one connected.             |
| `connecting with a self-signed certificate ... to request a signed one`    | NSClient++ found no `<endpoint name>.crt` / `.key` and made its own. `endpoint name` does not match the copied file names; fix it, delete the generated pair and restart. |
| `could not be used together and were replaced`                             | The copied certificate and key do not belong together (a stale file from an earlier agent install). Copy both again from the Icinga agent, or sign the new pair with `icinga2 ca sign`. |
| `Parent presented the certificate of 'x', expected 'y'`                    | `parent endpoint` is not the parent's endpoint name. Take it from the Icinga agent's `zones.conf`.         |
| `` `trust on first use` is off: not connecting ``                          | `ca.crt` is not in the certificate directory. Copy it; it is picked up within seconds.                     |
| Host DOWN, master logs connection attempts to the host                     | The `Endpoint` on the master still has `host` set and the agent is not connecting out: check outbound `5665` and the NSClient++ log. |
| A service is `OK` where it used to alert                                   | A threshold was copied with the wrong direction, or the query's default filter excludes the item. Run the query on the host as in step 4. |

The [agent scenario's troubleshooting table](icinga-agent.md#troubleshooting) covers the connection and certificate
messages in full.

---

## Next steps

- [Native Icinga 2 Agent](icinga-agent.md) - every part of the setup in depth, including how the master is trusted.
- [IcingaAgent reference](../reference/client/IcingaAgent.md) - every setting of the module.
- [Windows Server Health](windows-server-health.md) - a baseline of checks, if the migration is also a clean-up.
- [Permissions](../concepts/permissions.md) - grant the master only what it monitors.
