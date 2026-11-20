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

#### How the parent is trusted

The very first connection cannot verify the master: the agent has no CA yet (the same trust on first use as
`icinga2 node setup` without `--trustedcert`). Nothing but the certificate exchange happens on that connection - a check
sent over it is answered `UNKNOWN`. From then on every connection verifies the master against the stored CA.

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

A `CheckCommand` that does not import `ifw-api` arrives with its name only, and runs the query of that name without
arguments.

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
