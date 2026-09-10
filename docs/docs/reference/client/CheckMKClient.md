# CheckMKClient

check_mk client can be used both from command line and from queries to check remote systems via check_mk

## Enable module

To enable this module and allow using the commands you need to add `CheckMKClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckMKClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckMKClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                           | Description                              |
|-----------------------------------|------------------------------------------|
| [check_mk_query](#check_mk_query) | Request remote information via check_mk. |

### check_mk_query

Request remote information via check_mk.

#### About `check_mk_query`

`check_mk_query` connects to a remote **check_mk agent**, retrieves its output
and hands it to a Lua script that turns it into a check result. It is the active
half of the check_mk support: the agent on the far end is a check_mk agent, not
NSClient++.

##### The Lua script does the work

Unlike the other client modules, `check_mk_query` does not itself interpret what
it fetched. The check_mk agent returns a sectioned plain-text dump — `<<<mem>>>`,
`<<<df>>>`, `<<<ps>>>` and so on — and a Lua script registered under the
module's `scripts` section is called back with the parsed packet to decide what
the status and message should be. `default_check_mk.lua` is loaded when no
script is configured.

That means the useful configuration for this command is mostly *not* on the
command line: what the check reports is whatever your script returns. Point
`scripts` at your own file when you want anything other than the default
behaviour.

##### Connection

The usual client options apply — `host=` (with `port=`, defaulting to the
module's configured value), or `target=` to use a target defined in the module's
settings, plus `timeout=`, `retries=` and the TLS options (`certificate=`,
`ca=`, `verify=`, `allowed-ciphers=`).

Note that a stock check_mk agent listens on **TCP 6556 in plain text**, with
access control done by source-IP allowlist rather than by authentication, so set
`port=6556` explicitly unless you have configured otherwise, and treat the
transport as unauthenticated unless you have put TLS in front of it.

See also the CheckMKServer module for the passive direction — serving check_mk
agent output *from* this host to a check_mk server.

**Jump to section:**

* [Sample Commands](#check_mk_query_samples)
* [Command-line Arguments](#check_mk_query_options)


<a id="check_mk_query_samples"></a>
#### Sample Commands

**Query a remote check_mk agent:**

A stock check_mk agent listens on TCP 6556, so the port normally has to be given
explicitly — the module's own default is 5667.

```
check_mk_query host=192.168.56.20 port=6556
OK: check_mk agent responded
```

**Use a configured target:**

```ini
[/settings/check_mk/client/targets/linux01]
address = 192.168.56.20:6556
timeout = 30
```

```
check_mk_query target=linux01
OK: check_mk agent responded
```

**What the check actually reports is decided by the Lua script:**

The agent returns a sectioned plain-text dump (`<<<mem>>>`, `<<<df>>>`,
`<<<ps>>>`, ...), and a Lua script registered on the module is called back with
the parsed packet to produce the status and message. With no script configured,
`default_check_mk.lua` is loaded.

```ini
[/settings/check_mk/client/scripts]
mine = check_mk_custom.lua
```

```
check_mk_query target=linux01
CRITICAL: /var 94% used
```

Change the script, not the command line, when you want different behaviour.

**Nothing listening:**

```
check_mk_query host=127.0.0.1 port=15670
UNKNOWN: Error: Failed to connect to: 127.0.0.1:15670 :Connection refused
```

**A note on the transport:**

A stock check_mk agent speaks plain text on 6556 with access control by
source-IP allowlist rather than authentication. Treat it as unauthenticated
unless you have put TLS in front of it, and configure `ca` / `verify` /
`certificate` accordingly where you have.

See also the CheckMKServer module for the passive direction — serving check_mk
agent output *from* this host.



<a id="check_mk_query_options"></a>
#### Command-line Arguments

<a id="check_mk_query_host"></a>
<a id="check_mk_query_port"></a>
<a id="check_mk_query_address"></a>
<a id="check_mk_query_timeout"></a>
<a id="check_mk_query_target"></a>
<a id="check_mk_query_retry"></a>
<a id="check_mk_query_retries"></a>
<a id="check_mk_query_source-host"></a>
<a id="check_mk_query_sender-host"></a>
<a id="check_mk_query_command"></a>
<a id="check_mk_query_argument"></a>
<a id="check_mk_query_separator"></a>
<a id="check_mk_query_batch"></a>
<a id="check_mk_query_certificate"></a>
<a id="check_mk_query_dh"></a>
<a id="check_mk_query_certificate-key"></a>
<a id="check_mk_query_certificate-format"></a>
<a id="check_mk_query_ca"></a>
<a id="check_mk_query_verify"></a>
<a id="check_mk_query_allowed-ciphers"></a>

| Option                     | Default Value | Description                                                                           |
|----------------------------|---------------|---------------------------------------------------------------------------------------|
| host                       |               | The host of the host running the server                                               |
| port                       |               | The port of the host running the server                                               |
| address                    |               | The address (host:port) of the host running the server                                |
| timeout                    |               | Number of seconds before connection times out (default=10)                            |
| target                     |               | Target to use (lookup connection info from config)                                    |
| retry                      |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                    |               | legacy version of retry                                                               |
| source-host                |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                    |               | The name of the command that the remote daemon should run                             |
| argument                   |               | Set command line arguments                                                            |
| separator                  |               | Separator to use for the batch command (default is |)                                 |
| batch                      |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                |               | The client certificate to use                                                         |
| dh                         |               | The DH key to use                                                                     |
| certificate-key            |               | Client certificate to use                                                             |
| certificate-format         |               | Client certificate format                                                             |
| ca                         |               | Certificate authority                                                                 |
| verify                     |               | Client certificate format                                                             |
| allowed-ciphers            |               | Client certificate format                                                             |
| [ssl](#check_mk_query_ssl) | true          | Initial an ssl handshake with the server.                                             |



<h5 id="check_mk_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                                  | Description               |
|-----------------------------------------------------------------|---------------------------|
| [/settings/check_mk/client](#check-mk-client-section)           | CHECK MK CLIENT SECTION   |
| [/settings/check_mk/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/check_mk/client/scripts](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |
| [/settings/check_mk/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### CHECK MK CLIENT SECTION <a id="/settings/check_mk/client"></a>

Section for check_mk active/passive check module.

| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | CheckMK       | CHANNEL     |


```ini
# Section for check_mk active/passive check module.
[/settings/check_mk/client]
channel=CheckMK
```

#### CHANNEL <a id="/settings/check_mk/client/channel"></a>

The channel to listen to.


| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/client](#/settings/check_mk/client) |
| Key:           | channel                                                 |
| Default value: | `CheckMK`                                               |


**Sample:**

```
[/settings/check_mk/client]
# CHANNEL
channel=CheckMK
```

### CLIENT HANDLER SECTION <a id="/settings/check_mk/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/check_mk/client/scripts"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/check_mk/client/targets"></a>




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
| port                |               | TARGET PORT           |
| retries             | 3             | RETRIES               |
| timeout             | 30            | TIMEOUT               |
| use ssl             |               | ENABLE SSL ENCRYPTION |
| verify mode         |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/check_mk/client/targets/sample]
#address=...
allow host override=false
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#host=...
#port=...
retries=3
timeout=30
#use ssl=...
#verify mode=...

```





