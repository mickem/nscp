# CheckMKServer

A server that listens for incoming check_mk connection and processes incoming requests.







## List of Configuration


### Common Keys

| Path / Section                                          | Key                                                           | Description           |
|---------------------------------------------------------|---------------------------------------------------------------|-----------------------|
| [/settings/check_mk/server](#/settings/check_mk/server) | [port](#/settings/check_mk/server_port)                       | PORT NUMBER           |
| [/settings/check_mk/server](#/settings/check_mk/server) | [use ssl](#/settings/check_mk/server_use ssl)                 | ENABLE SSL ENCRYPTION |
| [/settings/default](#/settings/default)                 | [allowed hosts](#/settings/default_allowed hosts)             | ALLOWED HOSTS         |
| [/settings/default](#/settings/default)                 | [bind to](#/settings/default_bind to)                         | BIND TO ADDRESS       |
| [/settings/default](#/settings/default)                 | [cache allowed hosts](#/settings/default_cache allowed hosts) | CACHE ALLOWED HOSTS   |
| [/settings/default](#/settings/default)                 | [inbox](#/settings/default_inbox)                             | INBOX                 |
| [/settings/default](#/settings/default)                 | [password](#/settings/default_password)                       | PASSWORD              |
| [/settings/default](#/settings/default)                 | [timeout](#/settings/default_timeout)                         | TIMEOUT               |

### Advanced keys

| Path / Section                                          | Key                                                                   | Description           |
|---------------------------------------------------------|-----------------------------------------------------------------------|-----------------------|
| [/settings/check_mk/server](#/settings/check_mk/server) | [allowed ciphers](#/settings/check_mk/server_allowed ciphers)         | ALLOWED CIPHERS       |
| [/settings/check_mk/server](#/settings/check_mk/server) | [allowed hosts](#/settings/check_mk/server_allowed hosts)             | ALLOWED HOSTS         |
| [/settings/check_mk/server](#/settings/check_mk/server) | [bind to](#/settings/check_mk/server_bind to)                         | BIND TO ADDRESS       |
| [/settings/check_mk/server](#/settings/check_mk/server) | [ca](#/settings/check_mk/server_ca)                                   | CA                    |
| [/settings/check_mk/server](#/settings/check_mk/server) | [cache allowed hosts](#/settings/check_mk/server_cache allowed hosts) | CACHE ALLOWED HOSTS   |
| [/settings/check_mk/server](#/settings/check_mk/server) | [certificate](#/settings/check_mk/server_certificate)                 | SSL CERTIFICATE       |
| [/settings/check_mk/server](#/settings/check_mk/server) | [certificate format](#/settings/check_mk/server_certificate format)   | CERTIFICATE FORMAT    |
| [/settings/check_mk/server](#/settings/check_mk/server) | [certificate key](#/settings/check_mk/server_certificate key)         | SSL CERTIFICATE       |
| [/settings/check_mk/server](#/settings/check_mk/server) | [dh](#/settings/check_mk/server_dh)                                   | DH KEY                |
| [/settings/check_mk/server](#/settings/check_mk/server) | [socket queue size](#/settings/check_mk/server_socket queue size)     | LISTEN QUEUE          |
| [/settings/check_mk/server](#/settings/check_mk/server) | [ssl options](#/settings/check_mk/server_ssl options)                 | VERIFY MODE           |
| [/settings/check_mk/server](#/settings/check_mk/server) | [thread pool](#/settings/check_mk/server_thread pool)                 | THREAD POOL           |
| [/settings/check_mk/server](#/settings/check_mk/server) | [timeout](#/settings/check_mk/server_timeout)                         | TIMEOUT               |
| [/settings/check_mk/server](#/settings/check_mk/server) | [verify mode](#/settings/check_mk/server_verify mode)                 | VERIFY MODE           |
| [/settings/default](#/settings/default)                 | [encoding](#/settings/default_encoding)                               | NRPE PAYLOAD ENCODING |
| [/settings/default](#/settings/default)                 | [socket queue size](#/settings/default_socket queue size)             | LISTEN QUEUE          |
| [/settings/default](#/settings/default)                 | [thread pool](#/settings/default_thread pool)                         | THREAD POOL           |






# Configuration

<a name="/settings/check_mk/server"/>
## CHECK MK SERVER SECTION

Section for check_mk (CheckMKServer.dll) protocol options.

```ini
# Section for check_mk (CheckMKServer.dll) protocol options.
[/settings/check_mk/server]
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
allowed hosts=127.0.0.1
ca=${certificate-path}/ca.pem
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
certificate format=PEM
dh=${certificate-path}/nrpe_dh_512.pem
port=6556
socket queue size=0
thread pool=10
timeout=30
use ssl=false
verify mode=none

```


| Key                                                                   | Default Value                       | Description           |
|-----------------------------------------------------------------------|-------------------------------------|-----------------------|
| [allowed ciphers](#/settings/check_mk/server_allowed ciphers)         | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH   | ALLOWED CIPHERS       |
| [allowed hosts](#/settings/check_mk/server_allowed hosts)             | 127.0.0.1                           | ALLOWED HOSTS         |
| [bind to](#/settings/check_mk/server_bind to)                         |                                     | BIND TO ADDRESS       |
| [ca](#/settings/check_mk/server_ca)                                   | ${certificate-path}/ca.pem          | CA                    |
| [cache allowed hosts](#/settings/check_mk/server_cache allowed hosts) | true                                | CACHE ALLOWED HOSTS   |
| [certificate](#/settings/check_mk/server_certificate)                 | ${certificate-path}/certificate.pem | SSL CERTIFICATE       |
| [certificate format](#/settings/check_mk/server_certificate format)   | PEM                                 | CERTIFICATE FORMAT    |
| [certificate key](#/settings/check_mk/server_certificate key)         |                                     | SSL CERTIFICATE       |
| [dh](#/settings/check_mk/server_dh)                                   | ${certificate-path}/nrpe_dh_512.pem | DH KEY                |
| [port](#/settings/check_mk/server_port)                               | 6556                                | PORT NUMBER           |
| [socket queue size](#/settings/check_mk/server_socket queue size)     | 0                                   | LISTEN QUEUE          |
| [ssl options](#/settings/check_mk/server_ssl options)                 |                                     | VERIFY MODE           |
| [thread pool](#/settings/check_mk/server_thread pool)                 | 10                                  | THREAD POOL           |
| [timeout](#/settings/check_mk/server_timeout)                         | 30                                  | TIMEOUT               |
| [use ssl](#/settings/check_mk/server_use ssl)                         | false                               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/check_mk/server_verify mode)                 | none                                | VERIFY MODE           |




<a name="/settings/check_mk/server_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

The chipers which are allowed to be used.
The default here will differ is used in "insecure" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | allowed ciphers                                         |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH`                     |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# ALLOWED CIPHERS
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
```


<a name="/settings/check_mk/server_allowed hosts"/>
### allowed hosts

**ALLOWED HOSTS**

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | allowed hosts                                           |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `127.0.0.1`                                             |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```


<a name="/settings/check_mk/server_bind to"/>
### bind to

**BIND TO ADDRESS**

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | bind to                                                 |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | _N/A_                                                   |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# BIND TO ADDRESS
bind to=
```


<a name="/settings/check_mk/server_ca"/>
### ca

**CA**






| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | ca                                                      |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `${certificate-path}/ca.pem`                            |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# CA
ca=${certificate-path}/ca.pem
```


<a name="/settings/check_mk/server_cache allowed hosts"/>
### cache allowed hosts

**CACHE ALLOWED HOSTS**

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | cache allowed hosts                                     |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `true`                                                  |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```


<a name="/settings/check_mk/server_certificate"/>
### certificate

**SSL CERTIFICATE**






| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | certificate                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `${certificate-path}/certificate.pem`                   |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# SSL CERTIFICATE
certificate=${certificate-path}/certificate.pem
```


<a name="/settings/check_mk/server_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**






| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | certificate format                                      |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `PEM`                                                   |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# CERTIFICATE FORMAT
certificate format=PEM
```


<a name="/settings/check_mk/server_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | certificate key                                         |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | _N/A_                                                   |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/check_mk/server_dh"/>
### dh

**DH KEY**






| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | dh                                                      |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `${certificate-path}/nrpe_dh_512.pem`                   |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# DH KEY
dh=${certificate-path}/nrpe_dh_512.pem
```


<a name="/settings/check_mk/server_port"/>
### port

**PORT NUMBER**

Port to use for check_mk.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | port                                                    |
| Default value: | `6556`                                                  |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# PORT NUMBER
port=6556
```


<a name="/settings/check_mk/server_socket queue size"/>
### socket queue size

**LISTEN QUEUE**

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | socket queue size                                       |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `0`                                                     |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# LISTEN QUEUE
socket queue size=0
```


<a name="/settings/check_mk/server_ssl options"/>
### ssl options

**VERIFY MODE**

Comma separated list of verification flags to set on the SSL socket.

| default-workarounds | Various workarounds for what I understand to be broken ssl implementations                                                                                                                                                          |
|---------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| no-sslv2            | Do not use the SSLv2 protocol.                                                                                                                                                                                                      |
| no-sslv3            | Do not use the SSLv3 protocol.                                                                                                                                                                                                      |
| no-tlsv1            | Do not use the TLSv1 protocol.                                                                                                                                                                                                      |
| single-dh-use       | Always create a new key when using temporary/ephemeral DH parameters. This option must be used to prevent small subgroup attacks, when the DH parameters were not generated using "strong" primes (e.g. when using DSA-parameters). |











| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | ssl options                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | _N/A_                                                   |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# VERIFY MODE
ssl options=
```


<a name="/settings/check_mk/server_thread pool"/>
### thread pool

**THREAD POOL**

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | thread pool                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `10`                                                    |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# THREAD POOL
thread pool=10
```


<a name="/settings/check_mk/server_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | timeout                                                 |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `30`                                                    |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# TIMEOUT
timeout=30
```


<a name="/settings/check_mk/server_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.




| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | use ssl                                                 |
| Default value: | `false`                                                 |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# ENABLE SSL ENCRYPTION
use ssl=false
```


<a name="/settings/check_mk/server_verify mode"/>
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










| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | verify mode                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `none`                                                  |
| Used by:       | CheckMKServer                                           |


#### Sample

```
[/settings/check_mk/server]
# VERIFY MODE
verify mode=none
```


<a name="/settings/check_mk/server/scripts"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/check_mk/server/scripts]

```






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


