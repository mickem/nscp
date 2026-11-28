`IcingaAgent` makes NSClient++ a native Icinga 2 agent. It connects to an Icinga 2 master or satellite over the Icinga
cluster protocol (JSON-RPC over mutual TLS, port `5665`), the way the PowerShell-only Icinga for Windows does, and runs
the checks the parent schedules on it with `command_endpoint`. There is no `icinga2` binary on the host and no Icinga
configuration to sync: the check definitions live on the master, the checks themselves are NSClient++ queries.

The agent connects out to its parent, so the host needs no inbound firewall opening. It always runs queries only: Icinga
event commands and the master's ad-hoc `execute-command` action are refused.

For a full walkthrough, master side included, see the [Native Icinga 2 Agent](../../scenarios/icinga-agent.md)
scenario.

#### Setting it up

1. On the master, add an `Endpoint` and a `Zone` for the agent (no `host` attribute: the agent connects in) and issue a
   ticket for it:

    ```
    icinga2 pki ticket --cn win-host-01
    ```

2. On the agent, enable the module and point it at the master:

    ```ini
    [/modules]
    IcingaAgent = enabled
    CheckSystem = enabled
    CheckDisk = enabled

    [/settings/Icinga/agent]
    parent host = icinga-master.example.com
    parent endpoint = icinga-master
    endpoint name = win-host-01
    ticket = <the ticket from step 1>
    ```

3. Restart NSClient++. On first start the agent generates a key and a self-signed certificate under `certificate
   directory`, connects, and asks the master to sign it. With a valid ticket the master answers immediately; the agent
   stores the signed certificate and the Icinga CA (`<endpoint name>.crt`, `<endpoint name>.key` and `ca.crt`) and
   reconnects with them. Without a ticket the request waits on the master until `icinga2 ca sign <fingerprint>`.

`parent endpoint` is required, and must be the CN of the master's certificate: a parent presenting any other name is
refused, even one the same CA signed. `endpoint name` must match the agent's `Endpoint` object on the master.

#### Taking over from the Icinga 2 agent

On a host that runs the Icinga 2 agent, `nscp icinga-agent import` does the setup above from that agent's own files:

```
nscp icinga-agent import --dry-run
nscp icinga-agent import
```

It reads the endpoint name from the agent's `constants.conf` and the parent from its `zones.conf`, copies
`<name>.crt`, `<name>.key` and `ca.crt` into the certificate directory, and writes `endpoint name`, `parent endpoint`,
`parent host`, `parent port` and `trust on first use = false`. NSClient++ then connects as the endpoint the master
already knows: no ticket, nothing to sign, and the first connection is verified. Stop the Icinga 2 agent before
NSClient++ is restarted, since both cannot be connected under one name; `--stop-icinga` does it as part of the
import.

| Option                                    | Default                                                                  |
|-------------------------------------------|--------------------------------------------------------------------------|
| `--from <directory>`                      | The Icinga 2 data directory: `%ProgramData%\icinga2` on Windows, `/` elsewhere |
| `--certificates`, `--config`              | `<from>/var/lib/icinga2/certs` and `<from>/etc/icinga2`                  |
| `--endpoint-name`                         | `NodeName` in `constants.conf`, else the name of the only key            |
| `--parent-endpoint`, `--parent-host`, `--parent-port` | The one other `Endpoint` in `zones.conf`                     |
| `--force`                                 | Off: a different key already stored under that name is not replaced      |
| `--stop-icinga`                           | Off: when set, the `icinga2` service is stopped and disabled once the import has succeeded |
| `--dry-run`                               | Off                                                                      |

Nothing is copied unless it would work: the certificate has to be for that endpoint name and that key, and issued by
the CA beside it. An agent with two parents, or one its parent connected to, is refused with the option that settles
it. The key is written readable only by the account NSClient++ runs as. The
[migration scenario](../../scenarios/migrating-from-icinga-agent.md) walks through the whole move.

#### How the parent is trusted

Icinga has no public CA: every installation has its own, created on the master. Once the agent holds it (`ca.crt` in
the certificate directory, stored from the master's answer to the certificate request), every connection verifies the
master against it. For the first connection there are three choices:

| How                                                     | First connection                                           |
|---------------------------------------------------------|------------------------------------------------------------|
| Install the master's `ca.crt` up front                  | Verified against the CA, checks run straight away          |
| Set `parent fingerprint` (the master's certificate)     | Verified against the pin, as `node setup --trustedcert`    |
| Neither (the default, `trust on first use = true`)      | Not verified: no checks, but the ticket is sent (see below) |

The CA to install is the master's `/var/lib/icinga2/certs/ca.crt` - the CA certificate, never its key - copied to
`<certificate directory>/ca.crt` by whatever deploys the agent. The fingerprint is what
`openssl x509 -noout -fingerprint -sha256 -in /var/lib/icinga2/certs/<master>.crt` prints on the master. Set
`trust on first use = false` to refuse a first connection that cannot be verified either way.

#### What an unverified first connection gives away

With neither the CA nor a pin, the agent cannot tell the master from anyone else who answers at `parent host` - a
machine on the path, a poisoned DNS record, a wrong address in the configuration. It runs no checks for that peer,
but the certificate exchange itself hands over two things:

- **The ticket.** It is sent in the first message. A ticket is derived from the endpoint name and the master's
  `TicketSalt`: it does not expire and is not used up. Whoever holds it can have the real master sign a certificate
  for this endpoint name, for a key of their own, at any time - and then connect to the master as this agent: report
  check results for the host, and receive the checks meant for it, with whatever the master puts in their arguments.
  The only way to withdraw a ticket is to change `TicketSalt` on the master, which invalidates every ticket it issued.
- **The agent's trust.** The agent stores the CA that peer sends and verifies every later connection against it. A
  peer that answers with its own CA is the agent's parent from then on, and may run every query the permission
  policy grants `IcingaAgent:<parent endpoint>` - every query at all while the policy is off.

Both are avoided by verifying the first connection: install `ca.crt` or set `parent fingerprint`, and set
`trust on first use = false` so a host that was deployed without either refuses to connect rather than falling back.
If the ticket may have gone to the wrong peer, treat it as leaked and change `TicketSalt` on the master; that stops
new certificates being signed with it, but does not revoke one that already was. The agent logs an error each time it
sends the ticket over an unverified connection.

The ticket is a password setting: the settings API and the web UI mask its value.

#### Certificate renewal

The master renews the agent's certificate, as for an Icinga agent. Icinga issues agent certificates for 397 days and
signs a new one - same key, no ticket needed - when the agent asks within 30 days of expiry, or when its CA is newer
than the agent's copy. The agent asks on every connection and, on a connection that stays up, every
`certificate request interval` seconds (default a day, as the Icinga agent does). It checks that the new certificate is
for its own key, stores it and the CA, and reconnects with them; there is nothing to do on either side.

#### Check commands on the master

Checks follow the `ifw-api` convention Icinga 2.14 introduced for Icinga for Windows: import `ifw-api` into the
`CheckCommand` and the master sends the query name (`ifw_api_command`, the command's own name by default) and its
argument definitions (`ifw_api_arguments`) together with the resolved values. The agent expands them the way Icinga
does (`value`, `set_if`, `order`, `skip_key`, `repeat_key`) and passes them to the query as `key=value` tokens, with
any leading dashes removed:

```icinga2
object CheckCommand "check_cpu" {
  import "ifw-api"
  vars.ifw_api_arguments = {
    "warning"  = { value = "$nscp_cpu_warning$" }
    "critical" = { value = "$nscp_cpu_critical$" }
  }
}
```

One generic command runs any query with its arguments written in NSClient++'s own syntax. Icinga reads `$` as a macro,
so `${keyword}` is written `$${keyword}` there:

```icinga2
object CheckCommand "nscp-query" {
  import "ifw-api"
  vars.ifw_api_command = "$nscp_query$"
  vars.ifw_api_arguments = {
    "(arguments)" = { value = "$nscp_arguments$", skip_key = true }
  }
}
```

A `CheckCommand` that does not import `ifw-api` arrives with its name and the macros its own arguments use. The agent
looks the name up among the Icinga commands it knows (next section); one it does not know runs the query of that name
without arguments.

#### Icinga commands under their own names

A host that ran the Icinga 2 agent has services on the Windows commands of the Icinga Template Library. The agent
answers those under their Icinga names, from the variables the service already sets, so the services keep working
unchanged when NSClient++ takes the Icinga agent's place:

| Icinga command                             | Runs                                      | Thresholds from                                                   |
|--------------------------------------------|-------------------------------------------|-------------------------------------------------------------------|
| `disk-windows`                             | `check_drivesize`                         | `disk_win_warn` / `_crit`, in `disk_win_unit`; free, or used with `disk_win_show_used` |
| `memory-windows`                           | `check_memory type=physical`              | `memory_win_warn` / `_crit`, likewise                             |
| `swap-windows`                             | `check_pagefile`, the total               | `swap_win_warn` / `_crit`, likewise                               |
| `load-windows`                             | `check_cpu`                               | `load_win_warn` / `_crit`                                         |
| `procs-windows`                            | `check_process process=*`                 | `procs_win_warn` / `_crit` on the count; `procs_win_user`         |
| `users-windows`                            | `check_users`                             | `users_win_warn` / `_crit`                                        |
| `uptime-windows`                           | `check_uptime`                            | `uptime_win_warn` / `_crit`, in `uptime_win_unit` (`s`, `m`, `h`) |
| `network-windows`                          | `check_network`                           | `network_win_warn` / `_crit`, bytes per second                    |
| `perfmon-windows`                          | `check_pdh counter=...`                   | `perfmon_win_warn` / `_crit`                                      |
| `ping-windows`, `ping4-`, `ping6-windows`  | `check_ping host=...`                     | `ping_win_wrta` / `_wpl`, `ping_win_crta` / `_cpl`                |
| `service-windows`                          | `check_service service=...`               | critical unless running; warning with `service_win_warn`          |
| `update-windows`                           | `check_os_updates`                        | `update_win_warn` / `_crit`; a pending reboot unless `ignore_reboot` |
| `nscp-local`, `nscp-local-*`               | the query in `nscp_query`                 | the command's own `nscp_*` arguments, as they are                 |

The plugins' thresholds are converted into the `-w` / `-c` of the query, with every comparison the way the plugin made
it:

| Icinga threshold | Alerts when the value is | For free space (disk, memory, swap) | Passed as                |
|------------------|--------------------------|-------------------------------------|--------------------------|
| `20`             | above 20                 | 20 or less                          | `~:20` / `@~:20`         |
| `!20`            | 20 or less               | above 20                            | `@~:20` / `~:20`         |
| `[10-20]`        | inside 10 to 20          | outside                             | `@10:20` / `10:20`       |
| `![10-20]`       | outside 10 to 20         | inside                              | `10:20` / `@10:20`       |

A percentage stays one (`20%`); any other number takes the command's unit (`500` with `disk_win_unit = "mb"` is
`500m`). A side the service gives no threshold for is passed as `none`: the plugin did not alert on it, so the
query's own default does not apply either. A use the agent cannot translate - a threshold the plugin would have
refused, a range whose ends are reversed (`[20-10]`, which the plugin never alerts on and the query's `-w` / `-c`
refuse), `service_win_description` - is reported as `UNKNOWN` on the service rather than run as something else.

The result is the query's, not the plugin's: the output text and the performance data labels are NSClient++'s. The
master shows the query that ran as the executed command.

Set `translate commands = false` to turn the built-in translations off. Your own go under `commands`, as
`<Icinga command> = <query> <argument>...`, where an argument may use the macros the master sends:

```ini
[/settings/Icinga/agent/commands]
; an in-house Icinga command, answered by a query
app-queue = check_pdh "counter=\MSMQ Queue($queue$)\Messages in Queue" w=$queue_warn$ c=$queue_crit$
; replace the built-in translation of an ITL command
load-windows = check_cpu time=5m w=$load_win_warn$ c=$load_win_crit$
```

An argument whose macro has no value is left out, and an argument that is one list macro becomes one argument per
element. An alias takes precedence over the built-in translation of the same name, and so does a query another module
already provides under it (an external script or a `CheckHelpers` alias called `disk-windows`): the built-in
translations only answer names the agent would otherwise not know. Values are passed as they are
here: the threshold conversion above applies to the built-in translations only.

Translated or aliased, the query is what the permission policy sees (`CheckDisk.check_drivesize`, not
`disk-windows`), and the thresholds count as arguments from the parent: with `allow arguments = false` only an alias
whose arguments are all its own still runs.

#### Which checks the parent may run

The core permission policy decides, as for every other transport: each check runs as the principal
`IcingaAgent:<parent endpoint>`. The module has no allow-list of its own; `allow arguments = false` restricts the parent
to argument-less queries and aliases. Enable the policy and grant the parent what it monitors:

```ini
[/settings/permissions]
enabled = true

[/settings/permissions/policies]
IcingaAgent:icinga-master = CheckSystem.*, CheckDisk.check_drivesize
```

See [Permissions](../../concepts/permissions.md#icingaagent-the-icinga-parent-as-principal).

#### TLS

`tls version` (default `1.2+`) and `allowed ciphers` take the same values as on the other servers and clients. An
invalid value is reported once when the module loads, and the agent does not connect.

#### Not supported yet

Listening for the master (the agent only connects out), several parents for high availability, check deadlines, and
local scheduling while the parent is unreachable: the agent runs what the parent sends while connected and nothing
else, as the new Icinga for Windows does.
