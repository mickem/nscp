# IcingaAgent

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

| Key                                                           | Default Value              | Description                  |
|---------------------------------------------------------------|----------------------------|------------------------------|
| [allow arguments](#allow-arguments)                           | true                       | ALLOW ARGUMENTS              |
| [allowed ciphers](#allowed-ciphers)                           |                            | ALLOWED CIPHERS              |
| [certificate directory](#certificate-directory)               | ${certificate-path}/icinga | CERTIFICATE DIRECTORY        |
| [certificate request interval](#certificate-request-interval) | 86400                      | CERTIFICATE REQUEST INTERVAL |
| [endpoint name](#endpoint-name)                               | mime-desktop               | ENDPOINT NAME                |
| [parent endpoint](#parent-endpoint)                           |                            | PARENT ENDPOINT              |
| [parent fingerprint](#parent-fingerprint)                     |                            | PARENT FINGERPRINT           |
| [parent host](#parent-host)                                   |                            | PARENT HOST                  |
| [parent port](#parent-port)                                   | 5665                       | PARENT PORT                  |
| [threads](#worker-threads)                                    | 4                          | WORKER THREADS               |
| [ticket](#ticket)                                             |                            | TICKET                       |
| [tls version](#tls-version)                                   | 1.2+                       | TLS VERSION                  |
| [trust on first use](#trust-on-first-use)                     | true                       | TRUST ON FIRST USE           |


```ini
# Native Icinga 2 agent: connect to an Icinga 2 parent over the cluster protocol and run the checks it schedules.
[/settings/Icinga/agent]
allow arguments=true
certificate directory=${certificate-path}/icinga
certificate request interval=86400
endpoint name=mime-desktop
parent port=5665
threads=4
tls version=1.2+
trust on first use=true
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

#### CERTIFICATE REQUEST INTERVAL <a id="/settings/Icinga/agent/certificate request interval"></a>

Seconds between requests for a renewed certificate on an open connection; one is also sent on every connect. The parent only issues a new certificate within 30 days of the current one expiring (or when its CA is newer than the agent's), so the default, a day like the Icinga agent's, renews in time on a connection that stays up for months. 0 asks only on connect.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | certificate request interval                      |
| Default value: | `86400`                                           |


**Sample:**

```
[/settings/Icinga/agent]
# CERTIFICATE REQUEST INTERVAL
certificate request interval=86400
```

#### ENDPOINT NAME <a id="/settings/Icinga/agent/endpoint name"></a>

This agent's endpoint name, the CN of its certificate. Must match the Endpoint object on the master. Defaults to the lower-cased host name.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | endpoint name                                     |
| Default value: | `mime-desktop`                                    |


**Sample:**

```
[/settings/Icinga/agent]
# ENDPOINT NAME
endpoint name=mime-desktop
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

#### PARENT FINGERPRINT <a id="/settings/Icinga/agent/parent fingerprint"></a>

SHA-256 fingerprint of the parent's certificate, as \`openssl x509 -noout -fingerprint -sha256 -in <cert>\` prints it (colons and the \`SHA256 Fingerprint=\` prefix are optional). Pins the parent on the first connection, before the agent has the Icinga CA - the equivalent of \`icinga2 node setup --trustedcert\`. Once the CA is stored the parent is verified against it instead, so a renewed parent certificate keeps working. Empty (the default): no pin.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | parent fingerprint                                |
| Default value: | _N/A_                                             |


**Sample:**

```
[/settings/Icinga/agent]
# PARENT FINGERPRINT
parent fingerprint=
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

A ticket from \`icinga2 pki ticket --cn <endpoint name>\` on the master. With it the master signs the agent's certificate right away; without it the request waits for \`icinga2 ca sign\` on the master. Treat it as a password: it does not expire, and whoever holds it can have the master sign a certificate for this endpoint name. The agent sends it to the parent on every connect, so on a first connection that is not verified (no \`ca.crt\`, no \`parent fingerprint\`) it goes to whoever answers at \`parent host\`.


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

#### TRUST ON FIRST USE <a id="/settings/Icinga/agent/trust on first use"></a>

Whether the agent may make its first connection without being able to verify the parent: no \`ca.crt\` in the certificate directory yet and no \`parent fingerprint\`. Only the certificate exchange happens on such a connection, never a check - but the exchange includes the \`ticket\`, and the CA the agent trusts from then on is the one that peer sends. True (the default) is how \`icinga2 node setup\` behaves without \`--trustedcert\`; set it to false to require the CA or the pin up front.


| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/Icinga/agent](#/settings/Icinga/agent) |
| Key:           | trust on first use                                |
| Default value: | `true`                                            |


**Sample:**

```
[/settings/Icinga/agent]
# TRUST ON FIRST USE
trust on first use=true
```
