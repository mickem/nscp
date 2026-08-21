**Require that specific containers are running (a missing or stopped container is CRITICAL):**

```
check_docker container=web-frontend
OK: web-frontend=running
```

```
check_docker container=web-frontend container=backup-agent
CRITICAL: backup-agent=missing
```

**List all running containers (inventory-style):**

```
check_docker
OK: web-frontend=running, database=running
```

**Include stopped containers (`docker ps -a`) — any non-running container trips the default critical:**

```
check_docker all=true
CRITICAL: old-job=exited
```

**Use the container keywords in the output:**

```
check_docker container=web-frontend "detail-syntax=%(names): %(image) %(container_status) ports=%(ports)" "top-syntax=${status}: ${list}"
OK: web-frontend: nginx:alpine Up 2 hours ports=0.0.0.0:18080->80/tcp,:::18080->80/tcp
```

**Alert on failing container health checks instead of state:**

```
check_docker "filter=has_health_check = 1" "warning=health = 'starting'" "critical=health = 'unhealthy'" "detail-syntax=%(names)=%(health)"
OK: web-frontend=healthy
```

**A daemon that is down is clearly reported (UNKNOWN):**

```
check_docker host=/var/run/missing.sock
Failed to connect to docker daemon at '/var/run/missing.sock': Failed to connect to /var/run/missing.sock: No such file or directory
```
