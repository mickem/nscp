# Op5Client

Client for connecting nativly to the Op5 Nortbound API






## Configuration



| Path / Section                                | Description          |
|-----------------------------------------------|----------------------|
| [/settings/op5](#op5-configuration)           | Op5 Configuration    |
| [/settings/op5/checks](#op5-passive-commands) | Op5 passive Commands |



### Op5 Configuration <a id="/settings/op5"/>

Section for the Op5 server




| Key                                       | Default Value | Description            |
|-------------------------------------------|---------------|------------------------|
| [channel](#channel)                       | op5           | CHANNEL                |
| [contactgroups](#contact-groups)          |               | Contact groups         |
| [default checks](#install-default-checks) | true          | Install default checks |
| [hostgroups](#host-groups)                |               | Host groups            |
| [hostname](#hostname)                     | auto          | HOSTNAME               |
| [interval](#check-interval)               | 5m            | Check interval         |
| [password](#op5-password)                 |               | Op5 password           |
| [remove](#remove-checks-on-exit)          | false         | Remove checks on exit  |
| [server](#op5-base-url)                   |               | Op5 base url           |
| [user](#op5-user)                         |               | Op5 user               |



```ini
# Section for the Op5 server
[/settings/op5]
channel=op5
default checks=true
hostname=auto
interval=5m
remove=false

```





#### CHANNEL <a id="/settings/op5/channel"></a>

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



#### Contact groups <a id="/settings/op5/contactgroups"></a>

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



#### Install default checks <a id="/settings/op5/default checks"></a>

Set to false to disable default checks





| Key            | Description                     |
|----------------|---------------------------------|
| Path:          | [/settings/op5](#/settings/op5) |
| Key:           | default checks                  |
| Default value: | `true`                          |
| Used by:       | Op5Client                       |


**Sample:**

```
[/settings/op5]
# Install default checks
default checks=true
```



#### Host groups <a id="/settings/op5/hostgroups"></a>

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



#### HOSTNAME <a id="/settings/op5/hostname"></a>

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



#### Check interval <a id="/settings/op5/interval"></a>

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



#### Op5 password <a id="/settings/op5/password"></a>

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



#### Remove checks on exit <a id="/settings/op5/remove"></a>

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



#### Op5 base url <a id="/settings/op5/server"></a>

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



#### Op5 user <a id="/settings/op5/user"></a>

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


### Op5 passive Commands <a id="/settings/op5/checks"/>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.






