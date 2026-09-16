# GearmanClient

*Available on Linux only.*

Run checks scheduled by a Naemon or Nagios Core through Mod-Gearman: the agent connects out to a gearmand job server, grabs the checks queued for it and answers them as native NSClient++ queries. It also submits passive results into the same result queue, which is what lets a Mod-Gearman installation drop NSCA

## Enable module

To enable this module and allow using the commands you need to add `GearmanClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
GearmanClient = enabled
```

## Queries

A quick reference for all available queries (check commands) in the GearmanClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                           | Description                                                    |
|-----------------------------------|----------------------------------------------------------------|
| [submit_gearman](#submit_gearman) | Submit a passive check result into a Mod-Gearman result queue. |

### submit_gearman

Submit a passive check result into a Mod-Gearman result queue.

#### About `submit_gearman`

`submit_gearman` files a **passive** check result into a Mod-Gearman result
queue. The core's result thread reads whatever arrives on `check_results` and
files a result marked `type=passive` exactly as if it had come from the external
command file — which is all `send_gearman` (or `nagios-send-gearman`) does.

In a Mod-Gearman installation this replaces NSCA: the results travel over the
same outbound connection to the same gearmand the checks already come from, so
there is one daemon fewer to run, no inbound port and no mcrypt.

The usual way to use it is to route results rather than call it by hand. Give a
scheduled check `channel = GEARMAN`, or add `GEARMAN` to the channels a check
reports on, and each result is submitted as it is produced. A direct call is
mainly useful for verifying that the key, the queue and the host name match what
the core expects.

##### Three things must match the core

Gearman negotiates nothing, and neither does Mod-Gearman's envelope. A mismatch
is not an error on either side — the result simply never appears.

- **`key`** — the shared password from the core's `module.conf` (`key=`). At
  most 32 bytes are used, as in mod_gearman. Leave `encryption` on: with it off
  the payload is plain base64 and the core must additionally have
  `accept_clear_results=yes`.
- **`queue`** — `check_results` unless the core's `module.conf` names a
  different result queue. A result on a queue nobody reads is discarded by
  gearmand without a word.
- **`hostname`** — the name the result is filed under, which has to be the
  `host_name` the core knows this host by. It defaults to `auto`, the operating
  system's own name; set it explicitly when the core spells the host
  differently.

If results never appear on the core, check these three before anything else.

##### What the shared key is and is not

The envelope is AES-256 in ECB mode with the password used directly as the key.
It hides the content and nothing else: it does not authenticate the sender and
it does not prevent replay, so anyone holding the key — or anyone at all with
`encryption = false` — can forge results for any host the core monitors. That is
the protocol's design, and this module cannot fix it. Treat access to gearmand
and to the key as access to the monitoring data itself, and keep both on a
network you trust.

##### Naming the service

The result's `service_description` is the check's alias, or its command name
when no alias is given. The alias `host_check` is the one exception: it files a
host result, which is a result with no `service_description` at all.

##### The other half of this module

The same module also runs the checks a core queues — that is the worker half,
configured under `/settings/gearman/worker`. The two are independent
deployments: an installation that only wants the passive channel configures
`/settings/gearman/client` and leaves the worker section empty, and one that
only wants the worker does the reverse.

**Jump to section:**

* [Sample Commands](#submit_gearman_samples)
* [Command-line Arguments](#submit_gearman_options)


<a id="submit_gearman_samples"></a>
#### Sample Commands

**Submit a passive result into a Mod-Gearman result queue:**

```
submit_gearman address=192.168.56.10:4730 key=<shared secret> command=cpu result=WARNING "message=cpu is busy|'load'=80%;70;90"
Submission successful
```

The core files it under the host name this agent reports (see `hostname` below)
and the service `cpu`, as a passive check.

**Submit a host result:**

The alias `host_check` is what makes the result a host result rather than a
service one:

```
submit_gearman address=192.168.56.10:4730 key=<shared secret> alias=host_check result=CRITICAL "message=host is down"
Submission successful
```

**Submit several results at once:**

`batch=` is repeatable and each value is a `command|result|message` record:

```
submit_gearman address=192.168.56.10:4730 key=<shared secret> "batch=job_a|OK|finished in 4m" "batch=job_b|CRITICAL|exit code 1"
Submission successful
```

**The usual arrangement — route results rather than calling this by hand:**

```ini
[/settings/gearman/client]
channel = GEARMAN
; The name the core knows this host by; auto is the machine's own name.
hostname = auto

[/settings/gearman/client/targets/default]
address = 192.168.56.10:4730
key = <shared secret>
; check_results unless the core's module.conf names another result queue.
queue = check_results

[/settings/scheduler/schedules/default]
channel = GEARMAN
interval = 5m

[/settings/scheduler/schedules/cpu]
command = check_cpu "warning=load gt 80" "critical=load gt 90"
```

Every schedule then reports through the same gearmand the checks come from, and
the core files each one as a passive result for the service named after the
schedule. `check_and_forward` takes the same channel:

```
check_and_forward command=check_drivesize channel=GEARMAN alias=drivesize
Message submitted: GEARMAN
```

**When the key is missing:**

The key is the only thing separating a result this agent filed from one anybody
who can reach gearmand made up, so an encrypted submission without one is
refused rather than sent:

```
submit_gearman address=192.168.56.10:4730 command=cpu result=OK "message=all good"
No key for 192.168.56.10:4730. The key is the only thing separating a result this agent filed from one anybody who can reach gearmand made up, so an encrypted submission without one is refused. Set the target's key to the same value as the core's module.conf.
```

**Sending unencrypted has to be said out loud:**

`encryption=false` makes the payload plain base64, readable and forgeable by
anyone who can reach gearmand. It also needs the core's own
`accept_clear_results=yes`:

```
submit_gearman address=192.168.56.10:4730 encryption=false command=cpu result=OK "message=all good"
Encryption is off for 192.168.56.10:4730 but 'insecure' is not set. An unencrypted result is readable and forgeable by anyone who can reach gearmand; set insecure=true to say that is intended.
```

```
submit_gearman address=192.168.56.10:4730 encryption=false insecure=true command=cpu result=OK "message=all good"
Submission successful
```

**When gearmand is not there:**

The submission waits for gearmand's acknowledgement that the result is on the
queue, so an unreachable job server is reported rather than passing for success:

```
submit_gearman address=192.168.56.11:4730 key=<shared secret> command=cpu result=OK "message=all good"
Gearman error: Failed to connect to 192.168.56.11:4730: Connection refused
```

**When results simply never appear on the core:**

Nothing fails in that case — gearmand accepts a result for a queue nobody reads,
and the core ignores one whose `host_name` it does not know. Check, in order:
the `key` against the core's `module.conf`, the `queue` against its
`result_queue`, and `hostname` against the `host_name` in the core's object
configuration.



<a id="submit_gearman_options"></a>
#### Command-line Arguments

<a id="submit_gearman_host"></a>
<a id="submit_gearman_port"></a>
<a id="submit_gearman_address"></a>
<a id="submit_gearman_timeout"></a>
<a id="submit_gearman_target"></a>
<a id="submit_gearman_retry"></a>
<a id="submit_gearman_retries"></a>
<a id="submit_gearman_source-host"></a>
<a id="submit_gearman_sender-host"></a>
<a id="submit_gearman_command"></a>
<a id="submit_gearman_alias"></a>
<a id="submit_gearman_message"></a>
<a id="submit_gearman_result"></a>
<a id="submit_gearman_separator"></a>
<a id="submit_gearman_batch"></a>
<a id="submit_gearman_key"></a>
<a id="submit_gearman_password"></a>
<a id="submit_gearman_queue"></a>

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
| Option                                   | Default Value | Description                                                                           |
|------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| host                                     |               | The host of the host running the server                                               |
| port                                     |               | The port of the host running the server                                               |
| address                                  |               | The address (host:port) of the host running the server                                |
| timeout                                  |               | Number of seconds before connection times out (default=10)                            |
| target                                   |               | Target to use (lookup connection info from config)                                    |
| retry                                    |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                                  |               | legacy version of retry                                                               |
| source-host                              |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                              |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                                  |               | The name of the command that the remote daemon should run                             |
| alias                                    |               | Same as command                                                                       |
| message                                  |               | Message                                                                               |
| result                                   |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| separator                                |               | Separator to use for the batch command (default is |)                                 |
| batch                                    |               | Add multiple records using the separator format is: command|result|message            |
| key                                      |               | The shared key from the core's module.conf (key=)                                     |
| password                                 |               | Same as key                                                                           |
| queue                                    |               | The queue the core reads results from (default check_results)                         |
| [encryption](#submit_gearman_encryption) | true          | Whether to wrap the result in the AES-256 envelope (default true)                     |
| [insecure](#submit_gearman_insecure)     | true          | Acknowledge sending results unencrypted; required together with encryption=false      |



<h5 id="submit_gearman_encryption">encryption:</h5>

Whether to wrap the result in the AES-256 envelope (default true)

*Default Value:* `true`

<h5 id="submit_gearman_insecure">insecure:</h5>

Acknowledge sending results unencrypted; required together with encryption=false

*Default Value:* `true`


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


## Configuration

| Path / Section                                                 | Description               |
|----------------------------------------------------------------|---------------------------|
| [/settings/gearman/client](#gearman-submit-channel)            | Gearman submit channel    |
| [/settings/gearman/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/gearman/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |
| [/settings/gearman/worker](#gearman-worker)                    | Gearman worker            |


### Gearman submit channel <a id="/settings/gearman/client"></a>

Section for submitting passive results into a Mod-Gearman result queue (GearmanClient.dll).

| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | GEARMAN       | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |


```ini
# Section for submitting passive results into a Mod-Gearman result queue (GearmanClient.dll).
[/settings/gearman/client]
channel=GEARMAN
hostname=auto
```

#### CHANNEL <a id="/settings/gearman/client/channel"></a>

The channel to listen to. A Scheduler entry (or any other submitting module) naming this channel has its results pushed into the target's result queue, which is how this module replaces NSCA in a Mod-Gearman installation.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/client](#/settings/gearman/client) |
| Key:           | channel                                               |
| Default value: | `GEARMAN`                                             |


**Sample:**

```
[/settings/gearman/client]
# CHANNEL
channel=GEARMAN
```

#### HOSTNAME <a id="/settings/gearman/client/hostname"></a>

The host name results are filed under on the core, which has to be the name the core knows this host by. Set this to auto (default) to use the name of this computer.

auto	Hostname
${host}	Hostname
${host_lc}	Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase
${address_ipv4}	IPv4 address of the computer
${address_ipv6}	IPv6 address of the computer (lowercase, compressed)



| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/client](#/settings/gearman/client) |
| Key:           | hostname                                              |
| Default value: | `auto`                                                |


**Sample:**

```
[/settings/gearman/client]
# HOSTNAME
hostname=auto
```

### CLIENT HANDLER SECTION <a id="/settings/gearman/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/gearman/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                 | Default Value | Description                |
|---------------------|---------------|----------------------------|
| address             |               | TARGET ADDRESS             |
| allow host override | false         | ALLOW HOST OVERRIDE        |
| encryption          | true          | ENCRYPT PAYLOADS           |
| host                |               | TARGET HOST                |
| insecure            | false         | ALLOW UNENCRYPTED PAYLOADS |
| key                 |               | SHARED KEY                 |
| port                |               | TARGET PORT                |
| queue               | check_results | RESULT QUEUE               |
| retries             | 3             | RETRIES                    |
| timeout             | 30            | TIMEOUT                    |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/gearman/client/targets/sample]
#address=...
allow host override=false
encryption=true
#host=...
insecure=false
#key=...
#port=...
queue=check_results
retries=3
timeout=30

```






### Gearman worker <a id="/settings/gearman/worker"></a>

Section for the Mod-Gearman worker (GearmanClient.dll).

| Key                                                       | Default Value | Description                    |
|-----------------------------------------------------------|---------------|--------------------------------|
| [allow arguments](#command-argument-processing)           | true          | COMMAND ARGUMENT PROCESSING    |
| [allow nasty characters](#command-allow-nasty-meta-chars) | false         | COMMAND ALLOW NASTY META CHARS |
| [allow shared queues](#answer-the-generic-queues)         | false         | ANSWER THE GENERIC QUEUES      |
| [encryption](#encrypt-payloads)                           | true          | ENCRYPT PAYLOADS               |
| [host names](#extra-host-names)                           |               | EXTRA HOST NAMES               |
| [hostgroups](#hostgroup-queues)                           |               | HOSTGROUP QUEUES               |
| [insecure](#allow-unencrypted-payloads)                   | false         | ALLOW UNENCRYPTED PAYLOADS     |
| [key](#shared-key)                                        |               | SHARED KEY                     |
| [key file](#shared-key-file)                              |               | SHARED KEY FILE                |
| [max age](#discard-jobs-older-than)                       | 0             | DISCARD JOBS OLDER THAN        |
| [mode](#worker-mode)                                      | agent         | WORKER MODE                    |
| [server](#gearmand-servers)                               |               | GEARMAND SERVERS               |
| [servicegroups](#servicegroup-queues)                     |               | SERVICEGROUP QUEUES            |
| [timeout return](#status-on-timeout)                      | 2             | STATUS ON TIMEOUT              |
| [workers](#worker-threads)                                | 2             | WORKER THREADS                 |


```ini
# Section for the Mod-Gearman worker (GearmanClient.dll).
[/settings/gearman/worker]
allow arguments=true
allow nasty characters=false
allow shared queues=false
encryption=true
insecure=false
max age=0
mode=agent
timeout return=2
workers=2
```

#### COMMAND ARGUMENT PROCESSING <a id="/settings/gearman/worker/allow arguments"></a>

Whether a job may carry arguments. On by default, unlike NRPE: the core has already expanded $ARGn$ before the job was queued, so a check_command defined on the core is nothing but arguments and turning this off leaves only bare commands.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | allow arguments                                       |
| Default value: | `true`                                                |


**Sample:**

```
[/settings/gearman/worker]
# COMMAND ARGUMENT PROCESSING
allow arguments=true
```

#### COMMAND ALLOW NASTY META CHARS <a id="/settings/gearman/worker/allow nasty characters"></a>

Whether a job may contain nasty (as in \|\`&><'"\\[]{}) characters. Same guard and same default as NRPEServer. Note that this rejects a threshold written the Nagios way, 'warn=load>80', because '>' is in the set: write it as 'warn=load gt 80' in the core's check_command, which the filter language understands and which needs no exception here, or turn this on if the command lines cannot be changed.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | allow nasty characters                                |
| Default value: | `false`                                               |


**Sample:**

```
[/settings/gearman/worker]
# COMMAND ALLOW NASTY META CHARS
allow nasty characters=false
```

#### ANSWER THE GENERIC QUEUES <a id="/settings/gearman/worker/allow shared queues"></a>

Also register the generic 'host' and 'service' queues that every check without a group lands on. Off by default because a Windows agent registering there grabs checks meant for every other host in the installation, including the Linux ones.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | allow shared queues                                   |
| Default value: | `false`                                               |


**Sample:**

```
[/settings/gearman/worker]
# ANSWER THE GENERIC QUEUES
allow shared queues=false
```

#### ENCRYPT PAYLOADS <a id="/settings/gearman/worker/encryption"></a>

Whether jobs and results travel inside the AES-256 envelope (mod_gearman's encryption=yes). Leave this on: with it off the payloads are plain base64 and anyone who can reach gearmand can read and forge checks. Turning it off also requires 'insecure = true'.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | encryption                                            |
| Default value: | `true`                                                |


**Sample:**

```
[/settings/gearman/worker]
# ENCRYPT PAYLOADS
encryption=true
```

#### EXTRA HOST NAMES <a id="/settings/gearman/worker/host names"></a>

Comma separated extra names this agent answers for, on top of its own host name. A queue carries the checks of every host in its group and the protocol does not say which worker a job was meant for, so a job for any other name is refused. Set this when the core knows the host under a different name than the operating system does. Agent mode only: a proxy answers for every host on its queues and ignores this.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | host names                                            |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/gearman/worker]
# EXTRA HOST NAMES
host names=
```

#### HOSTGROUP QUEUES <a id="/settings/gearman/worker/hostgroups"></a>

Comma separated hostgroup names, one queue each: 'windows' registers hostgroup_windows. These have to match the hostgroups= line in the core's module.conf, which is what decides that a check goes to gearmand at all. In agent mode the usual arrangement is one hostgroup per host; a proxy takes one group for every host it monitors, and two proxies on the same group share the load and cover each other with no further configuration.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | hostgroups                                            |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/gearman/worker]
# HOSTGROUP QUEUES
hostgroups=
```

#### ALLOW UNENCRYPTED PAYLOADS <a id="/settings/gearman/worker/insecure"></a>

Acknowledge that 'encryption = false' sends and accepts check jobs with no protection at all. Without this the module refuses to start unencrypted.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | insecure                                              |
| Default value: | `false`                                               |


**Sample:**

```
[/settings/gearman/worker]
# ALLOW UNENCRYPTED PAYLOADS
insecure=false
```

#### SHARED KEY <a id="/settings/gearman/worker/key"></a>

The shared password from the core's module.conf (key=). At most 32 bytes are used, as in mod_gearman. Use 'key file' instead to keep it out of the configuration file.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | key                                                   |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/gearman/worker]
# SHARED KEY
key=
```

#### SHARED KEY FILE <a id="/settings/gearman/worker/key file"></a>

Path to a file whose first line is the shared key, like mod_gearman's keyfile=. Takes effect only when 'key' is empty. Restrict it to the account the agent runs as: anyone who can read it can inject checks into every queue this agent serves.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | key file                                              |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/gearman/worker]
# SHARED KEY FILE
key file=
```

#### DISCARD JOBS OLDER THAN <a id="/settings/gearman/worker/max age"></a>

Refuse a job whose core_time is more than this many seconds in the past, answering unknown instead of running it (0 disables). After an outage the queue holds a backlog of checks whose answers describe a moment that has passed.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | max age                                               |
| Default value: | `0`                                                   |


**Sample:**

```
[/settings/gearman/worker]
# DISCARD JOBS OLDER THAN
max age=0
```

#### WORKER MODE <a id="/settings/gearman/worker/mode"></a>

Which deployment this is, 'agent' or 'proxy'. An agent answers for itself: it runs the checks of the host it is installed on, and refuses a job for any other host (see 'host names'). A proxy answers for others: it runs every check on the queues it registered, whichever host the core meant it for, which is what a check_command naming its own target - check_nrpe host=$HOSTADDRESS$ command=check_cpu, check_wmi target=$HOSTADDRESS$ - needs (the agent reads a check's arguments as key=value or --long, not as the Nagios plugin's -H). Proxy mode is how one domain-joined Windows box monitors a whole hostgroup without an agent, or an open port, on any of them; it is also the bigger target, since anyone who can queue a job on those queues reaches everything the proxy's own credentials reach.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | mode                                                  |
| Default value: | `agent`                                               |


**Sample:**

```
[/settings/gearman/worker]
# WORKER MODE
mode=agent
```

#### GEARMAND SERVERS <a id="/settings/gearman/worker/server"></a>

Comma separated list of gearmand job servers as host or host:port (port defaults to 4730), tried in order. This is the same list mod_gearman's worker.conf gives as repeated server= lines. The agent always connects outbound, so no port is opened on this host.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | server                                                |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/gearman/worker]
# GEARMAND SERVERS
server=
```

#### SERVICEGROUP QUEUES <a id="/settings/gearman/worker/servicegroups"></a>

Comma separated servicegroup names, one queue each: 'db' registers servicegroup_db. Matches servicegroups= in the core's module.conf.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | servicegroups                                         |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/gearman/worker]
# SERVICEGROUP QUEUES
servicegroups=
```

#### STATUS ON TIMEOUT <a id="/settings/gearman/worker/timeout return"></a>

The status reported when a check does not finish inside the timeout the core put in the job: 0 ok, 1 warning, 2 critical, 3 unknown. Same meaning as mod_gearman's timeout_return.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | timeout return                                        |
| Default value: | `2`                                                   |


**Sample:**

```
[/settings/gearman/worker]
# STATUS ON TIMEOUT
timeout return=2
```

#### WORKER THREADS <a id="/settings/gearman/worker/workers"></a>

Number of worker threads, each with its own connection. Two is plenty for one host's own checks.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/gearman/worker](#/settings/gearman/worker) |
| Key:           | workers                                               |
| Default value: | `2`                                                   |


**Sample:**

```
[/settings/gearman/worker]
# WORKER THREADS
workers=2
```
