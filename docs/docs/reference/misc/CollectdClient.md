# CollectdClient

CollectD client can be used to submit metrics to a collectd server







## List of Configuration


### Common Keys

| Path / Section                                                                          | Key                                                           | Description    |
|-----------------------------------------------------------------------------------------|---------------------------------------------------------------|----------------|
| [/settings/collectd/client](#/settings/collectd/client)                                 | [hostname](#/settings/collectd/client_hostname)               | HOSTNAME       |
| [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) | [address](#/settings/collectd/client/targets/default_address) | TARGET ADDRESS |
| [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) | [retries](#/settings/collectd/client/targets/default_retries) | RETRIES        |
| [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) | [timeout](#/settings/collectd/client/targets/default_timeout) | TIMEOUT        |

### Advanced keys

| Path / Section                                                                          | Key                                                     | Description |
|-----------------------------------------------------------------------------------------|---------------------------------------------------------|-------------|
| [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) | [host](#/settings/collectd/client/targets/default_host) | TARGET HOST |
| [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) | [port](#/settings/collectd/client/targets/default_port) | TARGET PORT |

### Sample keys

| Path / Section                                                                        | Key                                                          | Description    |
|---------------------------------------------------------------------------------------|--------------------------------------------------------------|----------------|
| [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) | [address](#/settings/collectd/client/targets/sample_address) | TARGET ADDRESS |
| [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) | [host](#/settings/collectd/client/targets/sample_host)       | TARGET HOST    |
| [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) | [port](#/settings/collectd/client/targets/sample_port)       | TARGET PORT    |
| [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) | [retries](#/settings/collectd/client/targets/sample_retries) | RETRIES        |
| [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) | [timeout](#/settings/collectd/client/targets/sample_timeout) | TIMEOUT        |





# Configuration

<a name="/settings/collectd/client"/>
## COLLECTD CLIENT SECTION

Section for NSCA passive check module.

```ini
# Section for NSCA passive check module.
[/settings/collectd/client]
hostname=auto

```


| Key                                             | Default Value | Description |
|-------------------------------------------------|---------------|-------------|
| [hostname](#/settings/collectd/client_hostname) | auto          | HOSTNAME    |




<a name="/settings/collectd/client_hostname"/>
### hostname

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


#### Sample

```
[/settings/collectd/client]
# HOSTNAME
hostname=auto
```


<a name="/settings/collectd/client/targets"/>
## REMOTE TARGET DEFINITIONS



```ini
# 
[/settings/collectd/client/targets]

```






<a name="/settings/collectd/client/targets/default"/>
## TARGET

Target definition for: default

```ini
# Target definition for: default
[/settings/collectd/client/targets/default]
retries=3
timeout=30

```


| Key                                                           | Default Value | Description    |
|---------------------------------------------------------------|---------------|----------------|
| [address](#/settings/collectd/client/targets/default_address) |               | TARGET ADDRESS |
| [host](#/settings/collectd/client/targets/default_host)       |               | TARGET HOST    |
| [port](#/settings/collectd/client/targets/default_port)       |               | TARGET PORT    |
| [retries](#/settings/collectd/client/targets/default_retries) | 3             | RETRIES        |
| [timeout](#/settings/collectd/client/targets/default_timeout) | 30            | TIMEOUT        |




<a name="/settings/collectd/client/targets/default_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) |
| Key:           | address                                                                                 |
| Default value: | _N/A_                                                                                   |
| Used by:       | CollectdClient                                                                          |


#### Sample

```
[/settings/collectd/client/targets/default]
# TARGET ADDRESS
address=
```


<a name="/settings/collectd/client/targets/default_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) |
| Key:           | host                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | CollectdClient                                                                          |


#### Sample

```
[/settings/collectd/client/targets/default]
# TARGET HOST
host=
```


<a name="/settings/collectd/client/targets/default_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) |
| Key:           | port                                                                                    |
| Advanced:      | Yes (means it is not commonly used)                                                     |
| Default value: | _N/A_                                                                                   |
| Used by:       | CollectdClient                                                                          |


#### Sample

```
[/settings/collectd/client/targets/default]
# TARGET PORT
port=
```


<a name="/settings/collectd/client/targets/default_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) |
| Key:           | retries                                                                                 |
| Default value: | `3`                                                                                     |
| Used by:       | CollectdClient                                                                          |


#### Sample

```
[/settings/collectd/client/targets/default]
# RETRIES
retries=3
```


<a name="/settings/collectd/client/targets/default_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                             |
|----------------|-----------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/default](#/settings/collectd/client/targets/default) |
| Key:           | timeout                                                                                 |
| Default value: | `30`                                                                                    |
| Used by:       | CollectdClient                                                                          |


#### Sample

```
[/settings/collectd/client/targets/default]
# TIMEOUT
timeout=30
```


<a name="/settings/collectd/client/targets/sample"/>
## TARGET

Target definition for: sample

```ini
# Target definition for: sample
[/settings/collectd/client/targets/sample]
retries=3
timeout=30

```


| Key                                                          | Default Value | Description    |
|--------------------------------------------------------------|---------------|----------------|
| [address](#/settings/collectd/client/targets/sample_address) |               | TARGET ADDRESS |
| [host](#/settings/collectd/client/targets/sample_host)       |               | TARGET HOST    |
| [port](#/settings/collectd/client/targets/sample_port)       |               | TARGET PORT    |
| [retries](#/settings/collectd/client/targets/sample_retries) | 3             | RETRIES        |
| [timeout](#/settings/collectd/client/targets/sample_timeout) | 30            | TIMEOUT        |




<a name="/settings/collectd/client/targets/sample_address"/>
### address

**TARGET ADDRESS**

Target host address





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) |
| Key:           | address                                                                               |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CollectdClient                                                                        |


#### Sample

```
[/settings/collectd/client/targets/sample]
# TARGET ADDRESS
address=
```


<a name="/settings/collectd/client/targets/sample_host"/>
### host

**TARGET HOST**

The target server to report results to.





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) |
| Key:           | host                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CollectdClient                                                                        |


#### Sample

```
[/settings/collectd/client/targets/sample]
# TARGET HOST
host=
```


<a name="/settings/collectd/client/targets/sample_port"/>
### port

**TARGET PORT**

The target server port





| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) |
| Key:           | port                                                                                  |
| Advanced:      | Yes (means it is not commonly used)                                                   |
| Default value: | _N/A_                                                                                 |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CollectdClient                                                                        |


#### Sample

```
[/settings/collectd/client/targets/sample]
# TARGET PORT
port=
```


<a name="/settings/collectd/client/targets/sample_retries"/>
### retries

**RETRIES**

Number of times to retry sending.




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) |
| Key:           | retries                                                                               |
| Default value: | `3`                                                                                   |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CollectdClient                                                                        |


#### Sample

```
[/settings/collectd/client/targets/sample]
# RETRIES
retries=3
```


<a name="/settings/collectd/client/targets/sample_timeout"/>
### timeout

**TIMEOUT**

Timeout when reading/writing packets to/from sockets.




| Key            | Description                                                                           |
|----------------|---------------------------------------------------------------------------------------|
| Path:          | [/settings/collectd/client/targets/sample](#/settings/collectd/client/targets/sample) |
| Key:           | timeout                                                                               |
| Default value: | `30`                                                                                  |
| Sample key:    | Yes (This section is only to show how this key is used)                               |
| Used by:       | CollectdClient                                                                        |


#### Sample

```
[/settings/collectd/client/targets/sample]
# TIMEOUT
timeout=30
```


