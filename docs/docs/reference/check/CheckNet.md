# CheckNet

Network related checks such as check_ping, check_tcp, check_dns, check_http, check_connections and check_ntp_offset.

#### Choosing the IP version (`address-family`)

Every network check in this module accepts an `address-family` argument that
pins which IP version it uses:

| Value  | Aliases              | Meaning                                                     |
|--------|----------------------|-------------------------------------------------------------|
| `any`  | `both`, `unspec`, `` | **Default.** Let the resolver choose (the historic behaviour). |
| `ipv4` | `4`, `v4`, `inet`    | Resolve and connect over IPv4 only.                          |
| `ipv6` | `6`, `v6`, `inet6`   | Resolve and connect over IPv6 only.                          |

Values are case-insensitive. Anything else is rejected with
`Invalid address-family: <value>` rather than silently falling back to `any` —
a typo must not quietly stop testing the family you asked for.

Supported by `check_ping`, `check_tcp`, `check_ssh`, `check_http`, `check_dns`
and `check_ntp_offset`.

On a dual-stack host the default leaves the choice to the resolver, so a check
that passes tells you *one* of the two stacks works, not which. Pinning the
family is what turns that into an assertion:

```
check_ssh host=srv.example.com address-family=ipv6
OK: localhost:22 ok in 1ms
```

```
check_http url=http://srv.example.com/health address-family=ipv6
OK: http://srv.example.com/health -> 200 ok (2B in 3ms)
```

Run the same check twice — once per family — to monitor both paths
independently. A host with no address in the requested family fails rather than
falling back:

```
check_tcp host=v6-only.example.com port=443 address-family=ipv4
CRITICAL: v6-only.example.com:443 resolve_failed in 0ms
```

The failure is `resolve_failed`, not `refused`: with the family pinned there is
no address to connect to, so the check never gets as far as a connection
attempt. "The name exists but has nothing in this family" is the answer being
asked for here, not an internal error.

For `check_dns` the flag selects how the **DNS server** is reached, which is
independent of the record `type=` being queried — you can ask an IPv6-reachable
server for an A record. When no `server=` is given and the type is A/AAAA (the
system-resolver path), it additionally restricts the answer to that family.

```
check_dns host=example.com server=2001:db8::53 address-family=ipv6
OK: example.com -> 10.1.2.3 (1) in 0ms [ok]
```

`check_dns` and `check_ntp_offset` were previously IPv4-only regardless of the
server address; they now open the socket in whichever family the server
resolves to, so an IPv6 DNS or NTP server is reachable at all.

#### IPv6 literals in URLs

`check_http` accepts a bracketed IPv6 literal, as required by RFC 3986:

```
check_http url=http://[::1]:8080/health
OK: http://[::1]:8080/health -> 200 ok (2B in 1ms)
```

The brackets are part of the URL syntax (an unbracketed `::1` is ambiguous with
the `host:port` separator) and are kept in the `Host:` header, while the `host`
keyword reports the bare address.

#### A note on `check_ping`

`check_ping` uses ICMP echo, and ICMPv4 and ICMPv6 are separate protocols
rather than two modes of one: with `address-family=ipv6` the check sends an
ICMPv6 echo request (type 128) on an ICMPv6 socket. Two consequences:

* The `ttl` field is not populated over IPv6. The IPv6 hop limit is only
  available through ancillary data the check does not request, so it reports
  `-1` there instead of an invented value. `-1` is the "not known" marker
  generally — an unanswered host reports it too, and the `total` row ignores
  those rather than letting them win its minimum.
* Raw ICMP sockets need privileges (root / `CAP_NET_RAW` on Linux,
  Administrator on Windows) for both families, exactly as before.


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

| Command                                                 | Description                                                                                 |
|---------------------------------------------------------|---------------------------------------------------------------------------------------------|
| [check_apache_status](#check_apache_status)             | Check an Apache httpd server via its mod_status page (server-status?auto).                  |
| [check_connections](#check_connections)                 | Count active TCP/UDP connections and report counts per protocol and TCP state.              |
| [check_dns](#check_dns)                                 | Resolve a host name and check the response time and resulting addresses.                    |
| [check_http](#check_http)                               | Send an HTTP/HTTPS request and check the response status, time, size and body.              |
| [check_nginx_status](#check_nginx_status)               | Check an NGINX server via its stub_status page.                                             |
| [check_nsclient_web_online](#check_nsclient_web_online) | Query the REST API of a remote NSClient++ agent (reachability or a remote check).           |
| [check_ntp_offset](#check_ntp_offset)                   | Query an NTP server and check the offset between the local clock and the server.            |
| [check_phpfpm_status](#check_phpfpm_status)             | Check a PHP-FPM pool via its status page.                                                   |
| [check_ping](#check_ping)                               | Ping another host and check the result.                                                     |
| [check_ssh](#check_ssh)                                 | Connect to an SSH port and verify the server presents a valid SSH banner.                   |
| [check_tcp](#check_tcp)                                 | Connect to a TCP port and optionally send/expect data to check that a service is reachable. |
| [check_tomcat_status](#check_tomcat_status)             | Check an Apache Tomcat server via the manager status page (XML).                            |

### check_apache_status

Check an Apache httpd server via its mod_status page (server-status?auto).

#### About `check_apache_status`

`check_apache_status` fetches Apache httpd's
[mod_status](https://httpd.apache.org/docs/current/mod/mod_status.html) page in
its machine-readable form (`/server-status?auto`) and exposes the reported
values as filter keywords. The `?auto` parameter is appended automatically when
the URL does not already carry it, so `url=http://host/server-status` is
enough. `ExtendedStatus On` (the default since Apache 2.3.6) is required for
the request/byte counters; `BusyWorkers`/`IdleWorkers` are always present.

The check emits a single record. By default it goes **critical** when the
endpoint cannot be fetched or does not look like a mod_status page
(`result != 'ok'`); worker/traffic thresholds are opt-in.

Connection options match `check_http` where applicable: `timeout`, `username`
/ `password` (Basic auth), and for https `tls-version`, `verify` and `ca`.

Note that `requests_per_sec`/`bytes_per_sec` are lifetime averages computed by
Apache itself, not a current rate; for spiky load, alert on `busy_workers`
instead.

**Jump to section:**

* [Sample Commands](#check_apache_status_samples)
* [Command-line Arguments](#check_apache_status_options)
* [Filter keywords](#check_apache_status_filter_keys)


<a id="check_apache_status_samples"></a>
#### Sample Commands

**Check a local Apache via mod_status (the `?auto` parameter is appended automatically):**

```
check_apache_status url=http://127.0.0.1/server-status
OK: ok: 3 busy and 47 idle workers, 1.14985 req/s, uptime 7254s|'127.0.0.1_busy_workers'=3;0;0 '127.0.0.1_idle_workers'=47;0;0 '127.0.0.1_requests_per_sec'=1.14985;0;0
```

**Alert when the worker pool is running out of spare workers:**

```
check_apache_status url=http://127.0.0.1/server-status "warning=idle_workers < 10" "critical=idle_workers < 3"
OK: ok: 3 busy and 47 idle workers, 1.14985 req/s, uptime 7254s|'127.0.0.1_idle_workers'=47;10;3 '127.0.0.1_busy_workers'=3;0;0 '127.0.0.1_requests_per_sec'=1.14985;0;0
```

**Alert on load (busy workers) instead:**

```
check_apache_status url=http://127.0.0.1/server-status "warning=busy_workers > 2"
WARNING: ok: 3 busy and 47 idle workers, 1.14985 req/s, uptime 7254s|'127.0.0.1_busy_workers'=3;2;0 '127.0.0.1_idle_workers'=47;0;0 '127.0.0.1_requests_per_sec'=1.14985;0;0
```

**A server that is down (or serving the wrong page) is CRITICAL by default:**

```
check_apache_status url=http://127.0.0.1/nope
CRITICAL: http_404: 0 busy and 0 idle workers, 0 req/s, uptime 0s|'127.0.0.1_busy_workers'=0;0;0 '127.0.0.1_idle_workers'=0;0;0 '127.0.0.1_requests_per_sec'=0;0;0
```



<a id="check_apache_status_options"></a>
#### Command-line Arguments

<a id="check_apache_status_username"></a>
<a id="check_apache_status_password"></a>

| Option                                          | Default Value                  | Description                                                                                                          |
|-------------------------------------------------|--------------------------------|----------------------------------------------------------------------------------------------------------------------|
| [url](#check_apache_status_url)                 | http://127.0.0.1/server-status | URL of the status endpoint (http://host[:port]/path or https://...).                                                 |
| [timeout](#check_apache_status_timeout)         | 30                             | Connection/read timeout in seconds.                                                                                  |
| username                                        |                                | Username for HTTP Basic authentication.                                                                              |
| password                                        |                                | Password for HTTP Basic authentication.                                                                              |
| [tls-version](#check_apache_status_tls-version) | tlsv1.2+                       | TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                         |
| [verify](#check_apache_status_verify)           | peer                           | Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate. |
| [ca](#check_apache_status_ca)                   | ${ca-path}                     | Path to a CA bundle used to verify the server certificate.                                                           |



<h5 id="check_apache_status_url">url:</h5>

URL of the status endpoint (http://host[:port]/path or https://...).

*Default Value:* `http://127.0.0.1/server-status`

<h5 id="check_apache_status_timeout">timeout:</h5>

Connection/read timeout in seconds.

*Default Value:* `30`

<h5 id="check_apache_status_tls-version">tls-version:</h5>

TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_apache_status_verify">verify:</h5>

Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.

*Default Value:* `peer`

<h5 id="check_apache_status_ca">ca:</h5>

Path to a CA bundle used to verify the server certificate.

*Default Value:* `${ca-path}`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                                                                                                  |
|---------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------|
| <a id="check_apache_status_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                |
| <a id="check_apache_status_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                                |
| <a id="check_apache_status_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                |
| <a id="check_apache_status_critical"></a>[critical](../common-options.md#critical)                                  | result != 'ok'                                                                                                 |
| <a id="check_apache_status_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                |
| <a id="check_apache_status_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                |
| <a id="check_apache_status_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                          |
| <a id="check_apache_status_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                          |
| <a id="check_apache_status_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                        |
| <a id="check_apache_status_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                |
| <a id="check_apache_status_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                          |
| <a id="check_apache_status_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                              |
| <a id="check_apache_status_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                             |
| <a id="check_apache_status_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                |
| <a id="check_apache_status_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No status page fetched                                                                                         |
| <a id="check_apache_status_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${result}: ${busy_workers} busy and ${idle_workers} idle workers, ${requests_per_sec} req/s, uptime ${uptime}s |
| <a id="check_apache_status_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${host}                                                                                                        |
| <a id="check_apache_status_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                |
| <a id="check_apache_status_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                |
| <a id="check_apache_status_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                             |
| <a id="check_apache_status_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_apache_status_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                           |
|------------------|-----------------------------------------------------------------------|
| busy_workers     | Workers currently serving requests                                    |
| bytes_per_sec    | Average bytes per second since start                                  |
| code             | HTTP status code of the response                                      |
| host             | Host part of the URL                                                  |
| idle_workers     | Idle (spare) workers                                                  |
| port             | TCP port that was used                                                |
| requests_per_sec | Average requests per second since start                               |
| result           | Result of the check: ok, parse_error, http_<code> or error: <message> |
| scoreboard       | The raw mod_status scoreboard string                                  |
| total_accesses   | Requests served since start                                           |
| total_kbytes     | kBytes served since start                                             |
| total_workers    | Busy plus idle workers (the currently running worker pool)            |
| uptime           | Server uptime in seconds                                              |
| url              | Full URL that was requested                                           |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_connections

Count active TCP/UDP connections and report counts per protocol and TCP state.

**Jump to section:**

* [Sample Commands](#check_connections_samples)
* [Command-line Arguments](#check_connections_options)
* [Filter keywords](#check_connections_filter_keys)


<a id="check_connections_samples"></a>
#### Sample Commands

**Default check (uses the `total` bucket):**

```
check_connections
L        cli OK: OK: total/all: 226
L        cli  Performance data: 'total_all_close_wait'=0;0;0 'total_all_closing'=0;0;0 'total_all_established'=90;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=0;0;0 'total_all_time_wait'=6;0;0 'total_all_total'=226;0;0 'total_all_udp'=61;0;0
```

**Per-protocol breakdown (disable the default total filter):**

```
check_connections "filter=state = 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(protocol)/%(family)=%(connections)"
L        cli OK: OK: tcp/ipv4=157, tcp6/ipv6=15, udp/ipv4=40, udp6/ipv6=21, total/any=233
L        cli  Performance data: 'tcp_all_close_wait'=0;0;0 'tcp_all_closing'=0;0;0 'tcp_all_established'=0;0;0 'tcp_all_fin_wait'=0;0;0 'tcp_all_last_ack'=0;0;0 'tcp_all_listen'=0;0;0 'tcp_all_syn_recv'=0;0;0 'tcp_all_syn_sent'=0;0;0 'tcp_all_time_wait'=0;0;0 'tcp_all_total'=0;0;0 'tcp_all_udp'=0;0;0 'tcp6_all_close_wait'=0;0;0 'tcp6_all_closing'=0;0;0 'tcp6_all_established'=0;0;0 'tcp6_all_fin_wait'=0;0;0 'tcp6_all_last_ack'=0;0;0 'tcp6_all_listen'=0;0;0 'tcp6_all_syn_recv'=0;0;0 'tcp6_all_syn_sent'=0;0;0 'tcp6_all_time_wait'=0;0;0 'tcp6_all_total'=0;0;0 'tcp6_all_udp'=0;0;0 'udp_all_close_wait'=0;0;0 'udp_all_closing'=0;0;0 'udp_all_established'=0;0;0 'udp_all_fin_wait'=0;0;0 'udp_all_last_ack'=0;0;0 'udp_all_listen'=0;0;0 'udp_all_syn_recv'=0;0;0 'udp_all_syn_sent'=0;0;0 'udp_all_time_wait'=0;0;0 'udp_all_total'=0;0;0 'udp_all_udp'=0;0;0 'udp6_all_close_wait'=0;0;0 'udp6_all_closing'=0;0;0 'udp6_all_established'=0;0;0 'udp6_all_fin_wait'=0;0;0 'udp6_all_last_ack'=0;0;0 'udp6_all_listen'=0;0;0 'udp6_all_syn_recv'=0;0;0 'udp6_all_syn_sent'=0;0;0 'udp6_all_time_wait'=0;0;0 'udp6_all_total'=0;0;0 'udp6_all_udp'=0;0;0 'total_all_close_wait'=1;0;0 'total_all_closing'=0;0;0 'total_all_established'=93;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=0;0;0 'total_all_time_wait'=9;0;0 'total_all_total'=233;0;0 'total_all_udp'=61;0;0
```

**Show only TCP states:**

```
check_connections "filter=protocol = 'tcp' and state != 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(state)=%(connections)"
check_connections "filter=protocol = 'tcp' and state != 'all'" "top-syntax=%(status): %(list)" "detail-syntax=%(state)=%(connections)"
L        cli OK: OK: ESTABLISHED=92, LISTEN=69, TIME_WAIT=9
```

**Warn/critical based on total connections:**

```
check_connections "warn=total_connections > 500" "crit=total_connections > 1000"
L        cli OK: OK: total/all: 231
```

**Warn when many sockets are stuck in TIME_WAIT:**

```
check_connections "filter=protocol = 'tcp' and state = 'TIME_WAIT'" "warn=connections > 200" "crit=connections > 1000"
L        cli OK: OK: tcp/TIME_WAIT: 14
```

**Alert on growing CLOSE_WAIT (often indicates leaks):**

```
check_connections "filter=state = 'CLOSE_WAIT'" "warn=connections > 50" "crit=connections > 200"
L        cli OK: No connection data
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_connections
OK: total/all: 231|'total_all_close_wait'=0;0;0 'total_all_closing'=0;0;0 'total_all_established'=85;0;0 'total_all_fin_wait'=0;0;0 'total_all_last_ack'=0;0;0 'total_all_listen'=69;0;0 'total_all_syn_recv'=0;0;0 'total_all_syn_sent'=1;0;0 'total_all_time_wait'=16;0;0 'total_all_total'=231;0;0 'total_all_udp'=60;0;0
```




<a id="check_connections_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                            | Default Value                        |
|-------------------------------------------------------------------------------------------------------------------|--------------------------------------|
| <a id="check_connections_filter"></a>[filter](../common-options.md#filter)                                        | protocol = 'total'                   |
| <a id="check_connections_warning"></a>[warning](../common-options.md#warning)                                     | total_connections > 1000             |
| <a id="check_connections_warn"></a>[warn](../common-options.md#warn)                                              |                                      |
| <a id="check_connections_critical"></a>[critical](../common-options.md#critical)                                  | total_connections > 2000             |
| <a id="check_connections_crit"></a>[crit](../common-options.md#crit)                                              |                                      |
| <a id="check_connections_ok"></a>[ok](../common-options.md#ok)                                                    |                                      |
| <a id="check_connections_debug"></a>[debug](../common-options.md#debug)                                           | false                                |
| <a id="check_connections_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                |
| <a id="check_connections_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                              |
| <a id="check_connections_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                      |
| <a id="check_connections_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                |
| <a id="check_connections_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                    |
| <a id="check_connections_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                   |
| <a id="check_connections_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(list)                   |
| <a id="check_connections_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No connection data                   |
| <a id="check_connections_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${protocol}/${state}: ${connections} |
| <a id="check_connections_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${protocol}_${state}                 |
| <a id="check_connections_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                      |
| <a id="check_connections_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                      |
| <a id="check_connections_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                   |
| <a id="check_connections_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_connections_filter_keys"></a>
#### Filter keywords

| Option            | Description                                                   |
|-------------------|---------------------------------------------------------------|
| close_wait        | Number of TCP connections in CLOSE_WAIT state (total bucket)  |
| closing           | Number of TCP connections in CLOSING state (total bucket)     |
| connections       | Number of connections matching this bucket                    |
| established       | Number of TCP connections in ESTABLISHED state (total bucket) |
| family            | Address family (ipv4, ipv6, any)                              |
| fin_wait          | Number of TCP connections in FIN_WAIT* state (total bucket)   |
| last_ack          | Number of TCP connections in LAST_ACK state (total bucket)    |
| listen            | Number of TCP sockets in LISTEN state (total bucket)          |
| protocol          | Protocol of this bucket (tcp, tcp6, udp, udp6, total)         |
| state             | TCP state name (ESTABLISHED, LISTEN, ...) or 'all'            |
| syn_recv          | Number of TCP connections in SYN_RECV state (total bucket)    |
| syn_sent          | Number of TCP connections in SYN_SENT state (total bucket)    |
| time_wait         | Number of TCP connections in TIME_WAIT state (total bucket)   |
| total_connections | Total number of connections (only on the 'total' bucket)      |
| udp               | Number of UDP sockets (total bucket)                          |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_dns

Resolve a host name and check the response time and resulting addresses.

**Jump to section:**

* [Sample Commands](#check_dns_samples)
* [Command-line Arguments](#check_dns_options)
* [Filter keywords](#check_dns_filter_keys)


<a id="check_dns_samples"></a>
#### Sample Commands

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

**Query a specific record type (`type=A|AAAA|MX|TXT|CNAME|NS|SOA|PTR`):**

```
check_dns host=google.com type=MX server=8.8.8.8
OK: google.com -> 10 smtp.google.com (1) in 9ms [ok]|'google.com_time'=9;1000;0
```

**Query a specific DNS server (A/AAAA without `server=` use the system resolver; any other type or an explicit `server=` uses a direct DNS-over-UDP query):**

```
check_dns host=nsclient.org type=TXT server=1.1.1.1
OK: nsclient.org -> v=spf1 include:_spf.google.com ~all (1) in 12ms [ok]
```

**Non-recursive query against an authoritative server on a custom port:**

```
check_dns host=example.com type=A server=192.168.10.53 port=5353 norecursion=true
OK: example.com -> 93.184.216.34 (1) in 3ms [ok]
```




<a id="check_dns_options"></a>
#### Command-line Arguments

<a id="check_dns_host"></a>
<a id="check_dns_lookup"></a>
<a id="check_dns_server"></a>
<a id="check_dns_expected-address"></a>
<a id="check_dns_expected"></a>
<a id="check_dns_address-family"></a>

| Option                                | Default Value | Description                                                                                                                                                                                                                                  |
|---------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                                  |               | Hostname to look up.                                                                                                                                                                                                                         |
| lookup                                |               | Alias for --host.                                                                                                                                                                                                                            |
| [type](#check_dns_type)               | A             | DNS record type to query: A, AAAA, MX, TXT, CNAME, NS, SOA, PTR.                                                                                                                                                                             |
| server                                |               | DNS server to query (default: the system resolver for A/AAAA, /etc/resolv.conf otherwise).                                                                                                                                                   |
| [port](#check_dns_port)               | 53            | UDP port of the DNS server.                                                                                                                                                                                                                  |
| [norecursion](#check_dns_norecursion) | false         | Do not request recursion (RD=0).                                                                                                                                                                                                             |
| [timeout](#check_dns_timeout)         | 5000          | Timeout in milliseconds.                                                                                                                                                                                                                     |
| expected-address                      |               | Record that must be present in the answer (may be given multiple times).                                                                                                                                                                     |
| expected                              |               | Comma separated list of records that must all be present in the answer.                                                                                                                                                                      |
| address-family                        |               | IP version to use: any (default), ipv4 or ipv6. Selects which address of the DNS server to query; with the system resolver (A/AAAA and no server=) it also restricts the answer to that family. Accepts 4/v4/inet and 6/v6/inet6 as aliases. |



<h5 id="check_dns_type">type:</h5>

DNS record type to query: A, AAAA, MX, TXT, CNAME, NS, SOA, PTR.

*Default Value:* `A`

<h5 id="check_dns_port">port:</h5>

UDP port of the DNS server.

*Default Value:* `53`

<h5 id="check_dns_norecursion">norecursion:</h5>

Do not request recursion (RD=0).

*Default Value:* `false`

<h5 id="check_dns_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `5000`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                    | Default Value                                                 |
|-----------------------------------------------------------------------------------------------------------|---------------------------------------------------------------|
| <a id="check_dns_filter"></a>[filter](../common-options.md#filter)                                        |                                                               |
| <a id="check_dns_warning"></a>[warning](../common-options.md#warning)                                     | time > 1000                                                   |
| <a id="check_dns_warn"></a>[warn](../common-options.md#warn)                                              |                                                               |
| <a id="check_dns_critical"></a>[critical](../common-options.md#critical)                                  | result != 'ok'                                                |
| <a id="check_dns_crit"></a>[crit](../common-options.md#crit)                                              |                                                               |
| <a id="check_dns_ok"></a>[ok](../common-options.md#ok)                                                    |                                                               |
| <a id="check_dns_debug"></a>[debug](../common-options.md#debug)                                           | false                                                         |
| <a id="check_dns_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                         |
| <a id="check_dns_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                       |
| <a id="check_dns_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                               |
| <a id="check_dns_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                         |
| <a id="check_dns_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                             |
| <a id="check_dns_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                                    |
| <a id="check_dns_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(list)                                            |
| <a id="check_dns_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No DNS lookup performed                                       |
| <a id="check_dns_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${host} -> ${addresses} (${records}) in ${time}ms [${result}] |
| <a id="check_dns_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${host}                                                       |
| <a id="check_dns_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                               |
| <a id="check_dns_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                               |
| <a id="check_dns_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                            |
| <a id="check_dns_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_dns_filter_keys"></a>
#### Filter keywords

| Option    | Description                                                        |
|-----------|--------------------------------------------------------------------|
| addresses | Comma separated list of resolved records                           |
| host      | Hostname that was looked up                                        |
| records   | Number of records returned by the resolver                         |
| result    | Textual result of the lookup (ok, not_found, mismatch, error, ...) |
| server    | DNS server used (empty for the system resolver)                    |
| time      | Time taken by the lookup in milliseconds                           |
| type      | Record type that was queried (A, AAAA, MX, TXT, ...)               |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_http

Send an HTTP/HTTPS request and check the response status, time, size and body.

**Jump to section:**

* [Sample Commands](#check_http_samples)
* [Command-line Arguments](#check_http_options)
* [Filter keywords](#check_http_filter_keys)


<a id="check_http_samples"></a>
#### Sample Commands

**Default check against a single URL (success):**

```
check_http url=https://nsclient.org/
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 197ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=197ms;5000;0
```

**HTTPS with explicit port and path components (page not found):**

```
check_http protocol=https host=nsclient.org port=443 path=/no-such-page
L        cli CRITICAL: https://nsclient.org:443/no-such-page -> 404 http_404 (55109B in 165ms)
L        cli  Performance data: 'https://nsclient.org:443/no-such-page_code'=404;0;200 'https://nsclient.org:443/no-such-page_size'=55109B;0;0 'https://nsclient.org:443/no-such-page'=165ms;5000;0
```

**Connection / DNS failure (host does not resolve):**

```
check_http url=https://nope.invalid/
L        cli CRITICAL: https://nope.invalid/ -> 0 error: Failed to resolve nope.invalid:443: Ingen sådan värd är känd (0B in 10ms)
L        cli  Performance data: 'https://nope.invalid/_code'=0;0;200 'https://nope.invalid/_size'=0B;0;0 'https://nope.invalid/'=10ms;5000;0
```

**Multiple URLs in one call:**

```
check_http url=https://nsclient.org/ url=https://nsclient.org/nsclient/
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 59ms), https://nsclient.org/nsclient/ -> 200 ok (60820B in 179ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=59ms;5000;0 'https://nsclient.org/nsclient/_code'=200;0;200 'https://nsclient.org/nsclient/_size'=60820B;0;0 'https://nsclient.org/nsclient/'=179ms;5000;0
```

**Require an expected substring in the response body:**

```
check_http url=https://nsclient.org/ expected-body="NSClient"
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 47ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=47ms;5000;0
```

If the substring is absent the check goes CRITICAL with `result=no_match`:

```
check_http url=https://nsclient.org/ expected-body="this-string-is-not-present"
L        cli CRITICAL: https://nsclient.org/ -> 200 no_match (68937B in 52ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=52ms;5000;0
```

**Custom user-agent and extra headers:**

```
check_http url=https://nsclient.org/ user-agent="nscp-monitor/1" header="X-Trace: 1"
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 50ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=50ms;5000;0
```

**Tighter latency thresholds and code rules:**

```
check_http url=https://nsclient.org/ timeout=10000 "warn=time > 500 or code >= 400" "crit=time > 2000 or code >= 500 or result != 'ok'"
L        cli OK: https://nsclient.org/ -> 200 ok (68937B in 61ms)
L        cli  Performance data: 'https://nsclient.org/_code'=200;400;500 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=61ms;500;2000
```

**Custom output text (drop result if you don't want it):**

```
check_http url=https://nsclient.org/ "top-syntax=%(status): %(list)" "detail-syntax=%(url) -> %(code) in %(time)ms"
L        cli OK: https://nsclient.org/ -> 200 in 46ms
L        cli  Performance data: 'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=46ms;5000;0
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_http --argument "url=https://nsclient.org/"
OK: https://nsclient.org/ -> 200 ok (68937B in 197ms)|'https://nsclient.org/_code'=200;0;200 'https://nsclient.org/_size'=68937B;0;0 'https://nsclient.org/'=197ms;5000;0
```

**Use a specific HTTP method (`HEAD`, `POST`, `PUT`, …):**

```
check_http url=https://www.google.com method=HEAD
OK: https://www.google.com -> 200 ok (0B in 58ms)|'https://www.google.com_code'=200;0;200 'https://www.google.com_size'=0B;0;0 'https://www.google.com'=58ms;5000;0
```

**POST a body (`post-data` implies POST unless `method=` is given):**

```
check_http url=https://httpbin.org/post post-data="name=value" content-type="application/x-www-form-urlencoded" expected-body="name"
OK: https://httpbin.org/post -> 200 ok (429B in 380ms)
```

**HTTP Basic authentication:**

```
check_http url=https://example.com/private username=admin password=secret
OK: https://example.com/private -> 200 ok (1200B in 88ms)
```

**Follow redirects (default reports the 3xx as-is; `onredirect=follow` chases the Location):**

```
check_http url=http://github.com onredirect=follow "detail-syntax=code=${code}"
OK: code=200
```

**Accept a set of status codes with the `code` keyword, and match the body with a regex:**

```
check_http url=https://example.com "warn=code not in (200,301,302)" "crit=code >= 500 or body not regexp 'Welcome'"
OK: https://example.com -> 200 ok (1256B in 74ms)
```

**Alert when the TLS certificate is about to expire (`ssl_expiry_days`):**

```
check_http url=https://www.google.com "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 7" "detail-syntax=cert expires in ${ssl_expiry_days} days"
OK: cert expires in 58 days
```



<a id="check_http_options"></a>
#### Command-line Arguments

<a id="check_http_url"></a>
<a id="check_http_host"></a>
<a id="check_http_port"></a>
<a id="check_http_post-data"></a>
<a id="check_http_username"></a>
<a id="check_http_password"></a>
<a id="check_http_expected-body"></a>
<a id="check_http_header"></a>
<a id="check_http_sni"></a>
<a id="check_http_json-path"></a>
<a id="check_http_address-family"></a>

| Option                                   | Default Value                     | Description                                                                                                                                                                                                                               |
|------------------------------------------|-----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| url                                      |                                   | Full URL to check (http://host[:port]/path or https://...). May be given multiple times.                                                                                                                                                  |
| host                                     |                                   | Hostname (used when --url is not given).                                                                                                                                                                                                  |
| port                                     |                                   | TCP port (defaults to 80 or 443).                                                                                                                                                                                                         |
| [path](#check_http_path)                 | /                                 | Path component of the URL.                                                                                                                                                                                                                |
| [protocol](#check_http_protocol)         | http                              | Protocol to use: http or https.                                                                                                                                                                                                           |
| [ssl](#check_http_ssl)                   | false                             | Force https, alias for --protocol https (ssl=true).                                                                                                                                                                                       |
| [timeout](#check_http_timeout)           | 30000                             | Timeout in milliseconds.                                                                                                                                                                                                                  |
| [method](#check_http_method)             | GET                               | HTTP method to use (GET, HEAD, POST, PUT, DELETE, ...).                                                                                                                                                                                   |
| post-data                                |                                   | Request body to send; implies POST unless --method is given.                                                                                                                                                                              |
| [content-type](#check_http_content-type) | application/x-www-form-urlencoded | Content-Type header for the request body.                                                                                                                                                                                                 |
| username                                 |                                   | Username for HTTP Basic authentication.                                                                                                                                                                                                   |
| password                                 |                                   | Password for HTTP Basic authentication.                                                                                                                                                                                                   |
| expected-body                            |                                   | Substring that must appear in the body for the check to be ok.                                                                                                                                                                            |
| [user-agent](#check_http_user-agent)     | NSClient++                        | User-Agent header value.                                                                                                                                                                                                                  |
| header                                   |                                   | Additional request header in 'Name: value' form (may be given multiple times).                                                                                                                                                            |
| [onredirect](#check_http_onredirect)     | ok                                | How to handle 3xx redirects: 'follow' to follow the Location, 'ok' (default) to report the redirect as-is.                                                                                                                                |
| [max-redirs](#check_http_max-redirs)     | 15                                | Maximum number of redirects to follow (with --onredirect follow).                                                                                                                                                                         |
| sni                                      |                                   | TLS Server Name Indication / verification hostname override (defaults to the URL host).                                                                                                                                                   |
| [tls-version](#check_http_tls-version)   | tlsv1.2+                          | TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                                                                                                                                              |
| [verify](#check_http_verify)             | peer                              | Certificate verify mode: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.                                                                                                                                |
| [ca](#check_http_ca)                     | ${ca-path}                        | Path to a CA bundle to use when verifying the server certificate.                                                                                                                                                                         |
| json-path                                |                                   | Extract a value from the JSON response body as a filter keyword: 'alias:dotted.path' (repeatable). Numeric segments index arrays; single-quote a segment containing a dot. Example: --json-path qlen:data.queue.length "crit=qlen > 100". |
| address-family                           |                                   | IP version to use: any (default, let the resolver choose), ipv4 or ipv6. Accepts 4/v4/inet and 6/v6/inet6 as aliases.                                                                                                                     |



<h5 id="check_http_path">path:</h5>

Path component of the URL.

*Default Value:* `/`

<h5 id="check_http_protocol">protocol:</h5>

Protocol to use: http or https.

*Default Value:* `http`

<h5 id="check_http_ssl">ssl:</h5>

Force https, alias for --protocol https (ssl=true).

*Default Value:* `false`

<h5 id="check_http_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `30000`

<h5 id="check_http_method">method:</h5>

HTTP method to use (GET, HEAD, POST, PUT, DELETE, ...).

*Default Value:* `GET`

<h5 id="check_http_content-type">content-type:</h5>

Content-Type header for the request body.

*Default Value:* `application/x-www-form-urlencoded`

<h5 id="check_http_user-agent">user-agent:</h5>

User-Agent header value.

*Default Value:* `NSClient++`

<h5 id="check_http_onredirect">onredirect:</h5>

How to handle 3xx redirects: 'follow' to follow the Location, 'ok' (default) to report the redirect as-is.

*Default Value:* `ok`

<h5 id="check_http_max-redirs">max-redirs:</h5>

Maximum number of redirects to follow (with --onredirect follow).

*Default Value:* `15`

<h5 id="check_http_tls-version">tls-version:</h5>

TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_http_verify">verify:</h5>

Certificate verify mode: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.

*Default Value:* `peer`

<h5 id="check_http_ca">ca:</h5>

Path to a CA bundle to use when verifying the server certificate.

*Default Value:* `${ca-path}`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                     | Default Value                                       |
|------------------------------------------------------------------------------------------------------------|-----------------------------------------------------|
| <a id="check_http_filter"></a>[filter](../common-options.md#filter)                                        |                                                     |
| <a id="check_http_warning"></a>[warning](../common-options.md#warning)                                     | time > 5000                                         |
| <a id="check_http_warn"></a>[warn](../common-options.md#warn)                                              |                                                     |
| <a id="check_http_critical"></a>[critical](../common-options.md#critical)                                  | code < 200 or code >= 400 or result != 'ok'         |
| <a id="check_http_crit"></a>[crit](../common-options.md#crit)                                              |                                                     |
| <a id="check_http_ok"></a>[ok](../common-options.md#ok)                                                    |                                                     |
| <a id="check_http_debug"></a>[debug](../common-options.md#debug)                                           | false                                               |
| <a id="check_http_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                               |
| <a id="check_http_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                             |
| <a id="check_http_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                     |
| <a id="check_http_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                               |
| <a id="check_http_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                   |
| <a id="check_http_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                          |
| <a id="check_http_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(list)                                  |
| <a id="check_http_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No URL checked                                      |
| <a id="check_http_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${url} -> ${code} ${result} (${size}B in ${time}ms) |
| <a id="check_http_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${url}                                              |
| <a id="check_http_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                     |
| <a id="check_http_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                     |
| <a id="check_http_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                  |
| <a id="check_http_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_http_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                                                                                                                                                                 |
|-----------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| body            | Body of the response (use with substr/regex matching)                                                                                                                                                                                                                       |
| code            | HTTP status code                                                                                                                                                                                                                                                            |
| host            | Host part of the URL                                                                                                                                                                                                                                                        |
| path            | Path part of the URL                                                                                                                                                                                                                                                        |
| port            | TCP port that was used                                                                                                                                                                                                                                                      |
| protocol        | Protocol used (http or https)                                                                                                                                                                                                                                               |
| result          | Textual result of the check (ok, error, ...)                                                                                                                                                                                                                                |
| size            | Size of the response body in bytes                                                                                                                                                                                                                                          |
| ssl_expiry_days | Days until the server's TLS certificate expires; negative if already expired. Renders as 'no certificate' (and compares false against every number) for plain http, so `ssl_expiry_days < 30` cannot fire there; `ssl_expiry_days = 'no certificate'` tests for that state. |
| status_message  | HTTP status message                                                                                                                                                                                                                                                         |
| time            | Time taken by the request in milliseconds                                                                                                                                                                                                                                   |
| url             | Full URL that was requested                                                                                                                                                                                                                                                 |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nginx_status

Check an NGINX server via its stub_status page.

#### About `check_nginx_status`

`check_nginx_status` fetches NGINX's
[stub_status](https://nginx.org/en/docs/http/ngx_http_stub_status_module.html)
page and exposes the reported values as filter keywords. The endpoint must be
enabled in the NGINX configuration, conventionally as `/nginx_status` (or
`/stub_status`):

```nginx
location /nginx_status {
    stub_status;
    allow 127.0.0.1;
    deny all;
}
```

The check emits a single record. By default it goes **critical** when the
endpoint cannot be fetched or does not look like a stub_status page
(`result != 'ok'`); connection thresholds are opt-in.

`accepts`, `handled`, `requests` and `dropped` are cumulative since NGINX
started, so `dropped > 0` stays raised until the next restart once a drop has
ever happened; treat it as a "worker_connections is too low" indicator rather
than a live gauge.

**Jump to section:**

* [Sample Commands](#check_nginx_status_samples)
* [Command-line Arguments](#check_nginx_status_options)
* [Filter keywords](#check_nginx_status_filter_keys)


<a id="check_nginx_status_samples"></a>
#### Sample Commands

**Check a local NGINX via its stub_status endpoint:**

```
check_nginx_status url=http://127.0.0.1/nginx_status
OK: ok: 291 active (6 reading, 179 writing, 106 waiting)|'127.0.0.1_active'=291;0;0
```

**Alert when connections pile up:**

```
check_nginx_status url=http://127.0.0.1/nginx_status "warning=active > 200" "critical=active > 400"
WARNING: ok: 291 active (6 reading, 179 writing, 106 waiting)|'127.0.0.1_active'=291;200;400
```

**Alert when NGINX has started dropping connections (accepted minus handled):**

```
check_nginx_status url=http://127.0.0.1/nginx_status "warning=dropped > 0"
WARNING: ok: 291 active (6 reading, 179 writing, 106 waiting)|'127.0.0.1_active'=291;0;0 '127.0.0.1_dropped'=2c;0;0
```

**A server that is down is CRITICAL by default:**

```
check_nginx_status url=http://127.0.0.1:81/nginx_status
CRITICAL: error: Failed to connect to 127.0.0.1:81: No connection could be made because the target machine actively refused it: 0 active (0 reading, 0 writing, 0 waiting)|'127.0.0.1_active'=0;0;0
```



<a id="check_nginx_status_options"></a>
#### Command-line Arguments

<a id="check_nginx_status_username"></a>
<a id="check_nginx_status_password"></a>

| Option                                         | Default Value                 | Description                                                                                                          |
|------------------------------------------------|-------------------------------|----------------------------------------------------------------------------------------------------------------------|
| [url](#check_nginx_status_url)                 | http://127.0.0.1/nginx_status | URL of the status endpoint (http://host[:port]/path or https://...).                                                 |
| [timeout](#check_nginx_status_timeout)         | 30                            | Connection/read timeout in seconds.                                                                                  |
| username                                       |                               | Username for HTTP Basic authentication.                                                                              |
| password                                       |                               | Password for HTTP Basic authentication.                                                                              |
| [tls-version](#check_nginx_status_tls-version) | tlsv1.2+                      | TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                         |
| [verify](#check_nginx_status_verify)           | peer                          | Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate. |
| [ca](#check_nginx_status_ca)                   | ${ca-path}                    | Path to a CA bundle used to verify the server certificate.                                                           |



<h5 id="check_nginx_status_url">url:</h5>

URL of the status endpoint (http://host[:port]/path or https://...).

*Default Value:* `http://127.0.0.1/nginx_status`

<h5 id="check_nginx_status_timeout">timeout:</h5>

Connection/read timeout in seconds.

*Default Value:* `30`

<h5 id="check_nginx_status_tls-version">tls-version:</h5>

TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_nginx_status_verify">verify:</h5>

Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.

*Default Value:* `peer`

<h5 id="check_nginx_status_ca">ca:</h5>

Path to a CA bundle used to verify the server certificate.

*Default Value:* `${ca-path}`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                                            |
|--------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------|
| <a id="check_nginx_status_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                          |
| <a id="check_nginx_status_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                          |
| <a id="check_nginx_status_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                          |
| <a id="check_nginx_status_critical"></a>[critical](../common-options.md#critical)                                  | result != 'ok'                                                                           |
| <a id="check_nginx_status_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                          |
| <a id="check_nginx_status_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                          |
| <a id="check_nginx_status_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                    |
| <a id="check_nginx_status_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                    |
| <a id="check_nginx_status_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                  |
| <a id="check_nginx_status_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                          |
| <a id="check_nginx_status_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                    |
| <a id="check_nginx_status_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                        |
| <a id="check_nginx_status_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                       |
| <a id="check_nginx_status_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                          |
| <a id="check_nginx_status_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No status page fetched                                                                   |
| <a id="check_nginx_status_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${result}: ${active} active (${reading} reading, ${writing} writing, ${waiting} waiting) |
| <a id="check_nginx_status_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${host}                                                                                  |
| <a id="check_nginx_status_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                          |
| <a id="check_nginx_status_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                          |
| <a id="check_nginx_status_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                       |
| <a id="check_nginx_status_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_nginx_status_filter_keys"></a>
#### Filter keywords

| Option   | Description                                                            |
|----------|------------------------------------------------------------------------|
| accepts  | Accepted connections since start                                       |
| active   | Active client connections (including waiting)                          |
| code     | HTTP status code of the response                                       |
| dropped  | Connections accepted but not handled (resource exhaustion) since start |
| handled  | Handled connections since start                                        |
| host     | Host part of the URL                                                   |
| port     | TCP port that was used                                                 |
| reading  | Connections where nginx is reading the request                         |
| requests | Requests served since start                                            |
| result   | Result of the check: ok, parse_error, http_<code> or error: <message>  |
| url      | Full URL that was requested                                            |
| waiting  | Idle keep-alive connections                                            |
| writing  | Connections where nginx is writing the response                        |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nsclient_web_online

Query the REST API of a remote NSClient++ agent (reachability or a remote check).

#### About `check_nsclient_web_online`

`check_nsclient_web_online` queries the REST API of a **remote** NSClient++ 
agent over HTTPS. It has two modes:

* **Reachability probe** (no `command=`): it hits `/api/v1/info` and reports
  **OK** `REST API reachable …` when the agent answers, **CRITICAL** when it
  cannot be reached or authentication fails.
* **Remote check** (`command=<check>`): it runs that check on the remote agent
  (`/api/v1/queries/<check>/commands/execute`) and passes the remote Nagios
  status **and** message straight through, so the local result mirrors what the
  remote agent returned.

This is intended for *liveness / availability* monitoring of an agent from a
central host. (A fuller "run remote checks" command — `check_nsclient_web` — is
planned separately; this one focuses on whether the web API is online.)

Arguments:

| Argument            | Description                                                        |
|---------------------|-------------------------------------------------------------------|
| `url`               | Base URL of the remote agent, e.g. `https://host:8443`            |
| `host` / `port`     | Alternative to `url`; `port` defaults to `8443`                   |
| `password`          | REST API password (sent as the `password` header, as user `admin`)|
| `user`              | Optional username → switches to HTTP Basic authentication         |
| `command`           | Remote check to run (omit for a plain reachability probe)         |
| `argument`          | Argument for the remote check; repeat for multiple                |
| `timeout`           | Request timeout in milliseconds                                   |
| `tls-version`       | TLS version (default `tlsv1.2+`)                                  |
| `verify`            | Certificate verify mode (default `none`, for self-signed agents)  |
| `ca`                | CA bundle to verify the remote certificate                        |

By default the remote certificate is **not** verified (`verify=none`) because
agents commonly present a self-signed certificate; set `verify=peer` with `ca=`
to enforce verification.

**Jump to section:**

* [Sample Commands](#check_nsclient_web_online_samples)


<a id="check_nsclient_web_online_samples"></a>
#### Sample Commands

**Check that a remote NSClient++ agent's REST API is reachable:**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=secret
OK: REST API reachable on https://192.168.56.10:8443
```

**Give host and port separately instead of a URL:**

```
check_nsclient_web_online host=192.168.56.10 port=8443 password=secret
OK: REST API reachable on https://192.168.56.10:8443
```

**Run a check on the remote agent and pass its result through:**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=secret command=check_cpu
OK: CPU load is ok.
```

**Pass arguments to the remote check (repeat `argument=`):**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=secret command=check_drivesize argument=drive=/ "argument=warn=used>80%"
OK: / 42.1% used
```

**A wrong password reports the authentication failure:**

```
check_nsclient_web_online url=https://192.168.56.10:8443 password=wrong
CRITICAL: Authentication failed (HTTP 403) on https://192.168.56.10:8443
```

**An unreachable agent is CRITICAL:**

```
check_nsclient_web_online url=https://192.168.56.10:9999 password=secret
CRITICAL: Failed to reach https://192.168.56.10:9999: Connection refused
```




### check_ntp_offset

Query an NTP server and check the offset between the local clock and the server.

#### Is the clock wrong, or is the source unstable?

`offset` answers the first question. A source can answer promptly with a
believable offset and still be unusable, because that offset will not hold
still — that is what the remaining keywords (`jitter`, `samples`, `root_delay`
and `root_dispersion`) are for.

`root_delay` and `root_dispersion` come straight out of the packet header, so
they need no extra traffic and are available from the default single query.
They are the server's own statement about its accuracy — useful for spotting a
source that has lost its upstream and is coasting on a free-running clock,
which it will happily keep serving:

```
check_ntp_offset server=ntp.example.com "top-syntax=${list}" "detail-syntax=${server} root_delay=${root_delay}ms root_dispersion=${root_dispersion}ms stratum=${stratum}"
OK: ntp.example.com root_delay=11ms root_dispersion=33ms stratum=2
```

#### Measuring jitter (`samples`)

Jitter is the variation *between* measurements, so it needs more than one.
**`samples` defaults to 1**, which sends a single query exactly as before and
leaves `jitter` unmeasured:

```
check_ntp_offset server=ntp.example.com "top-syntax=${list}" "detail-syntax=samples=${samples} jitter=${jitter}"
OK: samples=1 jitter=unknown
```

Raise it to measure:

```
check_ntp_offset server=ntp.example.com samples=6 "warn=jitter > 50" "crit=jitter > 100" "top-syntax=${list}" "detail-syntax=${server} jitter=${jitter}ms over ${samples} samples"
WARNING: ntp.example.com jitter=70ms over 6 samples|'ntp.example.com_jitter'=70ms;50;100
```

`jitter` is an *optional number*: until measured it renders as `unknown`,
**every numeric comparison on it is false** (in both directions), and no jitter
perfdata is emitted — a sentinel would poison the series. The string form is
the presence test:

```
check_ntp_offset server=ntp.example.com samples=6 "warn=jitter > 50" "crit=jitter = 'unknown'"
```

Note that a threshold like `jitter > 50` is simply false while unmeasured, so
leaving `samples` at its default silently never alerts — set both together, or
add the `= 'unknown'` clause to catch a misconfiguration.

> **Upgrading.** `jitter` used to report `-1` before two samples existed. A
> filter written against that sentinel (`jitter = -1`) no longer matches and
> must become `jitter = 'unknown'`; perfdata is omitted rather than plotted as
> `-1` until the value is real.

Three things worth knowing about how the burst behaves:

* **Sampling stops at the first failure.** An unreachable or slow server costs
  one timeout, not `samples` of them, so raising `samples` does not multiply
  the worst-case runtime of the check.
* **The reported `offset` and `time` come from the quickest exchange.** A
  delayed packet biases the offset by roughly half its extra delay, so the
  fastest round trip is the most trustworthy estimate. With the default of one
  sample this is simply that sample.
* **A steady offset produces no jitter.** A clock that is consistently five
  seconds wrong is inaccurate but perfectly stable, so it shows a large
  `offset` and a near-zero `jitter`. The two conditions are independent and
  worth alerting on separately:

```
check_ntp_offset server=ntp.example.com samples=6 "warn=offset > 100 or jitter > 50" "crit=offset > 1000 or jitter > 200 or stratum >= 16" "top-syntax=${list}" "detail-syntax=offset=${offset_signed}ms jitter=${jitter}ms"
WARNING: offset=35ms jitter=70ms|'ntp.example.com_jitter'=70ms;50;200
```

**Jump to section:**

* [Sample Commands](#check_ntp_offset_samples)
* [Command-line Arguments](#check_ntp_offset_options)
* [Filter keywords](#check_ntp_offset_filter_keys)


<a id="check_ntp_offset_samples"></a>
#### Sample Commands

**Default check against a single NTP server:**

```
check_ntp_offset server=pool.ntp.org
L        cli OK: OK: pool.ntp.org offset=1326ms stratum=2
L        cli  Performance data: 'pool.ntp.org'=1326;60000;120000 'pool.ntp.org_stratum'=2;16;16
```

**Multiple servers via comma list (averaged across answers):**

```
check_ntp_offset "servers=0.pool.ntp.org,1.pool.ntp.org,2.pool.ntp.org" timeout=2000
L        cli OK: OK: 0.pool.ntp.org offset=1326ms stratum=2, 1.pool.ntp.org offset=1327ms stratum=1, 2.pool.ntp.org offset=1329ms stratum=2
L        cli  Performance data: '0.pool.ntp.org'=1326;60000;120000 '0.pool.ntp.org_stratum'=2;16;16 '1.pool.ntp.org'=1327;60000;120000 '1.pool.ntp.org_stratum'=1;16;16 '2.pool.ntp.org'=1329;60000;120000 '2.pool.ntp.org_stratum'=2;16;16
```

**Custom port and timeout:**

```
check_ntp_offset server=time.example.com port=123 timeout=1500
check_ntp_offset server=time.example.com port=123 timeout=1500
L        cli OK: OK: time.example.com offset=0ms stratum=0
L        cli  Performance data: 'time.example.com'=0;60000;120000 'time.example.com_stratum'=0;16;16
```

**Tighter thresholds (alert when more than 50ms / 200ms off):**

```
check_ntp_offset server=pool.ntp.org "warn=offset > 50 or stratum >= 8" "crit=offset > 200 or stratum >= 16"
L        cli CRITICAL: CRITICAL: pool.ntp.org offset=1326ms stratum=1
L        cli  Performance data: 'pool.ntp.org'=1326;50;200 'pool.ntp.org_stratum'=1;8;16
```

**Use signed offset to distinguish ahead vs behind:**

```
check_ntp_offset server=pool.ntp.org "top-syntax=%(status): %(list)" "detail-syntax=%(server) signed=%(offset_signed)ms abs=%(offset)ms s=%(stratum)"
L        cli OK: OK: pool.ntp.org signed=1327ms abs=1327ms s=1
L        cli  Performance data: 'pool.ntp.org'=1327;60000;120000 'pool.ntp.org_stratum'=1;16;16
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ntp_offset --argument "server=pool.ntp.org"
OK: pool.ntp.org offset=1326ms stratum=2| 'pool.ntp.org'=1326;60000;120000 'pool.ntp.org_stratum'=2;16;16
```


**Measure jitter across a burst of samples (needs `samples` >= 2):**

```
check_ntp_offset server=ntp.example.com samples=6 "warn=jitter > 50" "crit=jitter > 100" "top-syntax=${list}" "detail-syntax=${server} jitter=${jitter}ms over ${samples} samples"
WARNING: ntp.example.com jitter=70ms over 6 samples|'ntp.example.com_jitter'=70ms;50;100
```

**Alert on an inaccurate clock and an unstable source independently:**

```
check_ntp_offset server=ntp.example.com samples=6 "warn=offset > 100 or jitter > 50" "crit=offset > 1000 or jitter > 200 or stratum >= 16" "top-syntax=${list}" "detail-syntax=offset=${offset_signed}ms jitter=${jitter}ms"
WARNING: offset=35ms jitter=70ms|'ntp.example.com_jitter'=70ms;50;200
```

**Report what the server claims about its own accuracy (no extra traffic):**

```
check_ntp_offset server=ntp.example.com "top-syntax=${list}" "detail-syntax=${server} root_delay=${root_delay}ms root_dispersion=${root_dispersion}ms stratum=${stratum}"
OK: ntp.example.com root_delay=11ms root_dispersion=33ms stratum=2
```



<a id="check_ntp_offset_options"></a>
#### Command-line Arguments

<a id="check_ntp_offset_server"></a>
<a id="check_ntp_offset_servers"></a>
<a id="check_ntp_offset_address-family"></a>

| Option                               | Default Value | Description                                                                                                                                                                                                                             |
|--------------------------------------|---------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| server                               |               | NTP server to query (may be given multiple times).                                                                                                                                                                                      |
| servers                              |               | Comma separated list of NTP servers to query.                                                                                                                                                                                           |
| [port](#check_ntp_offset_port)       | 123           | UDP port to use (default: 123).                                                                                                                                                                                                         |
| [timeout](#check_ntp_offset_timeout) | 5000          | Timeout in milliseconds.                                                                                                                                                                                                                |
| address-family                       |               | IP version to use: any (default, let the resolver choose), ipv4 or ipv6. Accepts 4/v4/inet and 6/v6/inet6 as aliases.                                                                                                                   |
| [samples](#check_ntp_offset_samples) | 1             | Number of queries to send to each server (default: 1). At least 2 are needed for the jitter keyword, which is the variation between samples; sampling stops at the first failure so an unreachable server still costs only one timeout. |



<h5 id="check_ntp_offset_port">port:</h5>

UDP port to use (default: 123).

*Default Value:* `123`

<h5 id="check_ntp_offset_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `5000`

<h5 id="check_ntp_offset_samples">samples:</h5>

Number of queries to send to each server (default: 1). At least 2 are needed for the jitter keyword, which is the variation between samples; sampling stops at the first failure so an unreachable server still costs only one timeout.

*Default Value:* `1`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                                          |
|------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| <a id="check_ntp_offset_filter"></a>[filter](../common-options.md#filter)                                        |                                                        |
| <a id="check_ntp_offset_warning"></a>[warning](../common-options.md#warning)                                     | offset > 50 or stratum >= 16                           |
| <a id="check_ntp_offset_warn"></a>[warn](../common-options.md#warn)                                              |                                                        |
| <a id="check_ntp_offset_critical"></a>[critical](../common-options.md#critical)                                  | offset > 100 or stratum >= 16 or result != 'ok'        |
| <a id="check_ntp_offset_crit"></a>[crit](../common-options.md#crit)                                              |                                                        |
| <a id="check_ntp_offset_ok"></a>[ok](../common-options.md#ok)                                                    |                                                        |
| <a id="check_ntp_offset_debug"></a>[debug](../common-options.md#debug)                                           | false                                                  |
| <a id="check_ntp_offset_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                  |
| <a id="check_ntp_offset_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                                |
| <a id="check_ntp_offset_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                        |
| <a id="check_ntp_offset_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                  |
| <a id="check_ntp_offset_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                      |
| <a id="check_ntp_offset_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                             |
| <a id="check_ntp_offset_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(list)                                     |
| <a id="check_ntp_offset_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No NTP server checked                                  |
| <a id="check_ntp_offset_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${server} offset=${offset_signed}ms stratum=${stratum} |
| <a id="check_ntp_offset_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${server}                                              |
| <a id="check_ntp_offset_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                        |
| <a id="check_ntp_offset_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                        |
| <a id="check_ntp_offset_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                     |
| <a id="check_ntp_offset_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_ntp_offset_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                                                                                                                                                                               |
|-----------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| jitter          | RMS variation between the sampled offsets, in milliseconds; 'unknown' when fewer than 2 samples were taken (raise samples= to measure it). Renders as 'unknown', compares false against every number until measured, and emits no perfdata until real; `jitter = 'unknown'` tests for it. |
| offset          | Absolute clock offset between local host and server, in milliseconds                                                                                                                                                                                                                      |
| offset_signed   | Signed clock offset (positive = local clock is ahead of server), in milliseconds                                                                                                                                                                                                          |
| port            | UDP port the query was sent to                                                                                                                                                                                                                                                            |
| result          | Textual result of the query (ok, timeout, error, ...)                                                                                                                                                                                                                                     |
| root_delay      | Round trip delay the server reports to its own reference clock, in milliseconds                                                                                                                                                                                                           |
| root_dispersion | Maximum error the server claims for the time it is serving, in milliseconds                                                                                                                                                                                                               |
| samples         | Number of samples that answered                                                                                                                                                                                                                                                           |
| server          | NTP server that was queried                                                                                                                                                                                                                                                               |
| stratum         | Stratum reported by the server (0..16)                                                                                                                                                                                                                                                    |
| time            | Round trip time of the NTP query in milliseconds                                                                                                                                                                                                                                          |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_phpfpm_status

Check a PHP-FPM pool via its status page.

#### About `check_phpfpm_status`

`check_phpfpm_status` fetches a PHP-FPM pool's status page (the default text
format) and exposes the reported values as filter keywords. The page must be
enabled in the pool configuration (`pm.status_path = /status`) and the location
routed to FPM in the web server in front of it (or served via `fastcgi` on a
dedicated port).

The check emits a single record. By default it goes **warning** when requests
are waiting in the listen queue (`listen_queue > 0` — the pool has no free
worker to pick them up) and **critical** when the endpoint cannot be fetched
or does not look like an FPM status page (`result != 'ok'`).

`max_children_reached`, `slow_requests`, `max_listen_queue` and
`accepted_conn` are cumulative since the pool (re)started; a threshold on them
stays raised until the counter resets on reload.

**Jump to section:**

* [Sample Commands](#check_phpfpm_status_samples)
* [Command-line Arguments](#check_phpfpm_status_options)
* [Filter keywords](#check_phpfpm_status_filter_keys)


<a id="check_phpfpm_status_samples"></a>
#### Sample Commands

**Check a PHP-FPM pool via its status page:**

```
check_phpfpm_status url=http://127.0.0.1/status
OK: ok: pool www: 3 active, 7 idle, 0 queued|'www_active_processes'=3;0;0 'www_idle_processes'=7;0;0 'www_listen_queue'=0;0;0
```

**The default warning fires when requests are queueing up (the pool is saturated):**

```
check_phpfpm_status url=http://127.0.0.1/status
WARNING: ok: pool www: 8 active, 0 idle, 4 queued|'www_active_processes'=8;0;0 'www_idle_processes'=0;0;0 'www_listen_queue'=4;0;0
```

**Alert when the pool has ever hit pm.max_children or logged slow requests:**

```
check_phpfpm_status url=http://127.0.0.1/status "critical=max_children_reached > 0" "warning=slow_requests > 4"
CRITICAL: ok: pool www: 3 active, 7 idle, 0 queued|'www_max_children_reached'=1c;0;0 'www_slow_requests'=5c;4;0 'www_active_processes'=3;0;0 'www_idle_processes'=7;0;0 'www_listen_queue'=0;0;0
```

**An FPM pool that is down (or a missing status location) is CRITICAL by default:**

```
check_phpfpm_status url=http://127.0.0.1/status
CRITICAL: http_404: pool : 0 active, 0 idle, 0 queued|'_active_processes'=0;0;0 '_idle_processes'=0;0;0 '_listen_queue'=0;0;0
```



<a id="check_phpfpm_status_options"></a>
#### Command-line Arguments

<a id="check_phpfpm_status_username"></a>
<a id="check_phpfpm_status_password"></a>

| Option                                          | Default Value           | Description                                                                                                          |
|-------------------------------------------------|-------------------------|----------------------------------------------------------------------------------------------------------------------|
| [url](#check_phpfpm_status_url)                 | http://127.0.0.1/status | URL of the status endpoint (http://host[:port]/path or https://...).                                                 |
| [timeout](#check_phpfpm_status_timeout)         | 30                      | Connection/read timeout in seconds.                                                                                  |
| username                                        |                         | Username for HTTP Basic authentication.                                                                              |
| password                                        |                         | Password for HTTP Basic authentication.                                                                              |
| [tls-version](#check_phpfpm_status_tls-version) | tlsv1.2+                | TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                         |
| [verify](#check_phpfpm_status_verify)           | peer                    | Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate. |
| [ca](#check_phpfpm_status_ca)                   | ${ca-path}              | Path to a CA bundle used to verify the server certificate.                                                           |



<h5 id="check_phpfpm_status_url">url:</h5>

URL of the status endpoint (http://host[:port]/path or https://...).

*Default Value:* `http://127.0.0.1/status`

<h5 id="check_phpfpm_status_timeout">timeout:</h5>

Connection/read timeout in seconds.

*Default Value:* `30`

<h5 id="check_phpfpm_status_tls-version">tls-version:</h5>

TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_phpfpm_status_verify">verify:</h5>

Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.

*Default Value:* `peer`

<h5 id="check_phpfpm_status_ca">ca:</h5>

Path to a CA bundle used to verify the server certificate.

*Default Value:* `${ca-path}`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                                                                                       |
|---------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------|
| <a id="check_phpfpm_status_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                     |
| <a id="check_phpfpm_status_warning"></a>[warning](../common-options.md#warning)                                     | listen_queue > 0                                                                                    |
| <a id="check_phpfpm_status_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                     |
| <a id="check_phpfpm_status_critical"></a>[critical](../common-options.md#critical)                                  | result != 'ok'                                                                                      |
| <a id="check_phpfpm_status_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                     |
| <a id="check_phpfpm_status_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                     |
| <a id="check_phpfpm_status_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                               |
| <a id="check_phpfpm_status_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                               |
| <a id="check_phpfpm_status_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                             |
| <a id="check_phpfpm_status_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                     |
| <a id="check_phpfpm_status_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                               |
| <a id="check_phpfpm_status_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                   |
| <a id="check_phpfpm_status_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                  |
| <a id="check_phpfpm_status_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                     |
| <a id="check_phpfpm_status_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No status page fetched                                                                              |
| <a id="check_phpfpm_status_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${result}: pool ${pool}: ${active_processes} active, ${idle_processes} idle, ${listen_queue} queued |
| <a id="check_phpfpm_status_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${pool}                                                                                             |
| <a id="check_phpfpm_status_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                     |
| <a id="check_phpfpm_status_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                     |
| <a id="check_phpfpm_status_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                  |
| <a id="check_phpfpm_status_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_phpfpm_status_filter_keys"></a>
#### Filter keywords

| Option               | Description                                                             |
|----------------------|-------------------------------------------------------------------------|
| accepted_conn        | Connections accepted since start                                        |
| active_processes     | Workers currently serving requests                                      |
| code                 | HTTP status code of the response                                        |
| host                 | Host part of the URL                                                    |
| idle_processes       | Idle (spare) workers                                                    |
| listen_queue         | Requests currently waiting in the listen queue                          |
| listen_queue_len     | Size of the socket listen queue                                         |
| max_active_processes | Highest number of simultaneously active workers since start             |
| max_children_reached | Times the pool hit pm.max_children since start (the pool was saturated) |
| max_listen_queue     | Highest listen queue length seen since start                            |
| pool                 | Name of the FPM pool                                                    |
| port                 | TCP port that was used                                                  |
| process_manager      | Process manager mode (static, dynamic or ondemand)                      |
| result               | Result of the check: ok, parse_error, http_<code> or error: <message>   |
| slow_requests        | Requests that exceeded request_slowlog_timeout                          |
| total_processes      | Total workers in the pool                                               |
| url                  | Full URL that was requested                                             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_ping

Ping another host and check the result.

#### Jitter

Beyond "does it answer" (`loss`) and "how fast" (`time`), `check_ping` reports
how *steady* the latency is with `jitter`, the mean variation between the
round trip times in ms.

Jitter is the variation *between* packets, so it needs more than one. **`count`
defaults to 1**, which leaves `jitter` unmeasured; raise it to measure:

```
check_ping host=gw.example.com count=10 "warn=jitter > 20" "crit=jitter > 50" "top-syntax=${list}" "detail-syntax=${host} rtt=${time}ms jitter=${jitter}ms"
```

`jitter` is an *optional number*: until it can be measured it renders as
`unknown`, **every numeric comparison on it is false** (in both directions —
`jitter > 20` and `jitter < 20` alike), and no jitter perfdata is emitted. Test
for the unmeasured state explicitly with the string form:

```
check_ping host=gw.example.com count=10 "warn=jitter > 20 or jitter = 'unknown'"
```

Note that leaving `count` at its default means `jitter > 20` silently never
alerts — set both together, or add the `= 'unknown'` clause to catch it.

> **Upgrading.** `jitter` and `ttl` used to report `-1` when unmeasurable.
> Filters written against that sentinel (`jitter = -1`, `ttl != -1`) no longer
> match anything and must become `jitter = 'unknown'` / `ttl != 'unknown'`.
> Perfdata for an unmeasured value is now omitted rather than plotted as `-1`,
> so RRD-backed graphs will see the metric appear and disappear.

**A slow link is not a jittery one.** A host that consistently answers in 250 ms
has a large `time` and near-zero `jitter`; a host alternating between 10 ms and
200 ms has a small average `time` and large `jitter`. Latency-sensitive traffic
(VoIP, RDP, database replication) cares about the second far more than the
first, which is why they threshold separately:

```
check_ping host=voip-gw.example.com count=20 "warn=jitter > 30 or loss > 1%" "crit=jitter > 60 or loss > 5%"
```

**On the `total` row, `jitter` is the worst value across hosts**, not a jitter
computed over all the hosts' round trip times pooled together — mixing a fast
host with a slow one would manufacture a large number that describes nothing.
So a fleet-wide `crit=jitter > 50` fires when *any* host is that unstable:

```
check_ping hosts=a.example.com,b.example.com,c.example.com count=10 total=true "crit=jitter > 50"
```

Note that `time` remains the round trip time of the **last** reply, not an
average over the burst.

#### TTL

`ttl` and the `ttl=` argument are two different numbers that share a name, the
same way `ping -t` and the `ttl=` in its output do. The `ttl=N` *argument* is
the TTL / hop limit stamped on the packets **we send** (`0`, the default,
keeps the system default); the `${ttl}` *keyword* is the TTL of the **reply we
got back** — what is left of the remote host's own outgoing TTL after the
return path.

```
check_ping host=router.example.com "top-syntax=${list}" "detail-syntax=${host} replied with ttl=${ttl}"
```

The reply TTL is a rough proxy for path length, so a drop in it means the route
changed — traffic failing over to a longer path, for instance:

```
check_ping host=peer.example.com "warn=ttl < 50" "crit=ttl < 20"
```

Limiting the outgoing TTL is how you check that a host is where you think it is
on the network: with `ttl=1` only a directly attached neighbour can answer.

```
check_ping host=gw.example.com ttl=1
```

`ttl` is **`unknown`** when no reply carried one — nothing came back, or the
check ran over IPv6, where the hop limit is not available without ancillary
data the check does not request. Like `jitter` it is an optional number: while
unknown it renders as `unknown`, every numeric comparison on it is false (so
`ttl < 20` will not fire on an unanswered host — use `loss` for that), no ttl
perfdata is emitted, and `ttl = 'unknown'` tests for the state directly.

On the `total` row `ttl` is the **lowest** value across hosts (the reply closest
to running out of hops), and hosts with no TTL are ignored rather than dragging
the fleet-wide value to "unknown".

#### Packet size

`size=N` sets the ICMP payload to exactly N bytes. The `payload` string is
repeated and cut to length, so the bytes on the wire stay recognisable rather
than being a run of zeroes. `size=0` (the default) sends the `payload` string
as-is, unchanged from previous behaviour.

The 8-byte ICMP header sits on top of the payload, and IPv4 adds 20 more, so
**1472 bytes is the largest payload that fits an untagged 1500-byte MTU**. That
makes `size` the tool for finding a path-MTU or fragmentation problem — a link
that passes small packets and silently drops big ones:

```
check_ping host=remote.example.com size=1472 count=5 "crit=loss > 0%"
```

The accepted range is 0–65507 (65535 minus the IPv4 and ICMP headers); anything
outside it is rejected with a message rather than being silently clamped.

**Jump to section:**

* [Sample Commands](#check_ping_samples)
* [Command-line Arguments](#check_ping_options)
* [Filter keywords](#check_ping_filter_keys)


<a id="check_ping_samples"></a>
#### Sample Commands

**Pinging a single host:**

```
check_ping host=192.168.0.1
OK: All 1 hosts are ok|'192.168.0.1_loss'=0%;5;10 '192.168.0.1'=2ms;60;100
```

**Pinging multiple hosts (repeat `host=`) with a total bucket:**

```
check_ping host=1.1.1.1 host=8.8.8.8 host=google.com total
L        cli OK: All 4 hosts are ok
L        cli  Performance data: '1.1.1.1_loss'=0%;5;10 '1.1.1.1'=3ms;60;100 '8.8.8.8_loss'=0%;5;10 '8.8.8.8'=9ms;60;100 'google.com_loss'=0%;5;10 'google.com'=2ms;60;100 'total_loss'=0%;5;10 'total'=14ms;60;100
```

**Tighter thresholds with explicit count and timeout:**

```
check_ping host=8.8.8.8 count=4 timeout=300 "warn=time > 30 or loss > 0%" "crit=time > 80 or loss > 25%"
L        cli OK: All 1 hosts are ok
L        cli  Performance data: '8.8.8.8_loss'=0%;0;25 '8.8.8.8'=2ms;30;80
```

**Custom payload and per-host text output:**

```
check_ping host=1.1.1.1 host=8.8.8.8 payload="hello" "top-syntax=%(status): %(list)" "detail-syntax=%(host)=%(time)ms"
L        cli OK: 1.1.1.1=2ms, 8.8.8.8=2ms
L        cli  Performance data: '1.1.1.1_loss'=0%;5;10 '1.1.1.1'=2ms;60;100 '8.8.8.8_loss'=0%;5;10 '8.8.8.8'=2ms;60;100
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ping --argument "host=192.168.56.1"
OK: All 1 hosts are ok|'192.168.56.1_loss'=0%;5;10 '192.168.56.1'=1ms;60;100
```

**Report the TTL of the reply (a rough proxy for path length):**

```
check_ping host=192.168.56.10 "top-syntax=${list}" "detail-syntax=${host} replied with ttl=${ttl}"
OK: 192.168.56.10 replied with ttl=64
```

**Alert when the route grows (the reply TTL drops):**

```
check_ping host=peer.example.com "warn=ttl < 50" "crit=ttl < 20"
OK: peer.example.com Packet loss = 0%, RTA = 12ms
```

**Limit the outgoing TTL to check a host is a directly attached neighbour:**

```
check_ping host=192.168.56.1 ttl=1
OK: 192.168.56.1 Packet loss = 0%, RTA = 1ms
```

**Send a full-MTU packet to find a path-MTU or fragmentation problem:**

```
check_ping host=remote.example.com size=1472 count=5 "crit=loss > 0%"
OK: remote.example.com Packet loss = 0%, RTA = 24ms
```

**Sizes outside the ICMP payload range are rejected rather than clamped:**

```
check_ping host=192.168.56.10 size=99999
Invalid size: 99999 (expected 0-65507)
```



<a id="check_ping_options"></a>
#### Command-line Arguments

<a id="check_ping_host"></a>
<a id="check_ping_hosts"></a>
<a id="check_ping_address-family"></a>

| Option                         | Default Value          | Description                                                                                                                                                                                                                                                           |
|--------------------------------|------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                           |                        | The host to check (or multiple hosts).                                                                                                                                                                                                                                |
| [total](#check_ping_total)     | false                  | Include the total of all matching hosts                                                                                                                                                                                                                               |
| hosts                          |                        | The host to check (or multiple hosts).                                                                                                                                                                                                                                |
| [count](#check_ping_count)     | 1                      | Number of packets to send.                                                                                                                                                                                                                                            |
| [timeout](#check_ping_timeout) | 500                    | Timeout in milliseconds.                                                                                                                                                                                                                                              |
| [payload](#check_ping_payload) | Hello from NSClient++. | The payload to send in the ping request (default: 'Hello from NSClient++')                                                                                                                                                                                            |
| address-family                 |                        | IP version to use: any (default, let the resolver choose), ipv4 or ipv6. Accepts 4/v4/inet and 6/v6/inet6 as aliases.                                                                                                                                                 |
| [size](#check_ping_size)       | 0                      | Size of the ICMP payload in bytes (0 keeps the --payload string as-is). The payload is repeated or truncated to reach exactly this many bytes; the 8 byte ICMP header is on top, so a 1472 byte payload is the largest that fits an untagged 1500 byte MTU over IPv4. |
| [ttl](#check_ping_ttl)         | 0                      | TTL / hop limit to set on outgoing packets (0 keeps the system default). Note the ttl keyword reports the TTL of the reply, which is a different number: it is what is left of the remote host's own outgoing TTL after the return path.                              |



<h5 id="check_ping_total">total:</h5>

Include the total of all matching hosts

*Default Value:* `false`

<h5 id="check_ping_count">count:</h5>

Number of packets to send.

*Default Value:* `1`

<h5 id="check_ping_timeout">timeout:</h5>

Timeout in milliseconds.

*Default Value:* `500`

<h5 id="check_ping_payload">payload:</h5>

The payload to send in the ping request (default: 'Hello from NSClient++')

*Default Value:* `Hello from NSClient++.`

<h5 id="check_ping_size">size:</h5>

Size of the ICMP payload in bytes (0 keeps the --payload string as-is). The payload is repeated or truncated to reach exactly this many bytes; the 8 byte ICMP header is on top, so a 1472 byte payload is the largest that fits an untagged 1500 byte MTU over IPv4.

*Default Value:* `0`

<h5 id="check_ping_ttl">ttl:</h5>

TTL / hop limit to set on outgoing packets (0 keeps the system default). Note the ttl keyword reports the TTL of the reply, which is a different number: it is what is left of the remote host's own outgoing TTL after the return path.

*Default Value:* `0`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                     | Default Value                                     |
|------------------------------------------------------------------------------------------------------------|---------------------------------------------------|
| <a id="check_ping_filter"></a>[filter](../common-options.md#filter)                                        |                                                   |
| <a id="check_ping_warning"></a>[warning](../common-options.md#warning)                                     | time > 60 or loss > 5%                            |
| <a id="check_ping_warn"></a>[warn](../common-options.md#warn)                                              |                                                   |
| <a id="check_ping_critical"></a>[critical](../common-options.md#critical)                                  | time > 100 or loss > 10%                          |
| <a id="check_ping_crit"></a>[crit](../common-options.md#crit)                                              |                                                   |
| <a id="check_ping_ok"></a>[ok](../common-options.md#ok)                                                    |                                                   |
| <a id="check_ping_debug"></a>[debug](../common-options.md#debug)                                           | false                                             |
| <a id="check_ping_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                             |
| <a id="check_ping_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                           |
| <a id="check_ping_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                   |
| <a id="check_ping_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                             |
| <a id="check_ping_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                 |
| <a id="check_ping_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${ok_count}/${count} (${problem_list}) |
| <a id="check_ping_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) hosts are ok              |
| <a id="check_ping_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No hosts found                                    |
| <a id="check_ping_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${ip} Packet loss = ${loss}%, RTA = ${time}ms     |
| <a id="check_ping_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${host}                                           |
| <a id="check_ping_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                   |
| <a id="check_ping_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                   |
| <a id="check_ping_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                |
| <a id="check_ping_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                   |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_ping_filter_keys"></a>
#### Filter keywords

| Option  | Description                                                                                                                    |
|---------|--------------------------------------------------------------------------------------------------------------------------------|
| host    | The host name or ip address (as given on command line)                                                                         |
| ip      | The ip address name                                                                                                            |
| jitter  | Mean variation between the round trip times, in ms; 'unknown' when fewer than 2 packets came back (raise count= to measure it) |
| loss    | Packet loss                                                                                                                    |
| recv    | Number of packets received from the host                                                                                       |
| sent    | Number of packets sent to the host                                                                                             |
| time    | Round trip time in ms                                                                                                          |
| timeout | Number of packets which timed out from the host                                                                                |
| ttl     | TTL of the last reply; 'unknown' when no reply carried one (nothing came back, or IPv6, where the hop limit is not available)  |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_ssh

Connect to an SSH port and verify the server presents a valid SSH banner.

#### About `check_ssh`

`check_ssh` confirms that an SSH server is reachable and presents a valid SSH
protocol banner. It connects to the port (default **22**), reads the greeting
the server sends on connect, and requires it to start with `SSH-` (e.g.
`SSH-2.0-OpenSSH_9.6`). Nothing is written to the peer, so it does not initiate
a key exchange or authenticate — it is a lightweight "is sshd up and answering"
probe.

It builds on [`check_tcp`](#check_tcp) (the `service=ssh` preset), so it shares
`check_tcp`'s keywords (`host`, `port`, `time`, `result`, `response`,
`connected`) and thresholds; on an SSH check `response` holds the banner the
server returned.

Default thresholds: **warning** `time > 1000`, **critical**
`time > 5000 or result != 'ok'`. A port that answers but is not SSH yields
`result = no_match` (CRITICAL); a closed port yields `result = refused`.

#### The parsed identification string

On top of those, `check_ssh` splits the SSH identification string
(RFC 4253 §4.2) into its parts, so the server's protocol and software version
can be thresholded directly instead of regex-matching the raw `response`:

```
SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5
    │   │             └── comments
    │   └── version ─────── software "OpenSSH" + software_version "9.6p1"
    └── protocol
```

`banner` keeps the raw identification line, and `protocol_major` /
`protocol_minor` expose the protocol version as numbers (`2` and `0` for
`2.0`; `99` is the minor for `1.99`).

`software` / `software_version` are split on the last `_` that is followed by a
digit, which keeps multi-word names intact (`OpenSSH_for_Windows_9.5` →
`OpenSSH_for_Windows` + `9.5`). A server that publishes an opaque build id
rather than a version (e.g. `SSH-2.0-GitLab-SSHD`) keeps the whole string as
`software` and leaves `software_version` empty; `version` always holds the full
field, so it is the safe one to regex against.

All of these are **empty** (and the numeric ones `0`) when no banner was read —
a refused or timed-out connection, or a port that is not speaking SSH. Since
the default critical already covers `result != 'ok'`, that case is caught
regardless; guard on `result = 'ok'` explicitly if you add your own thresholds
and want to keep the two failure modes apart.

A note on `protocol`: `1.99` is not "older than 2.0" — it means the server
speaks 2.0 *and* still accepts the insecure SSHv1, which is exactly what
`protocol_major < 2` is for.

**Jump to section:**

* [Sample Commands](#check_ssh_samples)
* [Command-line Arguments](#check_ssh_options)
* [Filter keywords](#check_ssh_filter_keys)


<a id="check_ssh_samples"></a>
#### Sample Commands

**Check that an SSH server presents a valid banner:**

```
check_ssh host=github.com
OK: github.com:22 ok in 13ms
L        cli  Performance data: 'github.com_22_time'=13;1000;5000
```

**Non-standard SSH port:**

```
check_ssh host=192.168.56.10 port=2222
OK: 192.168.56.10:2222 ok in 2ms
```

**A port that is not speaking SSH is CRITICAL (`no_match`):**

```
check_ssh host=www.google.com port=443
CRITICAL: www.google.com:443 no_match in 12ms
```

**Report what the server is running:**

```
check_ssh host=192.168.56.10 "top-syntax=${list}" "detail-syntax=${host} runs ${software} ${software_version} (SSH ${protocol}, ${comments})"
OK: 192.168.56.10 runs OpenSSH 9.6p1 (SSH 2.0, Ubuntu-3ubuntu13.5)
```

**Show the raw identification string:**

```
check_ssh host=gitlab.com "top-syntax=${list}" "detail-syntax=${banner}"
OK: SSH-2.0-GitLab-SSHD
```

**Alert when the server still speaks the insecure SSHv1 (`1.99` or `1.x`):**

```
check_ssh host=192.168.56.10 "crit=protocol_major < 2" "top-syntax=${list}" "detail-syntax=${host} speaks SSH ${protocol}"
OK: 192.168.56.10 speaks SSH 2.0
```

**Alert on an outdated sshd:**

```
check_ssh host=192.168.56.10 "crit=software = 'OpenSSH' and software_version not like '9.'" "top-syntax=${list}" "detail-syntax=${software} ${software_version}"
OK: OpenSSH 9.6p1
```

**Check a fleet and list each server's version:**

```
check_ssh hosts=github.com,gitlab.com,bitbucket.org "top-syntax=${list}" "detail-syntax=${host}: ${version}"
OK: github.com: 7f27de7, gitlab.com: GitLab-SSHD, bitbucket.org: conker_20260806-85ca5cadcf
```

**Tighter response-time thresholds:**

```
check_ssh host=192.168.56.10 "warn=time > 200" "crit=time > 1000 or result != 'ok'"
OK: 192.168.56.10:22 ok in 3ms
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_ssh --argument "host=192.168.56.10"
OK: 192.168.56.10:22 ok in 2ms
```



<a id="check_ssh_options"></a>
#### Command-line Arguments

<a id="check_ssh_host"></a>
<a id="check_ssh_hosts"></a>
<a id="check_ssh_port"></a>
<a id="check_ssh_send"></a>
<a id="check_ssh_expect"></a>
<a id="check_ssh_ca"></a>
<a id="check_ssh_address-family"></a>

| Option                                | Default Value | Description                                                                                                           |
|---------------------------------------|---------------|-----------------------------------------------------------------------------------------------------------------------|
| host                                  |               | Host(s) to connect to (may be given multiple times).                                                                  |
| hosts                                 |               | Comma separated list of hosts to connect to.                                                                          |
| port                                  |               | TCP port to connect to.                                                                                               |
| [timeout](#check_ssh_timeout)         | 5000          | Connection / read timeout in milliseconds.                                                                            |
| send                                  |               | Optional payload to send after the connection is established.                                                         |
| expect                                |               | Optional substring expected in the response.                                                                          |
| [ssl](#check_ssh_ssl)                 | false         | Wrap the connection in TLS/SSL after connecting (ssl=true).                                                           |
| [tls-version](#check_ssh_tls-version) | tlsv1.2+      | TLS version when --ssl is used (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                 |
| [verify](#check_ssh_verify)           | none          | Certificate verify mode when --ssl is used: none (default), peer, ... (peer requires --ca).                           |
| ca                                    |               | CA bundle used to verify the server certificate when --ssl --verify peer is used.                                     |
| address-family                        |               | IP version to use: any (default, let the resolver choose), ipv4 or ipv6. Accepts 4/v4/inet and 6/v6/inet6 as aliases. |



<h5 id="check_ssh_timeout">timeout:</h5>

Connection / read timeout in milliseconds.

*Default Value:* `5000`

<h5 id="check_ssh_ssl">ssl:</h5>

Wrap the connection in TLS/SSL after connecting (ssl=true).

*Default Value:* `false`

<h5 id="check_ssh_tls-version">tls-version:</h5>

TLS version when --ssl is used (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_ssh_verify">verify:</h5>

Certificate verify mode when --ssl is used: none (default), peer, ... (peer requires --ca).

*Default Value:* `none`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                    | Default Value                          |
|-----------------------------------------------------------------------------------------------------------|----------------------------------------|
| <a id="check_ssh_filter"></a>[filter](../common-options.md#filter)                                        |                                        |
| <a id="check_ssh_warning"></a>[warning](../common-options.md#warning)                                     | time > 1000                            |
| <a id="check_ssh_warn"></a>[warn](../common-options.md#warn)                                              |                                        |
| <a id="check_ssh_critical"></a>[critical](../common-options.md#critical)                                  | time > 5000 or result != 'ok'          |
| <a id="check_ssh_crit"></a>[crit](../common-options.md#crit)                                              |                                        |
| <a id="check_ssh_ok"></a>[ok](../common-options.md#ok)                                                    |                                        |
| <a id="check_ssh_debug"></a>[debug](../common-options.md#debug)                                           | false                                  |
| <a id="check_ssh_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                  |
| <a id="check_ssh_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                |
| <a id="check_ssh_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                        |
| <a id="check_ssh_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                  |
| <a id="check_ssh_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                      |
| <a id="check_ssh_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}             |
| <a id="check_ssh_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(list)                     |
| <a id="check_ssh_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No hosts checked                       |
| <a id="check_ssh_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${host}:${port} ${result} in ${time}ms |
| <a id="check_ssh_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${host}_${port}                        |
| <a id="check_ssh_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                        |
| <a id="check_ssh_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                        |
| <a id="check_ssh_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                     |
| <a id="check_ssh_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_ssh_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                              |
|------------------|----------------------------------------------------------------------------------------------------------|
| banner           | The raw SSH identification string, e.g. SSH-2.0-OpenSSH_9.6p1 Ubuntu-3ubuntu13.5                         |
| comments         | Trailing comments of the identification string, e.g. the distribution patch level                        |
| connected        | 1 when the connection succeeded, 0 otherwise                                                             |
| host             | Host the check connected to                                                                              |
| port             | TCP port the check connected to                                                                          |
| protocol         | SSH protocol version the server announced, e.g. 2.0 or 1.99                                              |
| protocol_major   | Major SSH protocol version as a number (2 for 2.0); use protocol_major < 2 to catch an SSHv1-only server |
| protocol_minor   | Minor SSH protocol version as a number (0 for 2.0, 99 for 1.99)                                          |
| response         | The data received from the peer (use with 'like'/'regexp' for custom matching)                           |
| result           | Textual result of the check (ok, refused, timeout, no_match, resolve_failed, ...)                        |
| software         | Software name from the version string, e.g. OpenSSH or dropbear                                          |
| software_version | Software version number from the version string, e.g. 9.6p1 or 2022.83                                   |
| time             | Connection time in milliseconds                                                                          |
| version          | Software version the server announced, e.g. OpenSSH_9.6p1                                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_tcp

Connect to a TCP port and optionally send/expect data to check that a service is reachable.

#### TLS certificate expiry (`ssl_expiry_days` / `has_certificate`)

When the connection is wrapped in TLS — `ssl=true`, or one of the implicit-TLS
service presets (`spop`, `simap`, `ssmtp`) — `check_tcp` reads the certificate
the peer serves and exposes it as two keywords: `ssl_expiry_days`, the whole
days until the certificate expires (**negative** once it has expired), and
`has_certificate`, `1` when the peer presented one.

This makes certificate monitoring work for any TLS service, not just HTTPS —
LDAPS, IMAPS, SMTPS, RDP, a database listener, or anything else that speaks TLS
on a port:

```
check_tcp host=ldap.example.com port=636 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10"
```

Two details worth knowing.

**The count is truncated, not rounded.** A certificate with 23 hours left reads
as `0`, not `1` — the remainder is dropped rather than rounded up into a
reassuring number.

**A missing certificate is not a number.** `ssl_expiry_days` is an *optional
number*: with no certificate it renders as `no certificate`, every numeric
comparison on it is false, and no perfdata is emitted. A bare
`crit=ssl_expiry_days < 30` is therefore safe — it cannot fire on a plain
connection, while an expired certificate still reports its real (negative) day
count and fires as it should. Test for the no-certificate state explicitly with
the string form, or with `has_certificate`:

```
check_tcp host=mail.example.com port=993 ssl=true "crit=ssl_expiry_days < 30 or ssl_expiry_days = 'no certificate'"
```

> **Upgrading.** `ssl_expiry_days` used to report `-1` for a connection with no
> certificate, which made a bare `crit=ssl_expiry_days < 30` fire on every plain
> connection. That sentinel is gone: filters written as `ssl_expiry_days = -1`
> must become `ssl_expiry_days = 'no certificate'` (or use `has_certificate`),
> and no expiry perfdata is emitted when there is no certificate. The same
> change applies to `check_http`'s `ssl_expiry_days`.

**Reading the certificate does not verify it.** The expiry is a property of what
the peer served, so it is available at the default `verify=none` — a
self-signed or otherwise untrusted certificate still reports its real remaining
lifetime. Use `verify=peer` with a `ca=` bundle when you want the chain checked
as well; the two are independent.

This complements the other two certificate checks: `check_http`'s
`ssl_expiry_days` covers HTTPS endpoints specifically, and `check_certificate`
inspects certificates at rest (files on disk, the Windows certificate store)
rather than ones served over a connection.

**Jump to section:**

* [Sample Commands](#check_tcp_samples)
* [Command-line Arguments](#check_tcp_options)
* [Filter keywords](#check_tcp_filter_keys)


<a id="check_tcp_samples"></a>
#### Sample Commands

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

**Use a service preset (`ftp`, `pop`, `imap`, `smtp`, `ssh`) — sets the port, greeting and expected-response regex:**

```
check_tcp host=mail.example.com service=smtp
OK: mail.example.com:25 ok in 8ms
```

**Wrap the connection in TLS with `ssl=true` (e.g. to test an HTTPS listener answers):**

```
check_tcp host=www.google.com port=443 ssl=true
OK: www.google.com:443 ok in 11ms|'www.google.com_443_time'=11;1000;5000
```

**Implicit-TLS service presets (`spop`, `simap`, `ssmtp`) connect over TLS and check the greeting:**

```
check_tcp host=smtp.gmail.com service=ssmtp
OK: smtp.gmail.com:465 ok in 16ms|'smtp.gmail.com_465_time'=16;1000;5000
```

**Match the peer's response with a regex via the `response` keyword:**

```
check_tcp host=mail.example.com port=25 "crit=response not regexp '^220'"
OK: mail.example.com:25 ok in 8ms
```

**Check how long the peer's TLS certificate is still valid (`ssl_expiry_days`):**

```
check_tcp host=secure.example.com port=443 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "top-syntax=${list}" "detail-syntax=cert expires in ${ssl_expiry_days} days"
OK: cert expires in 399 days|'secure.example.com_443_ssl_expiry_days'=399;30;10
```

```
check_tcp host=expiring.example.com port=443 ssl=true "warn=ssl_expiry_days < 30" "crit=ssl_expiry_days < 10" "top-syntax=${list}" "detail-syntax=cert expires in ${ssl_expiry_days} days"
WARNING: cert expires in 19 days|'expiring.example.com_443_ssl_expiry_days'=19;30;10
```

**A plain connection cannot trip the expiry threshold — and can be tested for explicitly:**

```
check_tcp host=mail.example.com port=110 "warn=none" "crit=ssl_expiry_days < 30"
OK: mail.example.com:110 ok in 1ms
```

```
check_tcp host=mail.example.com port=110 "warn=none" "crit=ssl_expiry_days = 'no certificate'"
CRITICAL: mail.example.com:110 ok in 0ms
```

**The certificate keywords also work through the implicit-TLS presets:**

```
check_tcp host=imap.example.com service=simap "top-syntax=${list}" "detail-syntax=${host}:${port} cert=${has_certificate} days=${ssl_expiry_days}"
OK: imap.example.com:993 cert=1 days=399
```

**Without TLS there is no certificate at all:**

```
check_tcp host=mail.example.com port=110 "top-syntax=${list}" "detail-syntax=cert=${has_certificate} days=${ssl_expiry_days}"
OK: cert=0 days=no certificate
```

**Verify the server certificate when using TLS (needs a CA bundle):**

```
check_tcp host=secure.example.com port=443 ssl=true verify=peer ca=/etc/ssl/certs/ca-certificates.crt
OK: secure.example.com:443 ok in 21ms
```

**Default check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_tcp --argument "host=192.168.56.1" --argument "port=22"
OK: All 1 hosts are ok|'192.168.56.1_22 time'=2ms;1000;5000
```




<a id="check_tcp_options"></a>
#### Command-line Arguments

<a id="check_tcp_host"></a>
<a id="check_tcp_hosts"></a>
<a id="check_tcp_port"></a>
<a id="check_tcp_send"></a>
<a id="check_tcp_expect"></a>
<a id="check_tcp_ca"></a>
<a id="check_tcp_address-family"></a>
<a id="check_tcp_service"></a>

| Option                                | Default Value | Description                                                                                                                                                          |
|---------------------------------------|---------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| host                                  |               | Host(s) to connect to (may be given multiple times).                                                                                                                 |
| hosts                                 |               | Comma separated list of hosts to connect to.                                                                                                                         |
| port                                  |               | TCP port to connect to.                                                                                                                                              |
| [timeout](#check_tcp_timeout)         | 5000          | Connection / read timeout in milliseconds.                                                                                                                           |
| send                                  |               | Optional payload to send after the connection is established.                                                                                                        |
| expect                                |               | Optional substring expected in the response.                                                                                                                         |
| [ssl](#check_tcp_ssl)                 | false         | Wrap the connection in TLS/SSL after connecting (ssl=true).                                                                                                          |
| [tls-version](#check_tcp_tls-version) | tlsv1.2+      | TLS version when --ssl is used (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                                                                |
| [verify](#check_tcp_verify)           | none          | Certificate verify mode when --ssl is used: none (default), peer, ... (peer requires --ca).                                                                          |
| ca                                    |               | CA bundle used to verify the server certificate when --ssl --verify peer is used.                                                                                    |
| address-family                        |               | IP version to use: any (default, let the resolver choose), ipv4 or ipv6. Accepts 4/v4/inet and 6/v6/inet6 as aliases.                                                |
| service                               |               | Service preset (ftp, pop, imap, smtp, ssh, spop, simap, ssmtp): sets a default port, greeting and expected-response regex. The s-prefixed variants use implicit TLS. |



<h5 id="check_tcp_timeout">timeout:</h5>

Connection / read timeout in milliseconds.

*Default Value:* `5000`

<h5 id="check_tcp_ssl">ssl:</h5>

Wrap the connection in TLS/SSL after connecting (ssl=true).

*Default Value:* `false`

<h5 id="check_tcp_tls-version">tls-version:</h5>

TLS version when --ssl is used (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_tcp_verify">verify:</h5>

Certificate verify mode when --ssl is used: none (default), peer, ... (peer requires --ca).

*Default Value:* `none`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                    | Default Value                          |
|-----------------------------------------------------------------------------------------------------------|----------------------------------------|
| <a id="check_tcp_filter"></a>[filter](../common-options.md#filter)                                        |                                        |
| <a id="check_tcp_warning"></a>[warning](../common-options.md#warning)                                     | time > 1000                            |
| <a id="check_tcp_warn"></a>[warn](../common-options.md#warn)                                              |                                        |
| <a id="check_tcp_critical"></a>[critical](../common-options.md#critical)                                  | time > 5000 or result != 'ok'          |
| <a id="check_tcp_crit"></a>[crit](../common-options.md#crit)                                              |                                        |
| <a id="check_tcp_ok"></a>[ok](../common-options.md#ok)                                                    |                                        |
| <a id="check_tcp_debug"></a>[debug](../common-options.md#debug)                                           | false                                  |
| <a id="check_tcp_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                  |
| <a id="check_tcp_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ignored                                |
| <a id="check_tcp_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                        |
| <a id="check_tcp_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                  |
| <a id="check_tcp_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                      |
| <a id="check_tcp_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}             |
| <a id="check_tcp_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): %(list)                     |
| <a id="check_tcp_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No hosts checked                       |
| <a id="check_tcp_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${host}:${port} ${result} in ${time}ms |
| <a id="check_tcp_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${host}_${port}                        |
| <a id="check_tcp_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                        |
| <a id="check_tcp_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                        |
| <a id="check_tcp_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                     |
| <a id="check_tcp_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_tcp_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                                                                                                                                                                                                                                 |
|-----------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| connected       | 1 when the connection succeeded, 0 otherwise                                                                                                                                                                                                                                                                                                |
| has_certificate | 1 when the peer presented a TLS certificate, 0 otherwise                                                                                                                                                                                                                                                                                    |
| host            | Host the check connected to                                                                                                                                                                                                                                                                                                                 |
| port            | TCP port the check connected to                                                                                                                                                                                                                                                                                                             |
| response        | The data received from the peer (use with 'like'/'regexp' for custom matching)                                                                                                                                                                                                                                                              |
| result          | Textual result of the check (ok, refused, timeout, no_match, resolve_failed, ...)                                                                                                                                                                                                                                                           |
| ssl_expiry_days | Whole days until the peer's TLS certificate expires; negative once it has expired. Renders as 'no certificate' (and compares false against every number) when the connection is not TLS or the peer presented none, so `ssl_expiry_days < 30` cannot fire on a plain connection; `ssl_expiry_days = 'no certificate'` tests for that state. |
| time            | Connection time in milliseconds                                                                                                                                                                                                                                                                                                             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_tomcat_status

Check an Apache Tomcat server via the manager status page (XML).

#### About `check_tomcat_status`

`check_tomcat_status` fetches Apache Tomcat's manager status page in XML form
(`/manager/status?XML=true`) and reports one record per connector plus the JVM
heap numbers. The `?XML=true` parameter is appended automatically when the URL
does not already carry it.

The manager application must be deployed and the account used needs the
`manager-status` role (or `manager-gui`, which includes it) in
`conf/tomcat-users.xml`; pass it with `username=` / `password=`.

One record is emitted per connector (e.g. `http-nio-8080`, `ajp-nio-8009`).
By default the check goes **warning** at 75% and **critical** at 90% thread
pool usage, and **critical** when the page cannot be fetched or parsed
(`result != 'ok'` — including `http_401` for missing credentials).

Use `filter=connector like 'http'` to scope the check to specific connectors.
The JVM heap keywords repeat on every connector record, so combine them with a
`filter` to avoid the same memory alert firing once per connector.

**Jump to section:**

* [Sample Commands](#check_tomcat_status_samples)
* [Command-line Arguments](#check_tomcat_status_options)
* [Filter keywords](#check_tomcat_status_filter_keys)


<a id="check_tomcat_status_samples"></a>
#### Sample Commands

**Check a Tomcat server via the manager status page (the `?XML=true` parameter is appended automatically):**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret
OK: http-nio-8080 ok: 4/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_thread_usage'=2;75;90 'http-nio-8080_threads_busy'=4;0;0 'ajp-nio-8009_thread_usage'=0;75;90 'ajp-nio-8009_threads_busy'=0;0;0
```

**The default thresholds alert when a connector's thread pool fills up (75%/90%):**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret
WARNING: http-nio-8080 ok: 160/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_thread_usage'=80;75;90 'http-nio-8080_threads_busy'=160;0;0 'ajp-nio-8009_thread_usage'=0;75;90 'ajp-nio-8009_threads_busy'=0;0;0
```

**Alert on request errors per connector:**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret "warning=error_count > 10"
WARNING: http-nio-8080 ok: 4/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_error_count'=17c;10;0 'http-nio-8080_thread_usage'=2;0;90 'http-nio-8080_threads_busy'=4;0;0 'ajp-nio-8009_error_count'=0c;10;0 'ajp-nio-8009_thread_usage'=0;0;90 'ajp-nio-8009_threads_busy'=0;0;0
```

**Alert on a shrinking JVM heap:**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status username=tomcat password=s3cret "critical=memory_free < 100000000"
OK: http-nio-8080 ok: 4/200 threads busy, ajp-nio-8009 ok: 0/100 threads busy|'http-nio-8080_memory_free'=1734127416B;0;100000000 ...
```

**Missing or wrong manager credentials are CRITICAL:**

```
check_tomcat_status url=http://127.0.0.1:8080/manager/status
CRITICAL:  http_401: 0/0 threads busy|'_thread_usage'=0%;75;90 '_threads_busy'=0;0;0
```



<a id="check_tomcat_status_options"></a>
#### Command-line Arguments

<a id="check_tomcat_status_username"></a>
<a id="check_tomcat_status_password"></a>

| Option                                          | Default Value                        | Description                                                                                                          |
|-------------------------------------------------|--------------------------------------|----------------------------------------------------------------------------------------------------------------------|
| [url](#check_tomcat_status_url)                 | http://127.0.0.1:8080/manager/status | URL of the status endpoint (http://host[:port]/path or https://...).                                                 |
| [timeout](#check_tomcat_status_timeout)         | 30                                   | Connection/read timeout in seconds.                                                                                  |
| username                                        |                                      | Username for HTTP Basic authentication.                                                                              |
| password                                        |                                      | Password for HTTP Basic authentication.                                                                              |
| [tls-version](#check_tomcat_status_tls-version) | tlsv1.2+                             | TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).                                         |
| [verify](#check_tomcat_status_verify)           | peer                                 | Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate. |
| [ca](#check_tomcat_status_ca)                   | ${ca-path}                           | Path to a CA bundle used to verify the server certificate.                                                           |



<h5 id="check_tomcat_status_url">url:</h5>

URL of the status endpoint (http://host[:port]/path or https://...).

*Default Value:* `http://127.0.0.1:8080/manager/status`

<h5 id="check_tomcat_status_timeout">timeout:</h5>

Connection/read timeout in seconds.

*Default Value:* `30`

<h5 id="check_tomcat_status_tls-version">tls-version:</h5>

TLS version for https (tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+, tlsv1.3, sslv3).

*Default Value:* `tlsv1.2+`

<h5 id="check_tomcat_status_verify">verify:</h5>

Certificate verify mode for https: none, peer, peer-cert, fail-if-no-cert, fail-if-no-peer-cert, client-certificate.

*Default Value:* `peer`

<h5 id="check_tomcat_status_ca">ca:</h5>

Path to a CA bundle used to verify the server certificate.

*Default Value:* `${ca-path}`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                                                       |
|---------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------|
| <a id="check_tomcat_status_filter"></a>[filter](../common-options.md#filter)                                        |                                                                     |
| <a id="check_tomcat_status_warning"></a>[warning](../common-options.md#warning)                                     | thread_usage > 75                                                   |
| <a id="check_tomcat_status_warn"></a>[warn](../common-options.md#warn)                                              |                                                                     |
| <a id="check_tomcat_status_critical"></a>[critical](../common-options.md#critical)                                  | result != 'ok' or thread_usage > 90                                 |
| <a id="check_tomcat_status_crit"></a>[crit](../common-options.md#crit)                                              |                                                                     |
| <a id="check_tomcat_status_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                     |
| <a id="check_tomcat_status_debug"></a>[debug](../common-options.md#debug)                                           | false                                                               |
| <a id="check_tomcat_status_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                               |
| <a id="check_tomcat_status_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                             |
| <a id="check_tomcat_status_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                     |
| <a id="check_tomcat_status_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                               |
| <a id="check_tomcat_status_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                   |
| <a id="check_tomcat_status_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                  |
| <a id="check_tomcat_status_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                     |
| <a id="check_tomcat_status_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No connectors found                                                 |
| <a id="check_tomcat_status_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${connector} ${result}: ${threads_busy}/${threads_max} threads busy |
| <a id="check_tomcat_status_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${connector}                                                        |
| <a id="check_tomcat_status_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                     |
| <a id="check_tomcat_status_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                     |
| <a id="check_tomcat_status_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                  |
| <a id="check_tomcat_status_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_tomcat_status_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                    |
|-----------------|------------------------------------------------------------------------------------------------|
| bytes_received  | Bytes received since start                                                                     |
| bytes_sent      | Bytes sent since start                                                                         |
| code            | HTTP status code of the response                                                               |
| connector       | Name of the connector (e.g. http-nio-8080)                                                     |
| error_count     | Requests that ended in an error since start                                                    |
| host            | Host part of the URL                                                                           |
| max_time        | Slowest request in ms since start                                                              |
| memory_free     | Free JVM heap in bytes                                                                         |
| memory_max      | Maximum JVM heap size in bytes                                                                 |
| memory_total    | Current JVM heap size in bytes                                                                 |
| port            | TCP port that was used                                                                         |
| processing_time | Total request processing time in ms since start                                                |
| request_count   | Requests served since start                                                                    |
| result          | Result of the check: ok, parse_error, http_<code> or error: <message>                          |
| thread_usage    | Busy threads as a percentage of the maximum thread pool size (0 when the pool size is unknown) |
| threads_busy    | Threads currently serving requests                                                             |
| threads_current | Threads currently alive in the pool                                                            |
| threads_max     | Maximum size of the thread pool                                                                |
| url             | Full URL that was requested                                                                    |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

