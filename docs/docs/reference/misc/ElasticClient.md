# ElasticClient

Elastic sends metrics, events and logs to elastic search






## Configuration



| Path / Section                | Description |
|-------------------------------|-------------|
| [/settings/elastic/client](#) |             |



### /settings/elastic/client <a id="/settings/elastic/client"/>






| Key                                                   | Default Value            | Description                    |
|-------------------------------------------------------|--------------------------|--------------------------------|
| [address](#elastic-address)                           |                          | Elastic address                |
| [event index](#elastic-index-used-for-events)         | nsclient_event-%(date)   | Elastic index used for events  |
| [event type](#elastic-type-used-for-events)           | eventlog                 | Elastic type used for events   |
| [events](#event)                                      | eventlog:*,logfile:*     | Event                          |
| [hostname](#hostname)                                 | auto                     | HOSTNAME                       |
| [metrics index](#elastic-index-used-for-metrics)      | nsclient_metrics-%(date) | Elastic index used for metrics |
| [metrics type](#elastic-type-used-for-metrics)        | metrics                  | Elastic type used for metrics  |
| [nsclient log index](#elastic-index-used-for-metrics) | nsclient_log-%(date)     | Elastic index used for metrics |
| [nsclient log type](#elastic-type-used-for-metrics)   | nsclient log             | Elastic type used for metrics  |



```ini
# 
[/settings/elastic/client]
event index=nsclient_event-%(date)
event type=eventlog
events=eventlog:*,logfile:*
hostname=auto
metrics index=nsclient_metrics-%(date)
metrics type=metrics
nsclient log index=nsclient_log-%(date)
nsclient log type=nsclient log

```





#### Elastic address <a id="/settings/elastic/client/address"></a>

The address to send data to (http://127.0.0.1:9200/_bulk).






| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | address                                               |
| Default value: | _N/A_                                                 |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic address
address=
```



#### Elastic index used for events <a id="/settings/elastic/client/event index"></a>

The elastic index to use for events (log messages).





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | event index                                           |
| Default value: | `nsclient_event-%(date)`                              |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic index used for events
event index=nsclient_event-%(date)
```



#### Elastic type used for events <a id="/settings/elastic/client/event type"></a>

The elastic type to use for events (log messages).





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | event type                                            |
| Default value: | `eventlog`                                            |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic type used for events
event type=eventlog
```



#### Event <a id="/settings/elastic/client/events"></a>

The events to subscribe to such as eventlog:* or logfile:mylog.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | events                                                |
| Default value: | `eventlog:*,logfile:*`                                |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Event
events=eventlog:*,logfile:*
```



#### HOSTNAME <a id="/settings/elastic/client/hostname"></a>

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






| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | hostname                                              |
| Default value: | `auto`                                                |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# HOSTNAME
hostname=auto
```



#### Elastic index used for metrics <a id="/settings/elastic/client/metrics index"></a>

The elastic index to use for metrics.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | metrics index                                         |
| Default value: | `nsclient_metrics-%(date)`                            |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic index used for metrics
metrics index=nsclient_metrics-%(date)
```



#### Elastic type used for metrics <a id="/settings/elastic/client/metrics type"></a>

The elastic type to use for metrics.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | metrics type                                          |
| Default value: | `metrics`                                             |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic type used for metrics
metrics type=metrics
```



#### Elastic index used for metrics <a id="/settings/elastic/client/nsclient log index"></a>

The elastic index to use for metrics.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | nsclient log index                                    |
| Default value: | `nsclient_log-%(date)`                                |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic index used for metrics
nsclient log index=nsclient_log-%(date)
```



#### Elastic type used for metrics <a id="/settings/elastic/client/nsclient log type"></a>

The elastic type to use for metrics.





| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | nsclient log type                                     |
| Default value: | `nsclient log`                                        |
| Used by:       | ElasticClient                                         |


**Sample:**

```
[/settings/elastic/client]
# Elastic type used for metrics
nsclient log type=nsclient log
```


