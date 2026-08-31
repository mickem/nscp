# ElasticClient

Elastic sends metrics, events and logs to elastic search

## Enable module

To enable this module and and allow using the commands you need to ass `ElasticClient = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
ElasticClient = enabled
```


## Configuration

| Path / Section                                        | Description |
|-------------------------------------------------------|-------------|
| [/settings/elastic/client](#/settings/elastic/client) |             |


### /settings/elastic/client <a id="/settings/elastic/client"></a>



| Key                                                            | Default Value            | Description                             |
|----------------------------------------------------------------|--------------------------|-----------------------------------------|
| [address](#elastic-address)                                    |                          | Elastic address                         |
| [api key](#elastic-api-key)                                    |                          | Elastic API key                         |
| [ca](#certificate-authority)                                   | ${ca-path}               | Certificate authority                   |
| [event index](#elastic-index-used-for-events)                  | nsclient_event-%(date)   | Elastic index used for events           |
| [event type](#elastic-type-used-for-events)                    |                          | Elastic type used for events            |
| [events](#event)                                               | eventlog:*,logfile:*     | Event                                   |
| [hostname](#hostname)                                          | auto                     | HOSTNAME                                |
| [metrics index](#elastic-index-used-for-metrics)               | nsclient_metrics-%(date) | Elastic index used for metrics          |
| [metrics type](#elastic-type-used-for-metrics)                 |                          | Elastic type used for metrics           |
| [nsclient log index](#elastic-index-used-for-the-nsclient-log) | nsclient_log-%(date)     | Elastic index used for the nsclient log |
| [nsclient log type](#elastic-type-used-for-the-nsclient-log)   |                          | Elastic type used for the nsclient log  |
| [password](#elastic-password)                                  |                          | Elastic password                        |
| [timeout](#timeout)                                            | 30                       | Timeout                                 |
| [tls version](#tls-version)                                    | 1.2+                     | TLS version                             |
| [user](#elastic-user)                                          |                          | Elastic user                            |
| [verify mode](#tls-verify-mode)                                | peer                     | TLS verify mode                         |


```ini
# 
[/settings/elastic/client]
ca=${ca-path}
event index=nsclient_event-%(date)
events=eventlog:*,logfile:*
hostname=auto
metrics index=nsclient_metrics-%(date)
nsclient log index=nsclient_log-%(date)
timeout=30
tls version=1.2+
verify mode=peer
```

#### Elastic address <a id="/settings/elastic/client/address"></a>

The address to send data to (http://127.0.0.1:9200/_bulk).


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | address                                               |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic address
address=
```

#### Elastic API key <a id="/settings/elastic/client/api key"></a>

An Elasticsearch API key (the base64 encoded id:key value as returned when the key is created), sent as 'Authorization: ApiKey ...'. Takes precedence over user/password when both are set.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | api key                                               |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic API key
api key=
```

#### Certificate authority <a id="/settings/elastic/client/ca"></a>

The certificate authority bundle used to verify the Elasticsearch server certificate (used when 'verify mode' is not 'none').


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | ca                                                    |
| Default value: | `${ca-path}`                                          |


**Sample:**

```
[/settings/elastic/client]
# Certificate authority
ca=${ca-path}
```

#### Elastic index used for events <a id="/settings/elastic/client/event index"></a>

The elastic index to use for events (log messages).


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | event index                                           |
| Default value: | `nsclient_event-%(date)`                              |


**Sample:**

```
[/settings/elastic/client]
# Elastic index used for events
event index=nsclient_event-%(date)
```

#### Elastic type used for events <a id="/settings/elastic/client/event type"></a>

The elastic type to use for events (log messages). Only set this for Elasticsearch 6.x or older: mapping types were removed in Elasticsearch 8, which rejects requests that carry a type.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | event type                                            |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic type used for events
event type=
```

#### Event <a id="/settings/elastic/client/events"></a>

The events to subscribe to such as eventlog:* or logfile:mylog.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | events                                                |
| Default value: | `eventlog:*,logfile:*`                                |


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
${host_lc}	Hostname in lowercase
${host_uc}	Hostname in uppercase
${domain}	Domainname
${domain_lc}	Domainname in lowercase
${domain_uc}	Domainname in uppercase
${address_ipv4}	IPv4 address of the computer
${address_ipv6}	IPv6 address of the computer (lowercase, compressed)
${address_ipv6_lc}	IPv6 address in lowercase (compressed)
${address_ipv6_uc}	IPv6 address in uppercase (compressed)
${address_ipv6_lc_comp}	IPv6 address in lowercase, compressed (2001:db8::7)
${address_ipv6_lc_uncomp}	IPv6 address in lowercase, uncompressed (2001:0db8:0000:0000:0000:0000:0000:0007)
${address_ipv6_uc_comp}	IPv6 address in uppercase, compressed
${address_ipv6_uc_uncomp}	IPv6 address in uppercase, uncompressed



| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | hostname                                              |
| Default value: | `auto`                                                |


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


**Sample:**

```
[/settings/elastic/client]
# Elastic index used for metrics
metrics index=nsclient_metrics-%(date)
```

#### Elastic type used for metrics <a id="/settings/elastic/client/metrics type"></a>

The elastic type to use for metrics. Only set this for Elasticsearch 6.x or older: mapping types were removed in Elasticsearch 8, which rejects requests that carry a type.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | metrics type                                          |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic type used for metrics
metrics type=
```

#### Elastic index used for the nsclient log <a id="/settings/elastic/client/nsclient log index"></a>

The elastic index to use for the NSClient++ log.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | nsclient log index                                    |
| Default value: | `nsclient_log-%(date)`                                |


**Sample:**

```
[/settings/elastic/client]
# Elastic index used for the nsclient log
nsclient log index=nsclient_log-%(date)
```

#### Elastic type used for the nsclient log <a id="/settings/elastic/client/nsclient log type"></a>

The elastic type to use for the NSClient++ log. Only set this for Elasticsearch 6.x or older: mapping types were removed in Elasticsearch 8, which rejects requests that carry a type.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | nsclient log type                                     |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic type used for the nsclient log
nsclient log type=
```

#### Elastic password <a id="/settings/elastic/client/password"></a>

The password used to authenticate against Elasticsearch (basic authentication).


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | password                                              |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic password
password=
```

#### Timeout <a id="/settings/elastic/client/timeout"></a>

Timeout (in seconds) for each connect, read and write when talking to Elasticsearch. 0 waits forever.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | timeout                                               |
| Default value: | `30`                                                  |


**Sample:**

```
[/settings/elastic/client]
# Timeout
timeout=30
```

#### TLS version <a id="/settings/elastic/client/tls version"></a>

The TLS version to use when connecting over https (1.0, 1.1, 1.2, 1.2+ or 1.3).


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | tls version                                           |
| Default value: | `1.2+`                                                |


**Sample:**

```
[/settings/elastic/client]
# TLS version
tls version=1.2+
```

#### Elastic user <a id="/settings/elastic/client/user"></a>

The username used to authenticate against Elasticsearch (basic authentication). Leave empty to send no credentials.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | user                                                  |
| Default value: | _N/A_                                                 |


**Sample:**

```
[/settings/elastic/client]
# Elastic user
user=
```

#### TLS verify mode <a id="/settings/elastic/client/verify mode"></a>

How to verify the Elasticsearch server certificate when connecting over https. 'peer' (the default) validates the certificate chain and hostname against the configured CA. Set to 'none' to disable verification - this is insecure and lets an on-path attacker read the submitted data and any configured credentials.


| Key            | Description                                           |
|----------------|-------------------------------------------------------|
| Path:          | [/settings/elastic/client](#/settings/elastic/client) |
| Key:           | verify mode                                           |
| Default value: | `peer`                                                |


**Sample:**

```
[/settings/elastic/client]
# TLS verify mode
verify mode=peer
```
