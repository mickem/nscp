**Check that the API server is reachable and ready:**

```
check_kubernetes
OK: Kubernetes v1.30.4 at https://127.0.0.1:6443: API ready, 3/4 nodes ready
```

**Alert on nodes that are not Ready, with perf data:**

```
check_kubernetes "warning=nodes_not_ready > 0" "critical=nodes_ready < 2"
WARNING: Kubernetes v1.30.4 at https://127.0.0.1:6443: API ready, 3/4 nodes ready|'https://127.0.0.1:6443 not ready nodes'=1;0;0 'https://127.0.0.1:6443 ready nodes'=3;0;2
```

**Use the keywords in the output:**

```
check_kubernetes "detail-syntax=%(version) on %(platform), source %(source), readyz %(readyz)"
OK: v1.30.4 on linux/amd64, source settings, readyz ok
```

**An API server that is down is clearly reported (UNKNOWN):**

```
check_kubernetes
Failed to connect to Kubernetes API server at 'https://127.0.0.1:1' (settings): Failed to connect to 127.0.0.1:1: Connection refused
```
