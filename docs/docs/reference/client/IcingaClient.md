# IcingaClient

Icinga 2 client submits passive check results to an Icinga 2 server via the REST API

## Enable module

To enable this module and allow using the commands you need to add `IcingaClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
IcingaClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the IcingaClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                         | Description                                       |
|---------------------------------|---------------------------------------------------|
| [submit_icinga](#submit_icinga) | Submit information to the remote Icinga 2 Server. |

### submit_icinga

Submit information to the remote Icinga 2 Server.

#### About `submit_icinga`

`submit_icinga` submits a passive check result to an **Icinga 2** server through
its REST API (`/v1/actions/process-check-result`), over HTTPS.

The usual way to use it is to route results rather than call it by hand: give a
scheduled check `target=icinga`, or add the module's channel to the channels a
check reports on, and each result is submitted as it is produced. A direct call
is mainly useful for verifying that credentials, TLS and object names are right.

##### Matching Icinga's object model

Icinga addresses results by host and service *object*, so a submission only
lands if the objects already exist. `check source` sets the source name recorded
against the result and `check command` the command name it is attributed to.

`ensure objects` makes the module create a missing host or service object before
submitting, using `host template` and `service template` as the templates to
apply. That is convenient for a fleet that registers itself, but it means an
agent can create objects in your monitoring configuration — enable it
deliberately, and give the API user only the permissions it needs.

##### Authentication and TLS

Access uses an Icinga 2 API user: `username` and `password`. TLS is on by
definition (the API is HTTPS-only); `tls version` defaults to 1.3 and `ca`
points at the bundle used to verify the server, defaulting to the system CA
path. Icinga's API certificate is usually issued by the Icinga CA rather than a
public one, so point `ca` at `/var/lib/icinga2/ca/ca.crt` (or wherever your CA
lives) rather than turning verification off.

The API listens on **5665** by default, which is the port to set unless you have
moved it.

**Jump to section:**

* [Sample Commands](#submit_icinga_samples)
* [Command-line Arguments](#submit_icinga_options)


<a id="submit_icinga_samples"></a>
#### Sample Commands

**Submit a passive result to an Icinga 2 server:**

```
submit_icinga target=icinga command=nightly_backup result=CRITICAL "message=backup failed"
OK: Message submitted
```

**A typical target:**

The Icinga 2 API listens on 5665 and is HTTPS-only. Its certificate is normally
issued by the Icinga CA rather than a public one, so point `ca` at that CA
instead of disabling verification.

```ini
[/settings/icinga/client/targets/icinga]
address = https://icinga.example.com:5665
username = nscp
password = <api password>
ca = /var/lib/icinga2/ca/ca.crt
tls version = 1.3
check source = web01
check command = passive
```

**Route results rather than calling this by hand:**

```ini
[/settings/scheduler/schedules/disk]
command = check_drivesize
interval = 5m
channel = ICINGA
```

**Submit several results at once:**

```
submit_icinga target=icinga "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
OK: Message submitted
```

**When the host or service object does not exist:**

Icinga addresses results by object, so a submission for an object it does not
know is rejected by the server:

```
submit_icinga target=icinga command=nightly_backup result=CRITICAL "message=backup failed"
UNKNOWN: Icinga error: No objects found.
```

Either create the objects in the Icinga configuration, or let the module create
them:

```ini
[/settings/icinga/client/targets/icinga]
ensure objects = true
host template = generic-host
service template = generic-service
```

Enable that deliberately — it lets an agent create objects in your monitoring
configuration, so give the API user only the permissions it needs.

**Nothing listening:**

```
submit_icinga host=127.0.0.1 port=15670 command=nightly_backup result=CRITICAL "message=backup failed" username=root password=secret
UNKNOWN: Network error: Failed to connect to 127.0.0.1:15670: Connection refused
```



<a id="submit_icinga_options"></a>
#### Command-line Arguments

<a id="submit_icinga_host"></a>
<a id="submit_icinga_port"></a>
<a id="submit_icinga_address"></a>
<a id="submit_icinga_timeout"></a>
<a id="submit_icinga_target"></a>
<a id="submit_icinga_retry"></a>
<a id="submit_icinga_retries"></a>
<a id="submit_icinga_source-host"></a>
<a id="submit_icinga_sender-host"></a>
<a id="submit_icinga_command"></a>
<a id="submit_icinga_alias"></a>
<a id="submit_icinga_message"></a>
<a id="submit_icinga_result"></a>
<a id="submit_icinga_separator"></a>
<a id="submit_icinga_batch"></a>
<a id="submit_icinga_username"></a>
<a id="submit_icinga_password"></a>
<a id="submit_icinga_hostname"></a>
<a id="submit_icinga_host-template"></a>
<a id="submit_icinga_service-template"></a>
<a id="submit_icinga_check-command"></a>
<a id="submit_icinga_check-source"></a>
<a id="submit_icinga_verify-mode"></a>
<a id="submit_icinga_ca"></a>

| Option                                          | Default Value | Description                                                                                                                                                                                                                                                                                                                                                            |
|-------------------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                                            |               | The host of the host running the server                                                                                                                                                                                                                                                                                                                                |
| port                                            |               | The port of the host running the server                                                                                                                                                                                                                                                                                                                                |
| address                                         |               | The address (host:port) of the host running the server                                                                                                                                                                                                                                                                                                                 |
| timeout                                         |               | Number of seconds before connection times out (default=10)                                                                                                                                                                                                                                                                                                             |
| target                                          |               | Target to use (lookup connection info from config)                                                                                                                                                                                                                                                                                                                     |
| retry                                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                                                                                                                                                                                                                       |
| retries                                         |               | legacy version of retry                                                                                                                                                                                                                                                                                                                                                |
| source-host                                     |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                  |
| sender-host                                     |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                  |
| command                                         |               | The name of the command that the remote daemon should run                                                                                                                                                                                                                                                                                                              |
| alias                                           |               | Same as command                                                                                                                                                                                                                                                                                                                                                        |
| message                                         |               | Message                                                                                                                                                                                                                                                                                                                                                                |
| result                                          |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                                                                                                                                                                                                                 |
| separator                                       |               | Separator to use for the batch command (default is |)                                                                                                                                                                                                                                                                                                                  |
| batch                                           |               | Add multiple records using the separator format is: command|result|message                                                                                                                                                                                                                                                                                             |
| username                                        |               | The username used to authenticate against the Icinga 2 REST API.                                                                                                                                                                                                                                                                                                       |
| password                                        |               | The password used to authenticate against the Icinga 2 REST API.                                                                                                                                                                                                                                                                                                       |
| hostname                                        |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                                                                                                                                                                                                                  |
| [ensure-objects](#submit_icinga_ensure-objects) | true          | Create missing host/service objects in Icinga 2 before submitting (true/false).                                                                                                                                                                                                                                                                                        |
| host-template                                   |               | Templates used when auto-creating host objects (default: generic-host).                                                                                                                                                                                                                                                                                                |
| service-template                                |               | Templates used when auto-creating service objects (default: generic-service).                                                                                                                                                                                                                                                                                          |
| check-command                                   |               | The check_command to set on auto-created service objects (default: dummy).                                                                                                                                                                                                                                                                                             |
| check-source                                    |               | Override for the check_source field reported to Icinga 2.                                                                                                                                                                                                                                                                                                              |
| [tls-version](#submit_icinga_tls-version)       | 1.3           | The TLS version to use 1.0, 1.1, 1.2, 1.3 or any                                                                                                                                                                                                                                                                                                                       |
| verify-mode                                     |               | Comma separated list of options: none, peer (or certificate), peer-cert, fail-if-no-cert (or fail-if-no-peer-cert, client-certificate). Any other value is rejected and the connection fails. For a self signed certificate use peer-cert and point --ca at that certificate; none disables verification entirely and sends the API credentials to an unverified peer. |
| ca                                              |               | Certificate authority to use when verifying certificates.                                                                                                                                                                                                                                                                                                              |



<h5 id="submit_icinga_ensure-objects">ensure-objects:</h5>

Create missing host/service objects in Icinga 2 before submitting (true/false).

*Default Value:* `true`

<h5 id="submit_icinga_tls-version">tls-version:</h5>

The TLS version to use 1.0, 1.1, 1.2, 1.3 or any

*Default Value:* `1.3`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                                | Description               |
|---------------------------------------------------------------|---------------------------|
| [/settings/icinga/client](#icinga-2-client)                   | Icinga 2 client           |
| [/settings/icinga/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### Icinga 2 client <a id="/settings/icinga/client"></a>

Section for Icinga 2 (Icinga REST API) passive check submission.

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | ICINGA        | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |


```ini
# Section for Icinga 2 (Icinga REST API) passive check submission.
[/settings/icinga/client]
channel=ICINGA
hostname=auto
```

#### CHANNEL <a id="/settings/icinga/client/channel"></a>

The channel to listen to.


| Key            | Description                                         |
|----------------|-----------------------------------------------------|
| Path:          | [/settings/icinga/client](#/settings/icinga/client) |
| Key:           | channel                                             |
| Default value: | `ICINGA`                                            |


**Sample:**

```
[/settings/icinga/client]
# CHANNEL
channel=ICINGA
```

#### HOSTNAME <a id="/settings/icinga/client/hostname"></a>

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
| Path:          | [/settings/icinga/client](#/settings/icinga/client) |
| Key:           | hostname                                            |
| Default value: | `auto`                                              |


**Sample:**

```
[/settings/icinga/client]
# HOSTNAME
hostname=auto
```

### REMOTE TARGET DEFINITIONS <a id="/settings/icinga/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description                 |
|---------------------|---------------|-----------------------------|
| address             |               | TARGET ADDRESS              |
| allow host override | false         | ALLOW HOST OVERRIDE         |
| ca                  | ${ca-path}    | CERTIFICATE AUTHORITY       |
| check command       |               | CHECK COMMAND               |
| check source        |               | CHECK SOURCE                |
| ensure objects      |               | ENSURE HOST/SERVICE OBJECTS |
| host                |               | TARGET HOST                 |
| host template       |               | HOST TEMPLATE               |
| password            |               | ICINGA API PASSWORD         |
| port                |               | TARGET PORT                 |
| retries             | 3             | RETRIES                     |
| service template    |               | SERVICE TEMPLATE            |
| timeout             | 30            | TIMEOUT                     |
| tls version         | 1.3           | TLS VERSION                 |
| username            |               | ICINGA API USER             |
| verify mode         | peer          | TLS PEER VERIFY MODE        |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/icinga/client/targets/sample]
#address=...
allow host override=false
ca=${ca-path}
#check command=...
#check source=...
#ensure objects=...
#host=...
#host template=...
#password=...
#port=...
retries=3
#service template=...
timeout=30
tls version=1.3
#username=...
verify mode=peer

```





