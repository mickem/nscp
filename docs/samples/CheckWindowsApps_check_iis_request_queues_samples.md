**Check the HTTP.sys request queues (defaults: warn above 800 queued, critical above 1000):**

```
check_iis_request_queues
OK: DefaultAppPool (0 queued)|'DefaultAppPool_rejected'=0c;0;0
```

**A backed-up queue trips the defaults (1000 is HTTP.sys' default queue limit):**

```
check_iis_request_queues
WARNING: DefaultAppPool (912 queued)|'DefaultAppPool_queue_length'=912;800;1000 'DefaultAppPool_rejected'=0c;0;0
```

**Alert on rejected requests instead (503s served straight from HTTP.sys):**

```
check_iis_request_queues "warning=rejected > 0" "critical=queue_length > 1000"
WARNING: DefaultAppPool (14 queued)|'DefaultAppPool_rejected'=27c;0;0 'DefaultAppPool_queue_length'=14;0;1000
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_request_queues
IIS performance counters (HTTP Service Request Queues) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```
