# IcingaAgent

*Available on Linux only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

A native Icinga 2 agent: connects to an Icinga 2 master or satellite over the cluster protocol (JSON-RPC over TLS, port 5665) and runs the checks it schedules, without an icinga2 binary.

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


## Enable module

To enable this module and allow using the commands you need to add `IcingaAgent = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
IcingaAgent = enabled
```


## Configuration

| Path / Section                          | Description  |
|-----------------------------------------|--------------|
| [/settings/Icinga/agent](#icinga-agent) | Icinga agent |


### Icinga agent <a id="/settings/Icinga/agent"></a>

Native Icinga 2 agent: connect to an Icinga 2 parent over the cluster protocol and run the checks it schedules.

| Key                                             | Default Value              | Description           |
|-------------------------------------------------|----------------------------|-----------------------|
| [allow arguments](#allow-arguments)             | true                       | ALLOW ARGUMENTS       |
| [allowed ciphers](#allowed-ciphers)             |                            | ALLOWED CIPHERS       |
| [certificate directory](#certificate-directory) | ${certificate-path}/icinga | CERTIFICATE DIRECTORY |
| [endpoint name](#endpoint-name)                 | vm                         | ENDPOINT NAME         |
| [parent endpoint](#parent-endpoint)             |                            | PARENT ENDPOINT       |
| [parent host](#parent-host)                     |                            | PARENT HOST           |
| [parent port](#parent-port)                     | 5665                       | PARENT PORT           |
| [threads](#worker-threads)                      | 4                          | WORKER THREADS        |
| [ticket](#ticket)                               |                            | TICKET                |
| [tls version](#tls-version)                     | 1.2+                       | TLS VERSION           |


```ini
# Native Icinga 2 agent: connect to an Icinga 2 parent over the cluster protocol and run the checks it schedules.
[/settings/Icinga/agent]
allow arguments=true
certificate directory=${certificate-path}/icinga
endpoint name=vm
parent port=5665
threads=4
tls version=1.2+
```

#### ALLOW ARGUMENTS <a id="/settings/Icinga/agent/allow arguments"></a>

Whether the parent may pass arguments to the checks it runs. True (the default) because Icinga CheckCommands carry their thresholds as arguments; false runs only argument-less queries and aliases defined on the agent. Which queries the parent may run at all is the permission policy's job (/settings/permissions).


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | allow arguments                                   |
| Default value: | `true`                                            |


**Sample:**

```
[/settings/Icinga/agent]
# ALLOW ARGUMENTS
allow arguments=true
```

#### ALLOWED CIPHERS <a id="/settings/Icinga/agent/allowed ciphers"></a>

OpenSSL cipher list for TLS 1.2 and earlier. Empty (the default) leaves the library's own selection in place. TLS 1.3 suites are not affected by it.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | allowed ciphers                                   |
| Default value: | _N/A_                                             |


**Sample:**

```
[/settings/Icinga/agent]
# ALLOWED CIPHERS
allowed ciphers=
```

#### CERTIFICATE DIRECTORY <a id="/settings/Icinga/agent/certificate directory"></a>

Where the agent keeps <endpoint name>.crt, <endpoint name>.key and ca.crt. Generated on first start.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | certificate directory                             |
| Default value: | `${certificate-path}/icinga`                      |


**Sample:**

```
[/settings/Icinga/agent]
# CERTIFICATE DIRECTORY
certificate directory=${certificate-path}/icinga
```

#### ENDPOINT NAME <a id="/settings/Icinga/agent/endpoint name"></a>

This agent's endpoint name, the CN of its certificate. Must match the Endpoint object on the master. Defaults to the lower-cased host name.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | endpoint name                                     |
| Default value: | `vm`                                              |


**Sample:**

```
[/settings/Icinga/agent]
# ENDPOINT NAME
endpoint name=vm
```

#### PARENT ENDPOINT <a id="/settings/Icinga/agent/parent endpoint"></a>

The parent's endpoint name, the CN of its certificate. Required: a parent presenting any other certificate is refused, even one the Icinga CA signed. It is also the principal checks run as, so the permission policy sees \`IcingaAgent:<parent endpoint>\`.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | parent endpoint                                   |
| Default value: | _N/A_                                             |


**Sample:**

```
[/settings/Icinga/agent]
# PARENT ENDPOINT
parent endpoint=
```

#### PARENT HOST <a id="/settings/Icinga/agent/parent host"></a>

Address of the Icinga 2 master or satellite to connect to. Empty (the default) leaves the agent disabled.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | parent host                                       |
| Default value: | _N/A_                                             |


**Sample:**

```
[/settings/Icinga/agent]
# PARENT HOST
parent host=
```

#### PARENT PORT <a id="/settings/Icinga/agent/parent port"></a>

The parent's cluster port.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | parent port                                       |
| Default value: | `5665`                                            |


**Sample:**

```
[/settings/Icinga/agent]
# PARENT PORT
parent port=5665
```

#### WORKER THREADS <a id="/settings/Icinga/agent/threads"></a>

How many checks run at the same time.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | threads                                           |
| Default value: | `4`                                               |


**Sample:**

```
[/settings/Icinga/agent]
# WORKER THREADS
threads=4
```

#### TICKET <a id="/settings/Icinga/agent/ticket"></a>

A ticket from \`icinga2 pki ticket --cn <endpoint name>\` on the master. With it the master signs the agent's certificate right away; without it the request waits for \`icinga2 ca sign\` on the master.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | ticket                                            |
| Default value: | _N/A_                                             |


**Sample:**

```
[/settings/Icinga/agent]
# TICKET
ticket=
```

#### TLS VERSION <a id="/settings/Icinga/agent/tls version"></a>

Which TLS versions the agent negotiates with its parent, in the same vocabulary as the other servers and clients: an exact version (1.0, 1.1, 1.2, 1.3), a trailing + for that version or later, or \`any\`. The default, 1.2+, is what Icinga 2 accepts out of the box.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | tls version                                       |
| Default value: | `1.2+`                                            |


**Sample:**

```
[/settings/Icinga/agent]
# TLS VERSION
tls version=1.2+
```
