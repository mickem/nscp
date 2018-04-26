# WEBServer

A server that listens for incoming HTTP connection and processes incoming requests. It provides both a WEB UI as well as a REST API in addition to simplifying configuration of WEB Server module.






## Configuration



| Path / Section                                  | Description      |
|-------------------------------------------------|------------------|
| [/settings/default](#)                          |                  |
| [/settings/WEB/server](#web-server)             | Web server       |
| [/settings/WEB/server/roles](#web-server-roles) | Web server roles |
| [/settings/WEB/server/users](#web-server-users) | Web server users |



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





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | allowed hosts                                     |
| Default value: | `127.0.0.1`                                       |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# ALLOWED HOSTS
allowed hosts=127.0.0.1
```



#### BIND TO ADDRESS <a id="/settings/default/bind to"></a>

Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind to all available IP addresses.






| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | bind to                                           |
| Default value: | _N/A_                                             |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# BIND TO ADDRESS
bind to=
```



#### CACHE ALLOWED HOSTS <a id="/settings/default/cache allowed hosts"></a>

If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | cache allowed hosts                               |
| Default value: | `true`                                            |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# CACHE ALLOWED HOSTS
cache allowed hosts=true
```



#### NRPE PAYLOAD ENCODING <a id="/settings/default/encoding"></a>








| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | encoding                                          |
| Advanced:      | Yes (means it is not commonly used)               |
| Default value: | _N/A_                                             |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# NRPE PAYLOAD ENCODING
encoding=
```



#### INBOX <a id="/settings/default/inbox"></a>

The default channel to post incoming messages on





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | inbox                                             |
| Default value: | `inbox`                                           |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# INBOX
inbox=inbox
```



#### Password <a id="/settings/default/password"></a>

Password used to authenticate against server






| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | password                                          |
| Default value: | _N/A_                                             |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# Password
password=
```



#### LISTEN QUEUE <a id="/settings/default/socket queue size"></a>

Number of sockets to queue before starting to refuse new incoming connections. This can be used to tweak the amount of simultaneous sockets that the server accepts.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | socket queue size                                 |
| Advanced:      | Yes (means it is not commonly used)               |
| Default value: | `0`                                               |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# LISTEN QUEUE
socket queue size=0
```



#### THREAD POOL <a id="/settings/default/thread pool"></a>







| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | thread pool                                       |
| Advanced:      | Yes (means it is not commonly used)               |
| Default value: | `10`                                              |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# THREAD POOL
thread pool=10
```



#### TIMEOUT <a id="/settings/default/timeout"></a>

Timeout when reading packets on incoming sockets. If the data has not arrived within this time we will bail out.





| Key            | Description                                       |
|----------------|---------------------------------------------------|
| Path:          | [/settings/default](#/settings/default)           |
| Key:           | timeout                                           |
| Default value: | `30`                                              |
| Used by:       | NRPEServer, NSCAServer, NSClientServer, WEBServer |


**Sample:**

```
[/settings/default]
# TIMEOUT
timeout=30
```


### Web server <a id="/settings/WEB/server"/>

Section for WEB (WEBServer.dll) (check_WEB) protocol options.




| Key                                                 | Default Value                       | Description                 |
|-----------------------------------------------------|-------------------------------------|-----------------------------|
| [allowed hosts](#allowed-hosts)                     | 127.0.0.1                           | Allowed hosts               |
| [cache allowed hosts](#cache-list-of-allowed-hosts) | true                                | Cache list of allowed hosts |
| [certificate](#tls-certificate)                     | ${certificate-path}/certificate.pem | TLS Certificate             |
| [password](#password)                               |                                     | Password                    |
| [port](#server-port)                                | 8443                                | Server port                 |
| [threads](#server-threads)                          | 10                                  | Server threads              |



```ini
# Section for WEB (WEBServer.dll) (check_WEB) protocol options.
[/settings/WEB/server]
allowed hosts=127.0.0.1
cache allowed hosts=true
certificate=${certificate-path}/certificate.pem
port=8443
threads=10

```





#### Allowed hosts <a id="/settings/WEB/server/allowed hosts"></a>

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
# Allowed hosts
allowed hosts=127.0.0.1
```



#### Cache list of allowed hosts <a id="/settings/WEB/server/cache allowed hosts"></a>

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
# Cache list of allowed hosts
cache allowed hosts=true
```



#### TLS Certificate <a id="/settings/WEB/server/certificate"></a>

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
# TLS Certificate
certificate=${certificate-path}/certificate.pem
```



#### Password <a id="/settings/WEB/server/password"></a>

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
# Password
password=
```



#### Server port <a id="/settings/WEB/server/port"></a>

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
# Server port
port=8443
```



#### Server threads <a id="/settings/WEB/server/threads"></a>

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
# Server threads
threads=10
```


### Web server roles <a id="/settings/WEB/server/roles"/>

A list of roles and with coma separated list of access rights.




| Key                            | Default Value                                                                                    | Description          |
|--------------------------------|--------------------------------------------------------------------------------------------------|----------------------|
| [client](#role-for-read-only)  | public,info.get,info.get.version,queries.list,queries.get,queries.execute,login.get,modules.list | Role for read only   |
| [full](#role-for-full-access)  | *                                                                                                | Role for Full access |
| [legacy](#role-for-legacy-api) | legacy                                                                                           | Role for legacy API  |
| [view](#role-for-full-access)  | *                                                                                                | Role for Full access |



```ini
# A list of roles and with coma separated list of access rights.
[/settings/WEB/server/roles]
client=public,info.get,info.get.version,queries.list,queries.get,queries.execute,login.get,modules.list
full=*
legacy=legacy
view=*

```





#### Role for read only <a id="/settings/WEB/server/roles/client"></a>

Default role for read only





| Key            | Description                                                                                        |
|----------------|----------------------------------------------------------------------------------------------------|
| Path:          | [/settings/WEB/server/roles](#/settings/WEB/server/roles)                                          |
| Key:           | client                                                                                             |
| Default value: | `public,info.get,info.get.version,queries.list,queries.get,queries.execute,login.get,modules.list` |
| Used by:       | WEBServer                                                                                          |


**Sample:**

```
[/settings/WEB/server/roles]
# Role for read only
client=public,info.get,info.get.version,queries.list,queries.get,queries.execute,login.get,modules.list
```



#### Role for Full access <a id="/settings/WEB/server/roles/full"></a>

Default role for Full access





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/WEB/server/roles](#/settings/WEB/server/roles) |
| Key:           | full                                                      |
| Default value: | `*`                                                       |
| Used by:       | WEBServer                                                 |


**Sample:**

```
[/settings/WEB/server/roles]
# Role for Full access
full=*
```



#### Role for legacy API <a id="/settings/WEB/server/roles/legacy"></a>

Default role for legacy API





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/WEB/server/roles](#/settings/WEB/server/roles) |
| Key:           | legacy                                                    |
| Default value: | `legacy`                                                  |
| Used by:       | WEBServer                                                 |


**Sample:**

```
[/settings/WEB/server/roles]
# Role for legacy API
legacy=legacy
```



#### Role for Full access <a id="/settings/WEB/server/roles/view"></a>

Default role for Full access





| Key            | Description                                               |
|----------------|-----------------------------------------------------------|
| Path:          | [/settings/WEB/server/roles](#/settings/WEB/server/roles) |
| Key:           | view                                                      |
| Default value: | `*`                                                       |
| Used by:       | WEBServer                                                 |


**Sample:**

```
[/settings/WEB/server/roles]
# Role for Full access
view=*
```


### Web server users <a id="/settings/WEB/server/users"/>

Users which can access the REST API


This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key      | Default Value | Description |
|----------|---------------|-------------|
| password |               | PASSWORD    |
| role     |               | ROLE        |


**Sample:**

```ini
# An example of a Web server users section
[/settings/WEB/server/users/sample]
#password=...
#role=...

```






