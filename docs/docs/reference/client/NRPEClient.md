# NRPEClient

NRPE client can be used both from command line and from queries to check remote systems via NRPE as well as configure the NRPE server

## Enable module

To enable this module and and allow using the commands you need to ass `NRPEClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NRPEClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the NRPEClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                       | Description                                                                    |
|-------------------------------|--------------------------------------------------------------------------------|
| [check_nrpe](#check_nrpe)     | Request remote information via NRPE.                                           |
| [exec_nrpe](#exec_nrpe)       | Execute remote script via NRPE. (Most likely you want nrpe_query).             |
| [nrpe_forward](#nrpe_forward) | Forward the request as-is to remote host via NRPE.                             |
| [nrpe_query](#nrpe_query)     | Request remote information via NRPE.                                           |
| [submit_nrpe](#submit_nrpe)   | Submit information to remote host via NRPE. (Most likely you want nrpe_query). |

### check_nrpe

Request remote information via NRPE.

#### About `check_nrpe`

`check_nrpe` runs a command on a remote host over **NRPE** and returns its
result — the active-check direction, and the command this module exists for.

`check_nrpe` and [`nrpe_query`](#nrpe_query) are the same command under two
names; use whichever reads better in your configuration.

Name the remote host with `host=` (and `port=`, or `address=host:port`), or with
`target=` to pull the connection details from a target defined in the module's
settings — which is where credentials and TLS material belong, rather than on
every command line. `command=` is the command the remote agent should run, and
`argument=` passes arguments to it (repeatable).

The remote end decides what `command=` means: an NSClient++ agent maps it to its
own check commands, a Nagios `nrpe` daemon to a `command[...]` line in
`nrpe.cfg`. A command the far end does not know is an error there, not here.

##### Protocol version and payload length

`version=` selects the NRPE protocol version and must match what the remote
daemon speaks. Version 2 is the classic protocol with a **fixed 1024-byte
payload**, which silently truncates longer check output; version 3 and later
negotiate a larger payload.

The version-2 buffer size must equal the remote daemon's compiled-in value — a
mismatch corrupts the exchange rather than reporting a clean error. Note that
the two spellings are not interchangeable: the setting under
`[/settings/NRPE/client/targets/...]` is `payload length`, while the
command-line and REST option is **`payload-length`** (short form `-l`).

If a check works but its output is cut off at a suspiciously round length, this
is why.

##### TLS

NRPE's transport is TLS, configured with `certificate=`, `certificate-key=`,
`ca=`, `dh=`, `verify=` and `allowed-ciphers=`.

Classic NRPE deployments use **anonymous Diffie-Hellman** ciphers: encrypted, but
with no authentication of either end, so anyone who can reach the port can run
the daemon's commands and anyone on the path can impersonate the server. That is
what `insecure` mode preserves for compatibility with old daemons. Where both
ends are NSClient++, use real certificates and `verify=peer` instead.

##### Batching

`batch=` runs several commands in one connection, given as
`command|argument|argument` records separated by `separator=` (default `|`).

**Jump to section:**

* [Sample Commands](#check_nrpe_samples)
* [Command-line Arguments](#check_nrpe_options)


<a id="check_nrpe_samples"></a>
#### Sample Commands

The examples below run against an NSClient++ agent whose NRPE server is
listening on `127.0.0.1:15666` in legacy insecure (anonymous-DH) mode.

**Run a check on the remote host:**

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_ok
OK: No message
```

**A real check, with its performance data:**

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
'/ used'=8.41848GB;201.57782;226.77505;0;251.97227 '/ used %'=3%;80;90;0;100 '/opt/claude-code used'=202.74609MB;183.95937;206.95429;0;229.94921 '/opt/claude-code used %'=88%;80;90;0;100
```

**Pass arguments to the remote check (`argument=`, repeatable):**

The remote agent must be configured with `allow arguments = true`, or the
arguments are refused there.

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_ok "argument=message=hello from NRPE"
OK: hello from NRPE
```

**Use a configured target instead of spelling out the connection:**

Put the host, port and TLS material under `[/settings/NRPE/client/targets/...]`
and the command line stays short — and the credentials stay out of process
listings.

```
check_nrpe target=web01 command=check_drivesize
OK: OK All 3 drive(s) are ok
```

**Run several checks over one connection:**

```
check_nrpe host=127.0.0.1 port=15666 insecure=true "batch=check_ok|message=first" "batch=check_drivesize"
OK: first
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```

**A command the remote agent does not know:**

The error comes from the far end, not from the client.

```
check_nrpe host=127.0.0.1 port=15666 insecure=true command=check_no_such_thing
UNKNOWN: Unknown command(s): check_no_such_thing
```

**Nothing listening:**

```
check_nrpe host=127.0.0.1 port=15667 insecure=true command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15667 :Connection refused
```

**Client and server disagreeing about TLS:**

Dropping `insecure=true` against a server running in insecure mode fails the
handshake rather than falling back. Both ends must be configured the same way.

```
check_nrpe host=127.0.0.1 port=15666 command=check_ok
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15666 :sslv3 alert handshake failure (SSL routines)
```

`insecure=true` means anonymous Diffie-Hellman: encrypted, but with neither end
authenticated. Where both ends are NSClient++, configure real certificates and
`verify=peer` instead.

**Truncated output on protocol version 2:**

Version 2 has a fixed 1024-byte payload. If a check's output is cut off at a
suspiciously round length, set `version=3` — or raise `payload-length=` to match
the remote daemon exactly, a mismatch corrupting the exchange rather than
reporting an error. (`payload-length` is the command-line spelling; the setting
key under the target is `payload length`.)

```
check_nrpe host=192.168.56.103 command=check_files "argument=path=/var/log" version=3
OK: 412 files found
```



<a id="check_nrpe_options"></a>
#### Command-line Arguments

<a id="check_nrpe_host"></a>
<a id="check_nrpe_port"></a>
<a id="check_nrpe_address"></a>
<a id="check_nrpe_timeout"></a>
<a id="check_nrpe_target"></a>
<a id="check_nrpe_retry"></a>
<a id="check_nrpe_retries"></a>
<a id="check_nrpe_source-host"></a>
<a id="check_nrpe_sender-host"></a>
<a id="check_nrpe_command"></a>
<a id="check_nrpe_argument"></a>
<a id="check_nrpe_separator"></a>
<a id="check_nrpe_batch"></a>
<a id="check_nrpe_certificate"></a>
<a id="check_nrpe_dh"></a>
<a id="check_nrpe_certificate-key"></a>
<a id="check_nrpe_certificate-format"></a>
<a id="check_nrpe_ca"></a>
<a id="check_nrpe_verify"></a>
<a id="check_nrpe_allowed-ciphers"></a>
<a id="check_nrpe_payload-length"></a>
<a id="check_nrpe_version"></a>
<a id="check_nrpe_buffer-length"></a>

| Option                           | Default Value | Description                                                                                                                                                               |
|----------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                             |               | The host of the host running the server                                                                                                                                   |
| port                             |               | The port of the host running the server                                                                                                                                   |
| address                          |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                          |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                           |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                            |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                          |               | legacy version of retry                                                                                                                                                   |
| source-host                      |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                      |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                          |               | The name of the command that the remote daemon should run                                                                                                                 |
| argument                         |               | Set command line arguments                                                                                                                                                |
| separator                        |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                            |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| certificate                      |               | The client certificate to use                                                                                                                                             |
| dh                               |               | The DH key to use                                                                                                                                                         |
| certificate-key                  |               | Client certificate to use                                                                                                                                                 |
| certificate-format               |               | Client certificate format                                                                                                                                                 |
| ca                               |               | Certificate authority                                                                                                                                                     |
| verify                           |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers                  |               | Client certificate format                                                                                                                                                 |
| [ssl](#check_nrpe_ssl)           | true          | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#check_nrpe_insecure) | true          | Use insecure legacy mode                                                                                                                                                  |
| payload-length                   |               | Length of payload (has to be same as on the server)                                                                                                                       |
| version                          |               | The NRPE version to use (2 or 4)                                                                                                                                          |
| buffer-length                    |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="check_nrpe_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`

<h5 id="check_nrpe_insecure">insecure:</h5>

Use insecure legacy mode

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### exec_nrpe

Execute remote script via NRPE. (Most likely you want nrpe_query).

#### About `exec_nrpe`

`exec_nrpe` sends an **execute** request to a remote host over NRPE, rather than
a query.

The distinction matters. A *query* (`check_nrpe` / `nrpe_query`) asks the remote
agent to run a check and return a status, message and performance data — the
normal monitoring interaction. An *execute* request invokes the remote agent's
command-line interface and returns its textual output: the equivalent of running
`nscp <something>` on the far end, used for administrative operations rather
than for checks.

**Most of the time you want [`nrpe_query`](#nrpe_query) instead.** The module's
own description says so, and reaching for `exec_nrpe` to run a check will give
you raw text with no status to alert on.

The options are the same as for `check_nrpe`: `host=` / `port=` / `address=` or
`target=` for the connection, `command=` and `argument=` for what to run, plus
the shared TLS options.

The remote agent must be willing to serve execute requests at all — an
NSClient++ agent exposes them only where its configuration allows, and a stock
Nagios `nrpe` daemon has no such concept. Since an execute request is closer to
remote administration than to monitoring, be deliberate about which hosts accept
it and from where.

**Jump to section:**

* [Sample Commands](#exec_nrpe_samples)
* [Command-line Arguments](#exec_nrpe_options)


<a id="exec_nrpe_samples"></a>
#### Sample Commands

**Send an execute request to a remote agent:**

An execute request invokes the remote agent's command-line interface and returns
its textual output — the equivalent of running `nscp <something>` there.

```
exec_nrpe host=192.168.56.103 command=help
Usage: nscp <command> [options]
...
```

**When the remote agent does not serve execute requests:**

Nothing comes back — no status, no message. NSClient++ exposes execute requests
only where its configuration allows them, and a stock Nagios `nrpe` daemon has
no such concept at all.

```
exec_nrpe host=127.0.0.1 port=15666 insecure=true command=help

```

**This is not how you run a check:**

An execute request returns raw text with no status to alert on. For a check, use
[`nrpe_query`](#nrpe_query):

```
nrpe_query host=127.0.0.1 port=15666 insecure=true command=check_drivesize
WARNING: WARNING /opt/claude-code: 202.746MB/229.949MB used
```

**Nothing listening:**

```
exec_nrpe host=127.0.0.1 port=15667 command=help
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15667 :Connection refused
```



<a id="exec_nrpe_options"></a>
#### Command-line Arguments

<a id="exec_nrpe_host"></a>
<a id="exec_nrpe_port"></a>
<a id="exec_nrpe_address"></a>
<a id="exec_nrpe_timeout"></a>
<a id="exec_nrpe_target"></a>
<a id="exec_nrpe_retry"></a>
<a id="exec_nrpe_retries"></a>
<a id="exec_nrpe_source-host"></a>
<a id="exec_nrpe_sender-host"></a>
<a id="exec_nrpe_command"></a>
<a id="exec_nrpe_argument"></a>
<a id="exec_nrpe_separator"></a>
<a id="exec_nrpe_batch"></a>
<a id="exec_nrpe_certificate"></a>
<a id="exec_nrpe_dh"></a>
<a id="exec_nrpe_certificate-key"></a>
<a id="exec_nrpe_certificate-format"></a>
<a id="exec_nrpe_ca"></a>
<a id="exec_nrpe_verify"></a>
<a id="exec_nrpe_allowed-ciphers"></a>
<a id="exec_nrpe_payload-length"></a>
<a id="exec_nrpe_version"></a>
<a id="exec_nrpe_buffer-length"></a>

| Option                          | Default Value | Description                                                                                                                                                               |
|---------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                            |               | The host of the host running the server                                                                                                                                   |
| port                            |               | The port of the host running the server                                                                                                                                   |
| address                         |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                         |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                          |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                         |               | legacy version of retry                                                                                                                                                   |
| source-host                     |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                     |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                         |               | The name of the command that the remote daemon should run                                                                                                                 |
| argument                        |               | Set command line arguments                                                                                                                                                |
| separator                       |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                           |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| certificate                     |               | The client certificate to use                                                                                                                                             |
| dh                              |               | The DH key to use                                                                                                                                                         |
| certificate-key                 |               | Client certificate to use                                                                                                                                                 |
| certificate-format              |               | Client certificate format                                                                                                                                                 |
| ca                              |               | Certificate authority                                                                                                                                                     |
| verify                          |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers                 |               | Client certificate format                                                                                                                                                 |
| [ssl](#exec_nrpe_ssl)           | true          | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#exec_nrpe_insecure) | true          | Use insecure legacy mode                                                                                                                                                  |
| payload-length                  |               | Length of payload (has to be same as on the server)                                                                                                                       |
| version                         |               | The NRPE version to use (2 or 4)                                                                                                                                          |
| buffer-length                   |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="exec_nrpe_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`

<h5 id="exec_nrpe_insecure">insecure:</h5>

Use insecure legacy mode

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### nrpe_forward

Forward the request as-is to remote host via NRPE.

#### About `nrpe_forward`

`nrpe_forward` passes a request through to a remote host over NRPE **as-is**,
without interpreting it.

This is the relay command. Where `check_nrpe` builds an NRPE request from
`command=` and `argument=` options, `nrpe_forward` takes a request that has
already arrived at this agent and re-sends it to another one, returning whatever
comes back. That makes this host a proxy: a monitoring server that can reach it
can, through it, reach agents it cannot address directly — the usual case being
a DMZ or a management segment where only one host is exposed.

Register it as the fallback for a target and the arrangement becomes transparent
to the monitoring server, which believes it is talking to the final agent.

It takes **no options of its own**: the relay branch hands the incoming request
straight to the handler without parsing an option descriptor, so the destination
comes only from the target configuration and anything passed on the command line
is appended to the outgoing request rather than interpreted here. Configure the
target; do not try to steer the hop per call.

Two things follow from "as-is" that are worth being deliberate about. Because
the request is not inspected, **whatever the caller asks for is what the far end
is asked to run** — the relay adds no filtering of its own, so restrict what may
be forwarded, and to where, on this host rather than assuming the hop is a
control point. And because the relay terminates one TLS connection and opens
another, the far end sees this host as the client: any certificate-based
authorisation on the far end applies to the relay, not to the original caller.

Connection and TLS options are the same as for [`check_nrpe`](#check_nrpe).

**Jump to section:**

* [Sample Commands](#nrpe_forward_samples)
* [Command-line Arguments](#nrpe_forward_options)


<a id="nrpe_forward_samples"></a>
#### Sample Commands

`nrpe_forward` is not normally invoked by hand: it re-sends a request that has
already arrived at this agent, so it is configured as a fallback on a target and
then used implicitly.

**Configure this agent as an NRPE relay:**

```ini
[/modules]
NRPEServer = enabled
NRPEClient = enabled

[/settings/NRPE/client/targets/default]
address = nrpe://10.0.2.50:5666
verify = peer
ca = /etc/nsclient/ca.pem

[/settings/NRPE/client]
channel = NRPE
```

With `nrpe_forward` registered as the fallback, a request the relay does not
handle itself is passed on to `10.0.2.50` and the answer returned unchanged.

**From the monitoring server, the relay is invisible:**

```
check_nrpe --host 192.168.56.10 --command check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

The server addressed the relay; the result came from the agent behind it.

**It takes no options of its own:**

The relay branch in the client framework hands the incoming request straight to
the handler without building or parsing an option descriptor, so `host=`,
`port=` and `command=` are **not** interpreted here — the destination comes only
from the target configuration, and any arguments given are appended to the
request that goes out on the wire. There is nothing useful to invoke by hand;
configure the target and let requests arrive.

**Two consequences of forwarding "as-is":**

The relay does not inspect the request, so whatever the caller asks for is what
the far end is asked to run — restrict what may be forwarded on the relay
itself. And because it terminates one TLS connection and opens another, the far
end sees the *relay* as the client, so any certificate-based authorisation there
applies to the relay rather than to the original caller.

**Nothing listening on the far end:**

The failure surfaces at the monitoring server as if the check itself had failed
— the relay reports the connection error from its own attempt to reach the
configured target:

```
check_nrpe --host 192.168.56.10 --command check_drivesize
UNKNOWN: Error: Failed to connect to: 10.0.2.50:5666 :Connection refused
```



<a id="nrpe_forward_options"></a>
#### Command-line Arguments

<a id="nrpe_forward_*"></a>

| Option | Default Value | Description |
|--------|---------------|-------------|
| *      |               |             |


### nrpe_query

Request remote information via NRPE.

#### About `nrpe_query`

`nrpe_query` runs a command on a remote host over **NRPE** and returns its
result. It is the same command as [`check_nrpe`](#check_nrpe) under a second
name — the two are registered as aliases of one implementation, take the same
options and behave identically.

Both names exist because `check_nrpe` matches the classic Nagios plugin that
most people type from a shell, while `nrpe_query` follows this module's
`<protocol>_query` naming alongside `exec_nrpe`, `submit_nrpe` and
`nrpe_forward`. Pick whichever reads better in your configuration and stay
consistent.

See [`check_nrpe`](#check_nrpe) for the full description: targets, protocol
versions and payload length, TLS, and batching.

**Jump to section:**

* [Sample Commands](#nrpe_query_samples)
* [Command-line Arguments](#nrpe_query_options)


<a id="nrpe_query_samples"></a>
#### Sample Commands

`nrpe_query` is an alias of [`check_nrpe`](#check_nrpe) — same implementation,
same options, same behaviour.

**Run a check on the remote host:**

```
nrpe_query host=127.0.0.1 port=15666 insecure=true command=check_ok "argument=message=hello from NRPE"
OK: hello from NRPE
```

**With a configured target:**

```
nrpe_query target=web01 command=check_drivesize
WARNING: WARNING C:\: 91.2GB/100GB used
```

See [`check_nrpe`](#check_nrpe) for the full set of examples — targets, batching,
protocol versions, payload length and the TLS failure modes.



<a id="nrpe_query_options"></a>
#### Command-line Arguments

<a id="nrpe_query_host"></a>
<a id="nrpe_query_port"></a>
<a id="nrpe_query_address"></a>
<a id="nrpe_query_timeout"></a>
<a id="nrpe_query_target"></a>
<a id="nrpe_query_retry"></a>
<a id="nrpe_query_retries"></a>
<a id="nrpe_query_source-host"></a>
<a id="nrpe_query_sender-host"></a>
<a id="nrpe_query_command"></a>
<a id="nrpe_query_argument"></a>
<a id="nrpe_query_separator"></a>
<a id="nrpe_query_batch"></a>
<a id="nrpe_query_certificate"></a>
<a id="nrpe_query_dh"></a>
<a id="nrpe_query_certificate-key"></a>
<a id="nrpe_query_certificate-format"></a>
<a id="nrpe_query_ca"></a>
<a id="nrpe_query_verify"></a>
<a id="nrpe_query_allowed-ciphers"></a>
<a id="nrpe_query_payload-length"></a>
<a id="nrpe_query_version"></a>
<a id="nrpe_query_buffer-length"></a>

| Option                           | Default Value | Description                                                                                                                                                               |
|----------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                             |               | The host of the host running the server                                                                                                                                   |
| port                             |               | The port of the host running the server                                                                                                                                   |
| address                          |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                          |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                           |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                            |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                          |               | legacy version of retry                                                                                                                                                   |
| source-host                      |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                      |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                          |               | The name of the command that the remote daemon should run                                                                                                                 |
| argument                         |               | Set command line arguments                                                                                                                                                |
| separator                        |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                            |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| certificate                      |               | The client certificate to use                                                                                                                                             |
| dh                               |               | The DH key to use                                                                                                                                                         |
| certificate-key                  |               | Client certificate to use                                                                                                                                                 |
| certificate-format               |               | Client certificate format                                                                                                                                                 |
| ca                               |               | Certificate authority                                                                                                                                                     |
| verify                           |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers                  |               | Client certificate format                                                                                                                                                 |
| [ssl](#nrpe_query_ssl)           | true          | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#nrpe_query_insecure) | true          | Use insecure legacy mode                                                                                                                                                  |
| payload-length                   |               | Length of payload (has to be same as on the server)                                                                                                                       |
| version                          |               | The NRPE version to use (2 or 4)                                                                                                                                          |
| buffer-length                    |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="nrpe_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`

<h5 id="nrpe_query_insecure">insecure:</h5>

Use insecure legacy mode

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### submit_nrpe

Submit information to remote host via NRPE. (Most likely you want nrpe_query).

#### About `submit_nrpe`

`submit_nrpe` sends a **passive result** to a remote host over NRPE: instead of
asking the far end to run a check, it hands it a result that has already been
produced here.

**Most of the time you want [`nrpe_query`](#nrpe_query) instead** — the module's
own description says so. NRPE is fundamentally an active-check protocol, and
passive results normally travel over a transport designed for them, such as
[NSCA-ng](NSCANgClient.md), [NRDP](NRDPClient.md) or
[NSCA](NSCAClient.md). Use this only where the receiving end is an NSClient++
agent that accepts submissions over NRPE.

The result is described with `command=` (or its synonym `alias=`, the service
name to report against), `result=` (a number, or `OK` / `WARN` / `CRIT` /
`UNKNOWN`) and `message=`. `batch=` submits several results in one connection as
`command|result|message` records separated by `separator=` (default `|`).

Connection and TLS options are the same as for [`check_nrpe`](#check_nrpe), and
the same payload-length caveat applies: on protocol version 2 a message longer
than the negotiated buffer is truncated, so long check output submitted this way
may not arrive whole.

**Jump to section:**

* [Sample Commands](#submit_nrpe_samples)
* [Command-line Arguments](#submit_nrpe_options)


<a id="submit_nrpe_samples"></a>
#### Sample Commands

**Submit a passive result to a remote agent:**

```
submit_nrpe host=192.168.56.103 command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**Submit several results over one connection:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_nrpe host=192.168.56.103 "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**When the receiving end does not accept submissions:**

A stock Nagios `nrpe` daemon has no notion of passive results, and an
NSClient++ agent only accepts them where its configuration allows. The far end
answers as if you had asked it to *run* the named command:

```
submit_nrpe host=127.0.0.1 port=15666 insecure=true command=nightly_backup result=CRITICAL "message=backup failed"
UNKNOWN: Unknown command(s): nightly_backup
```

This is the usual reason to reach for a transport designed for passive results
instead — [NSCA-ng](NSCANgClient.md), [NRDP](NRDPClient.md) or
[NSCA](NSCAClient.md) — or, if the far end is NSClient++,
[`submit_remote_nscp`](NSCPClient.md#submit_remote_nscp), which has no payload
ceiling and carries performance data as structured data.

**Nothing listening:**

```
submit_nrpe host=127.0.0.1 port=15667 command=nightly_backup result=OK "message=done"
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15667 :Connection refused
```



<a id="submit_nrpe_options"></a>
#### Command-line Arguments

<a id="submit_nrpe_host"></a>
<a id="submit_nrpe_port"></a>
<a id="submit_nrpe_address"></a>
<a id="submit_nrpe_timeout"></a>
<a id="submit_nrpe_target"></a>
<a id="submit_nrpe_retry"></a>
<a id="submit_nrpe_retries"></a>
<a id="submit_nrpe_source-host"></a>
<a id="submit_nrpe_sender-host"></a>
<a id="submit_nrpe_command"></a>
<a id="submit_nrpe_alias"></a>
<a id="submit_nrpe_message"></a>
<a id="submit_nrpe_result"></a>
<a id="submit_nrpe_separator"></a>
<a id="submit_nrpe_batch"></a>
<a id="submit_nrpe_certificate"></a>
<a id="submit_nrpe_dh"></a>
<a id="submit_nrpe_certificate-key"></a>
<a id="submit_nrpe_certificate-format"></a>
<a id="submit_nrpe_ca"></a>
<a id="submit_nrpe_verify"></a>
<a id="submit_nrpe_allowed-ciphers"></a>
<a id="submit_nrpe_payload-length"></a>
<a id="submit_nrpe_version"></a>
<a id="submit_nrpe_buffer-length"></a>

| Option                            | Default Value | Description                                                                                                                                                               |
|-----------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                              |               | The host of the host running the server                                                                                                                                   |
| port                              |               | The port of the host running the server                                                                                                                                   |
| address                           |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                           |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                            |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                             |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                           |               | legacy version of retry                                                                                                                                                   |
| source-host                       |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                       |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                           |               | The name of the command that the remote daemon should run                                                                                                                 |
| alias                             |               | Same as command                                                                                                                                                           |
| message                           |               | Message                                                                                                                                                                   |
| result                            |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| separator                         |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                             |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| certificate                       |               | The client certificate to use                                                                                                                                             |
| dh                                |               | The DH key to use                                                                                                                                                         |
| certificate-key                   |               | Client certificate to use                                                                                                                                                 |
| certificate-format                |               | Client certificate format                                                                                                                                                 |
| ca                                |               | Certificate authority                                                                                                                                                     |
| verify                            |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers                   |               | Client certificate format                                                                                                                                                 |
| [ssl](#submit_nrpe_ssl)           | true          | Initial an ssl handshake with the server.                                                                                                                                 |
| [insecure](#submit_nrpe_insecure) | true          | Use insecure legacy mode                                                                                                                                                  |
| payload-length                    |               | Length of payload (has to be same as on the server)                                                                                                                       |
| version                           |               | The NRPE version to use (2 or 4)                                                                                                                                          |
| buffer-length                     |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="submit_nrpe_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`

<h5 id="submit_nrpe_insecure">insecure:</h5>

Use insecure legacy mode

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NRPE/client](#nrpe-client-section)               | NRPE CLIENT SECTION       |
| [/settings/NRPE/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NRPE/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### NRPE CLIENT SECTION <a id="/settings/NRPE/client"></a>

Section for NRPE active/passive check module.

| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | NRPE          | CHANNEL     |


```ini
# Section for NRPE active/passive check module.
[/settings/NRPE/client]
channel=NRPE
```

#### CHANNEL <a id="/settings/NRPE/client/channel"></a>

The channel to listen to.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/client](#/settings/NRPE/client) |
| Key:           | channel                                         |
| Default value: | `NRPE`                                          |


**Sample:**

```
[/settings/NRPE/client]
# CHANNEL
channel=NRPE
```

### CLIENT HANDLER SECTION <a id="/settings/NRPE/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NRPE/client/targets"></a>




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
| insecure            |               | Insecure legacy mode  |
| payload length      |               | PAYLOAD LENGTH        |
| port                |               | TARGET PORT           |
| retries             | 3             | RETRIES               |
| timeout             | 30            | TIMEOUT               |
| use ssl             |               | ENABLE SSL ENCRYPTION |
| verify mode         |               | VERIFY MODE           |
| version             |               | Version               |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NRPE/client/targets/sample]
#address=...
allow host override=false
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#host=...
#insecure=...
#payload length=...
#port=...
retries=3
timeout=30
#use ssl=...
#verify mode=...
#version=...

```





