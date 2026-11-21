# Migrating from the Icinga 2 Windows Agent

**Goal:** Replace the Icinga agent on a Windows host with NSClient++ - one host at a time, keeping the master, the
host and service objects, their check commands, their history and the host's certificate - by letting NSClient++
take the agent's place on the Icinga cluster protocol.

This page is the migration path. The [Native Icinga 2 Agent](icinga-agent.md) scenario describes the same module for
a host that never had an Icinga agent, and explains each setting in more depth.

<!-- @formatter:off -->
!!! warning "Experimental"
    The `IcingaAgent` module is new and marked experimental: its settings and behaviour may still change. Migrate a
    pilot host first and keep the rollback at the end of this page at hand.
<!-- @formatter:on -->

How much there is to do depends on what the host's services run today:

| The services use                                                              | What it takes                                                                            |
|-------------------------------------------------------------------------------|------------------------------------------------------------------------------------------|
| The ITL's Windows plugins: `disk-windows`, `load-windows`, `service-windows`... | **Swap the agent.** NSClient++ answers these commands under their Icinga names.          |
| The ITL's `nscp-local` commands                                               | **Swap the agent.** The queries are the same ones, now answered by a running agent.      |
| **Icinga for Windows** plugins (`Invoke-IcingaCheck*`)                        | Swap the agent **and** move the services to NSClient++ commands ([step 6](#step-6-move-to-native-commands)). |
| Your own plugins and scripts                                                  | Swap the agent and register them as [external scripts](external-scripts.md).             |

---

## What stays and what changes

| Stays                                                                  | Changes                                                                        |
|------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| The master, its zones, the host's `Endpoint` and `Zone`                | The software on the host: NSClient++ instead of `icinga2`                      |
| `Host` and `Service` objects, their names, history and downtimes       | The check output text and the performance data labels, so graphs start new series |
| `check_command`, thresholds and variables of services on ITL commands  | What a few options mean in detail: see [what differs](#what-differs-from-the-icinga-plugins) |
| `command_endpoint`: the master still schedules, the host still runs    | The master can no longer connect *to* the agent; the agent always connects out |
| The host's certificate and the Icinga CA (copied over, see step 3)     |                                                                                |

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

---

## Step 1 — Take stock of what the host runs

List the services the master runs on the agent and the commands behind them:

```
icinga2 object list --type Service --name 'win-host-01!*' | grep -E '^Object|check_command|command_endpoint'
```

### Commands NSClient++ answers as they are

The master sends such a check as the command's name and the variables the service set. NSClient++ turns both into
the matching query, thresholds included. Enable the module that provides the query:

| Icinga command                            | NSClient++ query             | Module        | Variables that carry over                                          |
|-------------------------------------------|------------------------------|---------------|--------------------------------------------------------------------|
| `disk-windows`                            | `check_drivesize`            | CheckDisk     | `disk_win_warn`, `_crit`, `_path`, `_unit`, `_exclude`, `_show_used` |
| `memory-windows`                          | `check_memory type=physical` | CheckSystem   | `memory_win_warn`, `_crit`, `_unit`, `_show_used`                  |
| `swap-windows`                            | `check_pagefile` (the total) | CheckSystem   | `swap_win_warn`, `_crit`, `_unit`, `_show_used`                    |
| `load-windows`                            | `check_cpu`                  | CheckSystem   | `load_win_warn`, `_crit`                                           |
| `service-windows`                         | `check_service`              | CheckSystem   | `service_win_service`, `_warn`                                     |
| `procs-windows`                           | `check_process process=*`    | CheckSystem   | `procs_win_warn`, `_crit`, `_user`                                 |
| `uptime-windows`                          | `check_uptime`               | CheckSystem   | `uptime_win_warn`, `_crit`, `_unit`                                |
| `network-windows`                         | `check_network`              | CheckSystem   | `network_win_warn`, `_crit`                                        |
| `perfmon-windows`                         | `check_pdh`                  | CheckSystem   | `perfmon_win_counter`, `_warn`, `_crit`, `_type`                   |
| `update-windows`                          | `check_os_updates`           | CheckSystem   | `update_win_warn`, `_crit`, `ignore_reboot`                        |
| `users-windows`                           | `check_users`                | CheckSecurity | `users_win_warn`, `_crit`                                          |
| `ping-windows`, `ping4-`, `ping6-windows` | `check_ping`                 | CheckNet      | `ping_win_address`, `_wrta`, `_wpl`, `_crta`, `_cpl`, `_packets`, `_timeout` |
| `nscp-local`                              | the query in `nscp_query`    | as before     | `nscp_query`, `nscp_arguments`, `nscp_showall`                     |
| `nscp-local-cpu`, `-memory`, `-disk`, `-service`, `-counter`, `-tasksched`, `-pagefile`, `-process`, `-uptime`, `-os-version`, `-version` | the same query as before | as before | the command's own `nscp_*` variables |

The plugins' threshold forms all carry over, each meaning what it meant:

| Threshold        | Alerts when the value is | On free space (disk, memory, swap) |
|------------------|--------------------------|------------------------------------|
| `20`, `20%`      | above 20                 | 20 or less                         |
| `!20`            | 20 or less               | above 20                           |
| `[10-20]`        | inside 10 to 20          | outside                            |
| `![10-20]`       | outside 10 to 20         | inside                             |

A side without a threshold stays without one. With the ITL's defaults, a `disk-windows` service runs as:

```
check_drivesize "filter=type in ('fixed', 'remote')" w=@~:20% c=@~:10%
```

which is *warning at 20% free or less, critical at 10% or less, on every fixed and remote volume* - what
`check_disk.exe -w 20% -c 10% -u mb` did. The [IcingaAgent reference](../reference/client/IcingaAgent.md) lists what
each command becomes.

### Commands that need a new definition

- **Icinga for Windows plugins.** An `Invoke-IcingaCheck*` command names a PowerShell plugin and passes that
  plugin's arguments (`-Warning 80`); neither means anything to NSClient++. Move these services to NSClient++
  commands as in [step 6](#step-6-move-to-native-commands), before or together with the swap.
- **Your own plugins and scripts.** A plugin or PowerShell script the Icinga agent runs today keeps working as an
  external script. Register it under the name of its Icinga command and the service needs no change either, as long
  as it takes no arguments from the master:

    ```ini
    [/modules]
    CheckExternalScripts = enabled

    [/settings/external scripts/scripts]
    check_backup = scripts\check_backup.ps1
    ```

    To hand it the service's variables, give the Icinga command an alias instead - see
    [Your own Icinga commands](#your-own-icinga-commands). [External Scripts](external-scripts.md) covers arguments
    and wrappings.

---

## Step 2 — Install NSClient++ next to the Icinga agent

Install NSClient++ on the host while the Icinga agent keeps running. The *Icinga 2 agent* feature of the installer is
the `IcingaAgent` module; it stays idle until it has a `parent host`.

```batch
msiexec /qn /i NSCP-<version>-x64.msi LAYOUT=modern
```

`LAYOUT=modern` keeps the configuration and the certificates in `C:\ProgramData\NSClient++`, which only `SYSTEM` and
`Administrators` can read, instead of in the install folder, which every logged-in user can. The host's Icinga key
is about to be copied there, so use it for this migration; on a host that already has NSClient++, the same command
moves the existing configuration and certificates. See [file layout](../concepts/file-layout.md).

Enable the modules behind the queries from step 1, in `C:\ProgramData\NSClient++\nsclient.ini`:

```ini
[/modules]
IcingaAgent = enabled
CheckSystem = enabled
CheckDisk = enabled
```

Now run the queries on the host itself, the way the agent will, before anything depends on them (from an elevated
prompt: the configuration is not readable otherwise):

```
nscp client --module CheckDisk --boot --query check_drivesize "filter=type in ('fixed', 'remote')" "w=@~:20%" "c=@~:10%"
nscp client --module CheckSystem --boot --query check_service service=Spooler warning=none "critical=state != 'running'"
```

Compare the states with what the services show in Icinga today. The host is still monitored by the old agent, so
this is the moment to find a service whose result differs.

---

## Step 3 — Cut over, keeping the host's identity

The Icinga agent already holds what NSClient++ would otherwise have to request: a key, a certificate the Icinga CA
signed for this endpoint, and the CA itself. NSClient++ uses the same three files under the same names, so copying
them lets it connect as the endpoint the master already knows - no ticket, no `icinga2 ca sign`, and the master is
verified from the first connection.

`nscp icinga-agent import` does the copying. It reads the Icinga agent's own files - the endpoint name from
`constants.conf`, the parent from `zones.conf`, the three certificate files - checks that they belong together, and
writes them and the matching settings. Look at what it found first, from an elevated prompt:

```
nscp icinga-agent import --dry-run
```

```
Would import the Icinga 2 agent 'win-host-01.example.com' from C:\ProgramData\icinga2\var\lib\icinga2\certs
  key:         C:\ProgramData\NSClient++\security\icinga\win-host-01.example.com.key
  certificate: C:\ProgramData\NSClient++\security\icinga\win-host-01.example.com.crt
  CA:          C:\ProgramData\NSClient++\security\icinga\ca.crt
Parent: icinga-master.example.com at icinga-master.example.com:5665
Nothing was changed (--dry-run).
```

Then schedule a downtime for the host in Icinga and swap the agents:

```powershell
nscp icinga-agent import --stop-icinga
Restart-Service nscp
```

`--stop-icinga` stops the `icinga2` service and disables it, as the last thing and only once everything else has
worked: a refused import leaves the Icinga agent running and the host monitored. Without the option the import
leaves the Icinga agent untouched, so it can be run ahead of time and the service stopped later
(`Stop-Service icinga2; Set-Service icinga2 -StartupType Disabled`). It writes this to `nsclient.ini`:

```ini
[/modules]
IcingaAgent = enabled

[/settings/Icinga/agent]
endpoint name = win-host-01.example.com    ; as the Icinga agent spelled it, capitals included
parent endpoint = icinga-master.example.com
parent host = icinga-master.example.com
parent port = 5665
trust on first use = false                 ; the CA is in place, so never connect unverified
```

The key is stored readable by `SYSTEM` and `Administrators` only, on either file layout.

| The import says                                        | What to do                                                                                   |
|--------------------------------------------------------|----------------------------------------------------------------------------------------------|
| `Cannot tell this agent's name`                        | The Icinga agent is not in `C:\ProgramData\icinga2`: point at it with `--from`, or give `--endpoint-name`. |
| `The Icinga agent has 2 parents`                       | NSClient++ connects to one: choose it with `--parent-endpoint`. See [does the host fit](#before-you-start-does-the-host-fit). |
| `has no host in zones.conf`                            | The master connected to the Icinga agent. Give `--parent-host`, and open outbound `5665`.    |
| `The certificate is self-signed`                       | The Icinga agent was never signed. There is nothing to take over: use a ticket (see the tip below). |
| `There is already a different key`                     | NSClient++ already has an identity under that name. `--force` replaces it.                   |
| `The import is complete, but the Icinga 2 agent was not stopped` | The service could not be stopped or disabled (the reason follows). Stop it yourself, then restart NSClient++. |

The NSClient++ log shows a verified connection straight away, without the bootstrap lines of a new agent:

```
Icinga agent: Connected to Icinga endpoint 'icinga-master.example.com' at icinga-master.example.com:5665
```

Nothing has to change on the master: the services keep their commands, and the master's next check goes to
NSClient++. One exception: if the host's `Endpoint` on the master has a `host` (and `port`) attribute, remove it. The
master would otherwise keep trying to connect to an agent that no longer listens, and log each failure.

<!-- @formatter:off -->
!!! tip "Starting with a fresh certificate instead"
    If you would rather not copy a private key, skip the copy: set `ticket` (from `icinga2 pki ticket --cn <endpoint
    name>` on the master), copy only `ca.crt`, and NSClient++ generates its own key and has it signed, as in
    [step 4 of the agent scenario](icinga-agent.md#step-4-configure-and-start-the-agent). The endpoint name stays
    the same, so nothing changes on the master.
<!-- @formatter:on -->

---

## Step 4 — Verify

In Icinga Web 2, or through the API, each service now shows the NSClient++ output, with `check_source` still the
host's endpoint name. The executed command is the query that ran, which is how to see what a service was turned
into:

```
curl -k -s -u <api user> 'https://localhost:5665/v1/objects/services?filter=host.name==%22win-host-01.example.com%22&attrs=last_check_result'
```

```
disk      OK   OK All 2 drive(s) are ok      check_drivesize filter=type in ('fixed', 'remote') w=@~:20% c=@~:10%
load      OK   OK: CPU load is ok.           check_cpu w=~:80 c=~:95
memory    OK   OK: physical = 6.2GB          check_memory type=physical w=@~:10% c=@~:5%
```

Check three things before you end the downtime:

- **Every service has a fresh result.** A service on a command NSClient++ does not know, or on a query whose module
  is not enabled, reports `UNKNOWN` and names it. So does a use the agent will not translate:
  `Cannot translate the Icinga command 'service-windows': ...`.
- **The states match what the old agent reported**, in particular for services that were not `OK` before.
- **The host is UP**: its `cluster-zone` check reports the zone as connected.

### What differs from the Icinga plugins

The thresholds are the same comparisons, boundaries included. These are the places where the query behind a command
measures or reports something else than the plugin did:

| Command           | Difference                                                                                                          |
|-------------------|---------------------------------------------------------------------------------------------------------------------|
| All               | The output text and the performance data labels are NSClient++'s. A graphing backend starts new series; the old ones remain. |
| `load-windows`    | `check_load.exe` sampled the CPU for one second. `check_cpu` reads the collector's averages over 5 seconds, 1 minute and 5 minutes and alerts when any of them is over: steadier, and slower to clear. |
| `network-windows` | The plugin added up all interfaces and compared the sum. `check_network` compares each interface on its own.         |
| `disk-windows`    | Without `disk_win_path` the plugin walked fixed and remote volumes; so does the query, through a filter on the drive type. Volumes without a drive letter are listed by their mount point. |
| `procs-windows`   | With `procs_win_user` the query resolves each process's owner, which needs the agent to run as an account that may (LocalSystem does). |
| `update-windows`  | `update_win_reboot` ("may need a reboot" counts as "needs one") has no counterpart and is ignored.                  |
| `service-windows` | `service_win_description` (match by display name) is refused: name the service.                                     |
| `uptime-windows`  | `uptime_win_unit = "ms"` is refused; `s`, `m` and `h` work.                                                         |
| `perfmon-windows` | `perfmon_win_wait` and `perfmon_win_syntax` are ignored: the collector samples the counter, and the label is the counter's name. |
| `nscp-local-*`    | The arguments reach the current NSClient++, not the old one the Icinga installer bundled. An expression written for a very old release may need its syntax brought up to date. |

---

## Step 5 — Restrict the master and remove the old agent

Once the host has run on NSClient++ for a while:

1. **Grant the master only what it monitors.** Every check runs as `IcingaAgent:<parent endpoint>`. The Icinga agent
   ran any command the master named; NSClient++ can be told which queries that is. The policy sees the query, not
   the Icinga command, so grant `check_drivesize` for `disk-windows`:

    ```ini
    [/settings/permissions]
    enabled = true

    [/settings/permissions/policies]
    IcingaAgent:icinga-master.example.com = CheckDisk.check_drivesize, CheckSystem.check_cpu, CheckSystem.check_memory, CheckSystem.check_service
    ```

    See [Permissions](../concepts/permissions.md) for the observe-then-enforce rollout.

2. **Uninstall the Icinga agent** from *Apps & features* (Icinga for Windows has its own
   `Uninstall-IcingaForWindows`), and close the inbound firewall rule for port `5665` if the host had one.

3. **Delete the old copy of the key.** Uninstalling leaves `C:\ProgramData\icinga2` behind, including
   `var\lib\icinga2\certs\<name>.key` - the same key NSClient++ now uses. Remove the folder.

Certificate renewal needs nothing from you: the master renews the certificate as it did for the Icinga agent, see
[Certificate renewal](icinga-agent.md#certificate-renewal).

---

## Step 6 — Move to native commands

The translated commands do what the Icinga plugins did, which is a small part of what the queries behind them can
do. Services on Icinga for Windows plugins have to move; for the others it is worth doing where you want more than
a number per side: several conditions in one threshold, filters, your own output.

A native command imports `ifw-api` (Icinga 2.14 or later on the master) and names the query and its arguments
itself:

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

// Any query, with its arguments written the NSClient++ way.
object CheckCommand "nscp-query" {
  import "ifw-api"
  vars.ifw_api_command = "$nscp_query$"
  vars.ifw_api_arguments = {
    "(arguments)" = { value = "$nscp_arguments$", skip_key = true }
  }
}
```

Both generations work on a migrated host at the same time, so services move one by one. A service that is to work
on hosts still running the Icinga agent as well needs a rule per agent, switched by a host variable, under the
**same service name** so it keeps its history:

```icinga2
apply Service "cpu" {
  check_command = "Invoke-IcingaCheckCPU"
  command_endpoint = host.name
  // ... the plugin's own variables
  assign where host.vars.os == "Windows" && host.vars.agent != "nscp"
}

apply Service "cpu" {
  check_command = "nscp-query"
  command_endpoint = host.name
  vars.nscp_query = "check_cpu"
  vars.nscp_arguments = [ "time=5m", "warning=load > 80", "critical=load > 95" ]
  assign where host.vars.os == "Windows" && host.vars.agent == "nscp"
}
```

Set `vars.agent = "nscp"` on the host in the same change that swaps its agent.

| Icinga for Windows plugin                | NSClient++ query                    | Module         |
|------------------------------------------|-------------------------------------|----------------|
| `Invoke-IcingaCheckCPU`                  | `check_cpu`                         | CheckSystem    |
| `Invoke-IcingaCheckMemory`               | `check_memory`                      | CheckSystem    |
| `Invoke-IcingaCheckUsedPartitionSpace`   | `check_drivesize`                   | CheckDisk      |
| `Invoke-IcingaCheckDiskHealth`           | `check_disk_health`                 | CheckDisk      |
| `Invoke-IcingaCheckService`              | `check_service`                     | CheckSystem    |
| `Invoke-IcingaCheckProcess`              | `check_process`                     | CheckSystem    |
| `Invoke-IcingaCheckUptime`               | `check_uptime`                      | CheckSystem    |
| `Invoke-IcingaCheckNetworkInterface`     | `check_network`                     | CheckSystem    |
| `Invoke-IcingaCheckPerfCounter`          | `check_pdh`                         | CheckSystem    |
| `Invoke-IcingaCheckUpdates`              | `check_os_updates`                  | CheckSystem    |
| `Invoke-IcingaCheckTimeSync`             | `check_w32time`, `check_ntp_offset` | CheckSystem    |
| `Invoke-IcingaCheckEventlog`             | `check_eventlog`                    | CheckEventLog  |
| `Invoke-IcingaCheckScheduledTask`        | `check_tasksched`                   | CheckTaskSched |
| `Invoke-IcingaCheckDirectory`            | `check_files`                       | CheckDisk      |
| `Invoke-IcingaCheckCertificate`          | `check_certificate`                 | CheckSecurity  |
| `Invoke-IcingaCheckFirewall`             | `check_firewall`                    | CheckSecurity  |
| `Invoke-IcingaCheckNLA`                  | `check_nla`                         | CheckSecurity  |
| `Invoke-IcingaCheckUsers`                | `check_users`                       | CheckSecurity  |

The [agent scenario](icinga-agent.md#step-2-define-the-check-commands) has more command definitions, and
[Checks In Depth](../concepts/checks.md) describes the threshold expressions.

### Your own Icinga commands

An Icinga command of your own - a plugin the Icinga agent ran, with arguments - can be answered on the agent
instead of being redefined on the master. Give it an alias: the query to run, with the variables the master sends:

```ini
[/settings/Icinga/agent/commands]
check-app-queue = check_pdh "counter=\\MSMQ Queue($queue_name$)\\Messages in Queue" w=$queue_warn$ c=$queue_crit$
```

An alias with the name of an ITL command replaces the built-in translation of it, and `translate commands = false`
turns the built-in ones off altogether.

---

## Rolling back

Until the old agent is uninstalled, a host goes back in a minute:

```powershell
Stop-Service nscp
Set-Service icinga2 -StartupType Automatic   # --stop-icinga disabled it
Start-Service icinga2
```

Both agents use the same certificate and answer the same commands, so the master needs no change either way; just
never run both against it at once. Clear `parent host` in `nsclient.ini` if NSClient++ is to keep running on the
host for anything else.

---

## Rolling out to more hosts

For each further host it is step 2 (install, with the check modules enabled by your deployment tool) and the swap
in step 3, which is the same two lines everywhere: the import reads each host's own name and parent. It exits
non-zero when it refuses, so a deployment script can stop before it touches the Icinga agent. To hand out the same
configuration to many hosts, see [Automated installation](../setup/installing.md#automated-installation-windows-msi).

---

## Troubleshooting

| Symptom                                                                    | Cause and fix                                                                                              |
|----------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
| `UNKNOWN: Cannot translate the Icinga command '...'`                       | The service uses the command in a way that has no counterpart; the message says which variable. Change the variable, or give the command an [alias](#your-own-icinga-commands). |
| Services `UNKNOWN`, naming `Invoke-IcingaCheckCPU` or a command of your own | NSClient++ has no query of that name. Move the service to a native command (step 6) or add an alias.       |
| Services `UNKNOWN: Permission denied: IcingaAgent:...` naming a query      | The permission policy is on and does not grant the query the command became. Grant the query (`CheckDisk.check_drivesize`), not the Icinga name. |
| Services `UNKNOWN: arguments are not allowed on this agent`                | `allow arguments = false`: translated thresholds are arguments from the master. Set it to `true`, or use aliases whose arguments are all written on the agent. |
| A service is `OK` where it used to alert, or the other way round           | Compare the executed command in Icinga with the old plugin's arguments, and see [what differs](#what-differs-from-the-icinga-plugins). |
| `connecting with a self-signed certificate ... to request a signed one`    | NSClient++ found no certificate for its `endpoint name` and made its own: the name was changed after the import, or the files were copied by hand under another name. Run `nscp icinga-agent import` again. |
| `Parent presented the certificate of 'x', expected 'y'`                    | `parent endpoint` is not the parent's endpoint name. Take it from the Icinga agent's `zones.conf`.         |
| `` `trust on first use` is off: not connecting ``                          | `ca.crt` is not in the certificate directory. Copy it; it is picked up within seconds.                     |
| Host DOWN, master logs connection attempts to the host                     | The `Endpoint` on the master still has `host` set and the agent is not connecting out: check outbound `5665` and the NSClient++ log. |

The [agent scenario's troubleshooting table](icinga-agent.md#troubleshooting) covers the connection and certificate
messages in full.

---

## Next steps

- [Native Icinga 2 Agent](icinga-agent.md) - every part of the setup in depth, including how the master is trusted.
- [IcingaAgent reference](../reference/client/IcingaAgent.md) - every setting of the module, and what each Icinga
  command becomes.
- [Windows Server Health](windows-server-health.md) - a baseline of checks, if the migration is also a clean-up.
- [Permissions](../concepts/permissions.md) - grant the master only what it monitors.
