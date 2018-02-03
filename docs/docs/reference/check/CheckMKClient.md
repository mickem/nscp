# CheckMKClient

check_mk client can be used both from command line and from queries to check remote systes via check_mk




## Queries

A quick reference for all available queries (check commands) in the CheckMKClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                           | Description                              |
|-----------------------------------|------------------------------------------|
| [check_mk_query](#check_mk_query) | Request remote information via check_mk. |




### check_mk_query

Request remote information via check_mk.


* [Command-line Arguments](#check_mk_query_options)





<a name="check_mk_query_help"/>
<a name="check_mk_query_help-pb"/>
<a name="check_mk_query_show-default"/>
<a name="check_mk_query_help-short"/>
<a name="check_mk_query_host"/>
<a name="check_mk_query_port"/>
<a name="check_mk_query_address"/>
<a name="check_mk_query_timeout"/>
<a name="check_mk_query_target"/>
<a name="check_mk_query_retry"/>
<a name="check_mk_query_retries"/>
<a name="check_mk_query_source-host"/>
<a name="check_mk_query_sender-host"/>
<a name="check_mk_query_command"/>
<a name="check_mk_query_argument"/>
<a name="check_mk_query_separator"/>
<a name="check_mk_query_batch"/>
<a name="check_mk_query_certificate"/>
<a name="check_mk_query_dh"/>
<a name="check_mk_query_certificate-key"/>
<a name="check_mk_query_certificate-format"/>
<a name="check_mk_query_ca"/>
<a name="check_mk_query_verify"/>
<a name="check_mk_query_allowed-ciphers"/>
<a name="check_mk_query_options"/>
#### Command-line Arguments


| Option                     | Default Value | Description                                                                           |
|----------------------------|---------------|---------------------------------------------------------------------------------------|
| help                       | N/A           | Show help screen (this screen)                                                        |
| help-pb                    | N/A           | Show help screen as a protocol buffer payload                                         |
| show-default               | N/A           | Show default values for a given command                                               |
| help-short                 | N/A           | Show help screen (short format).                                                      |
| host                       |               | The host of the host running the server                                               |
| port                       |               | The port of the host running the server                                               |
| address                    |               | The address (host:port) of the host running the server                                |
| timeout                    |               | Number of seconds before connection times out (default=10)                            |
| target                     |               | Target to use (lookup connection info from config)                                    |
| retry                      |               | Number of times ti retry a failed connection attempt (default=2)                      |
| retries                    |               | legacy version of retry                                                               |
| source-host                |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host                |               | Source/sender host name (default is auto which means use the name of the actual host) |
| command                    |               | The name of the command that the remote daemon should run                             |
| argument                   |               | Set command line arguments                                                            |
| separator                  |               | Separator to use for the batch command (default is |)                                 |
| batch                      |               | Add multiple records using the separator format is: command|argument|argument         |
| certificate                |               | Length of payload (has to be same as on the server)                                   |
| dh                         |               | Length of payload (has to be same as on the server)                                   |
| certificate-key            |               | Client certificate to use                                                             |
| certificate-format         |               | Client certificate format                                                             |
| ca                         |               | Certificate authority                                                                 |
| verify                     |               | Client certificate format                                                             |
| allowed-ciphers            |               | Client certificate format                                                             |
| [ssl](#check_mk_query_ssl) | 1             | Initial an ssl handshake with the server.                                             |



<h5 id="check_mk_query_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`




## Configuration



| Path / Section                                                  | Description               |
|-----------------------------------------------------------------|---------------------------|
| [/settings/check_mk/client](#check-mk-client-section)           | CHECK MK CLIENT SECTION   |
| [/settings/check_mk/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/check_mk/client/scripts](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |
| [/settings/check_mk/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### CHECK MK CLIENT SECTION <a id="/settings/check_mk/client"/>

Section for check_mk active/passive check module.




| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | CheckMK       | CHANNEL     |



```ini
# Section for check_mk active/passive check module.
[/settings/check_mk/client]
channel=CheckMK

```





#### CHANNEL <a id="/settings/check_mk/client/channel"></a>

The channel to listen to.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/client](#/settings/check_mk/client) |
| Key:           | channel                                                 |
| Default value: | `CheckMK`                                               |
| Used by:       | CheckMKClient                                           |


**Sample:**

```
[/settings/check_mk/client]
# CHANNEL
channel=CheckMK
```


### CLIENT HANDLER SECTION <a id="/settings/check_mk/client/handlers"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/check_mk/client/scripts"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/check_mk/client/targets"/>




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
[/settings/check_mk/client/targets/sample]
#address=...
#host=...
#port=...
retries=3
timeout=30

```






