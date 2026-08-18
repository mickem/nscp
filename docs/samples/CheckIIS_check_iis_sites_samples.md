**Check all IIS web sites (critical when an auto-start site is stopped):**

```
check_iis_sites
OK: Default Web Site (running)|'Default Web Site_connections'=12;0;0
```

**A stopped site that is configured to auto-start goes critical:**

```
check_iis_sites
CRITICAL: MySite (stopped)|'Default Web Site_connections'=12;0;0 'MySite_connections'=0;0;0
```

**Alert on connection count per site:**

```
check_iis_sites "warning=connections > 500" "critical=connections > 1000"
OK: Default Web Site (running)|'Default Web Site_connections'=12;500;1000
```

**Measure request/byte rates (takes one extra second for the second sample):**

```
check_iis_sites averages=true "warning=requests_per_sec > 200" "detail-syntax=${site}: ${requests_per_sec} req/s, ${bytes_per_sec} B/s" show-all
OK: Default Web Site: 3.5 req/s, 1024.5 B/s|'Default Web Site_requests_per_sec'=3.5;200;0 'Default Web Site_connections'=12;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_sites
IIS performance counters (Web Service) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```
