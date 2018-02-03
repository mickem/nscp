# WEBServer

A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module.






## Configuration



| Path / Section                       | Description |
|--------------------------------------|-------------|
| [/settings/default](#)               |             |
| [/settings/WEB/server](#web-server)  | Web server  |
| [/settings/WEB/server/roles](#roles) | Roles       |
| [/settings/WEB/server/users](#users) | Users       |



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


### Web server <a id="/settings/WEB/server"/>

Section for WEB (WEBServer.dll) (check_WEB) protocol options.




| Key                                         | Default Value                       | Description         |
|---------------------------------------------|-------------------------------------|---------------------|
| [allowed hosts](#allowed-hosts)             | 127.0.0.1                           | ALLOWED HOSTS       |
| [cache allowed hosts](#cache-allowed-hosts) | true                                | CACHE ALLOWED HOSTS |
| [certificate](#certificate)                 | ${certificate-path}/certificate.pem | CERTIFICATE         |
| [password](#password)                       |                                     | PASSWORD            |
| [port](#port-number)                        | 8443                                | PORT NUMBER         |
| [threads](#number-of-threads)               | 10                                  | NUMBER OF THREADS   |



```ini
# Section for WEB (WEBServer.dll) (check_WEB) protocol options.
[/settings/WEB/server]
allowed hosts=127.0.0.1
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
port=8443
threads=10

```





#### ALLOWED HOSTS <a id="/settings/WEB/server/allowed hosts"></a>

A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | allowed hosts                                 |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `127.0.0.1`                                   |
| Used by:       | WEBServer                                     |


**Sample:**

```
[/settings/WEB/server]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```



#### CACHE ALLOWED HOSTS <a id="/settings/WEB/server/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server. parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.





| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | cache allowed hosts                           |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `true`                                        |
| Used by:       | WEBServer                                     |


**Sample:**

```
[/settings/WEB/server]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```



#### CERTIFICATE <a id="/settings/WEB/server/certificate"></a>

Ssl certificate to use for the ssl server





| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | certificate                                   |
| Default value: | `${certificate-path}/certificate.pem`         |
| Used by:       | WEBServer                                     |


**Sample:**

```
[/settings/WEB/server]
# CERTIFICATE
certificate=${certificate-path}/certificate.pem
```



#### PASSWORD <a id="/settings/WEB/server/password"></a>

Password used to authenticate against server parent for this key is found under: /settings/default this is marked as advanced in favor of the parent.






| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | password                                      |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |
| Used by:       | WEBServer                                     |


**Sample:**

```
[/settings/WEB/server]
# PASSWORD
password=
```



#### PORT NUMBER <a id="/settings/WEB/server/port"></a>

Port to use for WEB server.





| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | port                                          |
| Default value: | `8443`                                        |
| Used by:       | WEBServer                                     |


**Sample:**

```
[/settings/WEB/server]
# PORT NUMBER
port=8443
```



#### NUMBER OF THREADS <a id="/settings/WEB/server/threads"></a>

The number of threads in the sever response pool.





| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/WEB/server](#/settings/WEB/server) |
| Key:           | threads                                       |
| Default value: | `10`                                          |
| Used by:       | WEBServer                                     |


**Sample:**

```
[/settings/WEB/server]
# NUMBER OF THREADS
threads=10
```


### Roles <a id="/settings/WEB/server/roles"/>

A list of roles and with coma separated list of access rights.







```ini
# A list of roles and with coma separated list of access rights.
[/settings/WEB/server/roles]

```




### Users <a id="/settings/WEB/server/users"/>

Users which can access the REST API


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key         | Default Value | Description |
|-------------|---------------|-------------|
| alias       |               | ALIAS       |
| is template | false         | IS TEMPLATE |
| parent      | default       | PARENT      |
| password    |               | PASSWORD    |
| role        |               | ROLE        |


**Sample:**

```ini
# An example of a Users section
[/settings/WEB/server/users/sample]
#alias=...
is template=false
parent=default
#password=...
#role=...

```






