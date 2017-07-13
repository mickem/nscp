# SMTPClient

SMTP client can be used both from command line and from queries to check remote systes via SMTP



## List of commands

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_smtp](#submit_smtp) | Submit information to the remote SMTP server. |




## List of Configuration


### Common Keys

| Path / Section                                                                  | Key                                                       | Description    |
|---------------------------------------------------------------------------------|-----------------------------------------------------------|----------------|
| [/settings/SMTP/client](#/settings/SMTP/client)                                 | [channel](#/settings/SMTP/client_channel)                 | CHANNEL        |
| [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) | [address](#/settings/SMTP/client/targets/default_address) | TARGET ADDRESS |
| [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) | [retries](#/settings/SMTP/client/targets/default_retries) | RETRIES        |
| [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) | [timeout](#/settings/SMTP/client/targets/default_timeout) | TIMEOUT        |

### Advanced keys

| Path / Section                                                                  | Key                                                 | Description |
|---------------------------------------------------------------------------------|-----------------------------------------------------|-------------|
| [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) | [host](#/settings/SMTP/client/targets/default_host) | TARGET HOST |
| [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) | [port](#/settings/SMTP/client/targets/default_port) | TARGET PORT |

### Sample keys

| Path / Section                                                                | Key                                                      | Description    |
|-------------------------------------------------------------------------------|----------------------------------------------------------|----------------|
| [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) | [address](#/settings/SMTP/client/targets/sample_address) | TARGET ADDRESS |
| [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) | [host](#/settings/SMTP/client/targets/sample_host)       | TARGET HOST    |
| [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) | [port](#/settings/SMTP/client/targets/sample_port)       | TARGET PORT    |
| [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) | [retries](#/settings/SMTP/client/targets/sample_retries) | RETRIES        |
| [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) | [timeout](#/settings/SMTP/client/targets/sample_timeout) | TIMEOUT        |



# Queries

A quick reference for all available queries (check commands) in the SMTPClient module.

## submit_smtp

Submit information to the remote SMTP server.


### Usage


| Option                                    | Default Value | Description                                                                           |
|-------------------------------------------|---------------|---------------------------------------------------------------------------------------|
| [help](#submit_smtp_help)                 | N/A           | Show help screen (this screen)                                                        |
| [help-pb](#submit_smtp_help-pb)           | N/A           | Show help screen as a protocol buffer payload                                         |
| [show-default](#submit_smtp_show-default) | N/A           | Show default values for a given command                                               |
| [help-short](#submit_smtp_help-short)     | N/A           | Show help screen (short format).                                                      |
| [host](#submit_smtp_host)                 |               | The host of the host running the server                                               |
| [port](#submit_smtp_port)                 |               | The port of the host running the server                                               |
| [address](#submit_smtp_address)           |               | The address (host:port) of the host running the server                                |
| [timeout](#submit_smtp_timeout)           |               | Number of seconds before connection times out (default=10)                            |
| [target](#submit_smtp_target)             |               | Target to use (lookup connection info from config)                                    |
| [retry](#submit_smtp_retry)               |               | Number of times ti retry a failed connection attempt (default=2)                      |
| [retries](#submit_smtp_retries)           |               | legacy version of retry                                                               |
| [source-host](#submit_smtp_source-host)   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#submit_smtp_sender-host)   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [command](#submit_smtp_command)           |               | The name of the command that the remote daemon should run                             |
| [alias](#submit_smtp_alias)               |               | Same as command                                                                       |
| [message](#submit_smtp_message)           |               | Message                                                                               |
| [result](#submit_smtp_result)             |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| [separator](#submit_smtp_separator)       |               | Separator to use for the batch command (default is |)                                 |
| [batch](#submit_smtp_batch)               |               | Add multiple records using the separator format is: command|result|message            |
| [sender](#submit_smtp_sender)             |               | Length of payload (has to be same as on the server)                                   |
| [recipient](#submit_smtp_recipient)       |               | Length of payload (has to be same as on the server)                                   |
| [template](#submit_smtp_template)         |               | Do not initial an ssl handshake with the server, talk in plain text.                  |
| [source-host](#submit_smtp_source-host)   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| [sender-host](#submit_smtp_sender-host)   |               | Source/sender host name (default is auto which means use the name of the actual host) |


<a name="submit_smtp_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="submit_smtp_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="submit_smtp_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="submit_smtp_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="submit_smtp_host"/>
### host



**Description:**
The host of the host running the server

<a name="submit_smtp_port"/>
### port



**Description:**
The port of the host running the server

<a name="submit_smtp_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="submit_smtp_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="submit_smtp_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="submit_smtp_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="submit_smtp_retries"/>
### retries



**Description:**
legacy version of retry

<a name="submit_smtp_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_smtp_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_smtp_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="submit_smtp_alias"/>
### alias



**Description:**
Same as command

<a name="submit_smtp_message"/>
### message



**Description:**
Message

<a name="submit_smtp_result"/>
### result



**Description:**
Result code either a number or OK, WARN, CRIT, UNKNOWN

<a name="submit_smtp_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="submit_smtp_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|result|message

<a name="submit_smtp_sender"/>
### sender



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_smtp_recipient"/>
### recipient



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_smtp_template"/>
### template



**Description:**
Do not initial an ssl handshake with the server, talk in plain text.

<a name="submit_smtp_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_smtp_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)



# Configuration

<a name="/settings/SMTP/client"/>
## SMTP CLIENT SECTION

Section for SMTP passive check module.

```ini
# Section for SMTP passive check module.
[/settings/SMTP/client]
channel=SMTP

```


| Key                                       | Default Value | Description |
|-------------------------------------------|---------------|-------------|
| [channel](#/settings/SMTP/client_channel) | SMTP          | CHANNEL     |




<a name="/settings/SMTP/client_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/SMTP/client](#/settings/SMTP/client) |
| Key:           | channel                                         |
| Default value: | `SMTP`                                          |
| Used by:       | SMTPClient                                      |


#### Sample

```
[/settings/SMTP/client]
# CHANNEL
channel=SMTP
```


<a name="/settings/SMTP/client/handlers"/>
## CLIENT HANDLER SECTION



```ini
# 
[/settings/SMTP/client/handlers]

```






<a name="/settings/SMTP/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/SMTP/client/targets]

```






<a name="/settings/SMTP/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/SMTP/client/targets/default]
retries=3
timeout=30

```


| Key                                                       | Default Value | Description    |
|-----------------------------------------------------------|---------------|----------------|
| [address](#/settings/SMTP/client/targets/default_address) |               | TARGET ADDRESS |
| [host](#/settings/SMTP/client/targets/default_host)       |               | TARGET HOST    |
| [port](#/settings/SMTP/client/targets/default_port)       |               | TARGET PORT    |
| [retries](#/settings/SMTP/client/targets/default_retries) | 3             | RETRIES        |
| [timeout](#/settings/SMTP/client/targets/default_timeout) | 30            | TIMEOUT        |




<a name="/settings/SMTP/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) |
| Key:           | address                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | SMTPClient                                                                      |


#### Sample

```
[/settings/SMTP/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/SMTP/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) |
| Key:           | host                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | SMTPClient                                                                      |


#### Sample

```
[/settings/SMTP/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/SMTP/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) |
| Key:           | port                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | SMTPClient                                                                      |


#### Sample

```
[/settings/SMTP/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/SMTP/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) |
| Key:           | retries                                                                         |
| Default value: | `3`                                                                             |
| Used by:       | SMTPClient                                                                      |


#### Sample

```
[/settings/SMTP/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/SMTP/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/default](#/settings/SMTP/client/targets/default) |
| Key:           | timeout                                                                         |
| Default value: | `30`                                                                            |
| Used by:       | SMTPClient                                                                      |


#### Sample

```
[/settings/SMTP/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/SMTP/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/SMTP/client/targets/sample]
retries=3
timeout=30

```


| Key                                                      | Default Value | Description    |
|----------------------------------------------------------|---------------|----------------|
| [address](#/settings/SMTP/client/targets/sample_address) |               | TARGET ADDRESS |
| [host](#/settings/SMTP/client/targets/sample_host)       |               | TARGET HOST    |
| [port](#/settings/SMTP/client/targets/sample_port)       |               | TARGET PORT    |
| [retries](#/settings/SMTP/client/targets/sample_retries) | 3             | RETRIES        |
| [timeout](#/settings/SMTP/client/targets/sample_timeout) | 30            | TIMEOUT        |




<a name="/settings/SMTP/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) |
| Key:           | address                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | SMTPClient                                                                    |


#### Sample

```
[/settings/SMTP/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/SMTP/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) |
| Key:           | host                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | SMTPClient                                                                    |


#### Sample

```
[/settings/SMTP/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/SMTP/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) |
| Key:           | port                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | SMTPClient                                                                    |


#### Sample

```
[/settings/SMTP/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/SMTP/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) |
| Key:           | retries                                                                       |
| Default value: | `3`                                                                           |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | SMTPClient                                                                    |


#### Sample

```
[/settings/SMTP/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/SMTP/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/SMTP/client/targets/sample](#/settings/SMTP/client/targets/sample) |
| Key:           | timeout                                                                       |
| Default value: | `30`                                                                          |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | SMTPClient                                                                    |


#### Sample

```
[/settings/SMTP/client/targets/sample]
# TIMEOUT
timeout=30
```


