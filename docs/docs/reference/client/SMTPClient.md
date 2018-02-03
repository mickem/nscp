# SMTPClient

SMTP client can be used both from command line and from queries to check remote systes via SMTP




## Queries

A quick reference for all available queries (check commands) in the SMTPClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_smtp](#submit_smtp) | Submit information to the remote SMTP server. |




### submit_smtp

Submit information to the remote SMTP server.


* [Command-line Arguments](#submit_smtp_options)





<a name="submit_smtp_help"/>
<a name="submit_smtp_help-pb"/>
<a name="submit_smtp_show-default"/>
<a name="submit_smtp_help-short"/>
<a name="submit_smtp_host"/>
<a name="submit_smtp_port"/>
<a name="submit_smtp_address"/>
<a name="submit_smtp_timeout"/>
<a name="submit_smtp_target"/>
<a name="submit_smtp_retry"/>
<a name="submit_smtp_retries"/>
<a name="submit_smtp_source-host"/>
<a name="submit_smtp_sender-host"/>
<a name="submit_smtp_command"/>
<a name="submit_smtp_alias"/>
<a name="submit_smtp_message"/>
<a name="submit_smtp_result"/>
<a name="submit_smtp_separator"/>
<a name="submit_smtp_batch"/>
<a name="submit_smtp_sender"/>
<a name="submit_smtp_recipient"/>
<a name="submit_smtp_template"/>
<a name="submit_smtp_source-host"/>
<a name="submit_smtp_sender-host"/>
<a name="submit_smtp_options"/>
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
| sender       |               | Length of payload (has to be same as on the server)                                   |
| recipient    |               | Length of payload (has to be same as on the server)                                   |
| template     |               | Do not initial an ssl handshake with the server, talk in plain text.                  |
| source-host  |               | Source/sender host name (default is auto which means use the name of the actual host) |
| sender-host  |               | Source/sender host name (default is auto which means use the name of the actual host) |






## Configuration



| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/SMTP/client](#smtp-client-section)               | SMTP CLIENT SECTION       |
| [/settings/SMTP/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/SMTP/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### SMTP CLIENT SECTION <a id="/settings/SMTP/client"/>

Section for SMTP passive check module.




| Key                 | Default Value | Description |
|---------------------|---------------|-------------|
| [channel](#channel) | SMTP          | CHANNEL     |



```ini
# Section for SMTP passive check module.
[/settings/SMTP/client]
channel=SMTP

```





#### CHANNEL <a id="/settings/SMTP/client/channel"></a>

The channel to listen to.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/SMTP/client](#/settings/SMTP/client) |
| Key:           | channel                                         |
| Default value: | `SMTP`                                          |
| Used by:       | SMTPClient                                      |


**Sample:**

```
[/settings/SMTP/client]
# CHANNEL
channel=SMTP
```


### CLIENT HANDLER SECTION <a id="/settings/SMTP/client/handlers"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/SMTP/client/targets"/>




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
[/settings/SMTP/client/targets/sample]
#address=...
#host=...
#port=...
retries=3
timeout=30

```






