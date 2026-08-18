#### About `check_iis_sites`

`check_iis_sites` reports one record per IIS web site from the "Web Service"
performance counters (current connections, uptime, request/byte rates). When
the IIS WMI provider is installed, records are enriched from
`root\WebAdministration`: sites gain their `ServerAutoStart` flag
(`auto_start`) and sites with no counter instance (stopped) are still listed.

A site is considered `stopped` when the Web Service counters report no uptime
for it. The default **critical** expression is
`state = 'stopped' and auto_start != 0` — an auto-start site that is not
serving alerts, an intentionally stopped one (`auto_start` = 0) stays quiet.
Without the WMI provider `auto_start` is `-1`, so every stopped site alerts.

The rate keywords need two counter samples: pass `averages=true` to collect a
second sample after one second (the check then takes a second longer);
without it `requests_per_sec`/`bytes_per_sec` read 0.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword            | Description                                                    |
|--------------------|----------------------------------------------------------------|
| `site`             | Name of the web site                                           |
| `state`            | `running` or `stopped`                                         |
| `connections`      | Current connections to the site                                |
| `uptime`           | Seconds the site has been up (0 when stopped)                  |
| `requests_per_sec` | Requests/second (`averages=true` required)                     |
| `bytes_per_sec`    | Bytes sent+received/second (`averages=true` required)          |
| `auto_start`       | `1`/`0` from the site configuration, `-1` when WMI is unavailable |
