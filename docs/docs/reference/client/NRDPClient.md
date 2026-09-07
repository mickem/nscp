# NRDPClient

NRDP client can be used both from command line and from queries to check remote systems via NRDP

## Enable module

To enable this module and and allow using the commands you need to ass `NRDPClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NRDPClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the NRDPClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_nrdp](#submit_nrdp) | Submit information to the remote NRDP Server. |

### submit_nrdp

Submit information to the remote NRDP Server.

#### About `submit_nrdp`

`submit_nrdp` submits a passive check result to a **Nagios Remote Data
Processor** endpoint — the HTTP(S) submission API used by Nagios XI and by NRDP
installations in front of Nagios Core.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=nrdp`, or add the module's channel to the channels a
check reports on. A direct call is mainly useful for verifying that the token,
URL and TLS settings are right.

##### The token

NRDP authenticates with a **shared token**, configured as `token` (`key` and
`password` are accepted as synonyms). It is the only credential, it is sent with
every submission, and it is per-endpoint rather than per-host — so treat it the
way you would a password: keep it out of command lines that end up in process
listings or logs, and put it in the module's settings instead.

##### Transport

Submissions go over HTTP or HTTPS depending on the configured target. Since the
token travels with every request, **use HTTPS**: `verify mode` defaults to
`peer` and `tls version` to 1.3, with `ca` pointing at the bundle used to verify
the server. If the NRDP endpoint sits behind a corporate proxy, `proxy` sets the
proxy URL and `no proxy` the list of hosts to reach directly.

Results are batched into one submission where several arrive together, so a
scheduler reporting many checks at once produces one HTTP round trip rather than
one per check.

**Jump to section:**

* [Sample Commands](#submit_nrdp_samples)
* [Command-line Arguments](#submit_nrdp_options)


<a id="submit_nrdp_samples"></a>
#### Sample Commands

**Submit a passive result to an NRDP endpoint:**

```
submit_nrdp target=nrdp command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

Keep the token in the settings rather than on the command line — it is the only
credential NRDP has, and a command line ends up in process listings and logs.

```ini
[/settings/NRDP/client/targets/nrdp]
address = https://nagios.example.com/nrdp/
token = <security token>
verify mode = peer
tls version = 1.3
ca = /etc/ssl/certs/ca-certificates.crt
```

**Submit several results at once:**

Results that arrive together are batched into one HTTP round trip rather than
one request per check.

```
submit_nrdp target=nrdp "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = NRDP
```

**Through a corporate proxy:**

```ini
[/settings/NRDP/client/targets/nrdp]
proxy = http://proxy.example.com:3128
no proxy = localhost,127.0.0.1,.internal.example.com
```

**Nothing listening:**

```
submit_nrdp host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" token=secret
UNKNOWN: Error: Failed to connect to 127.0.0.1:15670: Connection refused
```

**Use HTTPS:**

The token is sent with every submission, so plain HTTP puts it on the wire in
the clear. `verify mode` defaults to `peer` and `tls version` to 1.3; point `ca`
at the bundle that signs the endpoint's certificate rather than turning
verification off.



<a id="submit_nrdp_options"></a>
#### Command-line Arguments

<a id="submit_nrdp_host"></a>
<a id="submit_nrdp_port"></a>
<a id="submit_nrdp_address"></a>
<a id="submit_nrdp_timeout"></a>
<a id="submit_nrdp_target"></a>
<a id="submit_nrdp_retry"></a>
<a id="submit_nrdp_retries"></a>
<a id="submit_nrdp_source-host"></a>
<a id="submit_nrdp_sender-host"></a>
<a id="submit_nrdp_command"></a>
<a id="submit_nrdp_alias"></a>
<a id="submit_nrdp_message"></a>
<a id="submit_nrdp_result"></a>
<a id="submit_nrdp_separator"></a>
<a id="submit_nrdp_batch"></a>
<a id="submit_nrdp_key"></a>
<a id="submit_nrdp_password"></a>
<a id="submit_nrdp_token"></a>
<a id="submit_nrdp_tls-version"></a>
<a id="submit_nrdp_tls version"></a>
<a id="submit_nrdp_verify"></a>
<a id="submit_nrdp_verify-mode"></a>
<a id="submit_nrdp_verify mode"></a>
<a id="submit_nrdp_ca"></a>
<a id="submit_nrdp_proxy"></a>
<a id="submit_nrdp_no-proxy"></a>

| Option      | Default Value | Description                                                                                                                                                                                                                                                                                                 |
|-------------|---------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host        |               | The host of the host running the server                                                                                                                                                                                                                                                                     |
| port        |               | The port of the host running the server                                                                                                                                                                                                                                                                     |
| address     |               | The address (host:port) of the host running the server                                                                                                                                                                                                                                                      |
| timeout     |               | Number of seconds before connection times out (default=10)                                                                                                                                                                                                                                                  |
| target      |               | Target to use (lookup connection info from config)                                                                                                                                                                                                                                                          |
| retry       |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                                                                                                                                                            |
| retries     |               | legacy version of retry                                                                                                                                                                                                                                                                                     |
| source-host |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                       |
| sender-host |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                       |
| command     |               | The name of the command that the remote daemon should run                                                                                                                                                                                                                                                   |
| alias       |               | Same as command                                                                                                                                                                                                                                                                                             |
| message     |               | Message                                                                                                                                                                                                                                                                                                     |
| result      |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                                                                                                                                                      |
| separator   |               | Separator to use for the batch command (default is |)                                                                                                                                                                                                                                                       |
| batch       |               | Add multiple records using the separator format is: command|result|message                                                                                                                                                                                                                                  |
| key         |               | The security token                                                                                                                                                                                                                                                                                          |
| password    |               | The security token                                                                                                                                                                                                                                                                                          |
| token       |               | The security token                                                                                                                                                                                                                                                                                          |
| tls-version |               | The tls version to use: an exact version (1.0, 1.1, 1.2, 1.3) allows only that version, a trailing + (e.g. 1.2+) means that version or later, and any accepts whatever both sides support.                                                                                                                  |
| tls version |               | Legacy alias for --tls-version (kept for backwards compatibility).                                                                                                                                                                                                                                          |
| verify      |               | Comma separated list of options: none, peer (or certificate), peer-cert, fail-if-no-cert (or fail-if-no-peer-cert, client-certificate). For a self signed certificate use peer-cert and point --ca at that certificate; none disables verification entirely and sends the NRDP token to an unverified peer. |
| verify-mode |               | Alias for --verify.                                                                                                                                                                                                                                                                                         |
| verify mode |               | Legacy alias for --verify (kept for backwards compatibility).                                                                                                                                                                                                                                               |
| ca          |               | Certificate authority to use when verifying certificates.                                                                                                                                                                                                                                                   |
| proxy       |               | HTTP proxy URL to route requests through (e.g. http://user:pass@proxy:3128/).                                                                                                                                                                                                                               |
| no-proxy    |               | Comma-separated list of hostnames that bypass the proxy.                                                                                                                                                                                                                                                    |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NRDP/client](#smtp-client-section)               | SMTP CLIENT SECTION       |
| [/settings/NRDP/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NRDP/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### SMTP CLIENT SECTION <a id="/settings/NRDP/client"></a>

Section for SMTP passive check module.

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | NRDP          | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |


```ini
# Section for SMTP passive check module.
[/settings/NRDP/client]
channel=NRDP
hostname=auto
```

#### CHANNEL <a id="/settings/NRDP/client/channel"></a>

The channel to listen to.


| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRDP/client](#/settings/NRDP/client) |
| Key:           | channel                                         |
| Default value: | `NRDP`                                          |


**Sample:**

```
[/settings/NRDP/client]
# CHANNEL
channel=NRDP
```

#### HOSTNAME <a id="/settings/NRDP/client/hostname"></a>

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



| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRDP/client](#/settings/NRDP/client) |
| Key:           | hostname                                        |
| Default value: | `auto`                                          |


**Sample:**

```
[/settings/NRDP/client]
# HOSTNAME
hostname=auto
```

### CLIENT HANDLER SECTION <a id="/settings/NRDP/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NRDP/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description           |
|---------------------|---------------|-----------------------|
| address             |               | TARGET ADDRESS        |
| allow host override | false         | ALLOW HOST OVERRIDE   |
| ca                  | ${ca-path}    | Certificate Authority |
| host                |               | TARGET HOST           |
| key                 |               | SECURITY TOKEN        |
| no proxy            |               | No-proxy list         |
| password            |               | SECURITY TOKEN        |
| port                |               | TARGET PORT           |
| proxy               |               | HTTP proxy URL        |
| retries             | 3             | RETRIES               |
| timeout             | 30            | TIMEOUT               |
| tls version         | 1.3           | Tls version           |
| token               |               | SECURITY TOKEN        |
| verify mode         | peer          | TLS peer verify mode  |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NRDP/client/targets/sample]
#address=...
allow host override=false
ca=${ca-path}
#host=...
#key=...
#no proxy=...
#password=...
#port=...
#proxy=...
retries=3
timeout=30
tls version=1.3
#token=...
verify mode=peer

```





