# GraphiteClient

Graphite client can be used to submit graph data to a graphite graphing system

## Enable module

To enable this module and and allow using the commands you need to ass `GraphiteClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
GraphiteClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the GraphiteClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                             | Description                                       |
|-------------------------------------|---------------------------------------------------|
| [submit_graphite](#submit_graphite) | Submit information to the remote Graphite server. |

### submit_graphite

Submit information to the remote Graphite server.

#### About `submit_graphite`

`submit_graphite` sends a check result to a Graphite server over the plaintext
line protocol (Carbon). Unlike the Nagios-flavoured submit commands, Graphite
stores **numbers over time**, not states with messages — so what actually gets
sent is the check's performance data.

The usual way to use it is not to call it directly but to route results to it:
give a scheduled check `target=graphite` (or add `GRAPHITE` to the channels a
check reports on) and the module forwards each result as it is produced. Calling
the command by hand is mainly useful for testing that the connection and metric
paths are right.

##### Metric paths

Two settings on the module decide where the values land:
`path` for performance data and `status path` for the status value, both written
as Graphite dotted paths with the usual `${hostname}`, `${check_alias}` and
`${perf_alias}` placeholders. `send perfdata` and `send status` turn each half
on or off — sending status as a number is often not what you want, since a
Graphite dashboard renders `0/1/2/3` poorly compared to a real alerting system.

Get the path template right before pointing a fleet at it: Carbon creates a
whisper file per distinct metric path on first write, so a template that
interpolates something volatile (a PID, a timestamp, an unsanitised check name)
will litter the storage with files that then have to be cleaned up by hand.

##### Transport

Carbon's plaintext protocol is **unauthenticated**, and by default this module
speaks it in the clear. Set `ssl = true` to wrap the connection in TLS, and
supply `ca`, `certificate` and `certificate key` for a Carbon endpoint that
requires them. On an untrusted network, treat the plaintext default as
unsuitable — anyone on the path can both read and inject metrics.

**Jump to section:**

* [Sample Commands](#submit_graphite_samples)
* [Command-line Arguments](#submit_graphite_options)


<a id="submit_graphite_samples"></a>
#### Sample Commands

The examples below were sent to a Carbon listener on `127.0.0.1:2003`, with the
target configured as:

```ini
[/settings/graphite/client/targets/default]
address = 127.0.0.1:2003
path = nsclient.${hostname}.${check_alias}.${perf_alias}
status path = nsclient.${hostname}.${check_alias}.status
send perfdata = true
send status = true
```

**Submit a result directly (useful for testing the connection):**

```
submit_graphite host=127.0.0.1 port=2003 command=check_disk result=WARNING "message=/var is 91% full"
OK: Data presumably sent successfully
```

What arrives on the wire:

```
nsclient.vm..status 1 1788526945
```

Two things to notice. Only the status arrived — the submission carried no
performance data, and Graphite stores numbers, so there was nothing else to
send. And `${check_alias}` is **empty**, leaving a `..` in the path: neither
`command=` nor `alias=` populates it on this path.

**Route a real check to the target instead — which is how it is meant to be used:**

```
check_and_forward command=check_drivesize channel=GRAPHITE alias=drivesize
OK: Message submitted: GRAPHITE
```

```
nsclient.vm.drivesize./_used 9039142912 1788526958
nsclient.vm.drivesize./_used_percent 3 1788526958
nsclient.vm.drivesize./opt/claude-code_used 212594688 1788526958
nsclient.vm.drivesize./opt/claude-code_used_percent 88 1788526958
nsclient.vm.drivesize./opt/env-runner_used 31223808 1788526958
nsclient.vm.drivesize./opt/env-runner_used_percent 64 1788526958
nsclient.vm.drivesize.status 1 1788526958
```

Now `${check_alias}` is the alias the result was forwarded under, and every
performance counter becomes its own series.

**Watch out for separators in counter names:**

`${perf_alias}` is inserted verbatim, so the mount point `/opt/claude-code`
becomes `.../opt/claude-code_used` — a **dotted path split into extra levels**
by Graphite, and a whisper file created for each one. On Windows the same
happens with drive letters and colons.

Get the path template and the check's `perf-syntax` right before pointing a
fleet at it: Carbon creates a whisper file per distinct path on first write, so
anything volatile in the name has to be cleaned up by hand afterwards.

**Send only the numbers, not the status:**

Graphite renders a `0/1/2/3` status series poorly compared to a real alerting
system, so most installations turn it off:

```ini
[/settings/graphite/client/targets/default]
send status = false
```

**"Sent successfully" means handed to the socket:**

Carbon's plaintext protocol acknowledges nothing, so an OK here does not mean
the metrics were stored — only that the connection was accepted.



<a id="submit_graphite_options"></a>
#### Command-line Arguments

<a id="submit_graphite_host"></a>
<a id="submit_graphite_port"></a>
<a id="submit_graphite_address"></a>
<a id="submit_graphite_timeout"></a>
<a id="submit_graphite_target"></a>
<a id="submit_graphite_retry"></a>
<a id="submit_graphite_retries"></a>
<a id="submit_graphite_source-host"></a>
<a id="submit_graphite_sender-host"></a>
<a id="submit_graphite_command"></a>
<a id="submit_graphite_alias"></a>
<a id="submit_graphite_message"></a>
<a id="submit_graphite_result"></a>
<a id="submit_graphite_separator"></a>
<a id="submit_graphite_batch"></a>
<a id="submit_graphite_path"></a>

| Option      | Default Value | Description                                                                           |
|-------------|---------------|---------------------------------------------------------------------------------------|
| host        |               | The host of the host running the server                                               |
| port        |               | The port of the host running the server                                               |
| address     |               | The address (host:port) of the host running the server                                |
| timeout     |               | Number of seconds before connection times out (default=10)                            |
| target      |               | Target to use (lookup connection info from config)                                    |
| retry       |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries     |               | legacy version of retry                                                               |
| source-host |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command     |               | The name of the command that the remote daemon should run                             |
| alias       |               | Same as command                                                                       |
| message     |               | Message                                                                               |
| result      |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| separator   |               | Separator to use for the batch command (default is |)                                 |
| batch       |               | Add multiple records using the separator format is: command|result|message            |
| path        |               |                                                                                       |




This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                                  | Description               |
|-----------------------------------------------------------------|---------------------------|
| [/settings/graphite/client](#graphite-client-section)           | GRAPHITE CLIENT SECTION   |
| [/settings/graphite/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/graphite/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |


### GRAPHITE CLIENT SECTION <a id="/settings/graphite/client"></a>

Section for graphite passive check module.

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | GRAPHITE      | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |


```ini
# Section for graphite passive check module.
[/settings/graphite/client]
channel=GRAPHITE
hostname=auto
```

#### CHANNEL <a id="/settings/graphite/client/channel"></a>

The channel to listen to.


| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/graphite/client](#/settings/graphite/client) |
| Key:           | channel                                                 |
| Default value: | `GRAPHITE`                                              |


**Sample:**

```
[/settings/graphite/client]
# CHANNEL
channel=GRAPHITE
```

#### HOSTNAME <a id="/settings/graphite/client/hostname"></a>

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



| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/graphite/client](#/settings/graphite/client) |
| Key:           | hostname                                                |
| Default value: | `auto`                                                  |


**Sample:**

```
[/settings/graphite/client]
# HOSTNAME
hostname=auto
```

### CLIENT HANDLER SECTION <a id="/settings/graphite/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/graphite/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value                     | Description            |
|---------------------|-----------------------------------|------------------------|
| address             |                                   | TARGET ADDRESS         |
| allow host override | false                             | ALLOW HOST OVERRIDE    |
| allowed ciphers     | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH | ALLOWED CIPHERS        |
| ca                  | ${ca-path}                        | CA                     |
| certificate         |                                   | CLIENT CERTIFICATE     |
| certificate format  | PEM                               | CERTIFICATE FORMAT     |
| certificate key     |                                   | CLIENT CERTIFICATE KEY |
| host                |                                   | TARGET HOST            |
| path                |                                   | PATH FOR METRICS       |
| port                |                                   | TARGET PORT            |
| retries             | 3                                 | RETRIES                |
| send perfdata       |                                   | SEND PERF DATA         |
| send status         |                                   | SEND STATUS            |
| ssl                 | false                             | ENABLE TLS             |
| status path         |                                   | PATH FOR STATUS        |
| timeout             | 30                                | TIMEOUT                |
| tls version         | 1.2+                              | TLS VERSION            |
| verify mode         | peer                              | VERIFY MODE            |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/graphite/client/targets/sample]
#address=...
allow host override=false
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
ca=${ca-path}
#certificate=...
certificate format=PEM
#certificate key=...
#host=...
#path=...
#port=...
retries=3
#send perfdata=...
#send status=...
ssl=false
#status path=...
timeout=30
tls version=1.2+
verify mode=peer

```





