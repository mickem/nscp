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
