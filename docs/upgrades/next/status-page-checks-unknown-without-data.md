---
icon: "📊"
modules: [CheckNet]
action: conditional
---
**Web server status checks report UNKNOWN, not CRITICAL, when they get no
data.** `check_apache_status`, `check_nginx_status`, `check_phpfpm_status`
and `check_tomcat_status` used to answer a fetch failure (`invalid_url`,
`http_<code>`, `error: ...`) or a page they could not parse (`parse_error`)
by evaluating a record whose values were all 0. The default `critical`
(`result != 'ok'`) turned that into CRITICAL, and the zeros went out as
performance data: a counter such as `total_accesses`, `requests` or
`request_count` graphed as a reset, and a "below" threshold such as
`idle_workers < 5` fired on a value the server never reported. They now return
UNKNOWN with `<url>: <reason>` as the message and no performance data:

| Situation | Before | Now |
|---|---|---|
| Server down | `CRITICAL: error: Failed to connect ...: 0 active (...)\|'127.0.0.1_active'=0;0;0` | `UNKNOWN: http://127.0.0.1:81/nginx_status: error: Failed to connect ...` |
| Missing Tomcat manager credentials | `CRITICAL:  http_401: 0/0 threads busy\|...` | `UNKNOWN: http://127.0.0.1:8080/manager/status: http_401` |
| Not a status page | `CRITICAL: parse_error: ...\|...` | `UNKNOWN: <url>: parse_error` |

The `result` keyword is kept, but a reported record now always carries `ok`,
so a `critical=` or `warning=` that tests `result` no longer fires.

**What to do:** only if you alert on these checks being unreachable. An
unreachable or misconfigured status endpoint now raises UNKNOWN instead of
CRITICAL: make sure your monitoring notifies on UNKNOWN for these services,
or it will stay quiet when the web server goes down.
