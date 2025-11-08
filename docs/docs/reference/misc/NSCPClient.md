# NSCPClient

NSCP client can be used both from command line and from queries to check remote systems via NSCP (REST)



## Enable module

To enable this module and and allow using the commands you need to ass `NSCPClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NSCPClient = enabled
```


## Queries

A quick reference for all available queries (check commands) in the NSCPClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                                   | Description                                        |
|-------------------------------------------|----------------------------------------------------|
| [check_remote_nscp](#check_remote_nscp)   | Request remote information via NSCP.               |
| [exec_remote_nscp](#exec_remote_nscp)     | Execute remote script via NSCP.                    |
| [remote_nscp_query](#remote_nscp_query)   | Request remote information via NSCP.               |
| [remote_nscpforward](#remote_nscpforward) | Forward the request as-is to remote host via NSCP. |
| [submit_remote_nscp](#submit_remote_nscp) | Submit information to remote host via NSCP.        |




### check_remote_nscp

Request remote information via NSCP.


**Jump to section:**

* [Command-line Arguments](#check_remote_nscp_options)





<a id="check_remote_nscp_help"></a>
<a id="check_remote_nscp_help-pb"></a>
<a id="check_remote_nscp_show-default"></a>
<a id="check_remote_nscp_help-short"></a>
<a id="check_remote_nscp_host"></a>
<a id="check_remote_nscp_port"></a>
<a id="check_remote_nscp_address"></a>
<a id="check_remote_nscp_timeout"></a>
<a id="check_remote_nscp_target"></a>
<a id="check_remote_nscp_retry"></a>
<a id="check_remote_nscp_retries"></a>
<a id="check_remote_nscp_source-host"></a>
<a id="check_remote_nscp_sender-host"></a>
<a id="check_remote_nscp_command"></a>
<a id="check_remote_nscp_argument"></a>
<a id="check_remote_nscp_separator"></a>
<a id="check_remote_nscp_batch"></a>
<a id="check_remote_nscp_certificate"></a>
<a id="check_remote_nscp_dh"></a>
<a id="check_remote_nscp_certificate-key"></a>
<a id="check_remote_nscp_certificate-format"></a>
<a id="check_remote_nscp_ca"></a>
<a id="check_remote_nscp_verify"></a>
<a id="check_remote_nscp_allowed-ciphers"></a>
<a id="check_remote_nscp_password"></a>
<a id="check_remote_nscp_options"></a>
#### Command-line Arguments


| Option                        | Default Value | Description                                                                           |
|-------------------------------|---------------|---------------------------------------------------------------------------------------|
| help                          | N/A           | Show help screen (this screen)                                                        |
| help-pb                       | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default                  | N/A           | Show default values for a given command                                               |
| help-short                    | N/A           | Show help screen (short format).                                                      |
| host                          |               | The host of the host running the server                                               |
| port                          |               | The port of the host running the server                                               |
| address                       |               | The address (host:port) of the host running the server                                |
| timeout                       |               | Number of seconds before connection times out (default=10)                            |
| target                        |               | Target to use (lookup connection info from config)                                    |
| retry                         |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                       |               | legacy version of retry                                                               |
| source-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                       |               | The name of the command that the remote daemon should run                             |
| argument                      |               | Set command line arguments                                                            |
| separator                     |               | Separator to use for the batch command (default is |)                                 |
| batch                         |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                   |               | The client certificate to use                                                         |
| dh                            |               | The DH key to use                                                                     |
| certificate-key               |               | Client certificate to use                                                             |
| certificate-format            |               | Client certificate format                                                             |
| ca                            |               | Certificate authority                                                                 |
| verify                        |               | Client certificate format                                                             |
| allowed-ciphers               |               | Client certificate format                                                             |
| [ssl](#check_remote_nscp_ssl) | 1             | Initial an ssl handshake with the server.                                             |
| password                      |               | Password                                                                              |



<h5 id="check_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`


### exec_remote_nscp

Execute remote script via NSCP.


**Jump to section:**

* [Command-line Arguments](#exec_remote_nscp_options)





<a id="exec_remote_nscp_help"></a>
<a id="exec_remote_nscp_help-pb"></a>
<a id="exec_remote_nscp_show-default"></a>
<a id="exec_remote_nscp_help-short"></a>
<a id="exec_remote_nscp_host"></a>
<a id="exec_remote_nscp_port"></a>
<a id="exec_remote_nscp_address"></a>
<a id="exec_remote_nscp_timeout"></a>
<a id="exec_remote_nscp_target"></a>
<a id="exec_remote_nscp_retry"></a>
<a id="exec_remote_nscp_retries"></a>
<a id="exec_remote_nscp_source-host"></a>
<a id="exec_remote_nscp_sender-host"></a>
<a id="exec_remote_nscp_command"></a>
<a id="exec_remote_nscp_argument"></a>
<a id="exec_remote_nscp_separator"></a>
<a id="exec_remote_nscp_batch"></a>
<a id="exec_remote_nscp_certificate"></a>
<a id="exec_remote_nscp_dh"></a>
<a id="exec_remote_nscp_certificate-key"></a>
<a id="exec_remote_nscp_certificate-format"></a>
<a id="exec_remote_nscp_ca"></a>
<a id="exec_remote_nscp_verify"></a>
<a id="exec_remote_nscp_allowed-ciphers"></a>
<a id="exec_remote_nscp_password"></a>
<a id="exec_remote_nscp_options"></a>
#### Command-line Arguments


| Option                       | Default Value | Description                                                                           |
|------------------------------|---------------|---------------------------------------------------------------------------------------|
| help                         | N/A           | Show help screen (this screen)                                                        |
| help-pb                      | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default                 | N/A           | Show default values for a given command                                               |
| help-short                   | N/A           | Show help screen (short format).                                                      |
| host                         |               | The host of the host running the server                                               |
| port                         |               | The port of the host running the server                                               |
| address                      |               | The address (host:port) of the host running the server                                |
| timeout                      |               | Number of seconds before connection times out (default=10)                            |
| target                       |               | Target to use (lookup connection info from config)                                    |
| retry                        |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                      |               | legacy version of retry                                                               |
| source-host                  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                      |               | The name of the command that the remote daemon should run                             |
| argument                     |               | Set command line arguments                                                            |
| separator                    |               | Separator to use for the batch command (default is |)                                 |
| batch                        |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                  |               | The client certificate to use                                                         |
| dh                           |               | The DH key to use                                                                     |
| certificate-key              |               | Client certificate to use                                                             |
| certificate-format           |               | Client certificate format                                                             |
| ca                           |               | Certificate authority                                                                 |
| verify                       |               | Client certificate format                                                             |
| allowed-ciphers              |               | Client certificate format                                                             |
| [ssl](#exec_remote_nscp_ssl) | 1             | Initial an ssl handshake with the server.                                             |
| password                     |               | Password                                                                              |



<h5 id="exec_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`


### remote_nscp_query

Request remote information via NSCP.


**Jump to section:**

* [Command-line Arguments](#remote_nscp_query_options)





<a id="remote_nscp_query_help"></a>
<a id="remote_nscp_query_help-pb"></a>
<a id="remote_nscp_query_show-default"></a>
<a id="remote_nscp_query_help-short"></a>
<a id="remote_nscp_query_host"></a>
<a id="remote_nscp_query_port"></a>
<a id="remote_nscp_query_address"></a>
<a id="remote_nscp_query_timeout"></a>
<a id="remote_nscp_query_target"></a>
<a id="remote_nscp_query_retry"></a>
<a id="remote_nscp_query_retries"></a>
<a id="remote_nscp_query_source-host"></a>
<a id="remote_nscp_query_sender-host"></a>
<a id="remote_nscp_query_command"></a>
<a id="remote_nscp_query_argument"></a>
<a id="remote_nscp_query_separator"></a>
<a id="remote_nscp_query_batch"></a>
<a id="remote_nscp_query_certificate"></a>
<a id="remote_nscp_query_dh"></a>
<a id="remote_nscp_query_certificate-key"></a>
<a id="remote_nscp_query_certificate-format"></a>
<a id="remote_nscp_query_ca"></a>
<a id="remote_nscp_query_verify"></a>
<a id="remote_nscp_query_allowed-ciphers"></a>
<a id="remote_nscp_query_password"></a>
<a id="remote_nscp_query_options"></a>
#### Command-line Arguments


| Option                        | Default Value | Description                                                                           |
|-------------------------------|---------------|---------------------------------------------------------------------------------------|
| help                          | N/A           | Show help screen (this screen)                                                        |
| help-pb                       | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default                  | N/A           | Show default values for a given command                                               |
| help-short                    | N/A           | Show help screen (short format).                                                      |
| host                          |               | The host of the host running the server                                               |
| port                          |               | The port of the host running the server                                               |
| address                       |               | The address (host:port) of the host running the server                                |
| timeout                       |               | Number of seconds before connection times out (default=10)                            |
| target                        |               | Target to use (lookup connection info from config)                                    |
| retry                         |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                       |               | legacy version of retry                                                               |
| source-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                   |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                       |               | The name of the command that the remote daemon should run                             |
| argument                      |               | Set command line arguments                                                            |
| separator                     |               | Separator to use for the batch command (default is |)                                 |
| batch                         |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                   |               | The client certificate to use                                                         |
| dh                            |               | The DH key to use                                                                     |
| certificate-key               |               | Client certificate to use                                                             |
| certificate-format            |               | Client certificate format                                                             |
| ca                            |               | Certificate authority                                                                 |
| verify                        |               | Client certificate format                                                             |
| allowed-ciphers               |               | Client certificate format                                                             |
| [ssl](#remote_nscp_query_ssl) | 1             | Initial an ssl handshake with the server.                                             |
| password                      |               | Password                                                                              |



<h5 id="remote_nscp_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`


### remote_nscpforward

Forward the request as-is to remote host via NSCP.


**Jump to section:**

* [Command-line Arguments](#remote_nscpforward_options)





<a id="remote_nscpforward_options"></a>
#### Command-line Arguments






### submit_remote_nscp

Submit information to remote host via NSCP.


**Jump to section:**

* [Command-line Arguments](#submit_remote_nscp_options)





<a id="submit_remote_nscp_help"></a>
<a id="submit_remote_nscp_help-pb"></a>
<a id="submit_remote_nscp_show-default"></a>
<a id="submit_remote_nscp_help-short"></a>
<a id="submit_remote_nscp_host"></a>
<a id="submit_remote_nscp_port"></a>
<a id="submit_remote_nscp_address"></a>
<a id="submit_remote_nscp_timeout"></a>
<a id="submit_remote_nscp_target"></a>
<a id="submit_remote_nscp_retry"></a>
<a id="submit_remote_nscp_retries"></a>
<a id="submit_remote_nscp_source-host"></a>
<a id="submit_remote_nscp_sender-host"></a>
<a id="submit_remote_nscp_command"></a>
<a id="submit_remote_nscp_alias"></a>
<a id="submit_remote_nscp_message"></a>
<a id="submit_remote_nscp_result"></a>
<a id="submit_remote_nscp_separator"></a>
<a id="submit_remote_nscp_batch"></a>
<a id="submit_remote_nscp_certificate"></a>
<a id="submit_remote_nscp_dh"></a>
<a id="submit_remote_nscp_certificate-key"></a>
<a id="submit_remote_nscp_certificate-format"></a>
<a id="submit_remote_nscp_ca"></a>
<a id="submit_remote_nscp_verify"></a>
<a id="submit_remote_nscp_allowed-ciphers"></a>
<a id="submit_remote_nscp_password"></a>
<a id="submit_remote_nscp_options"></a>
#### Command-line Arguments


| Option                         | Default Value | Description                                                                           |
|--------------------------------|---------------|---------------------------------------------------------------------------------------|
| help                           | N/A           | Show help screen (this screen)                                                        |
| help-pb                        | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default                   | N/A           | Show default values for a given command                                               |
| help-short                     | N/A           | Show help screen (short format).                                                      |
| host                           |               | The host of the host running the server                                               |
| port                           |               | The port of the host running the server                                               |
| address                        |               | The address (host:port) of the host running the server                                |
| timeout                        |               | Number of seconds before connection times out (default=10)                            |
| target                         |               | Target to use (lookup connection info from config)                                    |
| retry                          |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                        |               | legacy version of retry                                                               |
| source-host                    |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                    |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                        |               | The name of the command that the remote daemon should run                             |
| alias                          |               | Same as command                                                                       |
| message                        |               | Message                                                                               |
| result                         |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                |
| separator                      |               | Separator to use for the batch command (default is |)                                 |
| batch                          |               | Add multiple records using the separator format is: command|result|message            |
| certificate                    |               | The client certificate to use                                                         |
| dh                             |               | The DH key to use                                                                     |
| certificate-key                |               | Client certificate to use                                                             |
| certificate-format             |               | Client certificate format                                                             |
| ca                             |               | Certificate authority                                                                 |
| verify                         |               | Client certificate format                                                             |
| allowed-ciphers                |               | Client certificate format                                                             |
| [ssl](#submit_remote_nscp_ssl) | 1             | Initial an ssl handshake with the server.                                             |
| password                       |               | Password                                                                              |



<h5 id="submit_remote_nscp_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`




## Configuration



| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NSCP/client](#nscp-client-section)               | NSCP CLIENT SECTION       |
| [/settings/NSCP/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NSCP/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### NSCP CLIENT SECTION <a id="/settings/NSCP/client"></a>

Section for NSCP active/passive check module.




| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | NSCP          | CHANNEL     |



```ini
# Section for NSCP active/passive check module.
[/settings/NSCP/client]
channel=NSCP

```





#### CHANNEL <a id="/settings/NSCP/client/channel"></a>

The channel to listen to.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCP/client](#/settings/NSCP/client) |
| Key:           | channel                                         |
| Default value: | `NSCP`                                          |
| Used by:       | NSCPClient                                      |


**Sample:**

```
[/settings/NSCP/client]
# CHANNEL
channel=NSCP
```


### CLIENT HANDLER SECTION <a id="/settings/NSCP/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NSCP/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key                | Default Value | Description           |
|--------------------|---------------|-----------------------|
| address            |               | TARGET ADDRESS        |
| allowed ciphers    |               | ALLOWED CIPHERS       |
| ca                 |               | CA                    |
| certificate        |               | SSL CERTIFICATE       |
| certificate format |               | CERTIFICATE FORMAT    |
| certificate key    |               | SSL CERTIFICATE       |
| dh                 |               | DH KEY                |
| host               |               | TARGET HOST           |
| password           |               | PASSWORD              |
| port               |               | TARGET PORT           |
| retries            | 3             | RETRIES               |
| timeout            | 30            | TIMEOUT               |
| use ssl            |               | ENABLE SSL ENCRYPTION |
| verify mode        |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NSCP/client/targets/sample]
#address=...
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#host=...
#password=...
#port=...
retries=3
timeout=30
#use ssl=...
#verify mode=...

```






