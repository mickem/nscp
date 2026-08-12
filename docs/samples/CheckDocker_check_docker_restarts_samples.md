**Detect restart loops (default: WARNING when a container restarted more than 3 times and last started within 15 minutes; OOM kills are CRITICAL):**

```
check_docker_restarts
WARNING: crashy-app: 8 restarts, restarting
```

A stable container is OK no matter how bumpy its distant past:

```
check_docker_restarts container=app-backend
OK: app-backend: 0 restarts, running
```

**An out-of-memory kill is CRITICAL by default:**

```
check_docker_restarts container=greedy-app
CRITICAL: greedy-app: 2 restarts, exited
```

**Custom rules using the keywords (e.g. any restart of a specific container within the last hour):**

```
check_docker_restarts container=app-backend "warning=restart_count > 0 and started < 1h" "detail-syntax=%(names): %(restart_count) restarts, up %(started)s, exit=%(exit_code) oom=%(oom_killed)"
OK: app-backend: 0 restarts, up 236s, exit=0 oom=0|'app-backend restarts'=0;0;0
```
