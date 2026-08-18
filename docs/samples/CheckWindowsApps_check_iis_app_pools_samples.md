**Check all IIS application pools (critical when an auto-start pool is not running):**

```
check_iis_app_pools
OK: DefaultAppPool (running)|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0
```

**A stopped pool that is configured to auto-start goes critical:**

```
check_iis_app_pools
CRITICAL: MyAppPool (disabled)|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0 'MyAppPool_uptime'=0s;0;0 'MyAppPool_recycles'=5c;0;0
```

**Detect recycle storms (a pool that keeps recycling):**

```
check_iis_app_pools "warning=recycles > 10" "critical=recycles > 50"
WARNING: MyAppPool (running)|'MyAppPool_recycles'=17c;10;50 'MyAppPool_uptime'=42s;0;0 ...
```

**Scope to one pool and show everything:**

```
check_iis_app_pools "filter=pool = 'DefaultAppPool'" show-all
OK: DefaultAppPool: running, uptime 86400s, 2 recycles|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_app_pools
IIS performance counters (APP_POOL_WAS) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```
