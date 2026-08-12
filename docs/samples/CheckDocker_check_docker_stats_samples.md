**Check resource usage of a specific container:**

```
check_docker_stats container=app-backend
OK: app-backend: cpu 2%, memory 45.2MB of 256MB (17%)
```

**Alert on memory pressure or CPU saturation (thresholded keywords become perf data):**

```
check_docker_stats container=app-backend "warning=memory_pct > 80" "critical=memory_pct > 95"
OK: app-backend: cpu 2%, memory 45.2MB of 256MB (17%)|'app-backend memory %'=17%;80;95
```

**Absolute memory thresholds take byte units (`1b`, `64k`, `200M`, `1G`, ...):**

```
check_docker_stats container=app-backend "critical=memory_used > 200M"
OK: app-backend: cpu 2%, memory 45.2MB of 256MB (17%)|'app-backend memory'=47401984B;0;209715200
```

**Sample every running container (about a second per container, so scope on busy hosts):**

```
check_docker_stats "warning=cpu_pct > 80"
OK: web-frontend: cpu 1%, memory 12.4MB of 15.35GB (0%), app-backend: cpu 2%, memory 45.2MB of 256MB (17%)
```
