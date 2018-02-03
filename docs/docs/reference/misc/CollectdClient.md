# CollectdClient

CollectD client can be used to submit metrics to a collectd server






## Configuration



| Path / Section                                                  | Description               |
|-----------------------------------------------------------------|---------------------------|
| [/settings/collectd/client](#collectd-client-section)           | COLLECTD CLIENT SECTION   |
| [/settings/collectd/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |



### COLLECTD CLIENT SECTION <a id="/settings/collectd/client"/>

Section for NSCA passive check module.




| Key                   | Default Value | Description |
|-----------------------|---------------|-------------|
| [hostname](#hostname) | auto          | HOSTNAME    |



```ini
# Section for NSCA passive check module.
[/settings/collectd/client]
hostname=auto

```





#### HOSTNAME <a id="/settings/collectd/client/hostname"></a>

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


### REMOTE TARGET DEFINITIONS <a id="/settings/collectd/client/targets"/>




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






