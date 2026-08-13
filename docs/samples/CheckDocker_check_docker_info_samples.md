**Check that the docker daemon itself is up and responding:**

```
check_docker_info
OK: docker 29.5.3 on docker-host: 16 running, 0 paused, 4 stopped containers, 231 images
```

**Alert when nothing is running (or too much is stopped), with perf data:**

```
check_docker_info "warning=running < 1" "critical=stopped > 100"
OK: docker 29.5.3 on docker-host: 16 running, 0 paused, 4 stopped containers, 231 images|'docker-host running'=16;1;0 'docker-host stopped'=4;0;100
```

**A daemon that is down is clearly reported (UNKNOWN):**

```
check_docker_info host=/var/run/missing.sock
Failed to connect to docker daemon at '/var/run/missing.sock': Failed to connect to /var/run/missing.sock: No such file or directory
```
