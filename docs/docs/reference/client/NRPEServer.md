# NRPEServer

A server that listens for incoming NRPE connection and processes incoming requests.







## List of Configuration


### Common Keys

| Path / Section                                  | Key                                                                     | Description                            |
|-------------------------------------------------|-------------------------------------------------------------------------|----------------------------------------|
| [/settings/default](#/settings/default)         | [allowed hosts](#/settings/default_allowed hosts)                       | ALLOWED HOSTS                          |
| [/settings/default](#/settings/default)         | [bind to](#/settings/default_bind to)                                   | BIND TO ADDRESS                        |
| [/settings/default](#/settings/default)         | [cache allowed hosts](#/settings/default_cache allowed hosts)           | CACHE ALLOWED HOSTS                    |
| [/settings/default](#/settings/default)         | [inbox](#/settings/default_inbox)                                       | INBOX                                  |
| [/settings/default](#/settings/default)         | [password](#/settings/default_password)                                 | PASSWORD                               |
| [/settings/default](#/settings/default)         | [timeout](#/settings/default_timeout)                                   | TIMEOUT                                |
| [/settings/NRPE/server](#/settings/NRPE/server) | [allow arguments](#/settings/NRPE/server_allow arguments)               | COMMAND ARGUMENT PROCESSING            |
| [/settings/NRPE/server](#/settings/NRPE/server) | [allow nasty characters](#/settings/NRPE/server_allow nasty characters) | COMMAND ALLOW NASTY META CHARS         |
| [/settings/NRPE/server](#/settings/NRPE/server) | [extended response](#/settings/NRPE/server_extended response)           | EXTENDED RESPONSE                      |
| [/settings/NRPE/server](#/settings/NRPE/server) | [insecure](#/settings/NRPE/server_insecure)                             | ALLOW INSECURE CHIPHERS and ENCRYPTION |
| [/settings/NRPE/server](#/settings/NRPE/server) | [port](#/settings/NRPE/server_port)                                     | PORT NUMBER                            |
| [/settings/NRPE/server](#/settings/NRPE/server) | [use ssl](#/settings/NRPE/server_use ssl)                               | ENABLE SSL ENCRYPTION                  |

### Advanced keys

| Path / Section                                  | Key                                                               | Description           |
|-------------------------------------------------|-------------------------------------------------------------------|-----------------------|
| [/settings/default](#/settings/default)         | [encoding](#/settings/default_encoding)                           | NRPE PAYLOAD ENCODING |
| [/settings/default](#/settings/default)         | [socket queue size](#/settings/default_socket queue size)         | LISTEN QUEUE          |
| [/settings/default](#/settings/default)         | [thread pool](#/settings/default_thread pool)                     | THREAD POOL           |
| [/settings/NRPE/server](#/settings/NRPE/server) | [allowed ciphers](#/settings/NRPE/server_allowed ciphers)         | ALLOWED CIPHERS       |
| [/settings/NRPE/server](#/settings/NRPE/server) | [allowed hosts](#/settings/NRPE/server_allowed hosts)             | ALLOWED HOSTS         |
| [/settings/NRPE/server](#/settings/NRPE/server) | [bind to](#/settings/NRPE/server_bind to)                         | BIND TO ADDRESS       |
| [/settings/NRPE/server](#/settings/NRPE/server) | [ca](#/settings/NRPE/server_ca)                                   | CA                    |
| [/settings/NRPE/server](#/settings/NRPE/server) | [cache allowed hosts](#/settings/NRPE/server_cache allowed hosts) | CACHE ALLOWED HOSTS   |
| [/settings/NRPE/server](#/settings/NRPE/server) | [certificate](#/settings/NRPE/server_certificate)                 | SSL CERTIFICATE       |
| [/settings/NRPE/server](#/settings/NRPE/server) | [certificate format](#/settings/NRPE/server_certificate format)   | CERTIFICATE FORMAT    |
| [/settings/NRPE/server](#/settings/NRPE/server) | [certificate key](#/settings/NRPE/server_certificate key)         | SSL CERTIFICATE       |
| [/settings/NRPE/server](#/settings/NRPE/server) | [dh](#/settings/NRPE/server_dh)                                   | DH KEY                |
| [/settings/NRPE/server](#/settings/NRPE/server) | [encoding](#/settings/NRPE/server_encoding)                       | NRPE PAYLOAD ENCODING |
| [/settings/NRPE/server](#/settings/NRPE/server) | [payload length](#/settings/NRPE/server_payload length)           | PAYLOAD LENGTH        |
| [/settings/NRPE/server](#/settings/NRPE/server) | [performance data](#/settings/NRPE/server_performance data)       | PERFORMANCE DATA      |
| [/settings/NRPE/server](#/settings/NRPE/server) | [socket queue size](#/settings/NRPE/server_socket queue size)     | LISTEN QUEUE          |
| [/settings/NRPE/server](#/settings/NRPE/server) | [ssl options](#/settings/NRPE/server_ssl options)                 | VERIFY MODE           |
| [/settings/NRPE/server](#/settings/NRPE/server) | [thread pool](#/settings/NRPE/server_thread pool)                 | THREAD POOL           |
| [/settings/NRPE/server](#/settings/NRPE/server) | [timeout](#/settings/NRPE/server_timeout)                         | TIMEOUT               |
| [/settings/NRPE/server](#/settings/NRPE/server) | [verify mode](#/settings/NRPE/server_verify mode)                 | VERIFY MODE           |






# Configuration

<a name="/settings/default"/>
## 



```ini
# 
[/settings/default]
allowed hosts=127.0.0.1
cache allowed hosts=true
inbox=inbox
socket queue size=0
thread pool=10
timeout=30

```


| Key                                                           | Default Value | Description           |
|---------------------------------------------------------------|---------------|-----------------------|
| [allowed hosts](#/settings/default_allowed hosts)             | 127.0.0.1     | ALLOWED HOSTS         |
| [bind to](#/settings/default_bind to)                         |               | BIND TO ADDRESS       |
| [cache allowed hosts](#/settings/default_cache allowed hosts) | true          | CACHE ALLOWED HOSTS   |
| [encoding](#/settings/default_encoding)                       |               | NRPE PAYLOAD ENCODING |
| [inbox](#/settings/default_inbox)                             | inbox         | INBOX                 |
| [password](#/settings/default_password)                       |               | PASSWORD              |
| [socket queue size](#/settings/default_socket queue size)     | 0             | LISTEN QUEUE          |
| [thread pool](#/settings/default_thread pool)                 | 10            | THREAD POOL           |
| [timeout](#/settings/default_timeout)                         | 30            | TIMEOUT               |




<a name="/settings/default_allowed hosts"/>
### allowed hosts

**ALLOWED HOSTS**

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.




| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | allowed hosts                                                    |
| Default value: | `127.0.0.1`                                                      |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```


<a name="/settings/default_bind to"/>
### bind to

**BIND TO ADDRESS**

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | bind to                                                          |
| Default value: | _N/A_                                                            |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# BIND TO ADDRESS
bind to=
```


<a name="/settings/default_cache allowed hosts"/>
### cache allowed hosts

**CACHE ALLOWED HOSTS**

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.




| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | cache allowed hosts                                              |
| Default value: | `true`                                                           |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```


<a name="/settings/default_encoding"/>
### encoding

**NRPE PAYLOAD ENCODING**







| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | encoding                                                         |
| Advanced:      | Yes (means it is not commonly used)                              |
| Default value: | _N/A_                                                            |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# NRPE PAYLOAD ENCODING
encoding=
```


<a name="/settings/default_inbox"/>
### inbox

**INBOX**

The default channel to post incoming messages on




| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | inbox                                                            |
| Default value: | `inbox`                                                          |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# INBOX
inbox=inbox
```


<a name="/settings/default_password"/>
### password

**PASSWORD**

Password used to authenticate against server





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | password                                                         |
| Default value: | _N/A_                                                            |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# PASSWORD
password=
```


<a name="/settings/default_socket queue size"/>
### socket queue size

**LISTEN QUEUE**

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.




| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | socket queue size                                                |
| Advanced:      | Yes (means it is not commonly used)                              |
| Default value: | `0`                                                              |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# LISTEN QUEUE
socket queue size=0
```


<a name="/settings/default_thread pool"/>
### thread pool

**THREAD POOL**






| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | thread pool                                                      |
| Advanced:      | Yes (means it is not commonly used)                              |
| Default value: | `10`                                                             |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# THREAD POOL
thread pool=10
```


<a name="/settings/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.




| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | timeout                                                          |
| Default value: | `30`                                                             |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


#### Sample

```
[/settings/default]
# TIMEOUT
timeout=30
```


<a name="/settings/NRPE/server"/>
## NRPE SERVER SECTION

Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options.

```ini
# Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options.
[/settings/NRPE/server]
allow arguments=false
allow nasty characters=false
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
allowed hosts=127.0.0.1
ca=${certificate-path}/ca.pem
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
certificate format=PEM
dh=${certificate-path}/nrpe_dh_512.pem
extended response=true
insecure=false
payload length=1024
performance data=true
port=5666
socket queue size=0
thread pool=10
timeout=30
use ssl=true
verify mode=none

```


| Key                                                                     | Default Value                       | Description                            |
|-------------------------------------------------------------------------|-------------------------------------|----------------------------------------|
| [allow arguments](#/settings/NRPE/server_allow arguments)               | false                               | COMMAND ARGUMENT PROCESSING            |
| [allow nasty characters](#/settings/NRPE/server_allow nasty characters) | false                               | COMMAND ALLOW NASTY META CHARS         |
| [allowed ciphers](#/settings/NRPE/server_allowed ciphers)               | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH   | ALLOWED CIPHERS                        |
| [allowed hosts](#/settings/NRPE/server_allowed hosts)                   | 127.0.0.1                           | ALLOWED HOSTS                          |
| [bind to](#/settings/NRPE/server_bind to)                               |                                     | BIND TO ADDRESS                        |
| [ca](#/settings/NRPE/server_ca)                                         | ${certificate-path}/ca.pem          | CA                                     |
| [cache allowed hosts](#/settings/NRPE/server_cache allowed hosts)       | true                                | CACHE ALLOWED HOSTS                    |
| [certificate](#/settings/NRPE/server_certificate)                       | ${certificate-path}/certificate.pem | SSL CERTIFICATE                        |
| [certificate format](#/settings/NRPE/server_certificate format)         | PEM                                 | CERTIFICATE FORMAT                     |
| [certificate key](#/settings/NRPE/server_certificate key)               |                                     | SSL CERTIFICATE                        |
| [dh](#/settings/NRPE/server_dh)                                         | ${certificate-path}/nrpe_dh_512.pem | DH KEY                                 |
| [encoding](#/settings/NRPE/server_encoding)                             |                                     | NRPE PAYLOAD ENCODING                  |
| [extended response](#/settings/NRPE/server_extended response)           | true                                | EXTENDED RESPONSE                      |
| [insecure](#/settings/NRPE/server_insecure)                             | false                               | ALLOW INSECURE CHIPHERS and ENCRYPTION |
| [payload length](#/settings/NRPE/server_payload length)                 | 1024                                | PAYLOAD LENGTH                         |
| [performance data](#/settings/NRPE/server_performance data)             | true                                | PERFORMANCE DATA                       |
| [port](#/settings/NRPE/server_port)                                     | 5666                                | PORT NUMBER                            |
| [socket queue size](#/settings/NRPE/server_socket queue size)           | 0                                   | LISTEN QUEUE                           |
| [ssl options](#/settings/NRPE/server_ssl options)                       |                                     | VERIFY MODE                            |
| [thread pool](#/settings/NRPE/server_thread pool)                       | 10                                  | THREAD POOL                            |
| [timeout](#/settings/NRPE/server_timeout)                               | 30                                  | TIMEOUT                                |
| [use ssl](#/settings/NRPE/server_use ssl)                               | true                                | ENABLE SSL ENCRYPTION                  |
| [verify mode](#/settings/NRPE/server_verify mode)                       | none                                | VERIFY MODE                            |




<a name="/settings/NRPE/server_allow arguments"/>
### allow arguments

**COMMAND ARGUMENT PROCESSING**

This option determines whether or not the we will allow clients to specify arguments to commands that are executed.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allow arguments                                 |
| Default value: | `false`                                         |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# COMMAND ARGUMENT PROCESSING
allow arguments=false
```


<a name="/settings/NRPE/server_allow nasty characters"/>
### allow nasty characters

**COMMAND ALLOW NASTY META CHARS**

This option determines whether or not the we will allow clients to specify nasty (as in \|\`&><'"\\[]{}) characters in arguments.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allow nasty characters                          |
| Default value: | `false`                                         |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# COMMAND ALLOW NASTY META CHARS
allow nasty characters=false
```


<a name="/settings/NRPE/server_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

The chipers which are allowed to be used.
The default here will differ is used in "insecure" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allowed ciphers                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH`             |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# ALLOWED CIPHERS
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
```


<a name="/settings/NRPE/server_allowed hosts"/>
### allowed hosts

**ALLOWED HOSTS**

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allowed hosts                                   |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `127.0.0.1`                                     |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```


<a name="/settings/NRPE/server_bind to"/>
### bind to

**BIND TO ADDRESS**

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | bind to                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# BIND TO ADDRESS
bind to=
```


<a name="/settings/NRPE/server_ca"/>
### ca

**CA**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | ca                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/ca.pem`                    |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# CA
ca=${certificate-path}/ca.pem
```


<a name="/settings/NRPE/server_cache allowed hosts"/>
### cache allowed hosts

**CACHE ALLOWED HOSTS**

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | cache allowed hosts                             |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```


<a name="/settings/NRPE/server_certificate"/>
### certificate

**SSL CERTIFICATE**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | certificate                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/certificate.pem`           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# SSL CERTIFICATE
certificate=${certificate-path}/certificate.pem
```


<a name="/settings/NRPE/server_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | certificate format                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `PEM`                                           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# CERTIFICATE FORMAT
certificate format=PEM
```


<a name="/settings/NRPE/server_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | certificate key                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NRPE/server_dh"/>
### dh

**DH KEY**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | dh                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/nrpe_dh_512.pem`           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# DH KEY
dh=${certificate-path}/nrpe_dh_512.pem
```


<a name="/settings/NRPE/server_encoding"/>
### encoding

**NRPE PAYLOAD ENCODING**

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | encoding                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# NRPE PAYLOAD ENCODING
encoding=
```


<a name="/settings/NRPE/server_extended response"/>
### extended response

**EXTENDED RESPONSE**

Send more then 1 return packet to allow response to go beyond payload size (requires modified client if legacy is true this defaults to false).




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | extended response                               |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# EXTENDED RESPONSE
extended response=true
```


<a name="/settings/NRPE/server_insecure"/>
### insecure

**ALLOW INSECURE CHIPHERS and ENCRYPTION**

Only enable this if you are using legacy check_nrpe client.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | insecure                                        |
| Default value: | `false`                                         |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# ALLOW INSECURE CHIPHERS and ENCRYPTION
insecure=false
```


<a name="/settings/NRPE/server_payload length"/>
### payload length

**PAYLOAD LENGTH**

Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | payload length                                  |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `1024`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# PAYLOAD LENGTH
payload length=1024
```


<a name="/settings/NRPE/server_performance data"/>
### performance data

**PERFORMANCE DATA**

Send performance data back to nagios (set this to 0 to remove all performance data).




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | performance data                                |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# PERFORMANCE DATA
performance data=true
```


<a name="/settings/NRPE/server_port"/>
### port

**PORT NUMBER**

Port to use for NRPE.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | port                                            |
| Default value: | `5666`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# PORT NUMBER
port=5666
```


<a name="/settings/NRPE/server_socket queue size"/>
### socket queue size

**LISTEN QUEUE**

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | socket queue size                               |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `0`                                             |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# LISTEN QUEUE
socket queue size=0
```


<a name="/settings/NRPE/server_ssl options"/>
### ssl options

**VERIFY MODE**

Comma separated list of verification flags to set on the SSL socket.

| default-workarounds | Various workarounds for what I understand to be broken ssl implementations                                                                                                                                                          |
|---------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| no-sslv2            | Do not use the SSLv2 protocol.                                                                                                                                                                                                      |
| no-sslv3            | Do not use the SSLv3 protocol.                                                                                                                                                                                                      |
| no-tlsv1            | Do not use the TLSv1 protocol.                                                                                                                                                                                                      |
| single-dh-use       | Always create a new key when using temporary/ephemeral DH parameters. This option must be used to prevent small subgroup attacks, when the DH parameters were not generated using "strong" primes (e.g. when using DSA-parameters). |











| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | ssl options                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# VERIFY MODE
ssl options=
```


<a name="/settings/NRPE/server_thread pool"/>
### thread pool

**THREAD POOL**

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | thread pool                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `10`                                            |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# THREAD POOL
thread pool=10
```


<a name="/settings/NRPE/server_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | timeout                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `30`                                            |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# TIMEOUT
timeout=30
```


<a name="/settings/NRPE/server_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | use ssl                                         |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# ENABLE SSL ENCRYPTION
use ssl=true
```


<a name="/settings/NRPE/server_verify mode"/>
### verify mode

**VERIFY MODE**

Comma separated list of verification flags to set on the SSL socket.

| none            | The server will not send a client certificate request to the client, so the client will not send a certificate.                         |
|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------|
| peer            | The server sends a client certificate request to the client and the certificate returned (if any) is checked.                           |
| fail-if-no-cert | if the client did not return a certificate, the TLS/SSL handshake is immediately terminated. This flag must be used together with peer. |
| peer-cert       | Alias for peer and fail-if-no-cert.                                                                                                     |
| workarounds     | Various bug workarounds.                                                                                                                |
| single          | Always create a new key when using tmp_dh parameters.                                                                                   |
| client-once     | Only request a client certificate on the initial TLS/SSL handshake. This flag must be used together with verify-peer                    |










| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | verify mode                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `none`                                          |
| Used by:       | NRPEServer                                      |


#### Sample

```
[/settings/NRPE/server]
# VERIFY MODE
verify mode=none
```


