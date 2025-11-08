# NRDPClient

NRDP client can be used both from command line and from queries to check remote systems via NRDP



## Enable module

To enable this module and and allow using the commands you need to ass `NRDPClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NRDPClient = enabled
```


## Queries

A quick reference for all available queries (check commands) in the NRDPClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_nrdp](#submit_nrdp) | Submit information to the remote NRDP Server. |




### submit_nrdp

Submit information to the remote NRDP Server.


**Jump to section:**

* [Command-line Arguments](#submit_nrdp_options)





<a id="submit_nrdp_help"></a>
<a id="submit_nrdp_help-pb"></a>
<a id="submit_nrdp_show-default"></a>
<a id="submit_nrdp_help-short"></a>
<a id="submit_nrdp_host"></a>
<a id="submit_nrdp_port"></a>
<a id="submit_nrdp_address"></a>
<a id="submit_nrdp_timeout"></a>
<a id="submit_nrdp_target"></a>
<a id="submit_nrdp_retry"></a>
<a id="submit_nrdp_retries"></a>
<a id="submit_nrdp_source-host"></a>
<a id="submit_nrdp_sender-host"></a>
<a id="submit_nrdp_command"></a>
<a id="submit_nrdp_alias"></a>
<a id="submit_nrdp_message"></a>
<a id="submit_nrdp_result"></a>
<a id="submit_nrdp_separator"></a>
<a id="submit_nrdp_batch"></a>
<a id="submit_nrdp_key"></a>
<a id="submit_nrdp_password"></a>
<a id="submit_nrdp_source-host"></a>
<a id="submit_nrdp_sender-host"></a>
<a id="submit_nrdp_token"></a>
<a id="submit_nrdp_tls version"></a>
<a id="submit_nrdp_verify mode"></a>
<a id="submit_nrdp_ca"></a>
<a id="submit_nrdp_options"></a>
#### Command-line Arguments


| Option       | Default Value | Description                                                                                                                                                              |
|--------------|---------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help         | N/A           | Show help screen (this screen)                                                                                                                                           |
| help-pb      | N/A           | Show help screen as a protocol buffer payload                                                                                                                            |
| show-default | N/A           | Show default values for a given command                                                                                                                                  |
| help-short   | N/A           | Show help screen (short format).                                                                                                                                         |
| host         |               | The host of the host running the server                                                                                                                                  |
| port         |               | The port of the host running the server                                                                                                                                  |
| address      |               | The address (host:port) of the host running the server                                                                                                                   |
| timeout      |               | Number of seconds before connection times out (default=10)                                                                                                               |
| target       |               | Target to use (lookup connection info from config)                                                                                                                       |
| retry        |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                         |
| retries      |               | legacy version of retry                                                                                                                                                  |
| source-host  |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                    |
| sender-host  |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                    |
| command      |               | The name of the command that the remote daemon should run                                                                                                                |
| alias        |               | Same as command                                                                                                                                                          |
| message      |               | Message                                                                                                                                                                  |
| result       |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                   |
| separator    |               | Separator to use for the batch command (default is |)                                                                                                                    |
| batch        |               | Add multiple records using the separator format is: command|result|message                                                                                               |
| key          |               | The security token                                                                                                                                                       |
| password     |               | The security token                                                                                                                                                       |
| source-host  |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                    |
| sender-host  |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                    |
| token        |               | The security token                                                                                                                                                       |
| tls version  |               | The tls version to use 1.0, 1.1, 1.2, 1.3                                                                                                                                |
| verify mode  |               | Coma separated list o9f option none, peer, peer-cert, client-once, fail-if-no-cert, workarounds, single., In general use peer-cert or none for self signed certificates. |
| ca           |               | Certificate authority to use when verifying certificates.                                                                                                                |






## Configuration



| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NRDP/client](#smtp-client-section)               | SMTP CLIENT SECTION       |
| [/settings/NRDP/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NRDP/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### SMTP CLIENT SECTION <a id="/settings/NRDP/client"></a>

Section for SMTP passive check module.




| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | NRDP          | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |



```ini
# Section for SMTP passive check module.
[/settings/NRDP/client]
channel=NRDP
hostname=auto

```





#### CHANNEL <a id="/settings/NRDP/client/channel"></a>

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



#### HOSTNAME <a id="/settings/NRDP/client/hostname"></a>

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


### CLIENT HANDLER SECTION <a id="/settings/NRDP/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NRDP/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key         | Default Value | Description           |
|-------------|---------------|-----------------------|
| address     |               | TARGET ADDRESS        |
| ca          |               | Certificate Authority |
| host        |               | TARGET HOST           |
| key         |               | SECURITY TOKEN        |
| password    |               | SECURITY TOKEN        |
| port        |               | TARGET PORT           |
| retries     | 3             | RETRIES               |
| timeout     | 30            | TIMEOUT               |
| tls version |               | Tls version           |
| token       |               | SECURITY TOKEN        |
| verify mode |               | TLS peer verify mode  |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NRDP/client/targets/sample]
#address=...
#ca=...
#host=...
#key=...
#password=...
#port=...
retries=3
timeout=30
#tls version=...
#token=...
#verify mode=...

```






