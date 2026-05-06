# NSCANgClient

NSCA-NG client can be used both from command line and from queries to submit passive checks via NSCA-NG (TLS-based NSCA next generation)



## Enable module

To enable this module and and allow using the commands you need to ass `NSCANgClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
NSCANgClient = enabled
```


## Queries

A quick reference for all available queries (check commands) in the NSCANgClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                           | Description                                                                                                                                                                                              |
|-----------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [submit_nsca_ng](#submit_nsca_ng) | Submit information to the remote NSCA-NG server. Custom relay commands defined under [/settings/NSCA-NG/client/handlers] are registered automatically using the same `submit_<alias>` naming convention. |




### submit_nsca_ng

Submit information to the remote NSCA-NG server. Custom relay commands defined under [/settings/NSCA-NG/client/handlers] are registered automatically using the same `submit_<alias>` naming convention.


**Jump to section:**

* [Command-line Arguments](#submit_nsca_ng_options)





<a id="submit_nsca_ng_help"></a>
<a id="submit_nsca_ng_help-pb"></a>
<a id="submit_nsca_ng_show-default"></a>
<a id="submit_nsca_ng_help-short"></a>
<a id="submit_nsca_ng_host"></a>
<a id="submit_nsca_ng_port"></a>
<a id="submit_nsca_ng_address"></a>
<a id="submit_nsca_ng_timeout"></a>
<a id="submit_nsca_ng_target"></a>
<a id="submit_nsca_ng_retry"></a>
<a id="submit_nsca_ng_retries"></a>
<a id="submit_nsca_ng_source-host"></a>
<a id="submit_nsca_ng_sender-host"></a>
<a id="submit_nsca_ng_command"></a>
<a id="submit_nsca_ng_alias"></a>
<a id="submit_nsca_ng_message"></a>
<a id="submit_nsca_ng_result"></a>
<a id="submit_nsca_ng_separator"></a>
<a id="submit_nsca_ng_batch"></a>
<a id="submit_nsca_ng_certificate"></a>
<a id="submit_nsca_ng_dh"></a>
<a id="submit_nsca_ng_certificate-key"></a>
<a id="submit_nsca_ng_certificate-format"></a>
<a id="submit_nsca_ng_ca"></a>
<a id="submit_nsca_ng_verify"></a>
<a id="submit_nsca_ng_allowed-ciphers"></a>
<a id="submit_nsca_ng_password"></a>
<a id="submit_nsca_ng_identity"></a>
<a id="submit_nsca_ng_hostname"></a>
<a id="submit_nsca_ng_no-psk"></a>
<a id="submit_nsca_ng_host-check"></a>
<a id="submit_nsca_ng_max-output-length"></a>
<a id="submit_nsca_ng_options"></a>
#### Command-line Arguments


| Option                     | Default Value | Description                                                                                        |
|----------------------------|---------------|----------------------------------------------------------------------------------------------------|
| help                       | N/A           | Show help screen (this screen)                                                                     |
| help-pb                    | N/A           | Show help screen as a protocol buffer payload                                                      |
| show-default               | N/A           | Show default values for a given command                                                            |
| help-short                 | N/A           | Show help screen (short format).                                                                   |
| host                       |               | The host of the host running the server                                                            |
| port                       |               | The port of the host running the server                                                            |
| address                    |               | The address (host:port) of the host running the server                                             |
| timeout                    |               | Number of seconds before connection times out (default=10)                                         |
| target                     |               | Target to use (lookup connection info from config)                                                 |
| retry                      |               | Number of times ti retry a failed connection attempt (default=2)                                   |
| retries                    |               | legacy version of retry                                                                            |
| source-host                |               | Source/sender host name (default is auto which means use the name of the actual host)              |
| sender-host                |               | Source/sender host name (default is auto which means use the name of the actual host)              |
| command                    |               | The name of the command that the remote daemon should run                                          |
| alias                      |               | Same as command                                                                                    |
| message                    |               | Message                                                                                            |
| result                     |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                             |
| separator                  |               | Separator to use for the batch command (default is |)                                              |
| batch                      |               | Add multiple records using the separator format is: command|result|message                         |
| certificate                |               | The client certificate to use                                                                      |
| dh                         |               | The DH key to use                                                                                  |
| certificate-key            |               | Client certificate to use                                                                          |
| certificate-format         |               | Client certificate format                                                                          |
| ca                         |               | Certificate authority                                                                              |
| verify                     |               | Client certificate format                                                                          |
| allowed-ciphers            |               | Client certificate format                                                                          |
| [ssl](#submit_nsca_ng_ssl) | 1             | Initial an ssl handshake with the server.                                                          |
| password                   |               | The PSK password (must match the NSCA-NG server configuration)                                     |
| identity                   |               | PSK identity string (defaults to hostname when empty)                                              |
| hostname                   |               | Host name to report to the NSCA-NG server                                                          |
| no-psk                     | N/A           | Disable PSK and use certificate-based TLS authentication instead                                   |
| host-check                 | N/A           | Submit every result as a Nagios host check (PROCESS_HOST_CHECK_RESULT) instead of a service check. |
| max-output-length          |               | Maximum bytes of plugin output forwarded over the wire (default 65536)                             |



<h5 id="submit_nsca_ng_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`




## Configuration



| Path / Section                                                 | Description               |
|----------------------------------------------------------------|---------------------------|
| [/settings/NSCA-NG/client](#nsca-ng-client-section)            | NSCA-NG CLIENT SECTION    |
| [/settings/NSCA-NG/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NSCA-NG/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### NSCA-NG CLIENT SECTION <a id="/settings/NSCA-NG/client"></a>

Section for NSCA-NG passive check module.




| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [channel](#channel)   | NSCA-NG       | CHANNEL     |
| [hostname](#hostname) | auto          | HOSTNAME    |



```ini
# Section for NSCA-NG passive check module.
[/settings/NSCA-NG/client]
channel=NSCA-NG
hostname=auto

```





#### CHANNEL <a id="/settings/NSCA-NG/client/channel"></a>

The channel to listen to.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/NSCA-NG/client](#/settings/NSCA-NG/client) |
| Key:           | channel                                               |
| Default value: | `NSCA-NG`                                             |


**Sample:**

```
[/settings/NSCA-NG/client]
# CHANNEL
channel=NSCA-NG
```



#### HOSTNAME <a id="/settings/NSCA-NG/client/hostname"></a>

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






| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/NSCA-NG/client](#/settings/NSCA-NG/client) |
| Key:           | hostname                                              |
| Default value: | `auto`                                                |


**Sample:**

```
[/settings/NSCA-NG/client]
# HOSTNAME
hostname=auto
```


### CLIENT HANDLER SECTION <a id="/settings/NSCA-NG/client/handlers"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NSCA-NG/client/targets"></a>




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
| ciphers            |               | TLS CIPHERS           |
| dh                 |               | DH KEY                |
| host               |               | TARGET HOST           |
| host check         | false         | HOST CHECK            |
| identity           |               | IDENTITY              |
| max output length  | 65536         | MAX OUTPUT LENGTH     |
| password           |               | PASSWORD              |
| port               |               | TARGET PORT           |
| retries            | 3             | RETRIES               |
| timeout            | 30            | TIMEOUT               |
| use psk            | true          | USE PSK               |
| use ssl            |               | ENABLE SSL ENCRYPTION |
| verify mode        |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NSCA-NG/client/targets/sample]
#address=...
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#ciphers=...
#dh=...
#host=...
host check=false
#identity=...
max output length=65536
#password=...
#port=...
retries=3
timeout=30
use psk=true
#use ssl=...
#verify mode=...

```






