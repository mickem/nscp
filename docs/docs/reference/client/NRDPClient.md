# NRDPClient

NRDP client can be used both from command line and from queries to check remote systems via NRDP



**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_nrdp](#submit_nrdp) | Submit information to the remote NRDP Server. |




**Configuration Keys:**



    
    
    
| Path / Section                                  | Key                                         | Description |
|-------------------------------------------------|---------------------------------------------|-------------|
| [/settings/NRDP/client](#/settings/NRDP/client) | [channel](#/settings/NRDP/client_channel)   | CHANNEL     |
| [/settings/NRDP/client](#/settings/NRDP/client) | [hostname](#/settings/NRDP/client_hostname) | HOSTNAME    |


| Path / Section                                                    | Description               |
|-------------------------------------------------------------------|---------------------------|
| [/settings/NRDP/client/handlers](#/settings/NRDP/client/handlers) | CLIENT HANDLER SECTION    |
| [/settings/NRDP/client/targets](#/settings/NRDP/client/targets)   | REMOTE TARGET DEFINITIONS |



## Queries

A quick reference for all available queries (check commands) in the NRDPClient module.

### submit_nrdp

Submit information to the remote NRDP Server.


* [Command-line Arguments](#submit_nrdp_options)





<a name="submit_nrdp_help"/>

<a name="submit_nrdp_help-pb"/>

<a name="submit_nrdp_show-default"/>

<a name="submit_nrdp_help-short"/>

<a name="submit_nrdp_host"/>

<a name="submit_nrdp_port"/>

<a name="submit_nrdp_address"/>

<a name="submit_nrdp_timeout"/>

<a name="submit_nrdp_target"/>

<a name="submit_nrdp_retry"/>

<a name="submit_nrdp_retries"/>

<a name="submit_nrdp_source-host"/>

<a name="submit_nrdp_sender-host"/>

<a name="submit_nrdp_command"/>

<a name="submit_nrdp_alias"/>

<a name="submit_nrdp_message"/>

<a name="submit_nrdp_result"/>

<a name="submit_nrdp_separator"/>

<a name="submit_nrdp_batch"/>

<a name="submit_nrdp_key"/>

<a name="submit_nrdp_password"/>

<a name="submit_nrdp_source-host"/>

<a name="submit_nrdp_sender-host"/>

<a name="submit_nrdp_token"/>

<a name="submit_nrdp_options"/>
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
| key          |               | The security token                                                                    |
| password     |               | The security token                                                                    |
| source-host  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| token        |               | The security token                                                                    |






## Configuration

<a name="/settings/NRDP/client"/>
### SMTP CLIENT SECTION

Section for SMTP passive check module.




| Key                                         | Default Value | Description |
|---------------------------------------------|---------------|-------------|
| [channel](#/settings/NRDP/client_channel)   | NRDP          | CHANNEL     |
| [hostname](#/settings/NRDP/client_hostname) | auto          | HOSTNAME    |



```ini
# Section for SMTP passive check module.
[/settings/NRDP/client]
channel=NRDP
hostname=auto

```




<a name="/settings/NRDP/client_channel"/>

**CHANNEL**

The channel to listen to.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRDP/client](#/settings/NRDP/client) |
| Key:           | channel                                         |
| Default value: | `NRDP`                                          |
| Used by:       | NRDPClient                                      |


**Sample:**

```
[/settings/NRDP/client]
# CHANNEL
channel=NRDP
```


<a name="/settings/NRDP/client_hostname"/>

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






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRDP/client](#/settings/NRDP/client) |
| Key:           | hostname                                        |
| Default value: | `auto`                                          |
| Used by:       | NRDPClient                                      |


**Sample:**

```
[/settings/NRDP/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/NRDP/client/handlers"/>
### CLIENT HANDLER SECTION




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






<a name="/settings/NRDP/client/targets"/>
### REMOTE TARGET DEFINITIONS




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key      | Default Value | Description    |
|----------|---------------|----------------|
| address  |               | TARGET ADDRESS |
| host     |               | TARGET HOST    |
| key      |               | SECURITY TOKEN |
| password |               | SECURITY TOKEN |
| port     |               | TARGET PORT    |
| retries  | 3             | RETRIES        |
| timeout  | 30            | TIMEOUT        |
| token    |               | SECURITY TOKEN |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NRDP/client/targets/sample]
#address=...
#host=...
#key=...
#password=...
#port=...
retries=3
timeout=30
#token=...

```






