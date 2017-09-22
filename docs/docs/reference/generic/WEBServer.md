# WEBServer

A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module.







## List of Configuration


### Common Keys

| Path / Section                                | Key                                                           | Description         |
|-----------------------------------------------|---------------------------------------------------------------|---------------------|
| [/settings/default](#/settings/default)       | [allowed hosts](#/settings/default_allowed hosts)             | ALLOWED HOSTS       |
| [/settings/default](#/settings/default)       | [bind to](#/settings/default_bind to)                         | BIND TO ADDRESS     |
| [/settings/default](#/settings/default)       | [cache allowed hosts](#/settings/default_cache allowed hosts) | CACHE ALLOWED HOSTS |
| [/settings/default](#/settings/default)       | [inbox](#/settings/default_inbox)                             | INBOX               |
| [/settings/default](#/settings/default)       | [password](#/settings/default_password)                       | PASSWORD            |
| [/settings/default](#/settings/default)       | [timeout](#/settings/default_timeout)                         | TIMEOUT             |
| [/settings/WEB/server](#/settings/WEB/server) | [certificate](#/settings/WEB/server_certificate)              | CERTIFICATE         |
| [/settings/WEB/server](#/settings/WEB/server) | [port](#/settings/WEB/server_port)                            | PORT NUMBER         |
| [/settings/WEB/server](#/settings/WEB/server) | [threads](#/settings/WEB/server_threads)                      | NUMBER OF THREADS   |

### Advanced keys

| Path / Section                                | Key                                                              | Description           |
|-----------------------------------------------|------------------------------------------------------------------|-----------------------|
| [/settings/default](#/settings/default)       | [encoding](#/settings/default_encoding)                          | NRPE PAYLOAD ENCODING |
| [/settings/default](#/settings/default)       | [socket queue size](#/settings/default_socket queue size)        | LISTEN QUEUE          |
| [/settings/default](#/settings/default)       | [thread pool](#/settings/default_thread pool)                    | THREAD POOL           |
| [/settings/WEB/server](#/settings/WEB/server) | [allowed hosts](#/settings/WEB/server_allowed hosts)             | ALLOWED HOSTS         |
| [/settings/WEB/server](#/settings/WEB/server) | [cache allowed hosts](#/settings/WEB/server_cache allowed hosts) | CACHE ALLOWED HOSTS   |
| [/settings/WEB/server](#/settings/WEB/server) | [password](#/settings/WEB/server_password)                       | PASSWORD              |






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


<a name="/settings/WEB/server"/>
## WEB SERVER SECTION

Section for WEB (WEBServer.dll) (check_WEB) protocol options.

```ini
# Section for WEB (WEBServer.dll) (check_WEB) protocol options.
[/settings/WEB/server]
allowed hosts=127.0.0.1
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
port=8443
threads=10

```


| Key                                                              | Default Value                       | Description         |
|------------------------------------------------------------------|-------------------------------------|---------------------|
| [allowed hosts](#/settings/WEB/server_allowed hosts)             | 127.0.0.1                           | ALLOWED HOSTS       |
| [cache allowed hosts](#/settings/WEB/server_cache allowed hosts) | true                                | CACHE ALLOWED HOSTS |
| [certificate](#/settings/WEB/server_certificate)                 | ${certificate-path}/certificate.pem | CERTIFICATE         |
| [password](#/settings/WEB/server_password)                       |                                     | PASSWORD            |
| [port](#/settings/WEB/server_port)                               | 8443                                | PORT NUMBER         |
| [threads](#/settings/WEB/server_threads)                         | 10                                  | NUMBER OF THREADS   |




<a name="/settings/WEB/server_allowed hosts"/>
### allowed hosts

**ALLOWED HOSTS**

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | allowed hosts                                 |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `127.0.0.1`                                   |
| Used by:       | WEBServer                                     |


#### Sample

```
[/settings/WEB/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```


<a name="/settings/WEB/server_cache allowed hosts"/>
### cache allowed hosts

**CACHE ALLOWED HOSTS**

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.




| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | cache allowed hosts                           |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `true`                                        |
| Used by:       | WEBServer                                     |


#### Sample

```
[/settings/WEB/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```


<a name="/settings/WEB/server_certificate"/>
### certificate

**CERTIFICATE**

Ssl certificate to use for the ssl server




| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | certificate                                   |
| Default value: | `${certificate-path}/certificate.pem`         |
| Used by:       | WEBServer                                     |


#### Sample

```
[/settings/WEB/server]
# CERTIFICATE
certificate=${certificate-path}/certificate.pem
```


<a name="/settings/WEB/server_password"/>
### password

**PASSWORD**

Password used to authenticate against server parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | password                                      |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |
| Used by:       | WEBServer                                     |


#### Sample

```
[/settings/WEB/server]
# PASSWORD
password=
```


<a name="/settings/WEB/server_port"/>
### port

**PORT NUMBER**

Port to use for WEB server.




| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | port                                          |
| Default value: | `8443`                                        |
| Used by:       | WEBServer                                     |


#### Sample

```
[/settings/WEB/server]
# PORT NUMBER
port=8443
```


<a name="/settings/WEB/server_threads"/>
### threads

**NUMBER OF THREADS**

The number of threads in the sever response pool.




| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | threads                                       |
| Default value: | `10`                                          |
| Used by:       | WEBServer                                     |


#### Sample

```
[/settings/WEB/server]
# NUMBER OF THREADS
threads=10
```


