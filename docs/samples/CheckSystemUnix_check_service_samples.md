**Check all services (the default watches enabled units for failures):**

```
check_service
OK: All 42 service(s) are ok.
```

**Check one service by name:**

```
check_service service=cron
OK: All 1 service(s) are ok.
```

**Show the mapped state, raw systemd state and vendor preset:**

```
check_service service=cron "top-syntax=${list}" "detail-syntax=${name}=${state} active=${active} preset=${preset}"
cron=running active=active preset=enabled
```

**A failed or stopped enabled service is CRITICAL:**

```
check_service service=nginx
CRITICAL: nginx=stopped
```

**Only alert on a specific service being down:**

```
check_service service=ssh "crit=state != 'running'"
OK: All 1 service(s) are ok.
```

**Alert on a service using too much memory (process metrics):**

```
check_service service=mysql "warn=rss > 1G" "crit=rss > 2G" "detail-syntax=${name} rss=${rss} cpu=${cpu}%"
OK: All 1 service(s) are ok.
```

**Check via NRPE:**

```
check_nscp_client --host 192.168.56.103 --command check_service --argument "service=docker"
OK: All 1 service(s) are ok.
```
