# NSCANgClient

NSCA-NG client can be used both from command line and from queries to submit passive checks via NSCA-NG (TLS-based NSCA next generation)

## Enable module

To enable this module and and allow using the commands you need to ass `NSCANgClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NSCANgClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the NSCANgClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                           | Description                                                                                                                                                                                              |
|-----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [submit_nsca_ng](#submit_nsca_ng) | Submit information to the remote NSCA-NG server. Custom relay commands defined under [/settings/NSCA-NG/client/handlers] are registered automatically using the same `submit_<alias>` naming convention. |

### submit_nsca_ng

Submit information to the remote NSCA-NG server. Custom relay commands defined under [/settings/NSCA-NG/client/handlers] are registered automatically using the same `submit_<alias>` naming convention.

#### About `submit_nsca_ng`

`submit_nsca_ng` submits a passive check result to an **NSCA-ng** server.
NSCA-ng is the modern replacement for NSCA: it authenticates both ends over TLS
with a shared identity and password rather than obfuscating the payload with a
shared cipher, and it carries much larger output.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=nsca-ng`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying credentials and
TLS.

##### Identity and password

`identity` and `password` must match a client entry in the server's
`nsca-ng.cfg`. The identity is what the server uses to decide which hosts and
services this client may submit results for, so it is an authorisation
boundary — not just a label.

`host check = true` on the target submits results as **host** checks rather than
service checks, which is how you report host state through the same channel. On
the command line the equivalent is the bare flag `host-check` — it takes no
value, so `host-check=true` is an error, and because REST passes every argument
as `key=value` it cannot be set that way at all. Use the setting for anything
driven over REST.

##### TLS

The connection is TLS, configured with `certificate`, `certificate key`, `ca`,
`dh` and `allowed ciphers`.

`insecure = true` disables peer verification. It exists for bringing up a new
deployment before the CA is in place; leaving it on removes the guarantee that
you are talking to your own server, which is the main thing NSCA-ng gives you
over NSCA. Point `ca` at the server's CA instead. Like `host-check`, the
command-line form is a bare `insecure` flag and is not settable over REST.

##### Output length

`max output length` defaults to 65536 bytes — far more than NSCA's 512-byte
payload — so long check output survives intact. It still has to be no larger
than the server's own limit; a value above what the server accepts truncates on
its side.

##### Custom relay commands

Handlers defined under `[/settings/NSCA-NG/client/handlers]` are registered
automatically as additional commands, following the same `submit_<alias>`
naming, so a relay with several destinations does not need a module instance per
destination.

**Jump to section:**

* [Sample Commands](#submit_nsca_ng_samples)
* [Command-line Arguments](#submit_nsca_ng_options)


<a id="submit_nsca_ng_samples"></a>
#### Sample Commands

**Submit a passive result to an NSCA-ng server:**

```
submit_nsca_ng target=nsca-ng command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

```ini
[/settings/NSCA-NG/client/targets/nsca-ng]
address = nsca-ng://192.168.56.10:5668
identity = web01
password = <shared secret>
ca = /etc/nsclient/ca.pem
max output length = 65536
```

`identity` and `password` must match a client entry in the server's
`nsca-ng.cfg`. The identity is an authorisation boundary, not just a label — it
is what the server uses to decide which hosts and services this client may
submit results for.

**Submit a host check rather than a service check:**

The command-line option is `host-check` and it is a **bare flag** — it takes no
value, so `host-check=true` is rejected with an "Invalid command line" error:

```
submit_nsca_ng target=nsca-ng host-check result=OK "message=host is up"
OK: Message submitted
```

Because it is a flag rather than a valued option, it also **cannot be set over
REST**, which passes every argument as a `key=value` token. Set it on the target
instead, where the settings key is `host check` (the legacy alias `host_check`
is still honoured):

```ini
[/settings/NSCA-NG/client/targets/nsca-ng]
host check = true
```

**Submit several results at once:**

```
submit_nsca_ng target=nsca-ng "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = NSCA-NG
```

**Nothing listening:**

```
submit_nsca_ng host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" identity=agent1 password=secret
UNKNOWN: NSCA-NG network error: connect to 127.0.0.1:15670 failed: Connection refused
```

**Long output survives:**

`max output length` defaults to 65536 bytes, against NSCA's 512-byte payload, so
a full check message arrives intact — as long as it is also within the server's
own limit, which truncates on its side.

**`insecure` removes the point of using NSCA-ng:**

It disables peer verification, so you lose the guarantee that you are talking to
your own server. Use it only while bringing a deployment up, and point `ca` at
the server's CA instead.

Like `host-check` it is a bare flag on the command line — `insecure=true` is
rejected, and it cannot be set over REST at all:

```
submit_nsca_ng target=nsca-ng insecure command=nightly_backup result=OK "message=done"
OK: Message submitted
```

```ini
[/settings/NSCA-NG/client/targets/nsca-ng]
insecure = true
```

**Custom relay commands:**

Handlers defined under `[/settings/NSCA-NG/client/handlers]` are registered
automatically as `submit_<alias>` commands, so a relay with several destinations
does not need a module instance per destination.



<a id="submit_nsca_ng_options"></a>
#### Command-line Arguments

<a id="submit_nsca_ng_host"></a>
<a id="submit_nsca_ng_port"></a>
<a id="submit_nsca_ng_address"></a>
<a id="submit_nsca_ng_timeout"></a>
<a id="submit_nsca_ng_target"></a>
<a id="submit_nsca_ng_retry"></a>
<a id="submit_nsca_ng_retries"></a>
<a id="submit_nsca_ng_source-host"></a>
<a id="submit_nsca_ng_sender-host"></a>
<a id="submit_nsca_ng_command"></a>
<a id="submit_nsca_ng_alias"></a>
<a id="submit_nsca_ng_message"></a>
<a id="submit_nsca_ng_result"></a>
<a id="submit_nsca_ng_separator"></a>
<a id="submit_nsca_ng_batch"></a>
<a id="submit_nsca_ng_certificate"></a>
<a id="submit_nsca_ng_dh"></a>
<a id="submit_nsca_ng_certificate-key"></a>
<a id="submit_nsca_ng_certificate-format"></a>
<a id="submit_nsca_ng_ca"></a>
<a id="submit_nsca_ng_verify"></a>
<a id="submit_nsca_ng_allowed-ciphers"></a>
<a id="submit_nsca_ng_password"></a>
<a id="submit_nsca_ng_identity"></a>
<a id="submit_nsca_ng_hostname"></a>
<a id="submit_nsca_ng_no-psk"></a>
<a id="submit_nsca_ng_insecure"></a>
<a id="submit_nsca_ng_host-check"></a>
<a id="submit_nsca_ng_max-output-length"></a>

| Option                     | Default Value | Description                                                                                        |
|----------------------------|---------------|----------------------------------------------------------------------------------------------------|
| host                       |               | The host of the host running the server                                                            |
| port                       |               | The port of the host running the server                                                            |
| address                    |               | The address (host:port) of the host running the server                                             |
| timeout                    |               | Number of seconds before connection times out (default=10)                                         |
| target                     |               | Target to use (lookup connection info from config)                                                 |
| retry                      |               | Number of times ti retry a failed connection attempt (default=2)                                   |
| retries                    |               | legacy version of retry                                                                            |
| source-host                |               | Source/sender host name (default is auto which means use the name of the actual host)              |
| sender-host                |               | Source/sender host name (default is auto which means use the name of the actual host)              |
| command                    |               | The name of the command that the remote daemon should run                                          |
| alias                      |               | Same as command                                                                                    |
| message                    |               | Message                                                                                            |
| result                     |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                             |
| separator                  |               | Separator to use for the batch command (default is |)                                              |
| batch                      |               | Add multiple records using the separator format is: command|result|message                         |
| certificate                |               | The client certificate to use                                                                      |
| dh                         |               | The DH key to use                                                                                  |
| certificate-key            |               | Client certificate to use                                                                          |
| certificate-format         |               | Client certificate format                                                                          |
| ca                         |               | Certificate authority                                                                              |
| verify                     |               | Client certificate format                                                                          |
| allowed-ciphers            |               | Client certificate format                                                                          |
| [ssl](#submit_nsca_ng_ssl) | true          | Initial an ssl handshake with the server.                                                          |
| password                   |               | The PSK password (must match the NSCA-NG server configuration)                                     |
| identity                   |               | PSK identity string (defaults to hostname when empty)                                              |
| hostname                   |               | Host name to report to the NSCA-NG server                                                          |
| no-psk                     | N/A           | Disable PSK and use certificate-based TLS authentication instead                                   |
| insecure                   | N/A           | Allow TLS connections without PSK and without peer-cert verification. Disables MITM protection.    |
| host-check                 | N/A           | Submit every result as a Nagios host check (PROCESS_HOST_CHECK_RESULT) instead of a service check. |
| max-output-length          |               | Maximum bytes of plugin output forwarded over the wire (default 65536)                             |



<h5 id="submit_nsca_ng_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                                 | Description               |
|----------------------------------------------------------------|---------------------------|
| [/settings/NSCA-NG/client](#nsca-ng-client-section)            | NSCA-NG CLIENT SECTION    |
| [/settings/NSCA-NG/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NSCA-NG/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### NSCA-NG CLIENT SECTION <a id="/settings/NSCA-NG/client"></a>

Section for NSCA-NG passive check module.

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | NSCA-NG       | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |


```ini
# Section for NSCA-NG passive check module.
[/settings/NSCA-NG/client]
channel=NSCA-NG
hostname=auto
```

#### CHANNEL <a id="/settings/NSCA-NG/client/channel"></a>

The channel to listen to.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/NSCA-NG/client](#/settings/NSCA-NG/client) |
| Key:           | channel                                               |
| Default value: | `NSCA-NG`                                             |


**Sample:**

```
[/settings/NSCA-NG/client]
# CHANNEL
channel=NSCA-NG
```

#### HOSTNAME <a id="/settings/NSCA-NG/client/hostname"></a>

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



| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/NSCA-NG/client](#/settings/NSCA-NG/client) |
| Key:           | hostname                                              |
| Default value: | `auto`                                                |


**Sample:**

```
[/settings/NSCA-NG/client]
# HOSTNAME
hostname=auto
```

### CLIENT HANDLER SECTION <a id="/settings/NSCA-NG/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NSCA-NG/client/targets"></a>




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
| host check          | false         | HOST CHECK            |
| identity            |               | IDENTITY              |
| insecure            | false         | INSECURE              |
| max output length   | 65536         | MAX OUTPUT LENGTH     |
| password            |               | PASSWORD              |
| port                |               | TARGET PORT           |
| retries             | 3             | RETRIES               |
| timeout             | 30            | TIMEOUT               |
| use psk             | true          | USE PSK               |
| use ssl             |               | ENABLE SSL ENCRYPTION |
| verify mode         |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NSCA-NG/client/targets/sample]
#address=...
allow host override=false
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#host=...
host check=false
#identity=...
insecure=false
max output length=65536
#password=...
#port=...
retries=3
timeout=30
use psk=true
#use ssl=...
#verify mode=...

```





