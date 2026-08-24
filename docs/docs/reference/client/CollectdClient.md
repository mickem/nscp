# CollectdClient

CollectD client can be used to submit metrics to a collectd server

## Enable module

To enable this module and and allow using the commands you need to ass `CollectdClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CollectdClient = enabled
```


## Configuration

| Path / Section                                                  | Description               |
|-----------------------------------------------------------------|---------------------------|
| [/settings/collectd/client](#collectd-client-section)           | COLLECTD CLIENT SECTION   |
| [/settings/collectd/client/metrics](#metric-mappings)           | METRIC MAPPINGS           |
| [/settings/collectd/client/targets](#remote-target-definitions) | REMOTE TARGET DEFINITIONS |
| [/settings/collectd/client/variables](#variable-definitions)    | VARIABLE DEFINITIONS      |


### COLLECTD CLIENT SECTION <a id="/settings/collectd/client"></a>

Section for the collectd client; forwards NSClient++ metrics to a collectd server.

| Key                           | Default Value | Description      |
|-------------------------------|---------------|------------------|
| [hostname](#hostname)         | auto          | HOSTNAME         |
| [interval](#metrics-interval) | 10            | METRICS INTERVAL |


```ini
# Section for the collectd client; forwards NSClient++ metrics to a collectd server.
[/settings/collectd/client]
hostname=auto
interval=10
```

#### HOSTNAME <a id="/settings/collectd/client/hostname"></a>

The host name reported to collectd.
Set this to auto (default) to use the name of this computer.

auto	Hostname
${host}	Hostname
${host_lc}	Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase



| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/collectd/client](#/settings/collectd/client) |
| Key:           | hostname                                                |
| Default value: | `auto`                                                  |


**Sample:**

```
[/settings/collectd/client]
# HOSTNAME
hostname=auto
```

#### METRICS INTERVAL <a id="/settings/collectd/client/interval"></a>

The interval (in seconds) reported to collectd. Should match the core 'metrics interval' so collectd computes rates correctly.


| Key            | Description                                             |
|----------------|---------------------------------------------------------|
| Path:          | [/settings/collectd/client](#/settings/collectd/client) |
| Key:           | interval                                                |
| Default value: | `10`                                                    |


**Sample:**

```
[/settings/collectd/client]
# METRICS INTERVAL
interval=10
```

### METRIC MAPPINGS <a id="/settings/collectd/client/metrics"></a>

Mapping of collectd keys (e.g. cpu-total/cpu-user) to value expressions (e.g. derive:system.cpu.total.user). When empty a built-in default set is used.



```ini
# Mapping of collectd keys (e.g. cpu-total/cpu-user) to value expressions (e.g. derive:system.cpu.total.user). When empty a built-in default set is used.
[/settings/collectd/client/metrics]
```

### REMOTE TARGET DEFINITIONS <a id="/settings/collectd/client/targets"></a>




This is a section of objects. This means that you will create objects below this point by adding sections which all look the same.


**Keys:**


| Key      | Default Value | Description      |
|----------|---------------|------------------|
| address  |               | TARGET ADDRESS   |
| host     |               | TARGET HOST      |
| interval |               | METRICS INTERVAL |
| port     |               | TARGET PORT      |
| retries  | 3             | RETRIES          |
| timeout  | 30            | TIMEOUT          |


**Sample:**

```ini
# An example of a REMOTE TARGET DEFINITIONS section
[/settings/collectd/client/targets/sample]
#address=...
#host=...
#interval=...
#port=...
retries=3
timeout=30

```






### VARIABLE DEFINITIONS <a id="/settings/collectd/client/variables"></a>

Variables used to expand ${...} placeholders in metric keys. Each value is a regular expression matched against metric names; the captured groups become the variable's values. When empty a built-in default set is used.



```ini
# Variables used to expand ${...} placeholders in metric keys. Each value is a regular expression matched against metric names; the captured groups become the variable's values. When empty a built-in default set is used.
[/settings/collectd/client/variables]
```
