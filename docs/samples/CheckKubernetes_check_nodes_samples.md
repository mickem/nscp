**Check every node with the default thresholds (NotReady is critical, pressure or a cordon a warning):**

```
check_nodes
CRITICAL: worker-2=NotReady, worker-3=Ready,SchedulingDisabled
```

**Only alert on readiness, ignoring cordoned nodes:**

```
check_nodes warning=none "critical=ready != 'True'"
CRITICAL: worker-2=NotReady
```

**Require that specific nodes are still in the cluster:**

```
check_nodes node=worker-1 node=worker-9
CRITICAL: worker-9=missing
```

**Show what a node offers, using the node keywords:**

```
check_nodes node=worker-1 "detail-syntax=%(name): %(node_status), kubelet %(kubelet_version), %(os) %(arch), cpu %(cpu_allocatable)m of %(cpu_capacity)m, memory %(memory_allocatable) of %(memory_capacity) bytes, %(pods_capacity) pods, roles=%(roles) taints=%(taints)" "top-syntax=${list}" ok-syntax=
worker-1: Ready, kubelet v1.30.4, Ubuntu 22.04.4 LTS amd64, cpu 7900m of 8000m, memory 33285996544 of 34359738368 bytes, 110 pods, roles= taints=
```

**List cordoned nodes without alerting:**

```
check_nodes "filter=schedulable = 0" warning=none critical=none "detail-syntax=%(name)=%(node_status)" "top-syntax=${status}: ${list}" ok-syntax=
OK: worker-3=Ready,SchedulingDisabled
```
