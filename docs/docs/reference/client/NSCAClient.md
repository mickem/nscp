# NSCAClient

NSCA client can be used both from command line and from queries to submit passive checks via NSCA




## Queries

A quick reference for all available queries (check commands) in the NSCAClient module.

**List of commands:**

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_nsca](#submit_nsca) | Submit information to the remote NSCA server. |




### submit_nsca

Submit information to the remote NSCA server.


* [Command-line Arguments](#submit_nsca_options)





<a name="submit_nsca_help"/>
<a name="submit_nsca_help-pb"/>
<a name="submit_nsca_show-default"/>
<a name="submit_nsca_help-short"/>
<a name="submit_nsca_host"/>
<a name="submit_nsca_port"/>
<a name="submit_nsca_address"/>
<a name="submit_nsca_timeout"/>
<a name="submit_nsca_target"/>
<a name="submit_nsca_retry"/>
<a name="submit_nsca_retries"/>
<a name="submit_nsca_source-host"/>
<a name="submit_nsca_sender-host"/>
<a name="submit_nsca_command"/>
<a name="submit_nsca_alias"/>
<a name="submit_nsca_message"/>
<a name="submit_nsca_result"/>
<a name="submit_nsca_separator"/>
<a name="submit_nsca_batch"/>
<a name="submit_nsca_certificate"/>
<a name="submit_nsca_dh"/>
<a name="submit_nsca_certificate-key"/>
<a name="submit_nsca_certificate-format"/>
<a name="submit_nsca_ca"/>
<a name="submit_nsca_verify"/>
<a name="submit_nsca_allowed-ciphers"/>
<a name="submit_nsca_payload-length"/>
<a name="submit_nsca_buffer-length"/>
<a name="submit_nsca_password"/>
<a name="submit_nsca_time-offset"/>
<a name="submit_nsca_options"/>
#### Command-line Arguments


| Option                                | Default Value | Description                                                                                                                                                               |
|---------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| help                                  | N/A           | Show help screen (this screen)                                                                                                                                            |
| help-pb                               | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| show-default                          | N/A           | Show default values for a given command                                                                                                                                   |
| help-short                            | N/A           | Show help screen (short format).                                                                                                                                          |
| host                                  |               | The host of the host running the server                                                                                                                                   |
| port                                  |               | The port of the host running the server                                                                                                                                   |
| address                               |               | The address (host:port) of the host running the server                                                                                                                    |
| timeout                               |               | Number of seconds before connection times out (default=10)                                                                                                                |
| target                                |               | Target to use (lookup connection info from config)                                                                                                                        |
| retry                                 |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| retries                               |               | legacy version of retry                                                                                                                                                   |
| source-host                           |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| sender-host                           |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| command                               |               | The name of the command that the remote daemon should run                                                                                                                 |
| alias                                 |               | Same as command                                                                                                                                                           |
| message                               |               | Message                                                                                                                                                                   |
| result                                |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| separator                             |               | Separator to use for the batch command (default is |)                                                                                                                     |
| batch                                 |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| certificate                           |               | Length of payload (has to be same as on the server)                                                                                                                       |
| dh                                    |               | Length of payload (has to be same as on the server)                                                                                                                       |
| certificate-key                       |               | Client certificate to use                                                                                                                                                 |
| certificate-format                    |               | Client certificate format                                                                                                                                                 |
| ca                                    |               | Certificate authority                                                                                                                                                     |
| verify                                |               | Client certificate format                                                                                                                                                 |
| allowed-ciphers                       |               | Client certificate format                                                                                                                                                 |
| [ssl](#submit_nsca_ssl)               | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| [encryption](#submit_nsca_encryption) |               | Name of encryption algorithm to use.                                                                                                                                      |
| payload-length                        |               | Length of payload (has to be same as on the server)                                                                                                                       |
| buffer-length                         |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |
| password                              |               | Password                                                                                                                                                                  |
| time-offset                           |               |                                                                                                                                                                           |



<h5 id="submit_nsca_ssl">ssl:</h5>

Initial an ssl handshake with the server.

*Default Value:* `1`

<h5 id="submit_nsca_encryption">encryption:</h5>

Name of encryption algorithm to use.
Has to be the same as your server i using or it wont work at all.This is also independent of SSL and generally used instead of SSL.
Available encryption algorithms are:
none = No Encryption (not safe)
xor = XOR
des = DES
3des = DES-EDE3
cast128 = CAST-128
xtea = XTEA
blowfish = Blowfish
twofish = Twofish
rc2 = RC2
aes128 = AES
aes192 = AES
aes = AES
serpent = Serpent
gost = GOST





## Configuration



| Path / Section                                              | Description               |
|-------------------------------------------------------------|---------------------------|
| [/settings/NSCA/client](#nsca-client-section)               | NSCA CLIENT SECTION       |
| [/settings/NSCA/client/handlers](#client-handler-section)   | CLIENT HANDLER SECTION    |
| [/settings/NSCA/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### NSCA CLIENT SECTION <a id="/settings/NSCA/client"/>

Section for NSCA passive check module.




| Key                             | Default Value | Description        |
|---------------------------------|---------------|--------------------|
| [channel](#channel)             | NSCA          | CHANNEL            |
| [encoding](#nsca-data-encoding) |               | NSCA DATA ENCODING |
| [hostname](#hostname)           | auto          | HOSTNAME           |



```ini
# Section for NSCA passive check module.
[/settings/NSCA/client]
channel=NSCA
hostname=auto

```





#### CHANNEL <a id="/settings/NSCA/client/channel"></a>

The channel to listen to.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | channel                                         |
| Default value: | `NSCA`                                          |
| Used by:       | NSCAClient                                      |


**Sample:**

```
[/settings/NSCA/client]
# CHANNEL
channel=NSCA
```



#### NSCA DATA ENCODING <a id="/settings/NSCA/client/encoding"></a>








| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | encoding                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAClient                                      |


**Sample:**

```
[/settings/NSCA/client]
# NSCA DATA ENCODING
encoding=
```



#### HOSTNAME <a id="/settings/NSCA/client/hostname"></a>

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
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | hostname                                        |
| Default value: | `auto`                                          |
| Used by:       | NSCAClient                                      |


**Sample:**

```
[/settings/NSCA/client]
# HOSTNAME
hostname=auto
```


### CLIENT HANDLER SECTION <a id="/settings/NSCA/client/handlers"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### REMOTE TARGET DEFINITIONS <a id="/settings/NSCA/client/targets"/>




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
| encoding           |               | ENCODING              |
| encryption         | aes           | ENCRYPTION            |
| host               |               | TARGET HOST           |
| password           |               | PASSWORD              |
| payload length     | 512           | PAYLOAD LENGTH        |
| port               |               | TARGET PORT           |
| retries            | 3             | RETRIES               |
| time offset        | 0             | TIME OFFSET           |
| timeout            | 30            | TIMEOUT               |
| use ssl            |               | ENABLE SSL ENCRYPTION |
| verify mode        |               | VERIFY MODE           |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/NSCA/client/targets/sample]
#address=...
#allowed ciphers=...
#ca=...
#certificate=...
#certificate format=...
#certificate key=...
#dh=...
#encoding=...
encryption=aes
#host=...
#password=...
payload length=512
#port=...
retries=3
time offset=0
timeout=30
#use ssl=...
#verify mode=...

```






