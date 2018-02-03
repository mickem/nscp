# NSCAServer

A server that listens for incoming NSCA connection and processes incoming requests.






## Configuration



| Path / Section                                | Description         |
|-----------------------------------------------|---------------------|
| [/settings/default](#)                        |                     |
| [/settings/NSCA/server](#nsca-server-section) | NSCA SERVER SECTION |



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


### NSCA SERVER SECTION <a id="/settings/NSCA/server"/>

Section for NSCA (NSCAServer) (check_nsca) protocol options.




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
| [encryption](#encryption)                   | aes                                 | ENCRYPTION            |
| [inbox](#inbox)                             | inbox                               | INBOX                 |
| [password](#password)                       |                                     | PASSWORD              |
| [payload length](#payload-length)           | 512                                 | PAYLOAD LENGTH        |
| [performance data](#performance-data)       | true                                | PERFORMANCE DATA      |
| [port](#port-number)                        | 5667                                | PORT NUMBER           |
| [socket queue size](#listen-queue)          | 0                                   | LISTEN QUEUE          |
| [ssl options](#verify-mode)                 |                                     | VERIFY MODE           |
| [thread pool](#thread-pool)                 | 10                                  | THREAD POOL           |
| [timeout](#timeout)                         | 30                                  | TIMEOUT               |
| [use ssl](#enable-ssl-encryption)           | false                               | ENABLE SSL ENCRYPTION |
| [verify mode](#verify-mode)                 | none                                | VERIFY MODE           |



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





#### ALLOWED CIPHERS <a id="/settings/NSCA/server/allowed ciphers"></a>

The chipers which are allowed to be used.
The default here will differ is used in "insecure" mode or not. check_nrpe uses a very old chipers and should preferably not be used. For details of chipers please see the OPEN ssl documentation: https://www.openssl.org/docs/apps/ciphers.html





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | allowed ciphers                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH`             |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# ALLOWED CIPHERS
allowed ciphers=ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH
```



#### ALLOWED HOSTS <a id="/settings/NSCA/server/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | allowed hosts                                   |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `127.0.0.1`                                     |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```



#### BIND TO ADDRESS <a id="/settings/NSCA/server/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | bind to                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# BIND TO ADDRESS
bind to=
```



#### CA <a id="/settings/NSCA/server/ca"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | ca                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/ca.pem`                    |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# CA
ca=${certificate-path}/ca.pem
```



#### CACHE ALLOWED HOSTS <a id="/settings/NSCA/server/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | cache allowed hosts                             |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `true`                                          |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```



#### SSL CERTIFICATE <a id="/settings/NSCA/server/certificate"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | certificate                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/certificate.pem`           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# SSL CERTIFICATE
certificate=${certificate-path}/certificate.pem
```



#### CERTIFICATE FORMAT <a id="/settings/NSCA/server/certificate format"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | certificate format                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `PEM`                                           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# CERTIFICATE FORMAT
certificate format=PEM
```



#### SSL CERTIFICATE <a id="/settings/NSCA/server/certificate key"></a>








| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | certificate key                                 |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# SSL CERTIFICATE
certificate key=
```



#### DH KEY <a id="/settings/NSCA/server/dh"></a>







| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | dh                                              |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `${certificate-path}/nrpe_dh_512.pem`           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# DH KEY
dh=${certificate-path}/nrpe_dh_512.pem
```



#### ENCRYPTION <a id="/settings/NSCA/server/encryption"></a>

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


**Sample:**

```
[/settings/NSCA/server]
# ENCRYPTION
encryption=aes
```



#### INBOX <a id="/settings/NSCA/server/inbox"></a>

The default channel to post incoming messages on parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | inbox                                           |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `inbox`                                         |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# INBOX
inbox=inbox
```



#### PASSWORD <a id="/settings/NSCA/server/password"></a>

Password used to authenticate against server parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.






| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | password                                        |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# PASSWORD
password=
```



#### PAYLOAD LENGTH <a id="/settings/NSCA/server/payload length"></a>

Length of payload to/from the NSCA agent. This is a hard specific value so you have to "configure" (read recompile) your NSCA agent to use the same value for it to work.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | payload length                                  |
| Default value: | `512`                                           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# PAYLOAD LENGTH
payload length=512
```



#### PERFORMANCE DATA <a id="/settings/NSCA/server/performance data"></a>

Send performance data back to nagios (set this to false to remove all performance data).





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | performance data                                |
| Default value: | `true`                                          |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# PERFORMANCE DATA
performance data=true
```



#### PORT NUMBER <a id="/settings/NSCA/server/port"></a>

Port to use for NSCA.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | port                                            |
| Default value: | `5667`                                          |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# PORT NUMBER
port=5667
```



#### LISTEN QUEUE <a id="/settings/NSCA/server/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | socket queue size                               |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `0`                                             |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# LISTEN QUEUE
socket queue size=0
```



#### VERIFY MODE <a id="/settings/NSCA/server/ssl options"></a>

Comma separated list of verification flags to set on the SSL socket.

default-workarounds	Various workarounds for what I understand to be broken ssl implementations
no-sslv2	Do not use the SSLv2 protocol.
no-sslv3	Do not use the SSLv3 protocol.
no-tlsv1	Do not use the TLSv1 protocol.
single-dh-use	Always create a new key when using temporary/ephemeral DH parameters. This option must be used to prevent small subgroup attacks, when the DH parameters were not generated using "strong" primes (e.g. when using DSA-parameters).









| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | ssl options                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | _N/A_                                           |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# VERIFY MODE
ssl options=
```



#### THREAD POOL <a id="/settings/NSCA/server/thread pool"></a>

 parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | thread pool                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `10`                                            |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# THREAD POOL
thread pool=10
```



#### TIMEOUT <a id="/settings/NSCA/server/timeout"></a>

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | timeout                                         |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `30`                                            |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# TIMEOUT
timeout=30
```



#### ENABLE SSL ENCRYPTION <a id="/settings/NSCA/server/use ssl"></a>

This option controls if SSL should be enabled.





| Key            | Description                                     |
|----------------|-------------------------------------------------|
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | use ssl                                         |
| Default value: | `false`                                         |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# ENABLE SSL ENCRYPTION
use ssl=false
```



#### VERIFY MODE <a id="/settings/NSCA/server/verify mode"></a>

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
| Path:          | [/settings/NSCA/server](#/settings/NSCA/server) |
| Key:           | verify mode                                     |
| Advanced:      | Yes (means it is not commonly used)             |
| Default value: | `none`                                          |
| Used by:       | NSCAServer                                      |


**Sample:**

```
[/settings/NSCA/server]
# VERIFY MODE
verify mode=none
```


