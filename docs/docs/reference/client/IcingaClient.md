# IcingaClient

Icinga 2 client submits passive check results to an Icinga 2 server via the REST API



## Enable module

To enable this module and and allow using the commands you need to ass `IcingaClient = enabled` to the `[/modules]` section in nsclient.ini:

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


**Jump to section:**

* [Command-line Arguments](#submit_icinga_options)





<a id="submit_icinga_help"></a>
<a id="submit_icinga_help-pb"></a>
<a id="submit_icinga_show-default"></a>
<a id="submit_icinga_help-short"></a>
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
<a id="submit_icinga_options"></a>
#### Command-line Arguments


| Option                                          | Default Value | Description                                                                                                                                                               |
|-------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help                                            | N/A           | Show help screen (this screen)                                                                                                                                            |
| help-pb                                         | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| show-default                                    | N/A           | Show default values for a given command                                                                                                                                   |
| help-short                                      | N/A           | Show help screen (short format).                                                                                                                                          |
| host                                            |               | The host of the host running the server                                                                                                                                   |
| port                                            |               | The port of the host running the server                                                                                                                                   |
| address                                         |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                                         |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                                          |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                                         |               | legacy version of retry                                                                                                                                                   |
| source-host                                     |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                                     |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                                         |               | The name of the command that the remote daemon should run                                                                                                                 |
| alias                                           |               | Same as command                                                                                                                                                           |
| message                                         |               | Message                                                                                                                                                                   |
| result                                          |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| separator                                       |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                                           |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| username                                        |               | The username used to authenticate against the Icinga 2 REST API.                                                                                                          |
| password                                        |               | The password used to authenticate against the Icinga 2 REST API.                                                                                                          |
| hostname                                        |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [ensure-objects](#submit_icinga_ensure-objects) | true          | Create missing host/service objects in Icinga 2 before submitting (true/false).                                                                                           |
| host-template                                   |               | Templates used when auto-creating host objects (default: generic-host).                                                                                                   |
| service-template                                |               | Templates used when auto-creating service objects (default: generic-service).                                                                                             |
| check-command                                   |               | The check_command to set on auto-created service objects (default: dummy).                                                                                                |
| check-source                                    |               | Override for the check_source field reported to Icinga 2.                                                                                                                 |
| [tls-version](#submit_icinga_tls-version)       | 1.3           | The TLS version to use 1.0, 1.1, 1.2, 1.3 or any                                                                                                                          |
| verify-mode                                     |               | Comma separated list of options: none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single. In general use peer-cert or none for self signed certificates. |
| ca                                              |               | Certificate authority to use when verifying certificates.                                                                                                                 |



<h5 id="submit_icinga_ensure-objects">ensure-objects:</h5>

Create missing host/service objects in Icinga 2 before submitting (true/false).

*Default Value:* `true`

<h5 id="submit_icinga_tls-version">tls-version:</h5>

The TLS version to use 1.0, 1.1, 1.2, 1.3 or any

*Default Value:* `1.3`




## Configuration



| Path / Section                                                | Description               |
|---------------------------------------------------------------|---------------------------|
| [/settings/icinga/client](#icinga-2-client)                   | Icinga 2 client           |
| [/settings/icinga/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
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
${host_lc}
Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase






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


### CLIENT HANDLER SECTION <a id="/settings/icinga/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/icinga/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key              | Default Value | Description                 |
|------------------|---------------|-----------------------------|
| address          |               | TARGET ADDRESS              |
| ca               | ${ca-path}    | CERTIFICATE AUTHORITY       |
| check command    |               | CHECK COMMAND               |
| check source     |               | CHECK SOURCE                |
| ensure objects   |               | ENSURE HOST/SERVICE OBJECTS |
| host             |               | TARGET HOST                 |
| host template    |               | HOST TEMPLATE               |
| password         |               | ICINGA API PASSWORD         |
| port             |               | TARGET PORT                 |
| retries          | 3             | RETRIES                     |
| service template |               | SERVICE TEMPLATE            |
| timeout          | 30            | TIMEOUT                     |
| tls version      | 1.3           | TLS VERSION                 |
| username         |               | ICINGA API USER             |
| verify mode      |               | TLS PEER VERIFY MODE        |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/icinga/client/targets/sample]
#address=...
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
#verify mode=...

```






