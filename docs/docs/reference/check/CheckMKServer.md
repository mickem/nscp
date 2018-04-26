# CheckMKServer

A server that listens for incoming check_mk connection and processes incoming requests.






## Configuration



| Path / Section                                                  | Description               |
|-----------------------------------------------------------------|---------------------------|
| [/settings/check_mk/server](#check-mk-server-section)           | CHECK MK SERVER SECTION   |
| [/settings/check_mk/server/scripts](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |
| [/settings/default](#)                                          |                           |



### CHECK MK SERVER SECTION <a id="/settings/check_mk/server"/>

Section for check_mk (CheckMKServer.dll) protocol options.




| Key                                         | Default Value                       | Description           |
|---------------------------------------------|-------------------------------------|-----------------------|
| [allowed ciphers](#allowed-ciphers)         | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH   | ALLOWED CIPHERS       |
| [allowed hosts](#allowed-hosts)             | 127.0.0.1                           | ALLOWED HOSTS         |
| [bind to](#bind-to-address)                 |                                     | BIND TO ADDRESS       |
| [ca](#ca)                                   | ${certificate-path}/ca.pem          | CA                    |
| [cache allowed hosts](#cache-allowed-hosts) | true                                | CACHE ALLOWED HOSTS   |
| [certificate](#ssl-certificate)             | ${certificate-path}/certificate.pem | SSL CERTIFICATE       |
| [certificate format](#certificate-format)   | PEM                                 | CERTIFICATE FORMAT    |
| [certificate key](#ssl-certificate)         |                                     | SSL CERTIFICATE       |
| [dh](#dh-key)                               | ${certificate-path}/nrpe_dh_512.pem | DH KEY                |
| [port](#port-number)                        | 6556                                | PORT NUMBER           |
| [socket queue size](#listen-queue)          | 0                                   | LISTEN QUEUE          |
| [ssl options](#verify-mode)                 |                                     | VERIFY MODE           |
| [thread pool](#thread-pool)                 | 10                                  | THREAD POOL           |
| [timeout](#timeout)                         | 30                                  | TIMEOUT               |
| [use ssl](#enable-ssl-encryption)           | false                               | ENABLE SSL ENCRYPTION |
| [verify mode](#verify-mode)                 | none                                | VERIFY MODE           |



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





#### ALLOWED CIPHERS <a id="/settings/check_mk/server/allowed ciphers"></a>

The chipers which are allowed to be used.
The default here will differ is used in "insecure" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | allowed ciphers                                         |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH`                     |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# ALLOWED CIPHERS
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
```



#### ALLOWED HOSTS <a id="/settings/check_mk/server/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | allowed hosts                                           |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `127.0.0.1`                                             |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```



#### BIND TO ADDRESS <a id="/settings/check_mk/server/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.






| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | bind to                                                 |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | _N/A_                                                   |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# BIND TO ADDRESS
bind to=
```



#### CA <a id="/settings/check_mk/server/ca"></a>







| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | ca                                                      |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `${certificate-path}/ca.pem`                            |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# CA
ca=${certificate-path}/ca.pem
```



#### CACHE ALLOWED HOSTS <a id="/settings/check_mk/server/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | cache allowed hosts                                     |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `true`                                                  |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```



#### SSL CERTIFICATE <a id="/settings/check_mk/server/certificate"></a>







| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | certificate                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `${certificate-path}/certificate.pem`                   |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# SSL CERTIFICATE
certificate=${certificate-path}/certificate.pem
```



#### CERTIFICATE FORMAT <a id="/settings/check_mk/server/certificate format"></a>







| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | certificate format                                      |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `PEM`                                                   |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# CERTIFICATE FORMAT
certificate format=PEM
```



#### SSL CERTIFICATE <a id="/settings/check_mk/server/certificate key"></a>








| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | certificate key                                         |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | _N/A_                                                   |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# SSL CERTIFICATE
certificate key=
```



#### DH KEY <a id="/settings/check_mk/server/dh"></a>







| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | dh                                                      |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `${certificate-path}/nrpe_dh_512.pem`                   |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# DH KEY
dh=${certificate-path}/nrpe_dh_512.pem
```



#### PORT NUMBER <a id="/settings/check_mk/server/port"></a>

Port to use for check_mk.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | port                                                    |
| Default value: | `6556`                                                  |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# PORT NUMBER
port=6556
```



#### LISTEN QUEUE <a id="/settings/check_mk/server/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | socket queue size                                       |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `0`                                                     |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# LISTEN QUEUE
socket queue size=0
```



#### VERIFY MODE <a id="/settings/check_mk/server/ssl options"></a>

Comma separated list of verification flags to set on the SSL socket.

default-workarounds	Various workarounds for what I understand to be broken ssl implementations
no-sslv2	Do not use the SSLv2 protocol.
no-sslv3	Do not use the SSLv3 protocol.
no-tlsv1	Do not use the TLSv1 protocol.
single-dh-use	Always create a new key when using temporary/ephemeral DH parameters. This option must be used to prevent small subgroup attacks, when the DH parameters were not generated using "strong" primes (e.g. when using DSA-parameters).









| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | ssl options                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | _N/A_                                                   |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# VERIFY MODE
ssl options=
```



#### THREAD POOL <a id="/settings/check_mk/server/thread pool"></a>

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | thread pool                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `10`                                                    |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# THREAD POOL
thread pool=10
```



#### TIMEOUT <a id="/settings/check_mk/server/timeout"></a>

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | timeout                                                 |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `30`                                                    |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# TIMEOUT
timeout=30
```



#### ENABLE SSL ENCRYPTION <a id="/settings/check_mk/server/use ssl"></a>

This option controls if SSL should be enabled.





| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | use ssl                                                 |
| Default value: | `false`                                                 |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# ENABLE SSL ENCRYPTION
use ssl=false
```



#### VERIFY MODE <a id="/settings/check_mk/server/verify mode"></a>

Comma separated list of verification flags to set on the SSL socket.

none	The server will not send a client certificate request to the client, so the client will not send a certificate.
peer	The server sends a client certificate request to the client and the certificate returned (if any) is checked.
fail-if-no-cert	if the client did not return a certificate, the TLS/SSL handshake is immediately terminated. This flag must be used together with peer.
peer-cert	Alias for peer and fail-if-no-cert.
workarounds	Various bug workarounds.
single	Always create a new key when using tmp_dh parameters.
client-once	Only request a client certificate on the initial TLS/SSL handshake. This flag must be used together with verify-peer








| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/check_mk/server](#/settings/check_mk/server) |
| Key:           | verify mode                                             |
| Advanced:      | Yes (means it is not commonly used)                     |
| Default value: | `none`                                                  |
| Used by:       | CheckMKServer                                           |


**Sample:**

```
[/settings/check_mk/server]
# VERIFY MODE
verify mode=none
```


### REMOTE TARGET DEFINITIONS <a id="/settings/check_mk/server/scripts"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






### /settings/default <a id="/settings/default"/>






| Key                                         | Default Value | Description           |
|---------------------------------------------|---------------|-----------------------|
| [allowed hosts](#allowed-hosts)             | 127.0.0.1     | ALLOWED HOSTS         |
| [bind to](#bind-to-address)                 |               | BIND TO ADDRESS       |
| [cache allowed hosts](#cache-allowed-hosts) | true          | CACHE ALLOWED HOSTS   |
| [encoding](#nrpe-payload-encoding)          |               | NRPE PAYLOAD ENCODING |
| [inbox](#inbox)                             | inbox         | INBOX                 |
| [password](#password)                       |               | Password              |
| [socket queue size](#listen-queue)          | 0             | LISTEN QUEUE          |
| [thread pool](#thread-pool)                 | 10            | THREAD POOL           |
| [timeout](#timeout)                         | 30            | TIMEOUT               |



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





#### ALLOWED HOSTS <a id="/settings/default/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | allowed hosts                                                    |
| Default value: | `127.0.0.1`                                                      |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```



#### BIND TO ADDRESS <a id="/settings/default/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.






| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | bind to                                                          |
| Default value: | _N/A_                                                            |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# BIND TO ADDRESS
bind to=
```



#### CACHE ALLOWED HOSTS <a id="/settings/default/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | cache allowed hosts                                              |
| Default value: | `true`                                                           |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```



#### NRPE PAYLOAD ENCODING <a id="/settings/default/encoding"></a>








| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | encoding                                                         |
| Advanced:      | Yes (means it is not commonly used)                              |
| Default value: | _N/A_                                                            |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# NRPE PAYLOAD ENCODING
encoding=
```



#### INBOX <a id="/settings/default/inbox"></a>

The default channel to post incoming messages on





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | inbox                                                            |
| Default value: | `inbox`                                                          |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# INBOX
inbox=inbox
```



#### Password <a id="/settings/default/password"></a>

Password used to authenticate against server






| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | password                                                         |
| Default value: | _N/A_                                                            |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# Password
password=
```



#### LISTEN QUEUE <a id="/settings/default/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | socket queue size                                                |
| Advanced:      | Yes (means it is not commonly used)                              |
| Default value: | `0`                                                              |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# LISTEN QUEUE
socket queue size=0
```



#### THREAD POOL <a id="/settings/default/thread pool"></a>







| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | thread pool                                                      |
| Advanced:      | Yes (means it is not commonly used)                              |
| Default value: | `10`                                                             |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# THREAD POOL
thread pool=10
```



#### TIMEOUT <a id="/settings/default/timeout"></a>

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.





| Key            | Description                                                      |
|----------------|------------------------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)                          |
| Key:           | timeout                                                          |
| Default value: | `30`                                                             |
| Used by:       | CheckMKServer, NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# TIMEOUT
timeout=30
```


