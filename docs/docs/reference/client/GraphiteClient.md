# GraphiteClient

Graphite client can be used to submit graph data to a graphite graphing system



**List of commands:**

A list of all available queries (check commands)

| Command                             | Description                                       |
|-------------------------------------|---------------------------------------------------|
| [submit_graphite](#submit_graphite) | Submit information to the remote Graphite server. |




**Configuration Keys:**



    
    
    
| Path / Section                                          | Key                                             | Description |
|---------------------------------------------------------|-------------------------------------------------|-------------|
| [/settings/graphite/client](#/settings/graphite/client) | [channel](#/settings/graphite/client_channel)   | CHANNEL     |
| [/settings/graphite/client](#/settings/graphite/client) | [hostname](#/settings/graphite/client_hostname) | HOSTNAME    |


| Path / Section                                                            | Description               |
|---------------------------------------------------------------------------|---------------------------|
| [/settings/graphite/client/handlers](#/settings/graphite/client/handlers) | CLIENT HANDLER SECTION    |
| [/settings/graphite/client/targets](#/settings/graphite/client/targets)   | REMOTE TARGET DEFINITIONS |



## Queries

A quick reference for all available queries (check commands) in the GraphiteClient module.

### submit_graphite

Submit information to the remote Graphite server.


* [Command-line Arguments](#submit_graphite_options)





<a name="submit_graphite_help"/>

<a name="submit_graphite_help-pb"/>

<a name="submit_graphite_show-default"/>

<a name="submit_graphite_help-short"/>

<a name="submit_graphite_host"/>

<a name="submit_graphite_port"/>

<a name="submit_graphite_address"/>

<a name="submit_graphite_timeout"/>

<a name="submit_graphite_target"/>

<a name="submit_graphite_retry"/>

<a name="submit_graphite_retries"/>

<a name="submit_graphite_source-host"/>

<a name="submit_graphite_sender-host"/>

<a name="submit_graphite_command"/>

<a name="submit_graphite_alias"/>

<a name="submit_graphite_message"/>

<a name="submit_graphite_result"/>

<a name="submit_graphite_separator"/>

<a name="submit_graphite_batch"/>

<a name="submit_graphite_path"/>

<a name="submit_graphite_options"/>
#### Command-line Arguments


| Option       | Default Value | Description                                                                           |
|--------------|---------------|---------------------------------------------------------------------------------------|
| help         | N/A           | Show help screen (this screen)                                                        |
| help-pb      | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default | N/A           | Show default values for a given command                                               |
| help-short   | N/A           | Show help screen (short format).                                                      |
| host         |               | The host of the host running the server                                               |
| port         |               | The port of the host running the server                                               |
| address      |               | The address (host:port) of the host running the server                                |
| timeout      |               | Number of seconds before connection times out (default=10)                            |
| target       |               | Target to use (lookup connection info from config)                                    |
| retry        |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries      |               | legacy version of retry                                                               |
| source-host  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command      |               | The name of the command that the remote daemon should run                             |
| alias        |               | Same as command                                                                       |
| message      |               | Message                                                                               |
| result       |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| separator    |               | Separator to use for the batch command (default is |)                                 |
| batch        |               | Add multiple records using the separator format is: command|result|message            |
| path         |               |                                                                                       |






## Configuration

<a name="/settings/graphite/client"/>
### GRAPHITE CLIENT SECTION

Section for graphite passive check module.




| Key                                             | Default Value | Description |
|-------------------------------------------------|---------------|-------------|
| [channel](#/settings/graphite/client_channel)   | GRAPHITE      | CHANNEL     |
| [hostname](#/settings/graphite/client_hostname) | auto          | HOSTNAME    |



```ini
# Section for graphite passive check module.
[/settings/graphite/client]
channel=GRAPHITE
hostname=auto

```




<a name="/settings/graphite/client_channel"/>

**CHANNEL**

The channel to listen to.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/graphite/client](#/settings/graphite/client) |
| Key:           | channel                                                 |
| Default value: | `GRAPHITE`                                              |
| Used by:       | GraphiteClient                                          |


**Sample:**

```
[/settings/graphite/client]
# CHANNEL
channel=GRAPHITE
```


<a name="/settings/graphite/client_hostname"/>

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


**Sample:**

```
[/settings/graphite/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/graphite/client/handlers"/>
### CLIENT HANDLER SECTION




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






<a name="/settings/graphite/client/targets"/>
### REMOTE TARGET DEFINITIONS




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key           | Default Value | Description      |
|---------------|---------------|------------------|
| address       |               | TARGET ADDRESS   |
| host          |               | TARGET HOST      |
| path          |               | PATH FOR METRICS |
| port          |               | TARGET PORT      |
| retries       | 3             | RETRIES          |
| send perfdata |               | SEND PERF DATA   |
| send status   |               | SEND STATUS      |
| status path   |               | PATH FOR STATUS  |
| timeout       | 30            | TIMEOUT          |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/graphite/client/targets/sample]
#address=...
#host=...
#path=...
#port=...
retries=3
#send perfdata=...
#send status=...
#status path=...
timeout=30

```






