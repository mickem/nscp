# NRPEServer

A server that listens for incoming NRPE connection and processes incoming requests.






## Configuration



| Path / Section                        | Description |
|---------------------------------------|-------------|
| [/settings/default](#)                |             |
| [/settings/NRPE/server](#nrpe-server) | NRPE Server |



### /settings/default <a id="/settings/default"/>






| Key                                         | Default Value | Description           |
|---------------------------------------------|---------------|-----------------------|
| [allowed hosts](#allowed-hosts)             | 127.0.0.1     | ALLOWED HOSTS         |
| [bind to](#bind-to-address)                 |               | BIND TO ADDRESS       |
| [cache allowed hosts](#cache-allowed-hosts) | true          | CACHE ALLOWED HOSTS   |
| [encoding](#nrpe-payload-encoding)          |               | NRPE PAYLOAD ENCODING |
| [inbox](#inbox)                             | inbox         | INBOX                 |
| [password](#password)                       |               | PASSWORD              |
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



#### PASSWORD <a id="/settings/default/password"></a>

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
# PASSWORD
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


### NRPE Server <a id="/settings/NRPE/server"/>

Section for NRPE (NRPEServer.dll) (check_nrpe) protocol options.




| Key                                                       | Default Value                       | Description                            |
|-----------------------------------------------------------|-------------------------------------|----------------------------------------|
| [allow arguments](#command-argument-processing)           | false                               | COMMAND ARGUMENT PROCESSING            |
| [allow nasty characters](#command-allow-nasty-meta-chars) | false                               | COMMAND ALLOW NASTY META CHARS         |
| [allowed ciphers](#allowed-ciphers)                       | ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH   | ALLOWED CIPHERS                        |
| [allowed hosts](#allowed-hosts)                           | 127.0.0.1                           | ALLOWED HOSTS                          |
| [bind to](#bind-to-address)                               |                                     | BIND TO ADDRESS                        |
| [ca](#ca)                                                 | ${certificate-path}/ca.pem          | CA                                     |
| [cache allowed hosts](#cache-allowed-hosts)               | true                                | CACHE ALLOWED HOSTS                    |
| [certificate](#ssl-certificate)                           | ${certificate-path}/certificate.pem | SSL CERTIFICATE                        |
| [certificate format](#certificate-format)                 | PEM                                 | CERTIFICATE FORMAT                     |
| [certificate key](#ssl-certificate)                       |                                     | SSL CERTIFICATE                        |
| [dh](#dh-key)                                             | ${certificate-path}/nrpe_dh_512.pem | DH KEY                                 |
| [encoding](#nrpe-payload-encoding)                        |                                     | NRPE PAYLOAD ENCODING                  |
| [extended response](#extended-response)                   | true                                | EXTENDED RESPONSE                      |
| [insecure](#allow-insecure-chiphers-and-encryption)       | false                               | ALLOW INSECURE CHIPHERS and ENCRYPTION |
| [payload length](#payload-length)                         | 1024                                | PAYLOAD LENGTH                         |
| [performance data](#performance-data)                     | true                                | PERFORMANCE DATA                       |
| [port](#port-number)                                      | 5666                                | PORT NUMBER                            |
| [socket queue size](#listen-queue)                        | 0                                   | LISTEN QUEUE                           |
| [ssl options](#verify-mode)                               |                                     | VERIFY MODE                            |
| [thread pool](#thread-pool)                               | 10                                  | THREAD POOL                            |
| [timeout](#timeout)                                       | 30                                  | TIMEOUT                                |
| [use ssl](#enable-ssl-encryption)                         | true                                | ENABLE SSL ENCRYPTION                  |
| [verify mode](#verify-mode)                               | none                                | VERIFY MODE                            |



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





#### COMMAND ARGUMENT PROCESSING <a id="/settings/NRPE/server/allow arguments"></a>

This option determines whether or not the we will allow clients to specify arguments to commands that are executed.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allow arguments                                 |
| Default value: | `false`                                         |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# COMMAND ARGUMENT PROCESSING
allow arguments=false
```



#### COMMAND ALLOW NASTY META CHARS <a id="/settings/NRPE/server/allow nasty characters"></a>

This option determines whether or not the we will allow clients to specify nasty (as in \|\`&><'"\\[]{}) characters in arguments.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allow nasty characters                          |
| Default value: | `false`                                         |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# COMMAND ALLOW NASTY META CHARS
allow nasty characters=false
```



#### ALLOWED CIPHERS <a id="/settings/NRPE/server/allowed ciphers"></a>

The chipers which are allowed to be used.
The default here will differ is used in "insecure" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allowed ciphers                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH`             |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# ALLOWED CIPHERS
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
```



#### ALLOWED HOSTS <a id="/settings/NRPE/server/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | allowed hosts                                   |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `127.0.0.1`                                     |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```



#### BIND TO ADDRESS <a id="/settings/NRPE/server/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | bind to                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# BIND TO ADDRESS
bind to=
```



#### CA <a id="/settings/NRPE/server/ca"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | ca                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/ca.pem`                    |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# CA
ca=${certificate-path}/ca.pem
```



#### CACHE ALLOWED HOSTS <a id="/settings/NRPE/server/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | cache allowed hosts                             |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```



#### SSL CERTIFICATE <a id="/settings/NRPE/server/certificate"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | certificate                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/certificate.pem`           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# SSL CERTIFICATE
certificate=${certificate-path}/certificate.pem
```



#### CERTIFICATE FORMAT <a id="/settings/NRPE/server/certificate format"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | certificate format                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `PEM`                                           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# CERTIFICATE FORMAT
certificate format=PEM
```



#### SSL CERTIFICATE <a id="/settings/NRPE/server/certificate key"></a>








| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | certificate key                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# SSL CERTIFICATE
certificate key=
```



#### DH KEY <a id="/settings/NRPE/server/dh"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | dh                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/nrpe_dh_512.pem`           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# DH KEY
dh=${certificate-path}/nrpe_dh_512.pem
```



#### NRPE PAYLOAD ENCODING <a id="/settings/NRPE/server/encoding"></a>

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | encoding                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# NRPE PAYLOAD ENCODING
encoding=
```



#### EXTENDED RESPONSE <a id="/settings/NRPE/server/extended response"></a>

Send more then 1 return packet to allow response to go beyond payload size (requires modified client if legacy is true this defaults to false).





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | extended response                               |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# EXTENDED RESPONSE
extended response=true
```



#### ALLOW INSECURE CHIPHERS and ENCRYPTION <a id="/settings/NRPE/server/insecure"></a>

Only enable this if you are using legacy check_nrpe client.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | insecure                                        |
| Default value: | `false`                                         |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# ALLOW INSECURE CHIPHERS and ENCRYPTION
insecure=false
```



#### PAYLOAD LENGTH <a id="/settings/NRPE/server/payload length"></a>

Length of payload to/from the NRPE agent. This is a hard specific value so you have to "configure" (read recompile) your NRPE agent to use the same value for it to work.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | payload length                                  |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `1024`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# PAYLOAD LENGTH
payload length=1024
```



#### PERFORMANCE DATA <a id="/settings/NRPE/server/performance data"></a>

Send performance data back to nagios (set this to 0 to remove all performance data).





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | performance data                                |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# PERFORMANCE DATA
performance data=true
```



#### PORT NUMBER <a id="/settings/NRPE/server/port"></a>

Port to use for NRPE.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | port                                            |
| Default value: | `5666`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# PORT NUMBER
port=5666
```



#### LISTEN QUEUE <a id="/settings/NRPE/server/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | socket queue size                               |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `0`                                             |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# LISTEN QUEUE
socket queue size=0
```



#### VERIFY MODE <a id="/settings/NRPE/server/ssl options"></a>

Comma separated list of verification flags to set on the SSL socket.

default-workarounds	Various workarounds for what I understand to be broken ssl implementations
no-sslv2	Do not use the SSLv2 protocol.
no-sslv3	Do not use the SSLv3 protocol.
no-tlsv1	Do not use the TLSv1 protocol.
single-dh-use	Always create a new key when using temporary/ephemeral DH parameters. This option must be used to prevent small subgroup attacks, when the DH parameters were not generated using "strong" primes (e.g. when using DSA-parameters).









| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | ssl options                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# VERIFY MODE
ssl options=
```



#### THREAD POOL <a id="/settings/NRPE/server/thread pool"></a>

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | thread pool                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `10`                                            |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# THREAD POOL
thread pool=10
```



#### TIMEOUT <a id="/settings/NRPE/server/timeout"></a>

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | timeout                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `30`                                            |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# TIMEOUT
timeout=30
```



#### ENABLE SSL ENCRYPTION <a id="/settings/NRPE/server/use ssl"></a>

This option controls if SSL should be enabled.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | use ssl                                         |
| Default value: | `true`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# ENABLE SSL ENCRYPTION
use ssl=true
```



#### VERIFY MODE <a id="/settings/NRPE/server/verify mode"></a>

Comma separated list of verification flags to set on the SSL socket.

none	The server will not send a client certificate request to the client, so the client will not send a certificate.
peer	The server sends a client certificate request to the client and the certificate returned (if any) is checked.
fail-if-no-cert	if the client did not return a certificate, the TLS/SSL handshake is immediately terminated. This flag must be used together with peer.
peer-cert	Alias for peer and fail-if-no-cert.
workarounds	Various bug workarounds.
single	Always create a new key when using tmp_dh parameters.
client-once	Only request a client certificate on the initial TLS/SSL handshake. This flag must be used together with verify-peer








| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NRPE/server](#/settings/NRPE/server) |
| Key:           | verify mode                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `none`                                          |
| Used by:       | NRPEServer                                      |


**Sample:**

```
[/settings/NRPE/server]
# VERIFY MODE
verify mode=none
```


