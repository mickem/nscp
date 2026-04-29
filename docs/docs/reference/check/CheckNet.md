# CheckNet

Network related checks such as check_ping, check_tcp, check_dns, check_http, check_connections and check_ntp_offset.



## Enable module

To enable this module and and allow using the commands you need to ass `CheckNet = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckNet = enabled
```


## Queries

A quick reference for all available queries (check commands) in the CheckNet module.

**List of commands:**

A list of all available queries (check commands)

| Command                                 | Description                                                                                 |
|-----------------------------------------|---------------------------------------------------------------------------------------------|
| [check_connections](#check_connections) | Count active TCP/UDP connections and report counts per protocol and TCP state.              |
| [check_dns](#check_dns)                 | Resolve a host name and check the response time and resulting addresses.                    |
| [check_http](#check_http)               | Send an HTTP/HTTPS request and check the response status, time, size and body.              |
| [check_ntp_offset](#check_ntp_offset)   | Query an NTP server and check the offset between the local clock and the server.            |
| [check_ping](#check_ping)               | Ping another host and check the result.                                                     |
| [check_tcp](#check_tcp)                 | Connect to a TCP port and optionally send/expect data to check that a service is reachable. |




### check_connections

Count active TCP/UDP connections and report counts per protocol and TCP state.


**Jump to section:**

* [Sample Commands](#check_connections_samples)
* [Command-line Arguments](#check_connections_options)
* [Filter keywords](#check_connections_filter_keys)


<a id="check_connections_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckNet_check_connections_samples.md)_

**Default check (uses the `total` bucket):**

```
check_connections
L        cli OK: OK: total/all: 226
L        cli  Performance data: 'total_all_close_wait'=0;0;0 'total_all_closing'=0;0;0 'total_all_established'=90;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=0;0;0 'total_all_time_wait'=6;0;0 'total_all_total'=226;0;0 'total_all_udp'=61;0;0
```

**Per-protocol breakdown (disable the default total filter):**

```
check_connections "filter=state = 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(protocol)/%(family)=%(count)"
L        cli OK: OK: tcp/ipv4=157, tcp6/ipv6=15, udp/ipv4=40, udp6/ipv6=21, total/any=233
L        cli  Performance data: 'tcp_all_close_wait'=0;0;0 'tcp_all_closing'=0;0;0 'tcp_all_established'=0;0;0 'tcp_all_fin_wait'=0;0;0 'tcp_all_last_ack'=0;0;0 'tcp_all_listen'=0;0;0 'tcp_all_syn_recv'=0;0;0 'tcp_all_syn_sent'=0;0;0 'tcp_all_time_wait'=0;0;0 'tcp_all_total'=0;0;0 'tcp_all_udp'=0;0;0 'tcp6_all_close_wait'=0;0;0 'tcp6_all_closing'=0;0;0 'tcp6_all_established'=0;0;0 'tcp6_all_fin_wait'=0;0;0 'tcp6_all_last_ack'=0;0;0 'tcp6_all_listen'=0;0;0 'tcp6_all_syn_recv'=0;0;0 'tcp6_all_syn_sent'=0;0;0 'tcp6_all_time_wait'=0;0;0 'tcp6_all_total'=0;0;0 'tcp6_all_udp'=0;0;0 'udp_all_close_wait'=0;0;0 'udp_all_closing'=0;0;0 'udp_all_established'=0;0;0 'udp_all_fin_wait'=0;0;0 'udp_all_last_ack'=0;0;0 'udp_all_listen'=0;0;0 'udp_all_syn_recv'=0;0;0 'udp_all_syn_sent'=0;0;0 'udp_all_time_wait'=0;0;0 'udp_all_total'=0;0;0 'udp_all_udp'=0;0;0 'udp6_all_close_wait'=0;0;0 'udp6_all_closing'=0;0;0 'udp6_all_established'=0;0;0 'udp6_all_fin_wait'=0;0;0 'udp6_all_last_ack'=0;0;0 'udp6_all_listen'=0;0;0 'udp6_all_syn_recv'=0;0;0 'udp6_all_syn_sent'=0;0;0 'udp6_all_time_wait'=0;0;0 'udp6_all_total'=0;0;0 'udp6_all_udp'=0;0;0 'total_all_close_wait'=1;0;0 'total_all_closing'=0;0;0 'total_all_established'=93;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=0;0;0 'total_all_time_wait'=9;0;0 'total_all_total'=233;0;0 'total_all_udp'=61;0;0
```

**Show only TCP states:**

```
check_connections "filter=protocol = 'tcp' and state != 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(state)=%(count)"
check_connections "filter=protocol = 'tcp' and state != 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(state)=%(count)"
L        cli OK: OK: ESTABLISHED=92, LISTEN=69, TIME_WAIT=9
```

**Warn/critical based on total connections:**

```
check_connections "warn=total > 500" "crit=total > 1000"
L        cli OK: OK: total/all: 231
```

**Warn when many sockets are stuck in TIME_WAIT:**

```
check_connections "filter=protocol = 'tcp' and state = 'TIME_WAIT'" "warn=count > 200" "crit=count > 1000"
L        cli OK: OK: tcp/TIME_WAIT: 14
```

**Alert on growing CLOSE_WAIT (often indicates leaks):**

```
check_connections "filter=state = 'CLOSE_WAIT'" "warn=count > 50" "crit=count > 200"
L        cli OK: No connection data
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_connections
OK: total/all: 231|'total_all_close_wait'=0;0;0 'total_all_closing'=0;0;0 'total_all_established'=85;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=1;0;0 'total_all_time_wait'=16;0;0 'total_all_total'=231;0;0 'total_all_udp'=60;0;0
```




<a id="check_connections_warn"></a>
<a id="check_connections_crit"></a>
<a id="check_connections_debug"></a>
<a id="check_connections_show-all"></a>
<a id="check_connections_escape-html"></a>
<a id="check_connections_help"></a>
<a id="check_connections_help-pb"></a>
<a id="check_connections_show-default"></a>
<a id="check_connections_help-short"></a>
<a id="check_connections_options"></a>
#### Command-line Arguments


| Option                                            | Default Value                  | Description                                                                                                      |
|---------------------------------------------------|--------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_connections_filter)               | protocol = 'total'             | Filter which marks interesting items.                                                                            |
| [warning](#check_connections_warning)             |                                | Filter which marks items which generates a warning state.                                                        |
| warn                                              |                                | Short alias for warning                                                                                          |
| [critical](#check_connections_critical)           |                                | Filter which marks items which generates a critical state.                                                       |
| crit                                              |                                | Short alias for critical.                                                                                        |
| [ok](#check_connections_ok)                       |                                | Filter which marks items which generates an ok state.                                                            |
| debug                                             | N/A                            | Show debugging information in the log                                                                            |
| show-all                                          | N/A                            | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_connections_empty-state)     | ignored                        | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_connections_perf-config)     |                                | Performance data generation configuration                                                                        |
| escape-html                                       | N/A                            | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                              | N/A                            | Show help screen (this screen)                                                                                   |
| help-pb                                           | N/A                            | Show help screen as a protocol buffer payload                                                                    |
| show-default                                      | N/A                            | Show default values for a given command                                                                          |
| help-short                                        | N/A                            | Show help screen (short format).                                                                                 |
| [top-syntax](#check_connections_top-syntax)       | ${status}: ${list}             | Top level syntax.                                                                                                |
| [ok-syntax](#check_connections_ok-syntax)         | %(status): %(list)             | ok syntax.                                                                                                       |
| [empty-syntax](#check_connections_empty-syntax)   | No connection data             | Empty syntax.                                                                                                    |
| [detail-syntax](#check_connections_detail-syntax) | ${protocol}/${state}: ${count} | Detail level syntax.                                                                                             |
| [perf-syntax](#check_connections_perf-syntax)     | ${protocol}_${state}           | Performance alias syntax.                                                                                        |



<h5 id="check_connections_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.

*Default Value:* `protocol = 'total'`

<h5 id="check_connections_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.



<h5 id="check_connections_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.



<h5 id="check_connections_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_connections_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_connections_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_connections_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${list}`

<h5 id="check_connections_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): %(list)`

<h5 id="check_connections_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No connection data`

<h5 id="check_connections_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${protocol}/${state}: ${count}`

<h5 id="check_connections_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${protocol}_${state}`


<a id="check_connections_filter_keys"></a>
#### Filter keywords


| Option      | Description                                                   |
|-------------|---------------------------------------------------------------|
| close_wait  | Number of TCP connections in CLOSE_WAIT state (total bucket)  |
| closing     | Number of TCP connections in CLOSING state (total bucket)     |
| established | Number of TCP connections in ESTABLISHED state (total bucket) |
| family      | Address family (ipv4, ipv6, any)                              |
| fin_wait    | Number of TCP connections in FIN_WAIT* state (total bucket)   |
| last_ack    | Number of TCP connections in LAST_ACK state (total bucket)    |
| listen      | Number of TCP sockets in LISTEN state (total bucket)          |
| protocol    | Protocol of this bucket (tcp, tcp6, udp, udp6, total)         |
| state       | TCP state name (ESTABLISHED, LISTEN, ...) or 'all'            |
| syn_recv    | Number of TCP connections in SYN_RECV state (total bucket)    |
| syn_sent    | Number of TCP connections in SYN_SENT state (total bucket)    |
| time_wait   | Number of TCP connections in TIME_WAIT state (total bucket)   |
| udp         | Number of UDP sockets (total bucket)                          |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_dns

Resolve a host name and check the response time and resulting addresses.


**Jump to section:**

* [Sample Commands](#check_dns_samples)
* [Command-line Arguments](#check_dns_options)
* [Filter keywords](#check_dns_filter_keys)


<a id="check_dns_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckNet_check_dns_samples.md)_

**Default lookup of a hostname:**

```
check_dns host=google.com
check_dns host=google.com
L        cli OK: OK: google.com -> 172.217.20.174 (1) in 10ms [ok]
L        cli  Performance data: 'google.com_time'=10;1000;0
```

**Lookup with a custom timeout:**

```
check_dns host=google.com timeout=2000
check_dns host=google.com timeout=2000
L        cli OK: OK: google.com -> 172.217.20.174 (1) in 3ms [ok]
L        cli  Performance data: 'google.com_time'=3;1000;0
```

**Verify the resolver returns specific addresses:**

```
check_dns host=google.com expected-address=172.217.20.174
L        cli OK: OK: google.com -> 172.217.20.174 (1) in 3ms [ok]
L        cli  Performance data: 'google.com_time'=3;1000;0
```

**Verify against multiple expected addresses (comma list):**

```
check_dns host=google.com "expected=93.184.216.34,2606:2800:220:1:248:1893:25c8:1946"
L        cli CRITICAL: CRITICAL: google.com -> 172.217.20.174 (1) in 7ms [mismatch]
L        cli  Performance data: 'google.com_time'=7;1000;0
```

**Tighter latency thresholds:**

```
check_dns host=nsclient.org "warn=time > 100" "crit=time > 5 or result != 'ok'"
L        cli CRITICAL: CRITICAL: nsclient.org -> 188.114.97.1,188.114.96.1 (2) in 8ms [ok]
L        cli  Performance data: 'nsclient.org_time'=8;100;5
```

**Custom output text:**

```
check_dns host=google.com "top-syntax=%(status): %(list)" "detail-syntax=%(host)=%(addresses) [%(result)]"
L        cli OK: OK: google.com=172.217.20.174 [ok]
L        cli  Performance data: 'google.com_time'=5;1000;0
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_dns --argument "host=example.com"
OK: google.com -> 172.217.20.174 (1) in 10ms [ok]|'google.com_time'=10;1000;0
```




<a id="check_dns_warn"></a>
<a id="check_dns_crit"></a>
<a id="check_dns_debug"></a>
<a id="check_dns_show-all"></a>
<a id="check_dns_escape-html"></a>
<a id="check_dns_help"></a>
<a id="check_dns_help-pb"></a>
<a id="check_dns_show-default"></a>
<a id="check_dns_help-short"></a>
<a id="check_dns_host"></a>
<a id="check_dns_lookup"></a>
<a id="check_dns_expected-address"></a>
<a id="check_dns_expected"></a>
<a id="check_dns_options"></a>
#### Command-line Arguments


| Option                                    | Default Value                                               | Description                                                                                                      |
|-------------------------------------------|-------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_dns_filter)               |                                                             | Filter which marks interesting items.                                                                            |
| [warning](#check_dns_warning)             | time > 1000                                                 | Filter which marks items which generates a warning state.                                                        |
| warn                                      |                                                             | Short alias for warning                                                                                          |
| [critical](#check_dns_critical)           | result != 'ok'                                              | Filter which marks items which generates a critical state.                                                       |
| crit                                      |                                                             | Short alias for critical.                                                                                        |
| [ok](#check_dns_ok)                       |                                                             | Filter which marks items which generates an ok state.                                                            |
| debug                                     | N/A                                                         | Show debugging information in the log                                                                            |
| show-all                                  | N/A                                                         | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_dns_empty-state)     | ignored                                                     | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_dns_perf-config)     |                                                             | Performance data generation configuration                                                                        |
| escape-html                               | N/A                                                         | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                      | N/A                                                         | Show help screen (this screen)                                                                                   |
| help-pb                                   | N/A                                                         | Show help screen as a protocol buffer payload                                                                    |
| show-default                              | N/A                                                         | Show default values for a given command                                                                          |
| help-short                                | N/A                                                         | Show help screen (short format).                                                                                 |
| [top-syntax](#check_dns_top-syntax)       | ${status}: ${problem_list}                                  | Top level syntax.                                                                                                |
| [ok-syntax](#check_dns_ok-syntax)         | %(status): %(list)                                          | ok syntax.                                                                                                       |
| [empty-syntax](#check_dns_empty-syntax)   | No DNS lookup performed                                     | Empty syntax.                                                                                                    |
| [detail-syntax](#check_dns_detail-syntax) | ${host} -> ${addresses} (${count}) in ${time}ms [${result}] | Detail level syntax.                                                                                             |
| [perf-syntax](#check_dns_perf-syntax)     | ${host}                                                     | Performance alias syntax.                                                                                        |
| host                                      |                                                             | Hostname to look up.                                                                                             |
| lookup                                    |                                                             | Alias for --host.                                                                                                |
| [timeout](#check_dns_timeout)             | 5000                                                        | Timeout in milliseconds.                                                                                         |
| expected-address                          |                                                             | Address that must be present in the answer (may be given multiple times).                                        |
| expected                                  |                                                             | Comma separated list of addresses that must all be present in the answer.                                        |



<h5 id="check_dns_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_dns_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `time > 1000`

<h5 id="check_dns_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `result != 'ok'`

<h5 id="check_dns_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_dns_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_dns_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_dns_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_dns_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): %(list)`

<h5 id="check_dns_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No DNS lookup performed`

<h5 id="check_dns_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${host} -> ${addresses} (${count}) in ${time}ms [${result}]`

<h5 id="check_dns_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${host}`

<h5 id="check_dns_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `5000`


<a id="check_dns_filter_keys"></a>
#### Filter keywords


| Option    | Description                                                        |
|-----------|--------------------------------------------------------------------|
| addresses | Comma separated list of resolved addresses                         |
| host      | Hostname that was looked up                                        |
| result    | Textual result of the lookup (ok, not_found, mismatch, error, ...) |
| time      | Time taken by the lookup in milliseconds                           |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_http

Send an HTTP/HTTPS request and check the response status, time, size and body.


**Jump to section:**

* [Sample Commands](#check_http_samples)
* [Command-line Arguments](#check_http_options)
* [Filter keywords](#check_http_filter_keys)


<a id="check_http_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckNet_check_http_samples.md)_

**Default check against a single URL (success):**

```
check_http url=https://nsclient.org/
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 561ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=561;5000;0
```

**HTTPS with explicit port and path components (page not found):**

```
check_http protocol=https host=nsclient.org port=443 path=/no-such-page
L        cli CRITICAL: : https://nsclient.org:443/no-such-page -> 404 http_404 (46098B in 163ms)
L        cli  Performance data: 'https://nsclient.org:443/no-such-page_code'=404;0;200 'https://nsclient.org:443/no-such-page_time'=163;5000;0
```

**Connection / DNS failure (host does not resolve):**

```
check_http url=https://nope.invalid/
L        cli CRITICAL: : https://nope.invalid/ -> 0 error: resolve: Ingen sådan värd är känd (0B in 0ms)
L        cli  Performance data: 'https://nope.invalid/_code'=0;0;200 'https://nope.invalid/_time'=0;5000;0
```

**Multiple URLs in one call:**

```
check_http url=https://nsclient.org/ url=https://nsclient.org/nsclient/
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 39ms), https://nsclient.org/nsclient/ -> 200 ok (50656B in 160ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=39;5000;0 'https://nsclient.org/nsclient/_code'=200;0;200 'https://nsclient.org/nsclient/_time'=160;5000;0
```

**Require an expected substring in the response body:**

```
check_http url=https://nsclient.org/ expected-body="NSClient"
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 37ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=37;5000;0
```

If the substring is absent the check goes CRITICAL with `result=no_match`:

```
check_http url=https://nsclient.org/ expected-body="this-string-is-not-present"
L        cli CRITICAL: : https://nsclient.org/ -> 200 no_match (61204B in 34ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=34;5000;0
```

**Custom user-agent and extra headers:**

```
check_http url=https://nsclient.org/ user-agent="nscp-monitor/1" header="X-Trace: 1"
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 34ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=34;5000;0
```

**Tighter latency thresholds and code rules:**

```
check_http url=https://nsclient.org/ timeout=10000 "warn=time > 500 or code >= 400" "crit=time > 2000 or code >= 500 or result != 'ok'"
L        cli OK: : https://nsclient.org/ -> 200 ok (61204B in 36ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;400;500 'https://nsclient.org/_time'=36;500;2000
```

**Custom output text (drop result if you don't want it):**

```
check_http url=https://nsclient.org/ "top-syntax=%(status): %(list)" "detail-syntax=%(url) -> %(code) in %(time)ms"
L        cli OK: : https://nsclient.org/ -> 200 in 55ms
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=55;5000;0
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_http --argument "url=https://nsclient.org/"
OK: https://nsclient.org/ -> 200 ok (61204B in 561ms)| 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_time'=561;5000;0
```



<a id="check_http_warn"></a>
<a id="check_http_crit"></a>
<a id="check_http_debug"></a>
<a id="check_http_show-all"></a>
<a id="check_http_escape-html"></a>
<a id="check_http_help"></a>
<a id="check_http_help-pb"></a>
<a id="check_http_show-default"></a>
<a id="check_http_help-short"></a>
<a id="check_http_url"></a>
<a id="check_http_host"></a>
<a id="check_http_port"></a>
<a id="check_http_ssl"></a>
<a id="check_http_expected-body"></a>
<a id="check_http_header"></a>
<a id="check_http_ca"></a>
<a id="check_http_options"></a>
#### Command-line Arguments


| Option                                     | Default Value                                       | Description                                                                                                      |
|--------------------------------------------|-----------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_http_filter)               |                                                     | Filter which marks interesting items.                                                                            |
| [warning](#check_http_warning)             | time > 5000                                         | Filter which marks items which generates a warning state.                                                        |
| warn                                       |                                                     | Short alias for warning                                                                                          |
| [critical](#check_http_critical)           | code < 200 or code >= 400 or result != 'ok'         | Filter which marks items which generates a critical state.                                                       |
| crit                                       |                                                     | Short alias for critical.                                                                                        |
| [ok](#check_http_ok)                       |                                                     | Filter which marks items which generates an ok state.                                                            |
| debug                                      | N/A                                                 | Show debugging information in the log                                                                            |
| show-all                                   | N/A                                                 | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_http_empty-state)     | ignored                                             | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_http_perf-config)     |                                                     | Performance data generation configuration                                                                        |
| escape-html                                | N/A                                                 | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                       | N/A                                                 | Show help screen (this screen)                                                                                   |
| help-pb                                    | N/A                                                 | Show help screen as a protocol buffer payload                                                                    |
| show-default                               | N/A                                                 | Show default values for a given command                                                                          |
| help-short                                 | N/A                                                 | Show help screen (short format).                                                                                 |
| [top-syntax](#check_http_top-syntax)       | ${status}: ${problem_list}                          | Top level syntax.                                                                                                |
| [ok-syntax](#check_http_ok-syntax)         | %(status): %(list)                                  | ok syntax.                                                                                                       |
| [empty-syntax](#check_http_empty-syntax)   | No URL checked                                      | Empty syntax.                                                                                                    |
| [detail-syntax](#check_http_detail-syntax) | ${url} -> ${code} ${result} (${size}B in ${time}ms) | Detail level syntax.                                                                                             |
| [perf-syntax](#check_http_perf-syntax)     | ${url}                                              | Performance alias syntax.                                                                                        |
| url                                        |                                                     | Full URL to check (http://host[:port]/path or https://...). May be given multiple times.                         |
| host                                       |                                                     | Hostname (used when --url is not given).                                                                         |
| port                                       |                                                     | TCP port (defaults to 80 or 443).                                                                                |
| [path](#check_http_path)                   | /                                                   | Path component of the URL.                                                                                       |
| [protocol](#check_http_protocol)           | http                                                | Protocol to use: http or https.                                                                                  |
| ssl                                        | N/A                                                 | Force https (alias for --protocol https).                                                                        |
| [timeout](#check_http_timeout)             | 30000                                               | Timeout in milliseconds.                                                                                         |
| expected-body                              |                                                     | Substring that must appear in the body for the check to be ok.                                                   |
| [user-agent](#check_http_user-agent)       | NSClient++                                          | User-Agent header value.                                                                                         |
| header                                     |                                                     | Additional request header in 'Name: value' form (may be given multiple times).                                   |
| [tls-version](#check_http_tls-version)     | tlsv1.2+                                            | TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                     |
| [verify](#check_http_verify)               | none                                                | Certificate verify mode: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.       |
| ca                                         |                                                     | Path to a CA bundle to use when verifying the server certificate.                                                |



<h5 id="check_http_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_http_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `time > 5000`

<h5 id="check_http_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `code < 200 or code >= 400 or result != 'ok'`

<h5 id="check_http_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_http_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_http_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_http_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_http_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): %(list)`

<h5 id="check_http_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No URL checked`

<h5 id="check_http_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${url} -> ${code} ${result} (${size}B in ${time}ms)`

<h5 id="check_http_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${url}`

<h5 id="check_http_path">path:</h5>

Path component of the URL.

*Default Value:* `/`

<h5 id="check_http_protocol">protocol:</h5>

Protocol to use: http or https.

*Default Value:* `http`

<h5 id="check_http_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `30000`

<h5 id="check_http_user-agent">user-agent:</h5>

User-Agent header value.

*Default Value:* `NSClient++`

<h5 id="check_http_tls-version">tls-version:</h5>

TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_http_verify">verify:</h5>

Certificate verify mode: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.

*Default Value:* `none`


<a id="check_http_filter_keys"></a>
#### Filter keywords


| Option   | Description                                           |
|----------|-------------------------------------------------------|
| body     | Body of the response (use with substr/regex matching) |
| code     | HTTP status code                                      |
| host     | Host part of the URL                                  |
| path     | Path part of the URL                                  |
| port     | TCP port that was used                                |
| protocol | Protocol used (http or https)                         |
| result   | Textual result of the check (ok, error, ...)          |
| size     | Size of the response body in bytes                    |
| time     | Time taken by the request in milliseconds             |
| url      | Full URL that was requested                           |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_ntp_offset

Query an NTP server and check the offset between the local clock and the server.


**Jump to section:**

* [Sample Commands](#check_ntp_offset_samples)
* [Command-line Arguments](#check_ntp_offset_options)
* [Filter keywords](#check_ntp_offset_filter_keys)


<a id="check_ntp_offset_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckNet_check_ntp_offset_samples.md)_

**Default check against a single NTP server:**

```
check_ntp_offset server=pool.ntp.org
L        cli OK: OK: pool.ntp.org offset=1326ms stratum=2
L        cli  Performance data: 'pool.ntp.org_offset'=1326;60000;120000 'pool.ntp.org_stratum'=2;16;16
```

**Multiple servers via comma list (averaged across answers):**

```
check_ntp_offset "servers=0.pool.ntp.org,1.pool.ntp.org,2.pool.ntp.org" timeout=2000
L        cli OK: OK: 0.pool.ntp.org offset=1326ms stratum=2, 1.pool.ntp.org offset=1327ms stratum=1, 2.pool.ntp.org offset=1329ms stratum=2
L        cli  Performance data: '0.pool.ntp.org_offset'=1326;60000;120000 '0.pool.ntp.org_stratum'=2;16;16 '1.pool.ntp.org_offset'=1327;60000;120000 '1.pool.ntp.org_stratum'=1;16;16 '2.pool.ntp.org_offset'=1329;60000;120000 '2.pool.ntp.org_stratum'=2;16;16
```

**Custom port and timeout:**

```
check_ntp_offset server=time.example.com port=123 timeout=1500
check_ntp_offset server=time.example.com port=123 timeout=1500
L        cli OK: OK: time.example.com offset=0ms stratum=0
L        cli  Performance data: 'time.example.com_offset'=0;60000;120000 'time.example.com_stratum'=0;16;16
```

**Tighter thresholds (alert when more than 50ms / 200ms off):**

```
check_ntp_offset server=pool.ntp.org "warn=offset > 50 or stratum >= 8" "crit=offset > 200 or stratum >= 16"
L        cli CRITICAL: CRITICAL: pool.ntp.org offset=1326ms stratum=1
L        cli  Performance data: 'pool.ntp.org_offset'=1326;50;200 'pool.ntp.org_stratum'=1;8;16
```

**Use signed offset to distinguish ahead vs behind:**

```
check_ntp_offset server=pool.ntp.org "top-syntax=%(status): %(list)" "detail-syntax=%(server) signed=%(offset_signed)ms abs=%(offset)ms s=%(stratum)"
L        cli OK: OK: pool.ntp.org signed=1327ms abs=1327ms s=1
L        cli  Performance data: 'pool.ntp.org_offset'=1327;60000;120000 'pool.ntp.org_stratum'=1;16;16
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ntp_offset --argument "server=pool.ntp.org"
OK: pool.ntp.org offset=1326ms stratum=2| 'pool.ntp.org_offset'=1326;60000;120000 'pool.ntp.org_stratum'=2;16;16
```




<a id="check_ntp_offset_warn"></a>
<a id="check_ntp_offset_crit"></a>
<a id="check_ntp_offset_debug"></a>
<a id="check_ntp_offset_show-all"></a>
<a id="check_ntp_offset_escape-html"></a>
<a id="check_ntp_offset_help"></a>
<a id="check_ntp_offset_help-pb"></a>
<a id="check_ntp_offset_show-default"></a>
<a id="check_ntp_offset_help-short"></a>
<a id="check_ntp_offset_server"></a>
<a id="check_ntp_offset_servers"></a>
<a id="check_ntp_offset_options"></a>
#### Command-line Arguments


| Option                                           | Default Value                                          | Description                                                                                                      |
|--------------------------------------------------|--------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_ntp_offset_filter)               |                                                        | Filter which marks interesting items.                                                                            |
| [warning](#check_ntp_offset_warning)             | offset > 60000 or stratum >= 16                        | Filter which marks items which generates a warning state.                                                        |
| warn                                             |                                                        | Short alias for warning                                                                                          |
| [critical](#check_ntp_offset_critical)           | offset > 120000 or stratum >= 16 or result != 'ok'     | Filter which marks items which generates a critical state.                                                       |
| crit                                             |                                                        | Short alias for critical.                                                                                        |
| [ok](#check_ntp_offset_ok)                       |                                                        | Filter which marks items which generates an ok state.                                                            |
| debug                                            | N/A                                                    | Show debugging information in the log                                                                            |
| show-all                                         | N/A                                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_ntp_offset_empty-state)     | ignored                                                | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_ntp_offset_perf-config)     |                                                        | Performance data generation configuration                                                                        |
| escape-html                                      | N/A                                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                             | N/A                                                    | Show help screen (this screen)                                                                                   |
| help-pb                                          | N/A                                                    | Show help screen as a protocol buffer payload                                                                    |
| show-default                                     | N/A                                                    | Show default values for a given command                                                                          |
| help-short                                       | N/A                                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#check_ntp_offset_top-syntax)       | ${status}: ${problem_list}                             | Top level syntax.                                                                                                |
| [ok-syntax](#check_ntp_offset_ok-syntax)         | %(status): %(list)                                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_ntp_offset_empty-syntax)   | No NTP server checked                                  | Empty syntax.                                                                                                    |
| [detail-syntax](#check_ntp_offset_detail-syntax) | ${server} offset=${offset_signed}ms stratum=${stratum} | Detail level syntax.                                                                                             |
| [perf-syntax](#check_ntp_offset_perf-syntax)     | ${server}                                              | Performance alias syntax.                                                                                        |
| server                                           |                                                        | NTP server to query (may be given multiple times).                                                               |
| servers                                          |                                                        | Comma separated list of NTP servers to query.                                                                    |
| [port](#check_ntp_offset_port)                   | 123                                                    | UDP port to use (default: 123).                                                                                  |
| [timeout](#check_ntp_offset_timeout)             | 5000                                                   | Timeout in milliseconds.                                                                                         |



<h5 id="check_ntp_offset_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_ntp_offset_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `offset > 60000 or stratum >= 16`

<h5 id="check_ntp_offset_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `offset > 120000 or stratum >= 16 or result != 'ok'`

<h5 id="check_ntp_offset_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_ntp_offset_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_ntp_offset_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_ntp_offset_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_ntp_offset_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): %(list)`

<h5 id="check_ntp_offset_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No NTP server checked`

<h5 id="check_ntp_offset_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${server} offset=${offset_signed}ms stratum=${stratum}`

<h5 id="check_ntp_offset_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${server}`

<h5 id="check_ntp_offset_port">port:</h5>

UDP port to use (default: 123).

*Default Value:* `123`

<h5 id="check_ntp_offset_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `5000`


<a id="check_ntp_offset_filter_keys"></a>
#### Filter keywords


| Option        | Description                                                                      |
|---------------|----------------------------------------------------------------------------------|
| offset        | Absolute clock offset between local host and server, in milliseconds             |
| offset_signed | Signed clock offset (positive = local clock is ahead of server), in milliseconds |
| port          | UDP port the query was sent to                                                   |
| result        | Textual result of the query (ok, timeout, error, ...)                            |
| server        | NTP server that was queried                                                      |
| stratum       | Stratum reported by the server (0..16)                                           |
| time          | Round trip time of the NTP query in milliseconds                                 |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_ping

Ping another host and check the result.


**Jump to section:**

* [Sample Commands](#check_ping_samples)
* [Command-line Arguments](#check_ping_options)
* [Filter keywords](#check_ping_filter_keys)


<a id="check_ping_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckNet_check_ping_samples.md)_

**Pinging a single host:**

```
check_ping host=192.168.0.1
OK: All 1 hosts are ok|'192.168.0.1 loss'=0%;5;10 '192.168.0.1 time'=2ms;60;100
```

**Pinging multiple hosts (comma separated) with a total bucket:**

```
check_ping host=192.168.0.1 host=8.8.8.8 host=google.com total
L        cli OK: OK: All 4 hosts are ok
L        cli  Performance data: '192.168.1.1_loss'=0;5;10 '192.168.1.1_time'=2;60;100 '8.8.8.8_loss'=0;5;10 '8.8.8.8_time'=3;60;100 'google.com_loss'=0;5;10 'google.com_time'=2;60;100 'total_loss'=0;5;10 'total_time'=7;60;100
```

**Tighter thresholds with explicit count and timeout:**

```
check_ping host=8.8.8.8 count=4 timeout=300 "warn=time > 30 or loss > 0%" "crit=time > 80 or loss > 25%"
L        cli OK: OK: All 1 hosts are ok
L        cli  Performance data: '8.8.8.8_loss'=0;0;25 '8.8.8.8_time'=3;30;80
```

**Custom payload and per-host text output:**

```
check_ping host=192.168.0.1 host=192.168.0.2 payload="hello" "top-syntax=%(status): %(list)" "detail-syntax=%(host)=%(time)ms"
L        cli OK: OK: 192.168.0.1=2ms, 192.168.0.2=22ms
L        cli  Performance data: '192.168.0.1_loss'=0;5;10 '192.168.0.1_time'=2;60;100 '192.168.0.2_loss'=0;5;10 '192.168.0.2_time'=22;60;100
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ping --argument "host=192.168.56.1"
OK: All 1 hosts are ok|'192.168.56.1 loss'=0%;5;10 '192.168.56.1 time'=1ms;60;100
```




<a id="check_ping_warn"></a>
<a id="check_ping_crit"></a>
<a id="check_ping_debug"></a>
<a id="check_ping_show-all"></a>
<a id="check_ping_escape-html"></a>
<a id="check_ping_help"></a>
<a id="check_ping_help-pb"></a>
<a id="check_ping_show-default"></a>
<a id="check_ping_help-short"></a>
<a id="check_ping_host"></a>
<a id="check_ping_total"></a>
<a id="check_ping_hosts"></a>
<a id="check_ping_options"></a>
#### Command-line Arguments


| Option                                     | Default Value                                     | Description                                                                                                      |
|--------------------------------------------|---------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_ping_filter)               |                                                   | Filter which marks interesting items.                                                                            |
| [warning](#check_ping_warning)             | time > 60 or loss > 5%                            | Filter which marks items which generates a warning state.                                                        |
| warn                                       |                                                   | Short alias for warning                                                                                          |
| [critical](#check_ping_critical)           | time > 100 or loss > 10%                          | Filter which marks items which generates a critical state.                                                       |
| crit                                       |                                                   | Short alias for critical.                                                                                        |
| [ok](#check_ping_ok)                       |                                                   | Filter which marks items which generates an ok state.                                                            |
| debug                                      | N/A                                               | Show debugging information in the log                                                                            |
| show-all                                   | N/A                                               | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_ping_empty-state)     | unknown                                           | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_ping_perf-config)     |                                                   | Performance data generation configuration                                                                        |
| escape-html                                | N/A                                               | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                       | N/A                                               | Show help screen (this screen)                                                                                   |
| help-pb                                    | N/A                                               | Show help screen as a protocol buffer payload                                                                    |
| show-default                               | N/A                                               | Show default values for a given command                                                                          |
| help-short                                 | N/A                                               | Show help screen (short format).                                                                                 |
| [top-syntax](#check_ping_top-syntax)       | ${status}: ${ok_count}/${count} (${problem_list}) | Top level syntax.                                                                                                |
| [ok-syntax](#check_ping_ok-syntax)         | %(status): All %(count) hosts are ok              | ok syntax.                                                                                                       |
| [empty-syntax](#check_ping_empty-syntax)   | No hosts found                                    | Empty syntax.                                                                                                    |
| [detail-syntax](#check_ping_detail-syntax) | ${ip} Packet loss = ${loss}%, RTA = ${time}ms     | Detail level syntax.                                                                                             |
| [perf-syntax](#check_ping_perf-syntax)     | ${host}                                           | Performance alias syntax.                                                                                        |
| host                                       |                                                   | The host to check (or multiple hosts).                                                                           |
| total                                      | N/A                                               | Include the total of all matching hosts                                                                          |
| hosts                                      |                                                   | The host to check (or multiple hosts).                                                                           |
| [count](#check_ping_count)                 | 1                                                 | Number of packets to send.                                                                                       |
| [timeout](#check_ping_timeout)             | 500                                               | Timeout in milliseconds.                                                                                         |
| [payload](#check_ping_payload)             | Hello from NSClient++.                            | The payload to send in the ping request (default: 'Hello from NSClient++')                                       |



<h5 id="check_ping_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_ping_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `time > 60 or loss > 5%`

<h5 id="check_ping_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `time > 100 or loss > 10%`

<h5 id="check_ping_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_ping_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `unknown`

<h5 id="check_ping_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_ping_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${ok_count}/${count} (${problem_list})`

<h5 id="check_ping_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): All %(count) hosts are ok`

<h5 id="check_ping_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No hosts found`

<h5 id="check_ping_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${ip} Packet loss = ${loss}%, RTA = ${time}ms`

<h5 id="check_ping_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${host}`

<h5 id="check_ping_count">count:</h5>

Number of packets to send.

*Default Value:* `1`

<h5 id="check_ping_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `500`

<h5 id="check_ping_payload">payload:</h5>

The payload to send in the ping request (default: 'Hello from NSClient++')

*Default Value:* `Hello from NSClient++.`


<a id="check_ping_filter_keys"></a>
#### Filter keywords


| Option  | Description                                            |
|---------|--------------------------------------------------------|
| host    | The host name or ip address (as given on command line) |
| ip      | The ip address name                                    |
| loss    | Packet loss                                            |
| recv    | Number of packets received from the host               |
| sent    | Number of packets sent to the host                     |
| time    | Round trip time in ms                                  |
| timeout | Number of packets which timed out from the host        |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |


### check_tcp

Connect to a TCP port and optionally send/expect data to check that a service is reachable.


**Jump to section:**

* [Sample Commands](#check_tcp_samples)
* [Command-line Arguments](#check_tcp_options)
* [Filter keywords](#check_tcp_filter_keys)


<a id="check_tcp_samples"></a>
#### Sample Commands

_To edit these sample please edit [this page](https://github.com/mickem/nscp-docs/blob/master/samples/CheckNet_check_tcp_samples.md)_

**Default check against a single host/port:**

```
check_tcp host=127.0.0.1 port=8443
L        cli OK: OK: 127.0.0.1:8443 ok in 0ms
L        cli  Performance data: '127.0.0.1_8443_time'=0;1000;5000
```

**Multiple hosts via comma list:**

```
check_tcp host=www.google.com host=www.ibm.com port=443 timeout=2000
L        cli OK: OK: www.google.com:443 ok in 11ms, www.ibm.com:443 ok in 9ms
L        cli  Performance data: 'www.google.com_443_time'=11;1000;5000 'www.ibm.com_443_time'=9;1000;5000
```

**Send a payload and require an expected substring in the response:**

```
check_tcp host=smtp.gmail.com port=25 send="EHLO nsclient.org" expect="250"
L        cli CRITICAL: CRITICAL: smtp.gmail.com:25 no_match in 25ms
L        cli  Performance data: 'smtp.gmail.com_25_time'=25;1000;5000
```

**Tighter thresholds:**

```
check_tcp host=www.google.com port=443 "warn=time > 100" "crit=time > 500 or connected = 0"
L        cli OK: OK: www.google.com:443 ok in 11ms
L        cli  Performance data: 'www.google.com_443_connected'=1;0;0 'www.google.com_443_time'=11;100;500
```

**Show every host explicitly:**

```
check_tcp host=a.example.com host=b.example.com port=80 "top-syntax=%(status): %(list)" "detail-syntax=%(host):%(port)=%(result) in %(time)ms"
OK: a.example.com:80=ok in 14ms, b.example.com:80=ok in 19ms
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_tcp --argument "host=192.168.56.1" --argument "port=22"
OK: All 1 hosts are ok|'192.168.56.1_22 time'=2ms;1000;5000
```




<a id="check_tcp_warn"></a>
<a id="check_tcp_crit"></a>
<a id="check_tcp_debug"></a>
<a id="check_tcp_show-all"></a>
<a id="check_tcp_escape-html"></a>
<a id="check_tcp_help"></a>
<a id="check_tcp_help-pb"></a>
<a id="check_tcp_show-default"></a>
<a id="check_tcp_help-short"></a>
<a id="check_tcp_host"></a>
<a id="check_tcp_hosts"></a>
<a id="check_tcp_port"></a>
<a id="check_tcp_send"></a>
<a id="check_tcp_expect"></a>
<a id="check_tcp_options"></a>
#### Command-line Arguments


| Option                                    | Default Value                          | Description                                                                                                      |
|-------------------------------------------|----------------------------------------|------------------------------------------------------------------------------------------------------------------|
| [filter](#check_tcp_filter)               |                                        | Filter which marks interesting items.                                                                            |
| [warning](#check_tcp_warning)             | time > 1000                            | Filter which marks items which generates a warning state.                                                        |
| warn                                      |                                        | Short alias for warning                                                                                          |
| [critical](#check_tcp_critical)           | time > 5000 or result != 'ok'          | Filter which marks items which generates a critical state.                                                       |
| crit                                      |                                        | Short alias for critical.                                                                                        |
| [ok](#check_tcp_ok)                       |                                        | Filter which marks items which generates an ok state.                                                            |
| debug                                     | N/A                                    | Show debugging information in the log                                                                            |
| show-all                                  | N/A                                    | Show details for all matches regardless of status (normally details are only showed for warnings and criticals). |
| [empty-state](#check_tcp_empty-state)     | ignored                                | Return status to use when nothing matched filter.                                                                |
| [perf-config](#check_tcp_perf-config)     |                                        | Performance data generation configuration                                                                        |
| escape-html                               | N/A                                    | Escape any < and > characters to prevent HTML encoding                                                           |
| help                                      | N/A                                    | Show help screen (this screen)                                                                                   |
| help-pb                                   | N/A                                    | Show help screen as a protocol buffer payload                                                                    |
| show-default                              | N/A                                    | Show default values for a given command                                                                          |
| help-short                                | N/A                                    | Show help screen (short format).                                                                                 |
| [top-syntax](#check_tcp_top-syntax)       | ${status}: ${problem_list}             | Top level syntax.                                                                                                |
| [ok-syntax](#check_tcp_ok-syntax)         | %(status): %(list)                     | ok syntax.                                                                                                       |
| [empty-syntax](#check_tcp_empty-syntax)   | No hosts checked                       | Empty syntax.                                                                                                    |
| [detail-syntax](#check_tcp_detail-syntax) | ${host}:${port} ${result} in ${time}ms | Detail level syntax.                                                                                             |
| [perf-syntax](#check_tcp_perf-syntax)     | ${host}_${port}                        | Performance alias syntax.                                                                                        |
| host                                      |                                        | Host(s) to connect to (may be given multiple times).                                                             |
| hosts                                     |                                        | Comma separated list of hosts to connect to.                                                                     |
| port                                      |                                        | TCP port to connect to.                                                                                          |
| [timeout](#check_tcp_timeout)             | 5000                                   | Connection / read timeout in milliseconds.                                                                       |
| send                                      |                                        | Optional payload to send after the connection is established.                                                    |
| expect                                    |                                        | Optional substring expected in the response (requires --send or returns whatever the peer sent first).           |



<h5 id="check_tcp_filter">filter:</h5>

Filter which marks interesting items.
Interesting items are items which will be included in the check.
They do not denote warning or critical state instead it defines which items are relevant and you can remove unwanted items.


<h5 id="check_tcp_warning">warning:</h5>

Filter which marks items which generates a warning state.
If anything matches this filter the return status will be escalated to warning.


*Default Value:* `time > 1000`

<h5 id="check_tcp_critical">critical:</h5>

Filter which marks items which generates a critical state.
If anything matches this filter the return status will be escalated to critical.


*Default Value:* `time > 5000 or result != 'ok'`

<h5 id="check_tcp_ok">ok:</h5>

Filter which marks items which generates an ok state.
If anything matches this any previous state for this item will be reset to ok.


<h5 id="check_tcp_empty-state">empty-state:</h5>

Return status to use when nothing matched filter.
If no filter is specified this will never happen unless the file is empty.

*Default Value:* `ignored`

<h5 id="check_tcp_perf-config">perf-config:</h5>

Performance data generation configuration
TODO: obj ( key: value; key: value) obj (key:valuer;key:value)


<h5 id="check_tcp_top-syntax">top-syntax:</h5>

Top level syntax.
Used to format the message to return can include text as well as special keywords which will include information from the checks.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${status}: ${problem_list}`

<h5 id="check_tcp_ok-syntax">ok-syntax:</h5>

ok syntax.
DEPRECATED! This is the syntax for when an ok result is returned.
This value will not be used if your syntax contains %(list) or %(count).

*Default Value:* `%(status): %(list)`

<h5 id="check_tcp_empty-syntax">empty-syntax:</h5>

Empty syntax.
DEPRECATED! This is the syntax for when nothing matches the filter.

*Default Value:* `No hosts checked`

<h5 id="check_tcp_detail-syntax">detail-syntax:</h5>

Detail level syntax.
Used to format each resulting item in the message.
%(list) will be replaced with all the items formated by this syntax string in the top-syntax.
To add a keyword to the message you can use two syntaxes either ${keyword} or %(keyword) (there is no difference between them apart from ${} can be difficult to escape on linux).

*Default Value:* `${host}:${port} ${result} in ${time}ms`

<h5 id="check_tcp_perf-syntax">perf-syntax:</h5>

Performance alias syntax.
This is the syntax for the base names of the performance data.

*Default Value:* `${host}_${port}`

<h5 id="check_tcp_timeout">timeout:</h5>

Connection / read timeout in milliseconds.

*Default Value:* `5000`


<a id="check_tcp_filter_keys"></a>
#### Filter keywords


| Option    | Description                                                       |
|-----------|-------------------------------------------------------------------|
| connected | 1 when the connection succeeded, 0 otherwise                      |
| host      | Host the check connected to                                       |
| port      | TCP port the check connected to                                   |
| result    | Textual result of the check (ok, refused, timeout, no_match, ...) |
| time      | Connection time in milliseconds                                   |

**Common options for all checks:**

| Option        | Description                                                                    |
|---------------|--------------------------------------------------------------------------------|
| count         | Number of items matching the filter.                                           |
| crit_count    | Number of items matched the critical criteria.                                 |
| crit_list     | A list of all items which matched the critical criteria.                       |
| detail_list   | A special list with critical, then warning and finally ok.                     |
| list          | A list of all items which matched the filter.                                  |
| ok_count      | Number of items matched the ok criteria.                                       |
| ok_list       | A list of all items which matched the ok criteria.                             |
| problem_count | Number of items matched either warning or critical criteria.                   |
| problem_list  | A list of all items which matched either the critical or the warning criteria. |
| status        | The returned status (OK/WARN/CRIT/UNKNOWN).                                    |
| total         | Total number of items.                                                         |
| warn_count    | Number of items matched the warning criteria.                                  |
| warn_list     | A list of all items which matched the warning criteria.                        |




