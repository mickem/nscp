# SyslogClient

Forward information as syslog messages to a syslog server



## List of commands

A list of all available queries (check commands)

| Command                         | Description                                     |
|---------------------------------|-------------------------------------------------|
| [submit_syslog](#submit_syslog) | Submit information to the remote syslog server. |




## List of Configuration


### Common Keys

| Path / Section                                                                      | Key                                                         | Description    |
|-------------------------------------------------------------------------------------|-------------------------------------------------------------|----------------|
| [/settings/syslog/client](#/settings/syslog/client)                                 | [channel](#/settings/syslog/client_channel)                 | CHANNEL        |
| [/settings/syslog/client](#/settings/syslog/client)                                 | [hostname](#/settings/syslog/client_hostname)               | HOSTNAME       |
| [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) | [address](#/settings/syslog/client/targets/default_address) | TARGET ADDRESS |
| [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) | [retries](#/settings/syslog/client/targets/default_retries) | RETRIES        |
| [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) | [timeout](#/settings/syslog/client/targets/default_timeout) | TIMEOUT        |

### Advanced keys

| Path / Section                                                                      | Key                                                   | Description |
|-------------------------------------------------------------------------------------|-------------------------------------------------------|-------------|
| [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) | [host](#/settings/syslog/client/targets/default_host) | TARGET HOST |
| [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) | [port](#/settings/syslog/client/targets/default_port) | TARGET PORT |

### Sample keys

| Path / Section                                                                    | Key                                                        | Description    |
|-----------------------------------------------------------------------------------|------------------------------------------------------------|----------------|
| [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) | [address](#/settings/syslog/client/targets/sample_address) | TARGET ADDRESS |
| [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) | [host](#/settings/syslog/client/targets/sample_host)       | TARGET HOST    |
| [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) | [port](#/settings/syslog/client/targets/sample_port)       | TARGET PORT    |
| [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) | [retries](#/settings/syslog/client/targets/sample_retries) | RETRIES        |
| [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) | [timeout](#/settings/syslog/client/targets/sample_timeout) | TIMEOUT        |



# Queries

A quick reference for all available queries (check commands) in the SyslogClient module.

## submit_syslog

Submit information to the remote syslog server.


### Usage


| Option                                                | Default Value | Description                                                                           |
|-------------------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#submit_syslog_help)                           | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#submit_syslog_help-pb)                     | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#submit_syslog_show-default)           | N/A           | Show default values for a given command                                               |
| [help-short](#submit_syslog_help-short)               | N/A           | Show help screen (short format).                                                      |
| [host](#submit_syslog_host)                           |               | The host of the host running the server                                               |
| [port](#submit_syslog_port)                           |               | The port of the host running the server                                               |
| [address](#submit_syslog_address)                     |               | The address (host:port) of the host running the server                                |
| [timeout](#submit_syslog_timeout)                     |               | Number of seconds before connection times out (default=10)                            |
| [target](#submit_syslog_target)                       |               | Target to use (lookup connection info from config)                                    |
| [retry](#submit_syslog_retry)                         |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#submit_syslog_retries)                     |               | legacy version of retry                                                               |
| [source-host](#submit_syslog_source-host)             |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#submit_syslog_sender-host)             |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#submit_syslog_command)                     |               | The name of the command that the remote daemon should run                             |
| [alias](#submit_syslog_alias)                         |               | Same as command                                                                       |
| [message](#submit_syslog_message)                     |               | Message                                                                               |
| [result](#submit_syslog_result)                       |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| [separator](#submit_syslog_separator)                 |               | Separator to use for the batch command (default is |)                                 |
| [batch](#submit_syslog_batch)                         |               | Add multiple records using the separator format is: command|result|message            |
| [path](#submit_syslog_path)                           |               |                                                                                       |
| [severity](#submit_syslog_severity)                   |               | Severity of error message                                                             |
| [unknown-severity](#submit_syslog_unknown-severity)   |               | Severity of error message                                                             |
| [ok-severity](#submit_syslog_ok-severity)             |               | Severity of error message                                                             |
| [warning-severity](#submit_syslog_warning-severity)   |               | Severity of error message                                                             |
| [critical-severity](#submit_syslog_critical-severity) |               | Severity of error message                                                             |
| [facility](#submit_syslog_facility)                   |               | Facility of error message                                                             |
| [tag template](#submit_syslog_tag template)           |               | Tag template (TODO)                                                                   |
| [message template](#submit_syslog_message template)   |               | Message template (TODO)                                                               |


<a name="submit_syslog_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="submit_syslog_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="submit_syslog_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="submit_syslog_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="submit_syslog_host"/>
### host



**Description:**
The host of the host running the server

<a name="submit_syslog_port"/>
### port



**Description:**
The port of the host running the server

<a name="submit_syslog_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="submit_syslog_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="submit_syslog_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="submit_syslog_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="submit_syslog_retries"/>
### retries



**Description:**
legacy version of retry

<a name="submit_syslog_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_syslog_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_syslog_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="submit_syslog_alias"/>
### alias



**Description:**
Same as command

<a name="submit_syslog_message"/>
### message



**Description:**
Message

<a name="submit_syslog_result"/>
### result



**Description:**
Result code either a number or OK, WARN, CRIT, UNKNOWN

<a name="submit_syslog_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="submit_syslog_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|result|message

<a name="submit_syslog_path"/>
### path



**Description:**


<a name="submit_syslog_severity"/>
### severity



**Description:**
Severity of error message

<a name="submit_syslog_unknown-severity"/>
### unknown-severity



**Description:**
Severity of error message

<a name="submit_syslog_ok-severity"/>
### ok-severity



**Description:**
Severity of error message

<a name="submit_syslog_warning-severity"/>
### warning-severity



**Description:**
Severity of error message

<a name="submit_syslog_critical-severity"/>
### critical-severity



**Description:**
Severity of error message

<a name="submit_syslog_facility"/>
### facility



**Description:**
Facility of error message

<a name="submit_syslog_tag template"/>
### tag template



**Description:**
Tag template (TODO)

<a name="submit_syslog_message template"/>
### message template



**Description:**
Message template (TODO)



# Configuration

<a name="/settings/syslog/client"/>
## SYSLOG CLIENT SECTION

Section for SYSLOG passive check module.

```ini
# Section for SYSLOG passive check module.
[/settings/syslog/client]
channel=syslog
hostname=auto

```


| Key                                           | Default Value | Description |
|-----------------------------------------------|---------------|-------------|
| [channel](#/settings/syslog/client_channel)   | syslog        | CHANNEL     |
| [hostname](#/settings/syslog/client_hostname) | auto          | HOSTNAME    |




<a name="/settings/syslog/client_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                         |
|----------------|-----------------------------------------------------|
| Path:          | [/settings/syslog/client](#/settings/syslog/client) |
| Key:           | channel                                             |
| Default value: | `syslog`                                            |
| Used by:       | SyslogClient                                        |


#### Sample

```
[/settings/syslog/client]
# CHANNEL
channel=syslog
```


<a name="/settings/syslog/client_hostname"/>
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





| Key            | Description                                         |
|----------------|-----------------------------------------------------|
| Path:          | [/settings/syslog/client](#/settings/syslog/client) |
| Key:           | hostname                                            |
| Default value: | `auto`                                              |
| Used by:       | SyslogClient                                        |


#### Sample

```
[/settings/syslog/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/syslog/client/handlers"/>
## CLIENT HANDLER SECTION



```ini
# 
[/settings/syslog/client/handlers]

```






<a name="/settings/syslog/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/syslog/client/targets]

```






<a name="/settings/syslog/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/syslog/client/targets/default]
retries=3
timeout=30

```


| Key                                                         | Default Value | Description    |
|-------------------------------------------------------------|---------------|----------------|
| [address](#/settings/syslog/client/targets/default_address) |               | TARGET ADDRESS |
| [host](#/settings/syslog/client/targets/default_host)       |               | TARGET HOST    |
| [port](#/settings/syslog/client/targets/default_port)       |               | TARGET PORT    |
| [retries](#/settings/syslog/client/targets/default_retries) | 3             | RETRIES        |
| [timeout](#/settings/syslog/client/targets/default_timeout) | 30            | TIMEOUT        |




<a name="/settings/syslog/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) |
| Key:           | address                                                                             |
| Default value: | _N/A_                                                                               |
| Used by:       | SyslogClient                                                                        |


#### Sample

```
[/settings/syslog/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/syslog/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) |
| Key:           | host                                                                                |
| Advanced:      | Yes (means it is not commonly used)                                                 |
| Default value: | _N/A_                                                                               |
| Used by:       | SyslogClient                                                                        |


#### Sample

```
[/settings/syslog/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/syslog/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) |
| Key:           | port                                                                                |
| Advanced:      | Yes (means it is not commonly used)                                                 |
| Default value: | _N/A_                                                                               |
| Used by:       | SyslogClient                                                                        |


#### Sample

```
[/settings/syslog/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/syslog/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) |
| Key:           | retries                                                                             |
| Default value: | `3`                                                                                 |
| Used by:       | SyslogClient                                                                        |


#### Sample

```
[/settings/syslog/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/syslog/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                         |
|----------------|-------------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/default](#/settings/syslog/client/targets/default) |
| Key:           | timeout                                                                             |
| Default value: | `30`                                                                                |
| Used by:       | SyslogClient                                                                        |


#### Sample

```
[/settings/syslog/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/syslog/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/syslog/client/targets/sample]
retries=3
timeout=30

```


| Key                                                        | Default Value | Description    |
|------------------------------------------------------------|---------------|----------------|
| [address](#/settings/syslog/client/targets/sample_address) |               | TARGET ADDRESS |
| [host](#/settings/syslog/client/targets/sample_host)       |               | TARGET HOST    |
| [port](#/settings/syslog/client/targets/sample_port)       |               | TARGET PORT    |
| [retries](#/settings/syslog/client/targets/sample_retries) | 3             | RETRIES        |
| [timeout](#/settings/syslog/client/targets/sample_timeout) | 30            | TIMEOUT        |




<a name="/settings/syslog/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                       |
|----------------|-----------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) |
| Key:           | address                                                                           |
| Default value: | _N/A_                                                                             |
| Sample key:    | Yes (This section is only to show how this key is used)                           |
| Used by:       | SyslogClient                                                                      |


#### Sample

```
[/settings/syslog/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/syslog/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                       |
|----------------|-----------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) |
| Key:           | host                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                               |
| Default value: | _N/A_                                                                             |
| Sample key:    | Yes (This section is only to show how this key is used)                           |
| Used by:       | SyslogClient                                                                      |


#### Sample

```
[/settings/syslog/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/syslog/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                       |
|----------------|-----------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) |
| Key:           | port                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                               |
| Default value: | _N/A_                                                                             |
| Sample key:    | Yes (This section is only to show how this key is used)                           |
| Used by:       | SyslogClient                                                                      |


#### Sample

```
[/settings/syslog/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/syslog/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                       |
|----------------|-----------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) |
| Key:           | retries                                                                           |
| Default value: | `3`                                                                               |
| Sample key:    | Yes (This section is only to show how this key is used)                           |
| Used by:       | SyslogClient                                                                      |


#### Sample

```
[/settings/syslog/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/syslog/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                       |
|----------------|-----------------------------------------------------------------------------------|
| Path:          | [/settings/syslog/client/targets/sample](#/settings/syslog/client/targets/sample) |
| Key:           | timeout                                                                           |
| Default value: | `30`                                                                              |
| Sample key:    | Yes (This section is only to show how this key is used)                           |
| Used by:       | SyslogClient                                                                      |


#### Sample

```
[/settings/syslog/client/targets/sample]
# TIMEOUT
timeout=30
```


