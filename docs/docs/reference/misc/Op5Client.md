# Op5Client

Client for connecting nativly to the Op5 Nortbound API







**Configuration Keys:**



    
    
| Path / Section                  | Key                                           | Description           |
|---------------------------------|-----------------------------------------------|-----------------------|
| [/settings/op5](#/settings/op5) | [channel](#/settings/op5_channel)             | CHANNEL               |
| [/settings/op5](#/settings/op5) | [contactgroups](#/settings/op5_contactgroups) | Contact groups        |
| [/settings/op5](#/settings/op5) | [hostgroups](#/settings/op5_hostgroups)       | Host groups           |
| [/settings/op5](#/settings/op5) | [hostname](#/settings/op5_hostname)           | HOSTNAME              |
| [/settings/op5](#/settings/op5) | [interval](#/settings/op5_interval)           | Check interval        |
| [/settings/op5](#/settings/op5) | [password](#/settings/op5_password)           | Op5 password          |
| [/settings/op5](#/settings/op5) | [remove](#/settings/op5_remove)               | Remove checks on exit |
| [/settings/op5](#/settings/op5) | [server](#/settings/op5_server)               | Op5 base url          |
| [/settings/op5](#/settings/op5) | [user](#/settings/op5_user)                   | Op5 user              |


| Path / Section                                | Description          |
|-----------------------------------------------|----------------------|
| [/settings/op5/checks](#/settings/op5/checks) | Op5 passive Commands |





## Configuration

<a name="/settings/op5"/>
### Op5 Configuration

Section for the Op5 server




| Key                                           | Default Value | Description           |
|-----------------------------------------------|---------------|-----------------------|
| [channel](#/settings/op5_channel)             | op5           | CHANNEL               |
| [contactgroups](#/settings/op5_contactgroups) |               | Contact groups        |
| [hostgroups](#/settings/op5_hostgroups)       |               | Host groups           |
| [hostname](#/settings/op5_hostname)           | auto          | HOSTNAME              |
| [interval](#/settings/op5_interval)           | 5m            | Check interval        |
| [password](#/settings/op5_password)           |               | Op5 password          |
| [remove](#/settings/op5_remove)               | false         | Remove checks on exit |
| [server](#/settings/op5_server)               |               | Op5 base url          |
| [user](#/settings/op5_user)                   |               | Op5 user              |



```ini
# Section for the Op5 server
[/settings/op5]
channel=op5
hostname=auto
interval=5m
remove=false

```




<a name="/settings/op5_channel"/>

**CHANNEL**

The channel to listen to.





| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | channel                         |
| Default value: | `op5`                           |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# CHANNEL
channel=op5
```


<a name="/settings/op5_contactgroups"/>

**Contact groups**

A coma separated list of contact groups to add to this host when registering it in monitor






| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | contactgroups                   |
| Default value: | _N/A_                           |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Contact groups
contactgroups=
```


<a name="/settings/op5_hostgroups"/>

**Host groups**

A coma separated list of host groups to add to this host when registering it in monitor






| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | hostgroups                      |
| Default value: | _N/A_                           |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Host groups
hostgroups=
```


<a name="/settings/op5_hostname"/>

**HOSTNAME**

The host name of this monitored computer.
Set this to auto (default) to use the windows name of the computer.

auto	Hostname
${host}	Hostname
${host_lc}
Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase






| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | hostname                        |
| Default value: | `auto`                          |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# HOSTNAME
hostname=auto
```


<a name="/settings/op5_interval"/>

**Check interval**

How often to submit passive check results you can use an optional suffix to denote time (s, m, h)





| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | interval                        |
| Default value: | `5m`                            |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Check interval
interval=5m
```


<a name="/settings/op5_password"/>

**Op5 password**

The password for the user to authenticate as






| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | password                        |
| Default value: | _N/A_                           |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Op5 password
password=
```


<a name="/settings/op5_remove"/>

**Remove checks on exit**

If we should remove all checks when NSClient++ shuts down (for truly elastic scenarios)





| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | remove                          |
| Default value: | `false`                         |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Remove checks on exit
remove=false
```


<a name="/settings/op5_server"/>

**Op5 base url**

The op5 base url i.e. the url of the Op5 monitor REST API for instance https://monitor.mycompany.com






| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | server                          |
| Default value: | _N/A_                           |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Op5 base url
server=
```


<a name="/settings/op5_user"/>

**Op5 user**

The user to authenticate as






| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | user                            |
| Default value: | _N/A_                           |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Op5 user
user=
```


<a name="/settings/op5/checks"/>
### Op5 passive Commands




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






