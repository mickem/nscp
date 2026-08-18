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
