# NSCAClient

NSCA client can be used both from command line and from queries to submit passive checks via NSCA



## List of commands

A list of all available queries (check commands)

| Command                     | Description                                   |
|-----------------------------|-----------------------------------------------|
| [submit_nsca](#submit_nsca) | Submit information to the remote NSCA server. |




## List of Configuration


### Common Keys

| Path / Section                                                                  | Key                                                                       | Description           |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------|-----------------------|
| [/settings/NSCA/client](#/settings/NSCA/client)                                 | [channel](#/settings/NSCA/client_channel)                                 | CHANNEL               |
| [/settings/NSCA/client](#/settings/NSCA/client)                                 | [hostname](#/settings/NSCA/client_hostname)                               | HOSTNAME              |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [address](#/settings/NSCA/client/targets/default_address)                 | TARGET ADDRESS        |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [allowed ciphers](#/settings/NSCA/client/targets/default_allowed ciphers) | ALLOWED CIPHERS       |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [certificate](#/settings/NSCA/client/targets/default_certificate)         | SSL CERTIFICATE       |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [encryption](#/settings/NSCA/client/targets/default_encryption)           | ENCRYPTION            |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [password](#/settings/NSCA/client/targets/default_password)               | PASSWORD              |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [retries](#/settings/NSCA/client/targets/default_retries)                 | RETRIES               |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [timeout](#/settings/NSCA/client/targets/default_timeout)                 | TIMEOUT               |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [use ssl](#/settings/NSCA/client/targets/default_use ssl)                 | ENABLE SSL ENCRYPTION |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [verify mode](#/settings/NSCA/client/targets/default_verify mode)         | VERIFY MODE           |

### Advanced keys

| Path / Section                                                                  | Key                                                                             | Description        |
|---------------------------------------------------------------------------------|---------------------------------------------------------------------------------|--------------------|
| [/settings/NSCA/client](#/settings/NSCA/client)                                 | [encoding](#/settings/NSCA/client_encoding)                                     | NSCA DATA ENCODING |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [ca](#/settings/NSCA/client/targets/default_ca)                                 | CA                 |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [certificate format](#/settings/NSCA/client/targets/default_certificate format) | CERTIFICATE FORMAT |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [certificate key](#/settings/NSCA/client/targets/default_certificate key)       | SSL CERTIFICATE    |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [dh](#/settings/NSCA/client/targets/default_dh)                                 | DH KEY             |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [encoding](#/settings/NSCA/client/targets/default_encoding)                     | ENCODING           |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [host](#/settings/NSCA/client/targets/default_host)                             | TARGET HOST        |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [payload length](#/settings/NSCA/client/targets/default_payload length)         | PAYLOAD LENGTH     |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [port](#/settings/NSCA/client/targets/default_port)                             | TARGET PORT        |
| [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) | [time offset](#/settings/NSCA/client/targets/default_time offset)               | TIME OFFSET        |

### Sample keys

| Path / Section                                                                | Key                                                                            | Description           |
|-------------------------------------------------------------------------------|--------------------------------------------------------------------------------|-----------------------|
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [address](#/settings/NSCA/client/targets/sample_address)                       | TARGET ADDRESS        |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [allowed ciphers](#/settings/NSCA/client/targets/sample_allowed ciphers)       | ALLOWED CIPHERS       |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [ca](#/settings/NSCA/client/targets/sample_ca)                                 | CA                    |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [certificate](#/settings/NSCA/client/targets/sample_certificate)               | SSL CERTIFICATE       |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [certificate format](#/settings/NSCA/client/targets/sample_certificate format) | CERTIFICATE FORMAT    |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [certificate key](#/settings/NSCA/client/targets/sample_certificate key)       | SSL CERTIFICATE       |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [dh](#/settings/NSCA/client/targets/sample_dh)                                 | DH KEY                |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [encoding](#/settings/NSCA/client/targets/sample_encoding)                     | ENCODING              |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [encryption](#/settings/NSCA/client/targets/sample_encryption)                 | ENCRYPTION            |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [host](#/settings/NSCA/client/targets/sample_host)                             | TARGET HOST           |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [password](#/settings/NSCA/client/targets/sample_password)                     | PASSWORD              |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [payload length](#/settings/NSCA/client/targets/sample_payload length)         | PAYLOAD LENGTH        |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [port](#/settings/NSCA/client/targets/sample_port)                             | TARGET PORT           |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [retries](#/settings/NSCA/client/targets/sample_retries)                       | RETRIES               |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [time offset](#/settings/NSCA/client/targets/sample_time offset)               | TIME OFFSET           |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [timeout](#/settings/NSCA/client/targets/sample_timeout)                       | TIMEOUT               |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [use ssl](#/settings/NSCA/client/targets/sample_use ssl)                       | ENABLE SSL ENCRYPTION |
| [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) | [verify mode](#/settings/NSCA/client/targets/sample_verify mode)               | VERIFY MODE           |



# Queries

A quick reference for all available queries (check commands) in the NSCAClient module.

## submit_nsca

Submit information to the remote NSCA server.


### Usage


| Option                                                | Default Value | Description                                                                                                                                                               |
|-------------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [help](#submit_nsca_help)                             | N/A           | Show help screen (this screen)                                                                                                                                            |
| [help-pb](#submit_nsca_help-pb)                       | N/A           | Show help screen as a protocol buffer payload                                                                                                                             |
| [show-default](#submit_nsca_show-default)             | N/A           | Show default values for a given command                                                                                                                                   |
| [help-short](#submit_nsca_help-short)                 | N/A           | Show help screen (short format).                                                                                                                                          |
| [host](#submit_nsca_host)                             |               | The host of the host running the server                                                                                                                                   |
| [port](#submit_nsca_port)                             |               | The port of the host running the server                                                                                                                                   |
| [address](#submit_nsca_address)                       |               | The address (host:port) of the host running the server                                                                                                                    |
| [timeout](#submit_nsca_timeout)                       |               | Number of seconds before connection times out (default=10)                                                                                                                |
| [target](#submit_nsca_target)                         |               | Target to use (lookup connection info from config)                                                                                                                        |
| [retry](#submit_nsca_retry)                           |               | Number of times ti retry a failed connection attempt (default=2)                                                                                                          |
| [retries](#submit_nsca_retries)                       |               | legacy version of retry                                                                                                                                                   |
| [source-host](#submit_nsca_source-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [sender-host](#submit_nsca_sender-host)               |               | Source/sender host name (default is auto which means use the name of the actual host)                                                                                     |
| [command](#submit_nsca_command)                       |               | The name of the command that the remote daemon should run                                                                                                                 |
| [alias](#submit_nsca_alias)                           |               | Same as command                                                                                                                                                           |
| [message](#submit_nsca_message)                       |               | Message                                                                                                                                                                   |
| [result](#submit_nsca_result)                         |               | Result code either a number or OK, WARN, CRIT, UNKNOWN                                                                                                                    |
| [separator](#submit_nsca_separator)                   |               | Separator to use for the batch command (default is |)                                                                                                                     |
| [batch](#submit_nsca_batch)                           |               | Add multiple records using the separator format is: command|result|message                                                                                                |
| [certificate](#submit_nsca_certificate)               |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [dh](#submit_nsca_dh)                                 |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [certificate-key](#submit_nsca_certificate-key)       |               | Client certificate to use                                                                                                                                                 |
| [certificate-format](#submit_nsca_certificate-format) |               | Client certificate format                                                                                                                                                 |
| [ca](#submit_nsca_ca)                                 |               | Certificate authority                                                                                                                                                     |
| [verify](#submit_nsca_verify)                         |               | Client certificate format                                                                                                                                                 |
| [allowed-ciphers](#submit_nsca_allowed-ciphers)       |               | Client certificate format                                                                                                                                                 |
| [ssl](#submit_nsca_ssl)                               | 1             | Initial an ssl handshake with the server.                                                                                                                                 |
| [encryption](#submit_nsca_encryption)                 |               | Name of encryption algorithm to use.                                                                                                                                      |
| [payload-length](#submit_nsca_payload-length)         |               | Length of payload (has to be same as on the server)                                                                                                                       |
| [buffer-length](#submit_nsca_buffer-length)           |               | Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work. |
| [password](#submit_nsca_password)                     |               | Password                                                                                                                                                                  |
| [time-offset](#submit_nsca_time-offset)               |               |                                                                                                                                                                           |


<a name="submit_nsca_help"/>
### help



**Description:**
Show help screen (this screen)

<a name="submit_nsca_help-pb"/>
### help-pb



**Description:**
Show help screen as a protocol buffer payload

<a name="submit_nsca_show-default"/>
### show-default



**Description:**
Show default values for a given command

<a name="submit_nsca_help-short"/>
### help-short



**Description:**
Show help screen (short format).

<a name="submit_nsca_host"/>
### host



**Description:**
The host of the host running the server

<a name="submit_nsca_port"/>
### port



**Description:**
The port of the host running the server

<a name="submit_nsca_address"/>
### address



**Description:**
The address (host:port) of the host running the server

<a name="submit_nsca_timeout"/>
### timeout



**Description:**
Number of seconds before connection times out (default=10)

<a name="submit_nsca_target"/>
### target



**Description:**
Target to use (lookup connection info from config)

<a name="submit_nsca_retry"/>
### retry



**Description:**
Number of times ti retry a failed connection attempt (default=2)

<a name="submit_nsca_retries"/>
### retries



**Description:**
legacy version of retry

<a name="submit_nsca_source-host"/>
### source-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_nsca_sender-host"/>
### sender-host



**Description:**
Source/sender host name (default is auto which means use the name of the actual host)

<a name="submit_nsca_command"/>
### command



**Description:**
The name of the command that the remote daemon should run

<a name="submit_nsca_alias"/>
### alias



**Description:**
Same as command

<a name="submit_nsca_message"/>
### message



**Description:**
Message

<a name="submit_nsca_result"/>
### result



**Description:**
Result code either a number or OK, WARN, CRIT, UNKNOWN

<a name="submit_nsca_separator"/>
### separator



**Description:**
Separator to use for the batch command (default is |)

<a name="submit_nsca_batch"/>
### batch



**Description:**
Add multiple records using the separator format is: command|result|message

<a name="submit_nsca_certificate"/>
### certificate



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_nsca_dh"/>
### dh



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_nsca_certificate-key"/>
### certificate-key



**Description:**
Client certificate to use

<a name="submit_nsca_certificate-format"/>
### certificate-format



**Description:**
Client certificate format

<a name="submit_nsca_ca"/>
### ca



**Description:**
Certificate authority

<a name="submit_nsca_verify"/>
### verify



**Description:**
Client certificate format

<a name="submit_nsca_allowed-ciphers"/>
### allowed-ciphers



**Description:**
Client certificate format

<a name="submit_nsca_ssl"/>
### ssl


**Deafult Value:** 1

**Description:**
Initial an ssl handshake with the server.

<a name="submit_nsca_encryption"/>
### encryption



**Description:**
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

<a name="submit_nsca_payload-length"/>
### payload-length



**Description:**
Length of payload (has to be same as on the server)

<a name="submit_nsca_buffer-length"/>
### buffer-length



**Description:**
Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.

<a name="submit_nsca_password"/>
### password



**Description:**
Password

<a name="submit_nsca_time-offset"/>
### time-offset



**Description:**




# Configuration

<a name="/settings/NSCA/client"/>
## NSCA CLIENT SECTION

Section for NSCA passive check module.

```ini
# Section for NSCA passive check module.
[/settings/NSCA/client]
channel=NSCA
hostname=auto

```


| Key                                         | Default Value | Description        |
|---------------------------------------------|---------------|--------------------|
| [channel](#/settings/NSCA/client_channel)   | NSCA          | CHANNEL            |
| [encoding](#/settings/NSCA/client_encoding) |               | NSCA DATA ENCODING |
| [hostname](#/settings/NSCA/client_hostname) | auto          | HOSTNAME           |




<a name="/settings/NSCA/client_channel"/>
### channel

**CHANNEL**

The channel to listen to.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | channel                                         |
| Default value: | `NSCA`                                          |
| Used by:       | NSCAClient                                      |


#### Sample

```
[/settings/NSCA/client]
# CHANNEL
channel=NSCA
```


<a name="/settings/NSCA/client_encoding"/>
### encoding

**NSCA DATA ENCODING**







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | encoding                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAClient                                      |


#### Sample

```
[/settings/NSCA/client]
# NSCA DATA ENCODING
encoding=
```


<a name="/settings/NSCA/client_hostname"/>
### hostname

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
| Path:          | [/settings/NSCA/client](#/settings/NSCA/client) |
| Key:           | hostname                                        |
| Default value: | `auto`                                          |
| Used by:       | NSCAClient                                      |


#### Sample

```
[/settings/NSCA/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/NSCA/client/handlers"/>
## CLIENT HANDLER SECTION



```ini
# 
[/settings/NSCA/client/handlers]

```






<a name="/settings/NSCA/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/NSCA/client/targets]

```






<a name="/settings/NSCA/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/NSCA/client/targets/default]
encryption=aes
payload length=512
retries=3
time offset=0
timeout=30

```


| Key                                                                             | Default Value | Description           |
|---------------------------------------------------------------------------------|---------------|-----------------------|
| [address](#/settings/NSCA/client/targets/default_address)                       |               | TARGET ADDRESS        |
| [allowed ciphers](#/settings/NSCA/client/targets/default_allowed ciphers)       |               | ALLOWED CIPHERS       |
| [ca](#/settings/NSCA/client/targets/default_ca)                                 |               | CA                    |
| [certificate](#/settings/NSCA/client/targets/default_certificate)               |               | SSL CERTIFICATE       |
| [certificate format](#/settings/NSCA/client/targets/default_certificate format) |               | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NSCA/client/targets/default_certificate key)       |               | SSL CERTIFICATE       |
| [dh](#/settings/NSCA/client/targets/default_dh)                                 |               | DH KEY                |
| [encoding](#/settings/NSCA/client/targets/default_encoding)                     |               | ENCODING              |
| [encryption](#/settings/NSCA/client/targets/default_encryption)                 | aes           | ENCRYPTION            |
| [host](#/settings/NSCA/client/targets/default_host)                             |               | TARGET HOST           |
| [password](#/settings/NSCA/client/targets/default_password)                     |               | PASSWORD              |
| [payload length](#/settings/NSCA/client/targets/default_payload length)         | 512           | PAYLOAD LENGTH        |
| [port](#/settings/NSCA/client/targets/default_port)                             |               | TARGET PORT           |
| [retries](#/settings/NSCA/client/targets/default_retries)                       | 3             | RETRIES               |
| [time offset](#/settings/NSCA/client/targets/default_time offset)               | 0             | TIME OFFSET           |
| [timeout](#/settings/NSCA/client/targets/default_timeout)                       | 30            | TIMEOUT               |
| [use ssl](#/settings/NSCA/client/targets/default_use ssl)                       |               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NSCA/client/targets/default_verify mode)               |               | VERIFY MODE           |




<a name="/settings/NSCA/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | address                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/NSCA/client/targets/default_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | allowed ciphers                                                                 |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# ALLOWED CIPHERS
allowed ciphers=
```


<a name="/settings/NSCA/client/targets/default_ca"/>
### ca

**CA**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | ca                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# CA
ca=
```


<a name="/settings/NSCA/client/targets/default_certificate"/>
### certificate

**SSL CERTIFICATE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | certificate                                                                     |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# SSL CERTIFICATE
certificate=
```


<a name="/settings/NSCA/client/targets/default_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | certificate format                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# CERTIFICATE FORMAT
certificate format=
```


<a name="/settings/NSCA/client/targets/default_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | certificate key                                                                 |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NSCA/client/targets/default_dh"/>
### dh

**DH KEY**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | dh                                                                              |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# DH KEY
dh=
```


<a name="/settings/NSCA/client/targets/default_encoding"/>
### encoding

**ENCODING**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | encoding                                                                        |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# ENCODING
encoding=
```


<a name="/settings/NSCA/client/targets/default_encryption"/>
### encryption

**ENCRYPTION**

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




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | encryption                                                                      |
| Default value: | `aes`                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# ENCRYPTION
encryption=aes
```


<a name="/settings/NSCA/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | host                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/NSCA/client/targets/default_password"/>
### password

**PASSWORD**

The password to use. Again has to be the same as the server or it wont work at all.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | password                                                                        |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# PASSWORD
password=
```


<a name="/settings/NSCA/client/targets/default_payload length"/>
### payload length

**PAYLOAD LENGTH**

Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | payload length                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | `512`                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# PAYLOAD LENGTH
payload length=512
```


<a name="/settings/NSCA/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | port                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/NSCA/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | retries                                                                         |
| Default value: | `3`                                                                             |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/NSCA/client/targets/default_time offset"/>
### time offset

**TIME OFFSET**

Time offset.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | time offset                                                                     |
| Advanced:      | Yes (means it is not commonly used)                                             |
| Default value: | `0`                                                                             |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# TIME OFFSET
time offset=0
```


<a name="/settings/NSCA/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | timeout                                                                         |
| Default value: | `30`                                                                            |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/NSCA/client/targets/default_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.





| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | use ssl                                                                         |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# ENABLE SSL ENCRYPTION
use ssl=
```


<a name="/settings/NSCA/client/targets/default_verify mode"/>
### verify mode

**VERIFY MODE**







| Key            | Description                                                                     |
|----------------|---------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/default](#/settings/NSCA/client/targets/default) |
| Key:           | verify mode                                                                     |
| Default value: | _N/A_                                                                           |
| Used by:       | NSCAClient                                                                      |


#### Sample

```
[/settings/NSCA/client/targets/default]
# VERIFY MODE
verify mode=
```


<a name="/settings/NSCA/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/NSCA/client/targets/sample]
encryption=aes
payload length=512
retries=3
time offset=0
timeout=30

```


| Key                                                                            | Default Value | Description           |
|--------------------------------------------------------------------------------|---------------|-----------------------|
| [address](#/settings/NSCA/client/targets/sample_address)                       |               | TARGET ADDRESS        |
| [allowed ciphers](#/settings/NSCA/client/targets/sample_allowed ciphers)       |               | ALLOWED CIPHERS       |
| [ca](#/settings/NSCA/client/targets/sample_ca)                                 |               | CA                    |
| [certificate](#/settings/NSCA/client/targets/sample_certificate)               |               | SSL CERTIFICATE       |
| [certificate format](#/settings/NSCA/client/targets/sample_certificate format) |               | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NSCA/client/targets/sample_certificate key)       |               | SSL CERTIFICATE       |
| [dh](#/settings/NSCA/client/targets/sample_dh)                                 |               | DH KEY                |
| [encoding](#/settings/NSCA/client/targets/sample_encoding)                     |               | ENCODING              |
| [encryption](#/settings/NSCA/client/targets/sample_encryption)                 | aes           | ENCRYPTION            |
| [host](#/settings/NSCA/client/targets/sample_host)                             |               | TARGET HOST           |
| [password](#/settings/NSCA/client/targets/sample_password)                     |               | PASSWORD              |
| [payload length](#/settings/NSCA/client/targets/sample_payload length)         | 512           | PAYLOAD LENGTH        |
| [port](#/settings/NSCA/client/targets/sample_port)                             |               | TARGET PORT           |
| [retries](#/settings/NSCA/client/targets/sample_retries)                       | 3             | RETRIES               |
| [time offset](#/settings/NSCA/client/targets/sample_time offset)               | 0             | TIME OFFSET           |
| [timeout](#/settings/NSCA/client/targets/sample_timeout)                       | 30            | TIMEOUT               |
| [use ssl](#/settings/NSCA/client/targets/sample_use ssl)                       |               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NSCA/client/targets/sample_verify mode)               |               | VERIFY MODE           |




<a name="/settings/NSCA/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | address                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/NSCA/client/targets/sample_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

A better value is: ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | allowed ciphers                                                               |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# ALLOWED CIPHERS
allowed ciphers=
```


<a name="/settings/NSCA/client/targets/sample_ca"/>
### ca

**CA**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | ca                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# CA
ca=
```


<a name="/settings/NSCA/client/targets/sample_certificate"/>
### certificate

**SSL CERTIFICATE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | certificate                                                                   |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# SSL CERTIFICATE
certificate=
```


<a name="/settings/NSCA/client/targets/sample_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | certificate format                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# CERTIFICATE FORMAT
certificate format=
```


<a name="/settings/NSCA/client/targets/sample_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | certificate key                                                               |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NSCA/client/targets/sample_dh"/>
### dh

**DH KEY**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | dh                                                                            |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# DH KEY
dh=
```


<a name="/settings/NSCA/client/targets/sample_encoding"/>
### encoding

**ENCODING**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | encoding                                                                      |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# ENCODING
encoding=
```


<a name="/settings/NSCA/client/targets/sample_encryption"/>
### encryption

**ENCRYPTION**

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




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | encryption                                                                    |
| Default value: | `aes`                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# ENCRYPTION
encryption=aes
```


<a name="/settings/NSCA/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | host                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/NSCA/client/targets/sample_password"/>
### password

**PASSWORD**

The password to use. Again has to be the same as the server or it wont work at all.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | password                                                                      |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# PASSWORD
password=
```


<a name="/settings/NSCA/client/targets/sample_payload length"/>
### payload length

**PAYLOAD LENGTH**

Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | payload length                                                                |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | `512`                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# PAYLOAD LENGTH
payload length=512
```


<a name="/settings/NSCA/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | port                                                                          |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/NSCA/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | retries                                                                       |
| Default value: | `3`                                                                           |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/NSCA/client/targets/sample_time offset"/>
### time offset

**TIME OFFSET**

Time offset.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | time offset                                                                   |
| Advanced:      | Yes (means it is not commonly used)                                           |
| Default value: | `0`                                                                           |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# TIME OFFSET
time offset=0
```


<a name="/settings/NSCA/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | timeout                                                                       |
| Default value: | `30`                                                                          |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# TIMEOUT
timeout=30
```


<a name="/settings/NSCA/client/targets/sample_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.





| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | use ssl                                                                       |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# ENABLE SSL ENCRYPTION
use ssl=
```


<a name="/settings/NSCA/client/targets/sample_verify mode"/>
### verify mode

**VERIFY MODE**







| Key            | Description                                                                   |
|----------------|-------------------------------------------------------------------------------|
| Path:          | [/settings/NSCA/client/targets/sample](#/settings/NSCA/client/targets/sample) |
| Key:           | verify mode                                                                   |
| Default value: | _N/A_                                                                         |
| Sample key:    | Yes (This section is only to show how this key is used)                       |
| Used by:       | NSCAClient                                                                    |


#### Sample

```
[/settings/NSCA/client/targets/sample]
# VERIFY MODE
verify mode=
```


