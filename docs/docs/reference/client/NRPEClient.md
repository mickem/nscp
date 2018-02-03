# NRPEClient

NRPE client can be used both from command line and from queries to check remote systes via NRPE as well as configure the NRPE server




## Queries

A quick reference for all available queries (check commands) in the NRPEClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                       | Description                                                                    |
|-------------------------------|--------------------------------------------------------------------------------|
| [check_nrpe](#check_nrpe)     | Request remote information via NRPE.                                           |
| [exec_nrpe](#exec_nrpe)       | Execute remote script via NRPE. (Most likely you want nrpe_query).             |
| [nrpe_forward](#nrpe_forward) | Forward the request as-is to remote host via NRPE.                             |
| [nrpe_query](#nrpe_query)     | Request remote information via NRPE.                                           |
| [submit_nrpe](#submit_nrpe)   | Submit information to remote host via NRPE. (Most likely you want nrpe_query). |




### check_nrpe

Request remote information via NRPE.


* [Command-line Arguments](#check_nrpe_options)





<a name="check_nrpe_help"/>
<a name="check_nrpe_help-pb"/>
<a name="check_nrpe_show-default"/>
<a name="check_nrpe_help-short"/>
<a name="check_nrpe_host"/>
<a name="check_nrpe_port"/>
<a name="check_nrpe_address"/>
<a name="check_nrpe_timeout"/>
<a name="check_nrpe_target"/>
<a name="check_nrpe_retry"/>
<a name="check_nrpe_retries"/>
<a name="check_nrpe_source-host"/>
<a name="check_nrpe_sender-host"/>
<a name="check_nrpe_command"/>
<a name="check_nrpe_argument"/>
<a name="check_nrpe_separator"/>
<a name="check_nrpe_batch"/>
<a name="check_nrpe_certificate"/>
<a name="check_nrpe_dh"/>
<a name="check_nrpe_certificate-key"/>
<a name="check_nrpe_certificate-format"/>
<a name="check_nrpe_ca"/>
<a name="check_nrpe_verify"/>
<a name="check_nrpe_allowed-ciphers"/>
<a name="check_nrpe_insecure"/>
<a name="check_nrpe_payload-length"/>
<a name="check_nrpe_buffer-length"/>
<a name="check_nrpe_options"/>
#### Command-line Arguments


| Option                 | Default Value | Description                                                                                                                                                               |
|------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help                   | N/A           | Show help screen (this screen)                                                                                                                                            |
| help-pb                | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| show-default           | N/A           | Show default values for a given command                                                                                                                                   |
| help-short             | N/A           | Show help screen (short format).                                                                                                                                          |
| host                   |               | The host of the host running the server                                                                                                                                   |
| port                   |               | The port of the host running the server                                                                                                                                   |
| address                |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                 |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                  |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                |               | legacy version of retry                                                                                                                                                   |
| source-host            |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host            |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                |               | The name of the command that the remote daemon should run                                                                                                                 |
| argument               |               | Set command line arguments                                                                                                                                                |
| separator              |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                  |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| certificate            |               | Length of payload (has to be same as on the server)                                                                                                                       |
| dh                     |               | Length of payload (has to be same as on the server)                                                                                                                       |
| certificate-key        |               | Client certificate to use                                                                                                                                                 |
| certificate-format     |               | Client certificate format                                                                                                                                                 |
| ca                     |               | Certificate authority                                                                                                                                                     |
| verify                 |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers        |               | Client certificate format                                                                                                                                                 |
| [ssl](#check_nrpe_ssl) | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| insecure               | N/A           | Use insecure legacy mode                                                                                                                                                  |
| payload-length         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| buffer-length          |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="check_nrpe_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`


### exec_nrpe

Execute remote script via NRPE. (Most likely you want nrpe_query).


* [Command-line Arguments](#exec_nrpe_options)





<a name="exec_nrpe_help"/>
<a name="exec_nrpe_help-pb"/>
<a name="exec_nrpe_show-default"/>
<a name="exec_nrpe_help-short"/>
<a name="exec_nrpe_host"/>
<a name="exec_nrpe_port"/>
<a name="exec_nrpe_address"/>
<a name="exec_nrpe_timeout"/>
<a name="exec_nrpe_target"/>
<a name="exec_nrpe_retry"/>
<a name="exec_nrpe_retries"/>
<a name="exec_nrpe_source-host"/>
<a name="exec_nrpe_sender-host"/>
<a name="exec_nrpe_command"/>
<a name="exec_nrpe_argument"/>
<a name="exec_nrpe_separator"/>
<a name="exec_nrpe_batch"/>
<a name="exec_nrpe_certificate"/>
<a name="exec_nrpe_dh"/>
<a name="exec_nrpe_certificate-key"/>
<a name="exec_nrpe_certificate-format"/>
<a name="exec_nrpe_ca"/>
<a name="exec_nrpe_verify"/>
<a name="exec_nrpe_allowed-ciphers"/>
<a name="exec_nrpe_insecure"/>
<a name="exec_nrpe_payload-length"/>
<a name="exec_nrpe_buffer-length"/>
<a name="exec_nrpe_options"/>
#### Command-line Arguments


| Option                | Default Value | Description                                                                                                                                                               |
|-----------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help                  | N/A           | Show help screen (this screen)                                                                                                                                            |
| help-pb               | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| show-default          | N/A           | Show default values for a given command                                                                                                                                   |
| help-short            | N/A           | Show help screen (short format).                                                                                                                                          |
| host                  |               | The host of the host running the server                                                                                                                                   |
| port                  |               | The port of the host running the server                                                                                                                                   |
| address               |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout               |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                 |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries               |               | legacy version of retry                                                                                                                                                   |
| source-host           |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host           |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command               |               | The name of the command that the remote daemon should run                                                                                                                 |
| argument              |               | Set command line arguments                                                                                                                                                |
| separator             |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                 |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| certificate           |               | Length of payload (has to be same as on the server)                                                                                                                       |
| dh                    |               | Length of payload (has to be same as on the server)                                                                                                                       |
| certificate-key       |               | Client certificate to use                                                                                                                                                 |
| certificate-format    |               | Client certificate format                                                                                                                                                 |
| ca                    |               | Certificate authority                                                                                                                                                     |
| verify                |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers       |               | Client certificate format                                                                                                                                                 |
| [ssl](#exec_nrpe_ssl) | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| insecure              | N/A           | Use insecure legacy mode                                                                                                                                                  |
| payload-length        |               | Length of payload (has to be same as on the server)                                                                                                                       |
| buffer-length         |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="exec_nrpe_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`


### nrpe_forward

Forward the request as-is to remote host via NRPE.


* [Command-line Arguments](#nrpe_forward_options)





<a name="nrpe_forward_*"/>
<a name="nrpe_forward_options"/>
#### Command-line Arguments


| Option | Default Value | Description |
|--------|---------------|-------------|
| *      |               |             |




### nrpe_query

Request remote information via NRPE.


* [Command-line Arguments](#nrpe_query_options)





<a name="nrpe_query_help"/>
<a name="nrpe_query_help-pb"/>
<a name="nrpe_query_show-default"/>
<a name="nrpe_query_help-short"/>
<a name="nrpe_query_host"/>
<a name="nrpe_query_port"/>
<a name="nrpe_query_address"/>
<a name="nrpe_query_timeout"/>
<a name="nrpe_query_target"/>
<a name="nrpe_query_retry"/>
<a name="nrpe_query_retries"/>
<a name="nrpe_query_source-host"/>
<a name="nrpe_query_sender-host"/>
<a name="nrpe_query_command"/>
<a name="nrpe_query_argument"/>
<a name="nrpe_query_separator"/>
<a name="nrpe_query_batch"/>
<a name="nrpe_query_certificate"/>
<a name="nrpe_query_dh"/>
<a name="nrpe_query_certificate-key"/>
<a name="nrpe_query_certificate-format"/>
<a name="nrpe_query_ca"/>
<a name="nrpe_query_verify"/>
<a name="nrpe_query_allowed-ciphers"/>
<a name="nrpe_query_insecure"/>
<a name="nrpe_query_payload-length"/>
<a name="nrpe_query_buffer-length"/>
<a name="nrpe_query_options"/>
#### Command-line Arguments


| Option                 | Default Value | Description                                                                                                                                                               |
|------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help                   | N/A           | Show help screen (this screen)                                                                                                                                            |
| help-pb                | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| show-default           | N/A           | Show default values for a given command                                                                                                                                   |
| help-short             | N/A           | Show help screen (short format).                                                                                                                                          |
| host                   |               | The host of the host running the server                                                                                                                                   |
| port                   |               | The port of the host running the server                                                                                                                                   |
| address                |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                 |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                  |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                |               | legacy version of retry                                                                                                                                                   |
| source-host            |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host            |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                |               | The name of the command that the remote daemon should run                                                                                                                 |
| argument               |               | Set command line arguments                                                                                                                                                |
| separator              |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                  |               | Add multiple records using the separator format is: command|argument|argument                                                                                             |
| certificate            |               | Length of payload (has to be same as on the server)                                                                                                                       |
| dh                     |               | Length of payload (has to be same as on the server)                                                                                                                       |
| certificate-key        |               | Client certificate to use                                                                                                                                                 |
| certificate-format     |               | Client certificate format                                                                                                                                                 |
| ca                     |               | Certificate authority                                                                                                                                                     |
| verify                 |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers        |               | Client certificate format                                                                                                                                                 |
| [ssl](#nrpe_query_ssl) | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| insecure               | N/A           | Use insecure legacy mode                                                                                                                                                  |
| payload-length         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| buffer-length          |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="nrpe_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`


### submit_nrpe

Submit information to remote host via NRPE. (Most likely you want nrpe_query).


* [Command-line Arguments](#submit_nrpe_options)





<a name="submit_nrpe_help"/>
<a name="submit_nrpe_help-pb"/>
<a name="submit_nrpe_show-default"/>
<a name="submit_nrpe_help-short"/>
<a name="submit_nrpe_host"/>
<a name="submit_nrpe_port"/>
<a name="submit_nrpe_address"/>
<a name="submit_nrpe_timeout"/>
<a name="submit_nrpe_target"/>
<a name="submit_nrpe_retry"/>
<a name="submit_nrpe_retries"/>
<a name="submit_nrpe_source-host"/>
<a name="submit_nrpe_sender-host"/>
<a name="submit_nrpe_command"/>
<a name="submit_nrpe_alias"/>
<a name="submit_nrpe_message"/>
<a name="submit_nrpe_result"/>
<a name="submit_nrpe_separator"/>
<a name="submit_nrpe_batch"/>
<a name="submit_nrpe_certificate"/>
<a name="submit_nrpe_dh"/>
<a name="submit_nrpe_certificate-key"/>
<a name="submit_nrpe_certificate-format"/>
<a name="submit_nrpe_ca"/>
<a name="submit_nrpe_verify"/>
<a name="submit_nrpe_allowed-ciphers"/>
<a name="submit_nrpe_insecure"/>
<a name="submit_nrpe_payload-length"/>
<a name="submit_nrpe_buffer-length"/>
<a name="submit_nrpe_options"/>
#### Command-line Arguments


| Option                  | Default Value | Description                                                                                                                                                               |
|-------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help                    | N/A           | Show help screen (this screen)                                                                                                                                            |
| help-pb                 | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| show-default            | N/A           | Show default values for a given command                                                                                                                                   |
| help-short              | N/A           | Show help screen (short format).                                                                                                                                          |
| host                    |               | The host of the host running the server                                                                                                                                   |
| port                    |               | The port of the host running the server                                                                                                                                   |
| address                 |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                 |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                  |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                   |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                 |               | legacy version of retry                                                                                                                                                   |
| source-host             |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host             |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                 |               | The name of the command that the remote daemon should run                                                                                                                 |
| alias                   |               | Same as command                                                                                                                                                           |
| message                 |               | Message                                                                                                                                                                   |
| result                  |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| separator               |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                   |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| certificate             |               | Length of payload (has to be same as on the server)                                                                                                                       |
| dh                      |               | Length of payload (has to be same as on the server)                                                                                                                       |
| certificate-key         |               | Client certificate to use                                                                                                                                                 |
| certificate-format      |               | Client certificate format                                                                                                                                                 |
| ca                      |               | Certificate authority                                                                                                                                                     |
| verify                  |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers         |               | Client certificate format                                                                                                                                                 |
| [ssl](#submit_nrpe_ssl) | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| insecure                | N/A           | Use insecure legacy mode                                                                                                                                                  |
| payload-length          |               | Length of payload (has to be same as on the server)                                                                                                                       |
| buffer-length           |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |



<h5 id="submit_nrpe_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`




## Configuration



| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NRPE/client](#nrpe-client-section)               | NRPE CLIENT SECTION       |
| [/settings/NRPE/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NRPE/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### NRPE CLIENT SECTION <a id="/settings/NRPE/client"/>

Section for NRPE active/passive check module.




| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | NRPE          | CHANNEL     |



```ini
# Section for NRPE active/passive check module.
[/settings/NRPE/client]
channel=NRPE

```





#### CHANNEL <a id="/settings/NRPE/client/channel"></a>

The channel to listen to.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/client](#/settings/NRPE/client) |
| Key:           | channel                                         |
| Default value: | `NRPE`                                          |
| Used by:       | NRPEClient                                      |


**Sample:**

```
[/settings/NRPE/client]
# CHANNEL
channel=NRPE
```


### CLIENT HANDLER SECTION <a id="/settings/NRPE/client/handlers"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NRPE/client/targets"/>




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
| insecure           |               | Insecure legacy mode  |
| payload length     |               | PAYLOAD LENGTH        |
| port               |               | TARGET PORT           |
| retries            | 3             | RETRIES               |
| timeout            | 30            | TIMEOUT               |
| use ssl            |               | ENABLE SSL ENCRYPTION |
| verify mode        |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NRPE/client/targets/sample]
#address=...
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#host=...
#insecure=...
#payload length=...
#port=...
retries=3
timeout=30
#use ssl=...
#verify mode=...

```






