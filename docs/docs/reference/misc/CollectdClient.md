# CollectdClient

CollectD client can be used to submit metrics to a collectd server







**Configuration Keys:**



    
    
| Path / Section                                          | Key                                             | Description |
|---------------------------------------------------------|-------------------------------------------------|-------------|
| [/settings/collectd/client](#/settings/collectd/client) | [hostname](#/settings/collectd/client_hostname) | HOSTNAME    |


| Path / Section                                                          | Description               |
|-------------------------------------------------------------------------|---------------------------|
| [/settings/collectd/client/targets](#/settings/collectd/client/targets) | REMOTE TARGET DEFINITIONS |





## Configuration

<a name="/settings/collectd/client"/>
### COLLECTD CLIENT SECTION

Section for NSCA passive check module.




| Key                                             | Default Value | Description |
|-------------------------------------------------|---------------|-------------|
| [hostname](#/settings/collectd/client_hostname) | auto          | HOSTNAME    |



```ini
# Section for NSCA passive check module.
[/settings/collectd/client]
hostname=auto

```




<a name="/settings/collectd/client_hostname"/>

**HOSTNAME**

The host name of the monitored computer.
Set this to auto (default) to use the windows name of the computer.

auto	Hostname
${host}	Hostname
${host_lc}
Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase






| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/collectd/client](#/settings/collectd/client) |
| Key:           | hostname                                                |
| Default value: | `auto`                                                  |
| Used by:       | CollectdClient                                          |


**Sample:**

```
[/settings/collectd/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/collectd/client/targets"/>
### REMOTE TARGET DEFINITIONS




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key     | Default Value | Description    |
|---------|---------------|----------------|
| address |               | TARGET ADDRESS |
| host    |               | TARGET HOST    |
| port    |               | TARGET PORT    |
| retries | 3             | RETRIES        |
| timeout | 30            | TIMEOUT        |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/collectd/client/targets/sample]
#address=...
#host=...
#port=...
retries=3
timeout=30

```






