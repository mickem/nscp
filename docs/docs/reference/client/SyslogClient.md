# SyslogClient

Forward information as syslog messages to a syslog server

## Enable module

To enable this module and and allow using the commands you need to ass `SyslogClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
SyslogClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the SyslogClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                         | Description                                     |
|---------------------------------|-------------------------------------------------|
| [submit_syslog](#submit_syslog) | Submit information to the remote syslog server. |

### submit_syslog

Submit information to the remote syslog server.

#### About `submit_syslog`

`submit_syslog` sends a check result to a **syslog** server as a BSD-syslog
(RFC 3164) message over UDP, port 514 by default.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=syslog`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying that the messages
arrive and are classified as intended.

##### Mapping a check status to a syslog severity

Syslog has no notion of OK/WARNING/CRITICAL, so each status is mapped to a
severity. The defaults are deliberately loud at the top end:

| Check status | Severity        |
|--------------|-----------------|
| OK           | `informational` |
| WARNING      | `warning`       |
| CRITICAL     | `critical`      |
| UNKNOWN      | `emergency`     |

`emergency` for UNKNOWN is worth changing on most installations —
on a traditional syslog host `emerg` is broadcast to every logged-in terminal,
and a check that goes UNKNOWN because a service is restarting will do that
repeatedly. `unknown-severity=warning` (or `error`) is usually the saner choice.

`facility` (default `kernel`) sets the facility the messages are filed under.
`kernel` is another default worth changing: it collides with actual kernel
messages, and most syslog daemons treat `local0`–`local7` as the range reserved
for application use.

##### Message shape

`tag template` (default `NSCA`) is the syslog tag, and the message body comes
from the `message_syntax` setting (`message template` on the command line),
default `%message%`.

**`%message%` is the only substitution either template performs.** `%source%`
and the other placeholders used elsewhere in NSClient++ are passed through
literally here, so a template of `%source%: %message%` emits the text
`%source%: ` followed by the check output. Put the check identity in the tag, or
in the check's own message, rather than in the template.

Keep the tag short and stable — many syslog daemons and downstream parsers key
on it. Note that the default tag is `NSCA`, which is misleading in a log that
also carries real NSCA traffic; setting it to something like `nscp` is worth
doing.

##### Transport

This is **plain UDP**: no encryption, no authentication and no delivery
guarantee. Messages can be dropped silently by any hop, they are readable by
anyone on the path, and anyone who can reach the port can forge them. There is
no TLS (RFC 5425) or TCP transport option here.

Use it inside a trusted network segment, and do not treat the receiving log as
evidence that a check actually ran — a dropped datagram is indistinguishable
from a check that never fired. For anything that has to be reliable, submit
through a transport that acknowledges, such as
[NSCA-ng](NSCANgClient.md) or [NRDP](NRDPClient.md).

**Jump to section:**

* [Sample Commands](#submit_syslog_samples)
* [Command-line Arguments](#submit_syslog_options)


<a id="submit_syslog_samples"></a>
#### Sample Commands

All the examples below were sent to a listener on `127.0.0.1:5514`; the second
block of each pair is the datagram that arrived.

**Submit a single result:**

```
submit_syslog host=127.0.0.1 port=514 command=check_disk result=WARNING "message=/var is 91% full"
OK: Data presumably sent successfully
```

```
<4>Sep  4 13:01:06 vm NSCA /var is 91% full
```

Priority `4` is facility `kernel` (0) x 8 + severity `warning` (4) — the
defaults. Note the tag: it is `NSCA` out of the box, which is misleading in a
log that also carries real NSCA traffic.

**Use a sensible facility and tag:**

`local0`-`local7` is the range syslog reserves for application use; `kernel`
collides with actual kernel messages.

```
submit_syslog host=127.0.0.1 port=514 command=check_disk result=WARNING "message=/var is 91% full" facility=local0 "tag template=nscp"
OK: Data presumably sent successfully
```

```
<132>Sep  4 13:01:43 vm nscp /var is 91% full
```

**How each status maps to a severity:**

```
submit_syslog ... result=CRITICAL "message=/var is full" facility=local0
<130>Sep  4 13:01:07 vm NSCA /var is full          # local0.crit

submit_syslog ... result=OK "message=all good" facility=local0
<134>Sep  4 13:01:08 vm NSCA all good              # local0.info

submit_syslog ... result=UNKNOWN "message=no data" facility=local0
<128>Sep  4 13:01:09 vm NSCA no data               # local0.emerg
```

That last one is the default worth changing: on a traditional syslog host
`emerg` is broadcast to every logged-in terminal, and a check that flaps into
UNKNOWN will do that repeatedly.

```
submit_syslog ... result=UNKNOWN "message=no data" facility=local0 unknown-severity=warning
<132>Sep  4 13:01:10 vm NSCA no data               # local0.warning
```

**Submit several results at once:**

`batch=` is repeatable and each value is a `command|result|message` record.

```
submit_syslog host=127.0.0.1 port=514 "batch=check_a|OK|first" "batch=check_b|CRITICAL|second" facility=local0
OK: Data presumably sent successfully
```

```
<134>Sep  4 13:01:24 vm NSCA first
<130>Sep  4 13:01:24 vm NSCA second
```

Note that the record separator is `|` by default, not a comma. A malformed batch
is not rejected — `batch=check_a|OK|first,check_b|CRITICAL|second` is read as one
record whose message is `first,check_b`, and the rest is silently dropped.

**`%message%` is the only substitution in the message template:**

```
submit_syslog ... "message template=%source%: %message%" facility=local0
<132>Sep  4 13:01:44 vm nscp %source%: /var is 91% full
```

**"Sent successfully" is not delivery:**

The transport is plain UDP, so the OK means the datagram was handed to the
socket — nothing more. A dropped datagram is indistinguishable from a check that
never ran, which is why this is a poor choice for anything that has to be
reliable.

```
submit_syslog host=192.0.2.1 port=514 command=check_ok result=OK "message=nobody is listening"
OK: Data presumably sent successfully
```



<a id="submit_syslog_options"></a>
#### Command-line Arguments

<a id="submit_syslog_host"></a>
<a id="submit_syslog_port"></a>
<a id="submit_syslog_address"></a>
<a id="submit_syslog_timeout"></a>
<a id="submit_syslog_target"></a>
<a id="submit_syslog_retry"></a>
<a id="submit_syslog_retries"></a>
<a id="submit_syslog_source-host"></a>
<a id="submit_syslog_sender-host"></a>
<a id="submit_syslog_command"></a>
<a id="submit_syslog_alias"></a>
<a id="submit_syslog_message"></a>
<a id="submit_syslog_result"></a>
<a id="submit_syslog_separator"></a>
<a id="submit_syslog_batch"></a>
<a id="submit_syslog_path"></a>
<a id="submit_syslog_severity"></a>
<a id="submit_syslog_unknown-severity"></a>
<a id="submit_syslog_ok-severity"></a>
<a id="submit_syslog_warning-severity"></a>
<a id="submit_syslog_critical-severity"></a>
<a id="submit_syslog_facility"></a>
<a id="submit_syslog_tag template"></a>
<a id="submit_syslog_message template"></a>

| Option            | Default Value | Description                                                                           |
|-------------------|---------------|---------------------------------------------------------------------------------------|
| host              |               | The host of the host running the server                                               |
| port              |               | The port of the host running the server                                               |
| address           |               | The address (host:port) of the host running the server                                |
| timeout           |               | Number of seconds before connection times out (default=10)                            |
| target            |               | Target to use (lookup connection info from config)                                    |
| retry             |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries           |               | legacy version of retry                                                               |
| source-host       |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host       |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command           |               | The name of the command that the remote daemon should run                             |
| alias             |               | Same as command                                                                       |
| message           |               | Message                                                                               |
| result            |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| separator         |               | Separator to use for the batch command (default is |)                                 |
| batch             |               | Add multiple records using the separator format is: command|result|message            |
| path              |               |                                                                                       |
| severity          |               | Severity of error message                                                             |
| unknown-severity  |               | Severity to use when the check result is UNKNOWN                                      |
| ok-severity       |               | Severity to use when the check result is OK                                           |
| warning-severity  |               | Severity to use when the check result is WARNING                                      |
| critical-severity |               | Severity to use when the check result is CRITICAL                                     |
| facility          |               | Facility of error message                                                             |
| tag template      |               | Tag template (TODO)                                                                   |
| message template  |               | Message template (TODO)                                                               |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                                | Description               |
|---------------------------------------------------------------|---------------------------|
| [/settings/syslog/client](#syslog-client-section)             | SYSLOG CLIENT SECTION     |
| [/settings/syslog/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/syslog/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### SYSLOG CLIENT SECTION <a id="/settings/syslog/client"></a>

Section for SYSLOG passive check module.

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | syslog        | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |


```ini
# Section for SYSLOG passive check module.
[/settings/syslog/client]
channel=syslog
hostname=auto
```

#### CHANNEL <a id="/settings/syslog/client/channel"></a>

The channel to listen to.


| Key            | Description                                         |
|----------------|-----------------------------------------------------|
| Path:          | [/settings/syslog/client](#/settings/syslog/client) |
| Key:           | channel                                             |
| Default value: | `syslog`                                            |


**Sample:**

```
[/settings/syslog/client]
# CHANNEL
channel=syslog
```

#### HOSTNAME <a id="/settings/syslog/client/hostname"></a>

The host name of the monitored computer.
Set this to auto (default) to use the windows name of the computer.

auto	Hostname
${host}	Hostname
${host_lc}	Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase
${address_ipv4}	IPv4 address of the computer
${address_ipv6}	IPv6 address of the computer (lowercase, compressed)
${address_ipv6_lc}	IPv6 address in lowercase (compressed)
${address_ipv6_uc}	IPv6 address in uppercase (compressed)
${address_ipv6_lc_comp}	IPv6 address in lowercase, compressed (2001:db8::7)
${address_ipv6_lc_uncomp}	IPv6 address in lowercase, uncompressed (2001:0db8:0000:0000:0000:0000:0000:0007)
${address_ipv6_uc_comp}	IPv6 address in uppercase, compressed
${address_ipv6_uc_uncomp}	IPv6 address in uppercase, uncompressed



| Key            | Description                                         |
|----------------|-----------------------------------------------------|
| Path:          | [/settings/syslog/client](#/settings/syslog/client) |
| Key:           | hostname                                            |
| Default value: | `auto`                                              |


**Sample:**

```
[/settings/syslog/client]
# HOSTNAME
hostname=auto
```

### CLIENT HANDLER SECTION <a id="/settings/syslog/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/syslog/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description         |
|---------------------|---------------|---------------------|
| address             |               | TARGET ADDRESS      |
| allow host override | false         | ALLOW HOST OVERRIDE |
| critical severity   | critical      | TODO                |
| facility            | kernel        | TODO                |
| host                |               | TARGET HOST         |
| message_syntax      | %message%     | TODO                |
| ok severity         | informational | TODO                |
| port                |               | TARGET PORT         |
| retries             | 3             | RETRIES             |
| severity            | error         | TODO                |
| tag_syntax          | NSCA          | TODO                |
| timeout             | 30            | TIMEOUT             |
| unknown severity    | emergency     | TODO                |
| warning severity    | warning       | TODO                |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/syslog/client/targets/sample]
#address=...
allow host override=false
critical severity=critical
facility=kernel
#host=...
message_syntax=%message%
ok severity=informational
#port=...
retries=3
severity=error
tag_syntax=NSCA
timeout=30
unknown severity=emergency
warning severity=warning

```





