# GraphiteClient

Graphite client can be used to submit graph data to a graphite graphing system



## List of commands

A list of all available queries (check commands)

| Command                             | Description                                       |
|-------------------------------------|---------------------------------------------------|
| [submit_graphite](#submit_graphite) | Submit information to the remote Graphite server. |




## List of Configuration


### Common Keys

| Path / Section                                                                          | Key                                                                       | Description      |
|-----------------------------------------------------------------------------------------|---------------------------------------------------------------------------|------------------|
| [/settings/graphite/client](#/settings/graphite/client)                                 | [channel](#/settings/graphite/client_channel)                             | CHANNEL          |
| [/settings/graphite/client](#/settings/graphite/client)                                 | [hostname](#/settings/graphite/client_hostname)                           | HOSTNAME         |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [address](#/settings/graphite/client/targets/default_address)             | TARGET ADDRESS   |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [path](#/settings/graphite/client/targets/default_path)                   | PATH FOR METRICS |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [retries](#/settings/graphite/client/targets/default_retries)             | RETRIES          |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [send perfdata](#/settings/graphite/client/targets/default_send perfdata) | SEND PERF DATA   |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [send status](#/settings/graphite/client/targets/default_send status)     | SEND STATUS      |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [status path](#/settings/graphite/client/targets/default_status path)     | PATH FOR STATUS  |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [timeout](#/settings/graphite/client/targets/default_timeout)             | TIMEOUT          |

### Advanced keys

| Path / Section                                                                          | Key                                                     | Description |
|-----------------------------------------------------------------------------------------|---------------------------------------------------------|-------------|
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [host](#/settings/graphite/client/targets/default_host) | TARGET HOST |
| [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) | [port](#/settings/graphite/client/targets/default_port) | TARGET PORT |

### Sample keys

| Path / Section                                                                        | Key                                                                      | Description      |
|---------------------------------------------------------------------------------------|--------------------------------------------------------------------------|------------------|
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [address](#/settings/graphite/client/targets/sample_address)             | TARGET ADDRESS   |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [host](#/settings/graphite/client/targets/sample_host)                   | TARGET HOST      |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [path](#/settings/graphite/client/targets/sample_path)                   | PATH FOR METRICS |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [port](#/settings/graphite/client/targets/sample_port)                   | TARGET PORT      |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [retries](#/settings/graphite/client/targets/sample_retries)             | RETRIES          |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [send perfdata](#/settings/graphite/client/targets/sample_send perfdata) | SEND PERF DATA   |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [send status](#/settings/graphite/client/targets/sample_send status)     | SEND STATUS      |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [status path](#/settings/graphite/client/targets/sample_status path)     | PATH FOR STATUS  |
| [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) | [timeout](#/settings/graphite/client/targets/sample_timeout)             | TIMEOUT          |



# Queries

A quick reference for all available queries (check commands) in the GraphiteClient module.

## submit_graphite

Submit information to the remote Graphite server.


### Usage


| Option                                        | Default Value | Description                                                                           |
|-----------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#submit_graphite_help)                 | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#submit_graphite_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#submit_graphite_show-default) | N/A           | Show default values for a given command                                               |
| [help-short](#submit_graphite_help-short)     | N/A           | Show help screen (short format).                                                      |
| [host](#submit_graphite_host)                 |               | The host of the host running the server                                               |
| [port](#submit_graphite_port)                 |               | The port of the host running the server                                               |
| [address](#submit_graphite_address)           |               | The address (host:port) of the host running the server                                |
| [timeout](#submit_graphite_timeout)           |               | Number of seconds before connection times out (default=10)                            |
| [target](#submit_graphite_target)             |               | Target to use (lookup connection info from config)                                    |
| [retry](#submit_graphite_retry)               |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#submit_graphite_retries)           |               | legacy version of retry                                                               |
| [source-host](#submit_graphite_source-host)   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#submit_graphite_sender-host)   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#submit_graphite_command)           |               | The name of the command that the remote daemon should run                             |
| [alias](#submit_graphite_alias)               |               | Same as command                                                                       |
| [message](#submit_graphite_message)           |               | Message                                                                               |
| [result](#submit_graphite_result)             |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| [separator](#submit_graphite_separator)       |               | Separator to use for the batch command (default is |)                                 |
| [batch](#submit_graphite_batch)               |               | Add multiple records using the separator format is: command|result|message            |
| [path](#submit_graphite_path)                 |               |                                                                                       |


<a name="submit_graphite_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="submit_graphite_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="submit_graphite_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="submit_graphite_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="submit_graphite_host"/>
### host



**Description:**
The host of the host running the server

<a name="submit_graphite_port"/>
### port



**Description:**
The port of the host running the server

<a name="submit_graphite_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="submit_graphite_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="submit_graphite_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="submit_graphite_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="submit_graphite_retries"/>
### retries



**Description:**
legacy version of retry

<a name="submit_graphite_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_graphite_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_graphite_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="submit_graphite_alias"/>
### alias



**Description:**
Same as command

<a name="submit_graphite_message"/>
### message



**Description:**
Message

<a name="submit_graphite_result"/>
### result



**Description:**
Result code either a number or OK, WARN, CRIT, UNKNOWN

<a name="submit_graphite_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="submit_graphite_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|result|message

<a name="submit_graphite_path"/>
### path



**Description:**




# Configuration

<a name="/settings/graphite/client"/>
## GRAPHITE CLIENT SECTION

Section for graphite passive check module.

```ini
# Section for graphite passive check module.
[/settings/graphite/client]
channel=GRAPHITE
hostname=auto

```


| Key                                             | Default Value | Description |
|-------------------------------------------------|---------------|-------------|
| [channel](#/settings/graphite/client_channel)   | GRAPHITE      | CHANNEL     |
| [hostname](#/settings/graphite/client_hostname) | auto          | HOSTNAME    |




<a name="/settings/graphite/client_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/graphite/client](#/settings/graphite/client) |
| Key:           | channel                                                 |
| Default value: | `GRAPHITE`                                              |
| Used by:       | GraphiteClient                                          |


#### Sample

```
[/settings/graphite/client]
# CHANNEL
channel=GRAPHITE
```


<a name="/settings/graphite/client_hostname"/>
### hostname

**HOSTNAME**

The host name of the monitored computer.
Set this to auto (default) to use the windows name of the computer.

auto	Hostname
${host}	Hostname
${host_lc}
Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/graphite/client](#/settings/graphite/client) |
| Key:           | hostname                                                |
| Default value: | `auto`                                                  |
| Used by:       | GraphiteClient                                          |


#### Sample

```
[/settings/graphite/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/graphite/client/handlers"/>
## CLIENT HANDLER SECTION



```ini
# 
[/settings/graphite/client/handlers]

```






<a name="/settings/graphite/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/graphite/client/targets]

```






<a name="/settings/graphite/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/graphite/client/targets/default]
path=system.${hostname}.${check_alias}.${perf_alias}
retries=3
send perfdata=true
send status=true
status path=system.${hostname}.${check_alias}.status
timeout=30

```


| Key                                                                       | Default Value                                   | Description      |
|---------------------------------------------------------------------------|-------------------------------------------------|------------------|
| [address](#/settings/graphite/client/targets/default_address)             |                                                 | TARGET ADDRESS   |
| [host](#/settings/graphite/client/targets/default_host)                   |                                                 | TARGET HOST      |
| [path](#/settings/graphite/client/targets/default_path)                   | system.${hostname}.${check_alias}.${perf_alias} | PATH FOR METRICS |
| [port](#/settings/graphite/client/targets/default_port)                   |                                                 | TARGET PORT      |
| [retries](#/settings/graphite/client/targets/default_retries)             | 3                                               | RETRIES          |
| [send perfdata](#/settings/graphite/client/targets/default_send perfdata) | true                                            | SEND PERF DATA   |
| [send status](#/settings/graphite/client/targets/default_send status)     | true                                            | SEND STATUS      |
| [status path](#/settings/graphite/client/targets/default_status path)     | system.${hostname}.${check_alias}.status        | PATH FOR STATUS  |
| [timeout](#/settings/graphite/client/targets/default_timeout)             | 30                                              | TIMEOUT          |




<a name="/settings/graphite/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | address                                                                                 |
| Default value: | _N/A_                                                                                   |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/graphite/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | host                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/graphite/client/targets/default_path"/>
### path

**PATH FOR METRICS**

Path mapping for metrics




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | path                                                                                    |
| Default value: | `system.${hostname}.${check_alias}.${perf_alias}`                                       |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# PATH FOR METRICS
path=system.${hostname}.${check_alias}.${perf_alias}
```


<a name="/settings/graphite/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | port                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/graphite/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | retries                                                                                 |
| Default value: | `3`                                                                                     |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/graphite/client/targets/default_send perfdata"/>
### send perfdata

**SEND PERF DATA**

Send performance data to this server




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | send perfdata                                                                           |
| Default value: | `true`                                                                                  |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# SEND PERF DATA
send perfdata=true
```


<a name="/settings/graphite/client/targets/default_send status"/>
### send status

**SEND STATUS**

Send status data to this server




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | send status                                                                             |
| Default value: | `true`                                                                                  |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# SEND STATUS
send status=true
```


<a name="/settings/graphite/client/targets/default_status path"/>
### status path

**PATH FOR STATUS**

Path mapping for status




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | status path                                                                             |
| Default value: | `system.${hostname}.${check_alias}.status`                                              |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# PATH FOR STATUS
status path=system.${hostname}.${check_alias}.status
```


<a name="/settings/graphite/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/default](#/settings/graphite/client/targets/default) |
| Key:           | timeout                                                                                 |
| Default value: | `30`                                                                                    |
| Used by:       | GraphiteClient                                                                          |


#### Sample

```
[/settings/graphite/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/graphite/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/graphite/client/targets/sample]
retries=3
timeout=30

```


| Key                                                                      | Default Value | Description      |
|--------------------------------------------------------------------------|---------------|------------------|
| [address](#/settings/graphite/client/targets/sample_address)             |               | TARGET ADDRESS   |
| [host](#/settings/graphite/client/targets/sample_host)                   |               | TARGET HOST      |
| [path](#/settings/graphite/client/targets/sample_path)                   |               | PATH FOR METRICS |
| [port](#/settings/graphite/client/targets/sample_port)                   |               | TARGET PORT      |
| [retries](#/settings/graphite/client/targets/sample_retries)             | 3             | RETRIES          |
| [send perfdata](#/settings/graphite/client/targets/sample_send perfdata) |               | SEND PERF DATA   |
| [send status](#/settings/graphite/client/targets/sample_send status)     |               | SEND STATUS      |
| [status path](#/settings/graphite/client/targets/sample_status path)     |               | PATH FOR STATUS  |
| [timeout](#/settings/graphite/client/targets/sample_timeout)             | 30            | TIMEOUT          |




<a name="/settings/graphite/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | address                                                                               |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/graphite/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | host                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/graphite/client/targets/sample_path"/>
### path

**PATH FOR METRICS**

Path mapping for metrics





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | path                                                                                  |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# PATH FOR METRICS
path=
```


<a name="/settings/graphite/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | port                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/graphite/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | retries                                                                               |
| Default value: | `3`                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/graphite/client/targets/sample_send perfdata"/>
### send perfdata

**SEND PERF DATA**

Send performance data to this server





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | send perfdata                                                                         |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# SEND PERF DATA
send perfdata=
```


<a name="/settings/graphite/client/targets/sample_send status"/>
### send status

**SEND STATUS**

Send status data to this server





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | send status                                                                           |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# SEND STATUS
send status=
```


<a name="/settings/graphite/client/targets/sample_status path"/>
### status path

**PATH FOR STATUS**

Path mapping for status





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | status path                                                                           |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# PATH FOR STATUS
status path=
```


<a name="/settings/graphite/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/graphite/client/targets/sample](#/settings/graphite/client/targets/sample) |
| Key:           | timeout                                                                               |
| Default value: | `30`                                                                                  |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | GraphiteClient                                                                        |


#### Sample

```
[/settings/graphite/client/targets/sample]
# TIMEOUT
timeout=30
```


