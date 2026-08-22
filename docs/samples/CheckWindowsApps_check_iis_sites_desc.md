#### About `check_iis_sites`

`check_iis_sites` reports one record per IIS web site from the "Web Service"
performance counters (current connections, uptime, request/byte rates). When
the IIS WMI provider is installed, records are enriched from
`root\WebAdministration`: sites gain their `ServerAutoStart` flag
(`auto_start`) and sites with no counter instance (stopped) are still listed.

A site is considered `stopped` when it has no Web Service counter instance
(the counters only exist for started sites, so stopped sites are surfaced via
the WMI enrichment). The default **critical** expression is
`state = 'stopped' and auto_start != 0` — an auto-start site that is not
serving alerts, an intentionally stopped one (`auto_start` = 0) stays quiet.
Without the WMI provider `auto_start` is `-1`, so every stopped site alerts.

The rate keywords need two counter samples: pass `averages=true` to collect a
second sample after one second (the check then takes a second longer);
without it `requests_per_sec`/`bytes_per_sec` read 0.
