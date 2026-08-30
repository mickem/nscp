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

**Jump to section:**

* [Command-line Arguments](#submit_syslog_options)



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
<a id="submit_syslog_certificate"></a>
<a id="submit_syslog_dh"></a>
<a id="submit_syslog_certificate-key"></a>
<a id="submit_syslog_certificate-format"></a>
<a id="submit_syslog_ca"></a>
<a id="submit_syslog_verify"></a>
<a id="submit_syslog_allowed-ciphers"></a>
<a id="submit_syslog_ssl"></a>
<a id="submit_syslog_path"></a>
<a id="submit_syslog_transport"></a>
<a id="submit_syslog_framing"></a>
<a id="submit_syslog_tls-version"></a>
<a id="submit_syslog_insecure"></a>
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
| certificate       |               | The client certificate to use                                                         |
| dh                |               | The DH key to use                                                                     |
| certificate-key   |               | Client certificate to use                                                             |
| certificate-format |              | Client certificate format                                                             |
| ca                |               | Certificate authority                                                                 |
| verify            |               | Client certificate format                                                             |
| allowed-ciphers   |               | Client certificate format                                                             |
| ssl               |               | Initial an ssl handshake with the server.                                             |
| path              |               |                                                                                       |
| transport         |               | Transport to use: udp (default, RFC 3164), tcp (RFC 6587) or tls (RFC 5425)           |
| framing           |               | Stream framing: octet-counted (default) or non-transparent (legacy TCP receivers only) |
| tls-version       |               | The TLS version to use (1.0, 1.1, 1.2, 1.2+ or 1.3)                                   |
| insecure          |               | Allow TLS connections without verifying the server certificate (disables MITM protection) |
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


| Key                | Default Value                     | Description            |
|--------------------|-----------------------------------|------------------------|
| address            |                                   | TARGET ADDRESS         |
| allowed ciphers    | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH | ALLOWED CIPHERS        |
| ca                 | ${ca-path}                        | CA                     |
| certificate        |                                   | CLIENT CERTIFICATE     |
| certificate format | PEM                               | CERTIFICATE FORMAT     |
| certificate key    |                                   | CLIENT CERTIFICATE KEY |
| critical severity  | critical                          | CRITICAL SEVERITY      |
| facility           | kernel                            | FACILITY               |
| framing            |                                   | FRAMING                |
| host               |                                   | TARGET HOST            |
| insecure           | false                             | INSECURE               |
| message_syntax     | %message%                         | MESSAGE TEMPLATE       |
| ok severity        | informational                     | OK SEVERITY            |
| port               |                                   | TARGET PORT            |
| retries            | 3                                 | RETRIES                |
| severity           | error                             | SEVERITY               |
| tag_syntax         | NSCA                              | TAG TEMPLATE           |
| timeout            | 30                                | TIMEOUT                |
| tls version        | 1.2+                              | TLS VERSION            |
| transport          |                                   | TRANSPORT              |
| unknown severity   | emergency                         | UNKNOWN SEVERITY       |
| use ssl            | false                             | ENABLE TLS             |
| verify mode        | peer                              | VERIFY MODE            |
| warning severity   | warning                           | WARNING SEVERITY       |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/syslog/client/targets/sample]
#address=...
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
ca=${ca-path}
#certificate=...
certificate format=PEM
#certificate key=...
critical severity=critical
facility=kernel
#framing=...
#host=...
insecure=false
message_syntax=%message%
ok severity=informational
#port=...
retries=3
severity=error
tag_syntax=NSCA
timeout=30
tls version=1.2+
#transport=...
unknown severity=emergency
use ssl=false
verify mode=peer
warning severity=warning

```

The `transport` key selects how the syslog server is reached: `udp` (the
default, classic RFC 3164), `tcp` (RFC 6587, octet-counted framing by
default) or `tls` (RFC 5425 on port 6514, encrypted and authenticated).
Over TLS the server certificate is verified against `ca` by default
(`verify mode = peer`, including hostname verification); disabling
verification requires an explicit `verify mode = none` or `insecure = true`.





