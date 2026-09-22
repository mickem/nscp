# NSCPClient

NSCP client can be used both from command line and from queries to check remote systems over their REST API

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

| Command                                   | Description                                                                                                                      |
|-------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------|
| [check_remote_nscp](#check_remote_nscp)   | Request remote information via NSCP.                                                                                             |
| [remote_nscp_query](#remote_nscp_query)   | Request remote information via NSCP.                                                                                             |
| [remote_nscpforward](#remote_nscpforward) | Forward the request to a remote host via NSCP (the command and its arguments are re-issued against the remote agent's REST API). |

### check_remote_nscp

Request remote information via NSCP.

#### About `check_remote_nscp`

`check_remote_nscp` runs a check on a remote NSClient++ agent over that agent's
**REST API** and returns its result.

`check_remote_nscp` and [`remote_nscp_query`](#remote_nscp_query) are the same
command under two names; use whichever reads better in your configuration.

##### What the remote end needs

The far end answers on its web server, so the remote agent needs the
`WEBServer` module enabled and reachable on the port you point at (`8443` by
default). The request goes to `/api/v2/queries/<command>/commands/execute` with
`Accept: text/plain`, and the answer is a Nagios result: the status in the HTTP
status code, `message|perfdata` in the body, one line per response line.

This replaces the older raw-protobuf `/query.pb` transport, which let the caller
write the request header and so pick the identity the remote's permission layer
attributed the call to. The versioned API takes the command and its arguments
and stamps the identity from the authenticated session instead.

##### Why use this instead of NRPE

- **No payload ceiling.** NRPE version 2 truncates output at a fixed buffer.
  This transport does not, and the remote builds its performance data without
  truncation, so a long check result arrives whole.
- **Real authentication.** A password over TLS against a named user with an
  explicit permission grant, rather than NRPE's traditional anonymous
  Diffie-Hellman. TLS is on by default here.

Use [NRPE](NRPEClient.md) when the far end is a Nagios `nrpe` daemon or another
non-NSClient++ agent; use this when it is NSClient++.

##### Connecting

Name the host with `host=` (and `port=`, or `address=host:port`), or with
`target=` to pull the connection details from a target defined in the module's
settings — which is where the password and TLS material belong, rather than on
every command line. `command=` names the check to run on the far end and
`argument=` passes arguments to it (repeatable), exactly as if you were running
that check locally.

A target may also set `path` to override the API base, which defaults to
`/api/v2/queries`. It is a target setting rather than a command-line option.

##### Security

`password=` is sent as the `password` header — the same one Icinga's
`check_nscp_api` uses. The remote maps it to its implicit `admin` web user, so
that user must exist with this password and hold a role granting
`queries.execute`. A fresh install seeds both; a hardened one may not, and the
check then comes back UNKNOWN rather than passing.

TLS is enabled by default and is configured with `certificate=`,
`certificate-key=`, `ca=`, `dh=`, `verify=` and `allowed-ciphers=`. Certificate
verification is on by default (`verify mode = peer`) against the agent's own
trust bundle (`ca` defaults to `${ca-path}`), because the target carries the
remote's password and an unverified connection hands it to whichever host
answers for the address.

An agent still presenting the self-signed certificate it generates on first
start therefore does **not** verify out of the box: point `ca=` at that
certificate and use `verify=peer-cert`, or point `ca=` at the issuing CA and
keep `verify=peer`. `verify=none` restores the old behaviour — encrypted but
unauthenticated — and now has to be asked for explicitly. `ssl=false` turns TLS
off entirely, which sends the password in the clear.

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

Unlike NRPE, the transport imposes no payload ceiling and the remote builds its
performance data without truncation, so the result arrives intact regardless of
length.

**Pass arguments to the remote check (`argument=`, repeatable):**

```
check_remote_nscp host=192.168.56.103 command=check_drivesize "argument=drive=C:" "argument=crit=used > 95%"
OK: OK All 1 drive(s) are ok
```

**Use a configured target instead of spelling out the connection:**

Put the host, password and TLS material under
`[/settings/NSCP/client/targets/...]` so credentials stay out of process
listings. The password is the remote agent's web `admin` user's password, and
that user needs a role granting `queries.execute`:

```ini
[/settings/NSCP/client/targets/web01]
address = nscp://192.168.56.103:8443
password = <the remote's admin password>
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

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
| Option                        | Default Value | Description                                                                                                                                                                                                                                                                                                                                                                              |
|-------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                          |               | The host of the host running the server                                                                                                                                                                                                                                                                                                                                                  |
| port                          |               | The port of the host running the server                                                                                                                                                                                                                                                                                                                                                  |
| address                       |               | The address (host:port) of the host running the server                                                                                                                                                                                                                                                                                                                                   |
| timeout                       |               | Number of seconds before connection times out (default=10)                                                                                                                                                                                                                                                                                                                               |
| target                        |               | Target to use (lookup connection info from config)                                                                                                                                                                                                                                                                                                                                       |
| retry                         |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                                                                                                                                                                                                                                         |
| retries                       |               | legacy version of retry                                                                                                                                                                                                                                                                                                                                                                  |
| source-host                   |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                                    |
| sender-host                   |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                                    |
| command                       |               | The name of the command that the remote daemon should run                                                                                                                                                                                                                                                                                                                                |
| argument                      |               | Set command line arguments                                                                                                                                                                                                                                                                                                                                                               |
| separator                     |               | Separator to use for the batch command (default is |)                                                                                                                                                                                                                                                                                                                                    |
| batch                         |               | Add multiple records using the separator format is: command|argument|argument                                                                                                                                                                                                                                                                                                            |
| certificate                   |               | The client certificate to use                                                                                                                                                                                                                                                                                                                                                            |
| dh                            |               | The DH key to use                                                                                                                                                                                                                                                                                                                                                                        |
| certificate-key               |               | The private key belonging to the client certificate (when it is not in the certificate file itself)                                                                                                                                                                                                                                                                                      |
| certificate-format            |               | Client certificate format                                                                                                                                                                                                                                                                                                                                                                |
| ca                            |               | The certificate authority the server certificate is verified against                                                                                                                                                                                                                                                                                                                     |
| verify                        |               | How to verify the server certificate. Comma separated list of options: none, peer (or certificate), peer-cert, fail-if-no-cert (or fail-if-no-peer-cert, client-certificate). For a self signed certificate use peer-cert and point --ca at that certificate; none leaves the connection encrypted but the server unauthenticated, so an on-path attacker can impersonate it undetected. |
| allowed-ciphers               |               | The OpenSSL cipher list the connection is restricted to                                                                                                                                                                                                                                                                                                                                  |
| [ssl](#check_remote_nscp_ssl) | true          | Initial an ssl handshake with the server.                                                                                                                                                                                                                                                                                                                                                |
| password                      |               | Password                                                                                                                                                                                                                                                                                                                                                                                 |



<h5 id="check_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### remote_nscp_query

Request remote information via NSCP.

#### About `remote_nscp_query`

`remote_nscp_query` runs a check on a remote NSClient++ agent over that agent's
REST API and returns its result. It is the same command as
[`check_remote_nscp`](#check_remote_nscp) under a second name — the two are
registered as aliases of one implementation, take the same options and behave
identically.

Both names exist because `check_remote_nscp` reads naturally where a monitoring
configuration lists check commands, while `remote_nscp_query` follows this
module's `remote_nscp_*` naming alongside `remote_nscpforward`. Pick whichever
reads better in your configuration and stay consistent.

See [`check_remote_nscp`](#check_remote_nscp) for the full description: what the
remote end needs, when to prefer this over NRPE, targets, and the password and
TLS options.

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

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
| Option                        | Default Value | Description                                                                                                                                                                                                                                                                                                                                                                              |
|-------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                          |               | The host of the host running the server                                                                                                                                                                                                                                                                                                                                                  |
| port                          |               | The port of the host running the server                                                                                                                                                                                                                                                                                                                                                  |
| address                       |               | The address (host:port) of the host running the server                                                                                                                                                                                                                                                                                                                                   |
| timeout                       |               | Number of seconds before connection times out (default=10)                                                                                                                                                                                                                                                                                                                               |
| target                        |               | Target to use (lookup connection info from config)                                                                                                                                                                                                                                                                                                                                       |
| retry                         |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                                                                                                                                                                                                                                         |
| retries                       |               | legacy version of retry                                                                                                                                                                                                                                                                                                                                                                  |
| source-host                   |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                                    |
| sender-host                   |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                                    |
| command                       |               | The name of the command that the remote daemon should run                                                                                                                                                                                                                                                                                                                                |
| argument                      |               | Set command line arguments                                                                                                                                                                                                                                                                                                                                                               |
| separator                     |               | Separator to use for the batch command (default is |)                                                                                                                                                                                                                                                                                                                                    |
| batch                         |               | Add multiple records using the separator format is: command|argument|argument                                                                                                                                                                                                                                                                                                            |
| certificate                   |               | The client certificate to use                                                                                                                                                                                                                                                                                                                                                            |
| dh                            |               | The DH key to use                                                                                                                                                                                                                                                                                                                                                                        |
| certificate-key               |               | The private key belonging to the client certificate (when it is not in the certificate file itself)                                                                                                                                                                                                                                                                                      |
| certificate-format            |               | Client certificate format                                                                                                                                                                                                                                                                                                                                                                |
| ca                            |               | The certificate authority the server certificate is verified against                                                                                                                                                                                                                                                                                                                     |
| verify                        |               | How to verify the server certificate. Comma separated list of options: none, peer (or certificate), peer-cert, fail-if-no-cert (or fail-if-no-peer-cert, client-certificate). For a self signed certificate use peer-cert and point --ca at that certificate; none leaves the connection encrypted but the server unauthenticated, so an on-path attacker can impersonate it undetected. |
| allowed-ciphers               |               | The OpenSSL cipher list the connection is restricted to                                                                                                                                                                                                                                                                                                                                  |
| [ssl](#remote_nscp_query_ssl) | true          | Initial an ssl handshake with the server.                                                                                                                                                                                                                                                                                                                                                |
| password                      |               | Password                                                                                                                                                                                                                                                                                                                                                                                 |



<h5 id="remote_nscp_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


### remote_nscpforward

Forward the request to a remote host via NSCP (the command and its arguments are re-issued against the remote agent's REST API).

#### About `remote_nscpforward`

`remote_nscpforward` is the module's relay command: it is meant to pass a
request through to a remote NSClient++ agent **as-is**, without interpreting it,
so that this host can act as a proxy for agents a monitoring server cannot
address directly. The command and its arguments are re-issued against the remote
agent's REST API.

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
returns its result, message and performance data alike.

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
password = <the remote's admin password>
verify mode = peer
ca = /etc/nsclient/ca.pem
```

From the monitoring server the relay is then invisible — it addresses the relay
and gets the far agent's result, with status, message and performance data
intact.

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





