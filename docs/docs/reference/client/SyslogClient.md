# SyslogClient

Forward information as syslog messages to a syslog server




## Queries

A quick reference for all available queries (check commands) in the SyslogClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                         | Description                                     |
|---------------------------------|-------------------------------------------------|
| [submit_syslog](#submit_syslog) | Submit information to the remote syslog server. |




### submit_syslog

Submit information to the remote syslog server.


* [Command-line Arguments](#submit_syslog_options)





<a name="submit_syslog_help"/>
<a name="submit_syslog_help-pb"/>
<a name="submit_syslog_show-default"/>
<a name="submit_syslog_help-short"/>
<a name="submit_syslog_host"/>
<a name="submit_syslog_port"/>
<a name="submit_syslog_address"/>
<a name="submit_syslog_timeout"/>
<a name="submit_syslog_target"/>
<a name="submit_syslog_retry"/>
<a name="submit_syslog_retries"/>
<a name="submit_syslog_source-host"/>
<a name="submit_syslog_sender-host"/>
<a name="submit_syslog_command"/>
<a name="submit_syslog_alias"/>
<a name="submit_syslog_message"/>
<a name="submit_syslog_result"/>
<a name="submit_syslog_separator"/>
<a name="submit_syslog_batch"/>
<a name="submit_syslog_path"/>
<a name="submit_syslog_severity"/>
<a name="submit_syslog_unknown-severity"/>
<a name="submit_syslog_ok-severity"/>
<a name="submit_syslog_warning-severity"/>
<a name="submit_syslog_critical-severity"/>
<a name="submit_syslog_facility"/>
<a name="submit_syslog_tag template"/>
<a name="submit_syslog_message template"/>
<a name="submit_syslog_options"/>
#### Command-line Arguments


| Option            | Default Value | Description                                                                           |
|-------------------|---------------|---------------------------------------------------------------------------------------|
| help              | N/A           | Show help screen (this screen)                                                        |
| help-pb           | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default      | N/A           | Show default values for a given command                                               |
| help-short        | N/A           | Show help screen (short format).                                                      |
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
| unknown-severity  |               | Severity of error message                                                             |
| ok-severity       |               | Severity of error message                                                             |
| warning-severity  |               | Severity of error message                                                             |
| critical-severity |               | Severity of error message                                                             |
| facility          |               | Facility of error message                                                             |
| tag template      |               | Tag template (TODO)                                                                   |
| message template  |               | Message template (TODO)                                                               |






## Configuration



| Path / Section                                                | Description               |
|---------------------------------------------------------------|---------------------------|
| [/settings/syslog/client](#syslog-client-section)             | SYSLOG CLIENT SECTION     |
| [/settings/syslog/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/syslog/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### SYSLOG CLIENT SECTION <a id="/settings/syslog/client"/>

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
| Used by:       | SyslogClient                                        |


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


**Sample:**

```
[/settings/syslog/client]
# HOSTNAME
hostname=auto
```


### CLIENT HANDLER SECTION <a id="/settings/syslog/client/handlers"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/syslog/client/targets"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key     | Default Value | Description    |
|---------|---------------|----------------|
| address |               | TARGET ADDRESS |
| host    |               | TARGET HOST    |
| port    |               | TARGET PORT    |
| retries | 3             | RETRIES        |
| timeout | 30            | TIMEOUT        |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/syslog/client/targets/sample]
#address=...
#host=...
#port=...
retries=3
timeout=30

```






