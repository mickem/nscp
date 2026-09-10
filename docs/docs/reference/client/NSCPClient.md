# NSCPClient

NSCP client can be used both from command line and from queries to check remote systems via NSCP (REST)

## Enable module

To enable this module and allow using the commands you need to add `NSCPClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NSCPClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the NSCPClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                                   | Description                                        |
|-------------------------------------------|----------------------------------------------------|
| [check_remote_nscp](#check_remote_nscp)   | Request remote information via NSCP.               |
| [exec_remote_nscp](#exec_remote_nscp)     | Execute remote script via NSCP.                    |
| [remote_nscp_query](#remote_nscp_query)   | Request remote information via NSCP.               |
| [remote_nscpforward](#remote_nscpforward) | Forward the request as-is to remote host via NSCP. |
| [submit_remote_nscp](#submit_remote_nscp) | Submit information to remote host via NSCP.        |

### check_remote_nscp

Request remote information via NSCP.

#### About `check_remote_nscp`

`check_remote_nscp` runs a check on a remote NSClient++ agent over the **NSCP
protocol** and returns its result.

`check_remote_nscp` and [`remote_nscp_query`](#remote_nscp_query) are the same
command under two names; use whichever reads better in your configuration.

##### Why use this instead of NRPE

The NSCP protocol is NSClient++'s own agent-to-agent transport, and it is the
better choice when both ends run NSClient++:

- **No payload ceiling.** NRPE version 2 truncates output at a fixed buffer;
  NSCP carries the full result.
- **Structured results.** Status, message *and* performance data travel as
  protobuf rather than being squeezed into one Nagios line and re-parsed, so
  perf data survives intact.
- **Real authentication.** `password=` plus certificate verification, rather
  than NRPE's traditional anonymous Diffie-Hellman.

Use [NRPE](NRPEClient.md) when the far end is a Nagios `nrpe` daemon or another
non-NSClient++ agent; use this when it is NSClient++.

##### Connecting

Name the host with `host=` (and `port=`, or `address=host:port`), or with
`target=` to pull the connection details from a target defined in the module's
settings — which is where the password and TLS material belong, rather than on
every command line. `command=` names the check to run on the far end and
`argument=` passes arguments to it (repeatable), exactly as if you were running
that check locally.

##### Security

`password=` is the shared secret the remote agent requires. TLS is configured
with `certificate=`, `certificate-key=`, `ca=`, `dh=`, `verify=` and
`allowed-ciphers=`; set `verify=peer` with a real CA so the client actually
authenticates the server rather than merely encrypting to whoever answers.

**Jump to section:**

* [Sample Commands](#check_remote_nscp_samples)
* [Command-line Arguments](#check_remote_nscp_options)


<a id="check_remote_nscp_samples"></a>
#### Sample Commands

**Run a check on a remote NSClient++ agent:**

```
check_remote_nscp host=192.168.56.103 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used|'C:\ used'=91.2GB;80;90;0;100 'C:\ used %'=91%;80;90;0;100
```

Unlike NRPE, the result travels as structured data, so the performance data
arrives intact regardless of length.

**Pass arguments to the remote check (`argument=`, repeatable):**

```
check_remote_nscp host=192.168.56.103 command=check_drivesize "argument=drive=C:" "argument=crit=used > 95%"
OK: OK All 1 drive(s) are ok
```

**Use a configured target instead of spelling out the connection:**

Put the host, password and TLS material under
`[/settings/NSCP/client/targets/...]` so credentials stay out of process
listings:

```ini
[/settings/NSCP/client/targets/web01]
address = nscp://192.168.56.103:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem
```

```
check_remote_nscp target=web01 command=check_uptime
OK: uptime: 12d 04:31h, boot: 2026-08-23 08:29:11 (local)
```

**Nothing listening:**

```
check_remote_nscp host=127.0.0.1 port=15669 command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

**No host given:**

The default port is 8443, so a call with no target at all fails against an empty
address rather than doing something surprising:

```
check_remote_nscp
UNKNOWN: Error: Failed to connect to: :8443 :Address family not supported by protocol
```

**Long output survives:**

This is the main practical difference from NRPE, whose version-2 protocol
truncates at a fixed 1024-byte payload:

```
check_remote_nscp target=web01 command=check_files "argument=path=C:\logs" "argument=top-syntax=${list}"
OK: app-2026-09-01.log, app-2026-09-02.log, app-2026-09-03.log, ... (412 files)
```



<a id="check_remote_nscp_options"></a>
#### Command-line Arguments

<a id="check_remote_nscp_host"></a>
<a id="check_remote_nscp_port"></a>
<a id="check_remote_nscp_address"></a>
<a id="check_remote_nscp_timeout"></a>
<a id="check_remote_nscp_target"></a>
<a id="check_remote_nscp_retry"></a>
<a id="check_remote_nscp_retries"></a>
<a id="check_remote_nscp_source-host"></a>
<a id="check_remote_nscp_sender-host"></a>
<a id="check_remote_nscp_command"></a>
<a id="check_remote_nscp_argument"></a>
<a id="check_remote_nscp_separator"></a>
<a id="check_remote_nscp_batch"></a>
<a id="check_remote_nscp_certificate"></a>
<a id="check_remote_nscp_dh"></a>
<a id="check_remote_nscp_certificate-key"></a>
<a id="check_remote_nscp_certificate-format"></a>
<a id="check_remote_nscp_ca"></a>
<a id="check_remote_nscp_verify"></a>
<a id="check_remote_nscp_allowed-ciphers"></a>
<a id="check_remote_nscp_password"></a>

| Option                        | Default Value | Description                                                                           |
|-------------------------------|---------------|---------------------------------------------------------------------------------------|
| host                          |               | The host of the host running the server                                               |
| port                          |               | The port of the host running the server                                               |
| address                       |               | The address (host:port) of the host running the server                                |
| timeout                       |               | Number of seconds before connection times out (default=10)                            |
| target                        |               | Target to use (lookup connection info from config)                                    |
| retry                         |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                       |               | legacy version of retry                                                               |
| source-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                       |               | The name of the command that the remote daemon should run                             |
| argument                      |               | Set command line arguments                                                            |
| separator                     |               | Separator to use for the batch command (default is |)                                 |
| batch                         |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                   |               | The client certificate to use                                                         |
| dh                            |               | The DH key to use                                                                     |
| certificate-key               |               | Client certificate to use                                                             |
| certificate-format            |               | Client certificate format                                                             |
| ca                            |               | Certificate authority                                                                 |
| verify                        |               | Client certificate format                                                             |
| allowed-ciphers               |               | Client certificate format                                                             |
| [ssl](#check_remote_nscp_ssl) | true          | Initial an ssl handshake with the server.                                             |
| password                      |               | Password                                                                              |



<h5 id="check_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### exec_remote_nscp

Execute remote script via NSCP.

#### About `exec_remote_nscp`

`exec_remote_nscp` sends an **execute** request to a remote NSClient++ agent over
the NSCP protocol, rather than a query.

The distinction matters. A *query*
([`remote_nscp_query`](#remote_nscp_query)) asks the remote agent to run a check
and return a status, message and performance data — the normal monitoring
interaction. An *execute* request invokes the remote agent's command-line
interface and returns its textual output: the equivalent of running `nscp
<something>` on that host, used for administrative operations rather than for
checks.

Reach for it to drive an agent remotely — inspecting its settings, listing its
modules, running a maintenance command — not to collect check results. Using it
for a check gives you raw text with no status to alert on.

The options are the same as for
[`check_remote_nscp`](#check_remote_nscp): `host=` / `port=` / `address=` or
`target=` for the connection, `command=` and `argument=` for what to run, plus
`password=` and the TLS options.

Because an execute request is closer to remote administration than to
monitoring, be deliberate about which agents accept it and from where; the
remote agent's own configuration decides whether it serves execute requests at
all.

**Jump to section:**

* [Sample Commands](#exec_remote_nscp_samples)
* [Command-line Arguments](#exec_remote_nscp_options)


<a id="exec_remote_nscp_samples"></a>
#### Sample Commands

**Send an execute request to a remote agent:**

An execute request invokes the remote agent's command-line interface and returns
its textual output — the equivalent of running `nscp <something>` on that host.

```
exec_remote_nscp target=web01 command=help
Usage: nscp <command> [options]
...
```

**Inspect a remote agent's settings:**

```
exec_remote_nscp target=web01 command=settings "argument=--list" "argument=--path" "argument=/modules"
CheckDisk = enabled
CheckHelpers = enabled
CheckSystem = enabled
NRPEServer = enabled
```

**This is not how you run a check:**

An execute request returns raw text with no status to alert on. For a check, use
[`remote_nscp_query`](#remote_nscp_query):

```
remote_nscp_query target=web01 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

**Nothing listening:**

```
exec_remote_nscp host=127.0.0.1 port=15669 command=help
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

Because an execute request is closer to remote administration than to
monitoring, be deliberate about which agents accept it and from where; the
remote agent's own configuration decides whether it serves them at all.



<a id="exec_remote_nscp_options"></a>
#### Command-line Arguments

<a id="exec_remote_nscp_host"></a>
<a id="exec_remote_nscp_port"></a>
<a id="exec_remote_nscp_address"></a>
<a id="exec_remote_nscp_timeout"></a>
<a id="exec_remote_nscp_target"></a>
<a id="exec_remote_nscp_retry"></a>
<a id="exec_remote_nscp_retries"></a>
<a id="exec_remote_nscp_source-host"></a>
<a id="exec_remote_nscp_sender-host"></a>
<a id="exec_remote_nscp_command"></a>
<a id="exec_remote_nscp_argument"></a>
<a id="exec_remote_nscp_separator"></a>
<a id="exec_remote_nscp_batch"></a>
<a id="exec_remote_nscp_certificate"></a>
<a id="exec_remote_nscp_dh"></a>
<a id="exec_remote_nscp_certificate-key"></a>
<a id="exec_remote_nscp_certificate-format"></a>
<a id="exec_remote_nscp_ca"></a>
<a id="exec_remote_nscp_verify"></a>
<a id="exec_remote_nscp_allowed-ciphers"></a>
<a id="exec_remote_nscp_password"></a>

| Option                       | Default Value | Description                                                                           |
|------------------------------|---------------|---------------------------------------------------------------------------------------|
| host                         |               | The host of the host running the server                                               |
| port                         |               | The port of the host running the server                                               |
| address                      |               | The address (host:port) of the host running the server                                |
| timeout                      |               | Number of seconds before connection times out (default=10)                            |
| target                       |               | Target to use (lookup connection info from config)                                    |
| retry                        |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                      |               | legacy version of retry                                                               |
| source-host                  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                      |               | The name of the command that the remote daemon should run                             |
| argument                     |               | Set command line arguments                                                            |
| separator                    |               | Separator to use for the batch command (default is |)                                 |
| batch                        |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                  |               | The client certificate to use                                                         |
| dh                           |               | The DH key to use                                                                     |
| certificate-key              |               | Client certificate to use                                                             |
| certificate-format           |               | Client certificate format                                                             |
| ca                           |               | Certificate authority                                                                 |
| verify                       |               | Client certificate format                                                             |
| allowed-ciphers              |               | Client certificate format                                                             |
| [ssl](#exec_remote_nscp_ssl) | true          | Initial an ssl handshake with the server.                                             |
| password                     |               | Password                                                                              |



<h5 id="exec_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### remote_nscp_query

Request remote information via NSCP.

#### About `remote_nscp_query`

`remote_nscp_query` runs a check on a remote NSClient++ agent over the NSCP
protocol and returns its result. It is the same command as
[`check_remote_nscp`](#check_remote_nscp) under a second name — the two are
registered as aliases of one implementation, take the same options and behave
identically.

Both names exist because `check_remote_nscp` reads naturally where a monitoring
configuration lists check commands, while `remote_nscp_query` follows this
module's `remote_nscp_*` naming alongside `exec_remote_nscp`,
`submit_remote_nscp` and `remote_nscpforward`. Pick whichever reads better in
your configuration and stay consistent.

See [`check_remote_nscp`](#check_remote_nscp) for the full description: when to
prefer NSCP over NRPE, targets, and the password and TLS options.

**Jump to section:**

* [Sample Commands](#remote_nscp_query_samples)
* [Command-line Arguments](#remote_nscp_query_options)


<a id="remote_nscp_query_samples"></a>
#### Sample Commands

`remote_nscp_query` is an alias of
[`check_remote_nscp`](#check_remote_nscp) — same implementation, same options,
same behaviour.

**Run a check on a remote agent:**

```
remote_nscp_query target=web01 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

**Nothing listening:**

```
remote_nscp_query host=127.0.0.1 port=15669 command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

See [`check_remote_nscp`](#check_remote_nscp) for the full set of examples —
targets, arguments, the password and TLS options, and why NSCP is preferable to
NRPE when both ends are NSClient++.



<a id="remote_nscp_query_options"></a>
#### Command-line Arguments

<a id="remote_nscp_query_host"></a>
<a id="remote_nscp_query_port"></a>
<a id="remote_nscp_query_address"></a>
<a id="remote_nscp_query_timeout"></a>
<a id="remote_nscp_query_target"></a>
<a id="remote_nscp_query_retry"></a>
<a id="remote_nscp_query_retries"></a>
<a id="remote_nscp_query_source-host"></a>
<a id="remote_nscp_query_sender-host"></a>
<a id="remote_nscp_query_command"></a>
<a id="remote_nscp_query_argument"></a>
<a id="remote_nscp_query_separator"></a>
<a id="remote_nscp_query_batch"></a>
<a id="remote_nscp_query_certificate"></a>
<a id="remote_nscp_query_dh"></a>
<a id="remote_nscp_query_certificate-key"></a>
<a id="remote_nscp_query_certificate-format"></a>
<a id="remote_nscp_query_ca"></a>
<a id="remote_nscp_query_verify"></a>
<a id="remote_nscp_query_allowed-ciphers"></a>
<a id="remote_nscp_query_password"></a>

| Option                        | Default Value | Description                                                                           |
|-------------------------------|---------------|---------------------------------------------------------------------------------------|
| host                          |               | The host of the host running the server                                               |
| port                          |               | The port of the host running the server                                               |
| address                       |               | The address (host:port) of the host running the server                                |
| timeout                       |               | Number of seconds before connection times out (default=10)                            |
| target                        |               | Target to use (lookup connection info from config)                                    |
| retry                         |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                       |               | legacy version of retry                                                               |
| source-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                       |               | The name of the command that the remote daemon should run                             |
| argument                      |               | Set command line arguments                                                            |
| separator                     |               | Separator to use for the batch command (default is |)                                 |
| batch                         |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                   |               | The client certificate to use                                                         |
| dh                            |               | The DH key to use                                                                     |
| certificate-key               |               | Client certificate to use                                                             |
| certificate-format            |               | Client certificate format                                                             |
| ca                            |               | Certificate authority                                                                 |
| verify                        |               | Client certificate format                                                             |
| allowed-ciphers               |               | Client certificate format                                                             |
| [ssl](#remote_nscp_query_ssl) | true          | Initial an ssl handshake with the server.                                             |
| password                      |               | Password                                                                              |



<h5 id="remote_nscp_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### remote_nscpforward

Forward the request as-is to remote host via NSCP.

#### About `remote_nscpforward`

`remote_nscpforward` is the module's relay command: it is meant to pass a
request through to a remote NSClient++ agent over the NSCP protocol **as-is**,
without interpreting it, so that this host can act as a proxy for agents a
monitoring server cannot address directly.

##### The registered name does not dispatch

The client framework selects how to handle a command by matching its name
(`include/client/command_line_parser.cpp`): the relay path is taken for names
that **start with `forward_` or end with `_forward`**, and the query, exec and
submit paths for `check_*` / `*_query`, `exec_*` and `submit_*` respectively.

`remote_nscpforward` matches none of those — it ends in `nscpforward`, not
`_forward` — so it falls through to the final `else` and the call is answered
with:

```
remote_nscpforward not found
```

The command is registered and appears in the reference, but **invoking it does
nothing useful in this release**. The sibling `nrpe_forward` in
[NRPEClient](NRPEClient.md#nrpe_forward) does end in `_forward` and is
dispatched correctly.

##### What to use instead

For an NSCP relay today, register the module's own `fallback` handler on the
target, which routes unmatched requests through the same client without going
via this command name. Where an explicit command is needed and the far end is
NSClient++, [`check_remote_nscp`](#check_remote_nscp) forwards a named check and
returns its full structured result.

**Jump to section:**

* [Sample Commands](#remote_nscpforward_samples)


<a id="remote_nscpforward_samples"></a>
#### Sample Commands

**Invoking the command:**

The name matches none of the dispatch prefixes or suffixes the client framework
recognises (`forward_*`, `*_forward`, `check_*`, `*_query`, `exec_*`,
`submit_*`), so the call never reaches the relay code:

```
remote_nscpforward host=10.0.2.50 port=8443 command=check_ok
remote_nscpforward not found
```

The same answer comes back regardless of the arguments, and regardless of
whether anything is listening at the other end.

**Relaying NSCP traffic today:**

Configure the module's `fallback` handler on the target instead. A request this
agent does not handle itself is then passed to the configured NSCP target and
the answer returned unchanged, which is the behaviour this command was meant to
expose:

```ini
[/modules]
NSCPClient = enabled

[/settings/NSCP/client/targets/default]
address = nscp://10.0.2.50:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem
```

From the monitoring server the relay is then invisible — it addresses the relay
and gets the far agent's result, with status, message and performance data
intact, because NSCP carries the request and response as structured data.

**Forwarding a named check explicitly:**

```
check_remote_nscp target=relay command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used|'C:\ used'=91.2GB;80;90;0;100 'C:\ used %'=91%;80;90;0;100
```

**Two consequences of relaying at all:**

A relay that does not inspect requests asks the far end for whatever the caller
asked for, so restrict what may be forwarded on the relay itself. And because it
terminates one connection and opens another, the far end sees the *relay* as the
client — any password or certificate-based authorisation there applies to the
relay, not to the original caller.




### submit_remote_nscp

Submit information to remote host via NSCP.

#### About `submit_remote_nscp`

`submit_remote_nscp` sends a **passive result** to a remote NSClient++ agent over
the NSCP protocol: instead of asking the far end to run a check, it hands it a
result that has already been produced here.

This is how you chain NSClient++ agents. A host that cannot reach the monitoring
server — behind a firewall, in a DMZ, on a management segment — submits its
results to an agent that can, and that agent forwards them onward through
whatever transport the monitoring server expects.

The result is described with `command=` (or its synonym `alias=`, the service
name to report against), `result=` (a number, or `OK` / `WARN` / `CRIT` /
`UNKNOWN`) and `message=`. `batch=` submits several results in one connection as
`command|result|message` records separated by `separator=` (default `|`).

Unlike a passive submission over NRPE, there is no payload ceiling here and
performance data travels as structured data rather than being flattened into the
message, so a full check result survives the hop intact.

Connection, password and TLS options are the same as for
[`check_remote_nscp`](#check_remote_nscp). The usual way to use this command is
to route results to it — give a scheduled check the module's target, rather than
invoking it by hand.

**Jump to section:**

* [Sample Commands](#submit_remote_nscp_samples)
* [Command-line Arguments](#submit_remote_nscp_options)


<a id="submit_remote_nscp_samples"></a>
#### Sample Commands

**Submit a passive result to a remote agent:**

```
submit_remote_nscp target=relay command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**Submit several results over one connection:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_remote_nscp target=relay "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**The usual arrangement — route results rather than calling this by hand:**

A host that cannot reach the monitoring server submits to one that can:

```ini
[/modules]
NSCPClient = enabled
Scheduler = enabled

[/settings/NSCP/client/targets/relay]
address = nscp://10.0.2.10:8443
password = <shared secret>
verify mode = peer
ca = /etc/nsclient/ca.pem

[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = NSCP
```

Every five minutes the check runs locally and its result is submitted to the
relay, which forwards it onward.

**Nothing listening:**

```
submit_remote_nscp host=127.0.0.1 port=15669 command=nightly_backup result=OK "message=done"
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15669 :Connection refused
```

**Why this rather than `submit_nrpe`:**

There is no payload ceiling here and performance data travels as structured data
rather than being flattened into the message, so a full check result survives
the hop intact.



<a id="submit_remote_nscp_options"></a>
#### Command-line Arguments

<a id="submit_remote_nscp_host"></a>
<a id="submit_remote_nscp_port"></a>
<a id="submit_remote_nscp_address"></a>
<a id="submit_remote_nscp_timeout"></a>
<a id="submit_remote_nscp_target"></a>
<a id="submit_remote_nscp_retry"></a>
<a id="submit_remote_nscp_retries"></a>
<a id="submit_remote_nscp_source-host"></a>
<a id="submit_remote_nscp_sender-host"></a>
<a id="submit_remote_nscp_command"></a>
<a id="submit_remote_nscp_alias"></a>
<a id="submit_remote_nscp_message"></a>
<a id="submit_remote_nscp_result"></a>
<a id="submit_remote_nscp_separator"></a>
<a id="submit_remote_nscp_batch"></a>
<a id="submit_remote_nscp_certificate"></a>
<a id="submit_remote_nscp_dh"></a>
<a id="submit_remote_nscp_certificate-key"></a>
<a id="submit_remote_nscp_certificate-format"></a>
<a id="submit_remote_nscp_ca"></a>
<a id="submit_remote_nscp_verify"></a>
<a id="submit_remote_nscp_allowed-ciphers"></a>
<a id="submit_remote_nscp_password"></a>

| Option                         | Default Value | Description                                                                           |
|--------------------------------|---------------|---------------------------------------------------------------------------------------|
| host                           |               | The host of the host running the server                                               |
| port                           |               | The port of the host running the server                                               |
| address                        |               | The address (host:port) of the host running the server                                |
| timeout                        |               | Number of seconds before connection times out (default=10)                            |
| target                         |               | Target to use (lookup connection info from config)                                    |
| retry                          |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                        |               | legacy version of retry                                                               |
| source-host                    |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                    |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                        |               | The name of the command that the remote daemon should run                             |
| alias                          |               | Same as command                                                                       |
| message                        |               | Message                                                                               |
| result                         |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| separator                      |               | Separator to use for the batch command (default is |)                                 |
| batch                          |               | Add multiple records using the separator format is: command|result|message            |
| certificate                    |               | The client certificate to use                                                         |
| dh                             |               | The DH key to use                                                                     |
| certificate-key                |               | Client certificate to use                                                             |
| certificate-format             |               | Client certificate format                                                             |
| ca                             |               | Certificate authority                                                                 |
| verify                         |               | Client certificate format                                                             |
| allowed-ciphers                |               | Client certificate format                                                             |
| [ssl](#submit_remote_nscp_ssl) | true          | Initial an ssl handshake with the server.                                             |
| password                       |               | Password                                                                              |



<h5 id="submit_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NSCP/client](#nscp-client-section)               | NSCP CLIENT SECTION       |
| [/settings/NSCP/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NSCP/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### NSCP CLIENT SECTION <a id="/settings/NSCP/client"></a>

Section for NSCP active/passive check module.

| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | NSCP          | CHANNEL     |


```ini
# Section for NSCP active/passive check module.
[/settings/NSCP/client]
channel=NSCP
```

#### CHANNEL <a id="/settings/NSCP/client/channel"></a>

The channel to listen to.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCP/client](#/settings/NSCP/client) |
| Key:           | channel                                         |
| Default value: | `NSCP`                                          |


**Sample:**

```
[/settings/NSCP/client]
# CHANNEL
channel=NSCP
```

### CLIENT HANDLER SECTION <a id="/settings/NSCP/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NSCP/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description           |
|---------------------|---------------|-----------------------|
| address             |               | TARGET ADDRESS        |
| allow host override | false         | ALLOW HOST OVERRIDE   |
| allowed ciphers     |               | ALLOWED CIPHERS       |
| ca                  |               | CA                    |
| certificate         |               | SSL CERTIFICATE       |
| certificate format  |               | CERTIFICATE FORMAT    |
| certificate key     |               | SSL CERTIFICATE       |
| dh                  |               | DH KEY                |
| host                |               | TARGET HOST           |
| password            |               | PASSWORD              |
| port                |               | TARGET PORT           |
| retries             | 3             | RETRIES               |
| timeout             | 30            | TIMEOUT               |
| use ssl             |               | ENABLE SSL ENCRYPTION |
| verify mode         |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NSCP/client/targets/sample]
#address=...
allow host override=false
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#host=...
#password=...
#port=...
retries=3
timeout=30
#use ssl=...
#verify mode=...

```





