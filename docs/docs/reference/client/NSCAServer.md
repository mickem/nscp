# NSCAServer

A server that listens for incoming NSCA connection and processes incoming requests.







## List of Configuration


### Common Keys

| Path / Section                                  | Key                                                           | Description           |
|-------------------------------------------------|---------------------------------------------------------------|-----------------------|
| [/settings/default](#/settings/default)         | [allowed hosts](#/settings/default_allowed hosts)             | ALLOWED HOSTS         |
| [/settings/default](#/settings/default)         | [bind to](#/settings/default_bind to)                         | BIND TO ADDRESS       |
| [/settings/default](#/settings/default)         | [cache allowed hosts](#/settings/default_cache allowed hosts) | CACHE ALLOWED HOSTS   |
| [/settings/default](#/settings/default)         | [inbox](#/settings/default_inbox)                             | INBOX                 |
| [/settings/default](#/settings/default)         | [password](#/settings/default_password)                       | PASSWORD              |
| [/settings/default](#/settings/default)         | [timeout](#/settings/default_timeout)                         | TIMEOUT               |
| [/settings/NSCA/server](#/settings/NSCA/server) | [encryption](#/settings/NSCA/server_encryption)               | ENCRYPTION            |
| [/settings/NSCA/server](#/settings/NSCA/server) | [payload length](#/settings/NSCA/server_payload length)       | PAYLOAD LENGTH        |
| [/settings/NSCA/server](#/settings/NSCA/server) | [performance data](#/settings/NSCA/server_performance data)   | PERFORMANCE DATA      |
| [/settings/NSCA/server](#/settings/NSCA/server) | [port](#/settings/NSCA/server_port)                           | PORT NUMBER           |
| [/settings/NSCA/server](#/settings/NSCA/server) | [use ssl](#/settings/NSCA/server_use ssl)                     | ENABLE SSL ENCRYPTION |

### Advanced keys

| Path / Section                                  | Key                                                               | Description           |
|-------------------------------------------------|-------------------------------------------------------------------|-----------------------|
| [/settings/default](#/settings/default)         | [encoding](#/settings/default_encoding)                           | NRPE PAYLOAD ENCODING |
| [/settings/default](#/settings/default)         | [socket queue size](#/settings/default_socket queue size)         | LISTEN QUEUE          |
| [/settings/default](#/settings/default)         | [thread pool](#/settings/default_thread pool)                     | THREAD POOL           |
| [/settings/NSCA/server](#/settings/NSCA/server) | [allowed ciphers](#/settings/NSCA/server_allowed ciphers)         | ALLOWED CIPHERS       |
| [/settings/NSCA/server](#/settings/NSCA/server) | [allowed hosts](#/settings/NSCA/server_allowed hosts)             | ALLOWED HOSTS         |
| [/settings/NSCA/server](#/settings/NSCA/server) | [bind to](#/settings/NSCA/server_bind to)                         | BIND TO ADDRESS       |
| [/settings/NSCA/server](#/settings/NSCA/server) | [ca](#/settings/NSCA/server_ca)                                   | CA                    |
| [/settings/NSCA/server](#/settings/NSCA/server) | [cache allowed hosts](#/settings/NSCA/server_cache allowed hosts) | CACHE ALLOWED HOSTS   |
| [/settings/NSCA/server](#/settings/NSCA/server) | [certificate](#/settings/NSCA/server_certificate)                 | SSL CERTIFICATE       |
| [/settings/NSCA/server](#/settings/NSCA/server) | [certificate format](#/settings/NSCA/server_certificate format)   | CERTIFICATE FORMAT    |
| [/settings/NSCA/server](#/settings/NSCA/server) | [certificate key](#/settings/NSCA/server_certificate key)         | SSL CERTIFICATE       |
| [/settings/NSCA/server](#/settings/NSCA/server) | [dh](#/settings/NSCA/server_dh)                                   | DH KEY                |
| [/settings/NSCA/server](#/settings/NSCA/server) | [inbox](#/settings/NSCA/server_inbox)                             | INBOX                 |
| [/settings/NSCA/server](#/settings/NSCA/server) | [password](#/settings/NSCA/server_password)                       | PASSWORD              |
| [/settings/NSCA/server](#/settings/NSCA/server) | [socket queue size](#/settings/NSCA/server_socket queue size)     | LISTEN QUEUE          |
| [/settings/NSCA/server](#/settings/NSCA/server) | [ssl options](#/settings/NSCA/server_ssl options)                 | VERIFY MODE           |
| [/settings/NSCA/server](#/settings/NSCA/server) | [thread pool](#/settings/NSCA/server_thread pool)                 | THREAD POOL           |
| [/settings/NSCA/server](#/settings/NSCA/server) | [timeout](#/settings/NSCA/server_timeout)                         | TIMEOUT               |
| [/settings/NSCA/server](#/settings/NSCA/server) | [verify mode](#/settings/NSCA/server_verify mode)                 | VERIFY MODE           |






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


<a name="/settings/NSCA/server"/>
## NSCA SERVER SECTION

Section for NSCA (NSCAServer) (check_nsca) protocol options.

```ini
# Section for NSCA (NSCAServer) (check_nsca) protocol options.
[/settings/NSCA/server]
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
allowed hosts=127.0.0.1
ca=${certificate-path}/ca.pem
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
certificate format=PEM
dh=${certificate-path}/nrpe_dh_512.pem
encryption=aes
inbox=inbox
payload length=512
performance data=true
port=5667
socket queue size=0
thread pool=10
timeout=30
use ssl=false
verify mode=none

```


| Key                                                               | Default Value                       | Description           |
|-------------------------------------------------------------------|-------------------------------------|-----------------------|
| [allowed ciphers](#/settings/NSCA/server_allowed ciphers)         | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH   | ALLOWED CIPHERS       |
| [allowed hosts](#/settings/NSCA/server_allowed hosts)             | 127.0.0.1                           | ALLOWED HOSTS         |
| [bind to](#/settings/NSCA/server_bind to)                         |                                     | BIND TO ADDRESS       |
| [ca](#/settings/NSCA/server_ca)                                   | ${certificate-path}/ca.pem          | CA                    |
| [cache allowed hosts](#/settings/NSCA/server_cache allowed hosts) | true                                | CACHE ALLOWED HOSTS   |
| [certificate](#/settings/NSCA/server_certificate)                 | ${certificate-path}/certificate.pem | SSL CERTIFICATE       |
| [certificate format](#/settings/NSCA/server_certificate format)   | PEM                                 | CERTIFICATE FORMAT    |
| [certificate key](#/settings/NSCA/server_certificate key)         |                                     | SSL CERTIFICATE       |
| [dh](#/settings/NSCA/server_dh)                                   | ${certificate-path}/nrpe_dh_512.pem | DH KEY                |
| [encryption](#/settings/NSCA/server_encryption)                   | aes                                 | ENCRYPTION            |
| [inbox](#/settings/NSCA/server_inbox)                             | inbox                               | INBOX                 |
| [password](#/settings/NSCA/server_password)                       |                                     | PASSWORD              |
| [payload length](#/settings/NSCA/server_payload length)           | 512                                 | PAYLOAD LENGTH        |
| [performance data](#/settings/NSCA/server_performance data)       | true                                | PERFORMANCE DATA      |
| [port](#/settings/NSCA/server_port)                               | 5667                                | PORT NUMBER           |
| [socket queue size](#/settings/NSCA/server_socket queue size)     | 0                                   | LISTEN QUEUE          |
| [ssl options](#/settings/NSCA/server_ssl options)                 |                                     | VERIFY MODE           |
| [thread pool](#/settings/NSCA/server_thread pool)                 | 10                                  | THREAD POOL           |
| [timeout](#/settings/NSCA/server_timeout)                         | 30                                  | TIMEOUT               |
| [use ssl](#/settings/NSCA/server_use ssl)                         | false                               | ENABLE SSL ENCRYPTION |
| [verify mode](#/settings/NSCA/server_verify mode)                 | none                                | VERIFY MODE           |




<a name="/settings/NSCA/server_allowed ciphers"/>
### allowed ciphers

**ALLOWED CIPHERS**

The chipers which are allowed to be used.
The default here will differ is used in "insecure" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | allowed ciphers                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH`             |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# ALLOWED CIPHERS
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
```


<a name="/settings/NSCA/server_allowed hosts"/>
### allowed hosts

**ALLOWED HOSTS**

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | allowed hosts                                   |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `127.0.0.1`                                     |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```


<a name="/settings/NSCA/server_bind to"/>
### bind to

**BIND TO ADDRESS**

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | bind to                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# BIND TO ADDRESS
bind to=
```


<a name="/settings/NSCA/server_ca"/>
### ca

**CA**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | ca                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/ca.pem`                    |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# CA
ca=${certificate-path}/ca.pem
```


<a name="/settings/NSCA/server_cache allowed hosts"/>
### cache allowed hosts

**CACHE ALLOWED HOSTS**

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | cache allowed hosts                             |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `true`                                          |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```


<a name="/settings/NSCA/server_certificate"/>
### certificate

**SSL CERTIFICATE**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | certificate                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/certificate.pem`           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# SSL CERTIFICATE
certificate=${certificate-path}/certificate.pem
```


<a name="/settings/NSCA/server_certificate format"/>
### certificate format

**CERTIFICATE FORMAT**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | certificate format                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `PEM`                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# CERTIFICATE FORMAT
certificate format=PEM
```


<a name="/settings/NSCA/server_certificate key"/>
### certificate key

**SSL CERTIFICATE**







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | certificate key                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# SSL CERTIFICATE
certificate key=
```


<a name="/settings/NSCA/server_dh"/>
### dh

**DH KEY**






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | dh                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/nrpe_dh_512.pem`           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# DH KEY
dh=${certificate-path}/nrpe_dh_512.pem
```


<a name="/settings/NSCA/server_encryption"/>
### encryption

**ENCRYPTION**

Name of encryption algorithm to use.
Has to be the same as your agent i using or it wont work at all.This is also independent of SSL and generally used instead of SSL.
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




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | encryption                                      |
| Default value: | `aes`                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# ENCRYPTION
encryption=aes
```


<a name="/settings/NSCA/server_inbox"/>
### inbox

**INBOX**

The default channel to post incoming messages on parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | inbox                                           |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `inbox`                                         |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# INBOX
inbox=inbox
```


<a name="/settings/NSCA/server_password"/>
### password

**PASSWORD**

Password used to authenticate against server parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | password                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# PASSWORD
password=
```


<a name="/settings/NSCA/server_payload length"/>
### payload length

**PAYLOAD LENGTH**

Length of payload to/from the NSCA agent. This is a hard specific value so you have to "configure" (read recompile) your NSCA agent to use the same value for it to work.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | payload length                                  |
| Default value: | `512`                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# PAYLOAD LENGTH
payload length=512
```


<a name="/settings/NSCA/server_performance data"/>
### performance data

**PERFORMANCE DATA**

Send performance data back to nagios (set this to false to remove all performance data).




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | performance data                                |
| Default value: | `true`                                          |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# PERFORMANCE DATA
performance data=true
```


<a name="/settings/NSCA/server_port"/>
### port

**PORT NUMBER**

Port to use for NSCA.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | port                                            |
| Default value: | `5667`                                          |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# PORT NUMBER
port=5667
```


<a name="/settings/NSCA/server_socket queue size"/>
### socket queue size

**LISTEN QUEUE**

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | socket queue size                               |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `0`                                             |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# LISTEN QUEUE
socket queue size=0
```


<a name="/settings/NSCA/server_ssl options"/>
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
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | ssl options                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# VERIFY MODE
ssl options=
```


<a name="/settings/NSCA/server_thread pool"/>
### thread pool

**THREAD POOL**

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | thread pool                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `10`                                            |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# THREAD POOL
thread pool=10
```


<a name="/settings/NSCA/server_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | timeout                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `30`                                            |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# TIMEOUT
timeout=30
```


<a name="/settings/NSCA/server_use ssl"/>
### use ssl

**ENABLE SSL ENCRYPTION**

This option controls if SSL should be enabled.




| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | use ssl                                         |
| Default value: | `false`                                         |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# ENABLE SSL ENCRYPTION
use ssl=false
```


<a name="/settings/NSCA/server_verify mode"/>
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
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | verify mode                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `none`                                          |
| Used by:       | NSCAServer                                      |


#### Sample

```
[/settings/NSCA/server]
# VERIFY MODE
verify mode=none
```


