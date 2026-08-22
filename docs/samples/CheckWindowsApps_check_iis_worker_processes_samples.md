**Check the running IIS worker processes (w3wp):**

```
check_iis_worker_processes
OK: DefaultAppPool (pid 4711, 2 active)|'DefaultAppPool_4711_active_requests'=2;0;0
```

**No workers is a normal state (idle pools spin down), so the empty set is OK:**

```
check_iis_worker_processes
OK: No IIS worker processes running
```

**Alert when requests pile up inside a worker:**

```
check_iis_worker_processes "warning=active_requests > 50" "critical=active_requests > 200"
WARNING: DefaultAppPool (pid 4711, 73 active)|'DefaultAppPool_4711_active_requests'=73;50;200
```

**Scope to one pool's workers:**

```
check_iis_worker_processes "filter=pool = 'DefaultAppPool'" show-all
OK: DefaultAppPool (pid 4711): 2 active requests|'DefaultAppPool_4711_active_requests'=2;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_worker_processes
IIS performance counters (W3SVC_W3WP) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```
