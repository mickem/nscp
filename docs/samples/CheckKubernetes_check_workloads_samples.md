**Check every deployment, statefulset and daemonset with the default thresholds:**

```
check_workloads
CRITICAL: Deployment shop/api=1/3, Deployment ops/legacy=0/2, StatefulSet shop/db=2/3, DaemonSet monitoring/node-exporter=3/4|'shop/web available'=2;0;0 'shop/web desired'=2;0;0 'shop/web missing'=0;0;0 'shop/api available'=1;0;0 'shop/api desired'=3;0;0 'shop/api missing'=2;0;0 'ops/legacy available'=0;0;0 'ops/legacy desired'=2;0;0 'ops/legacy missing'=2;0;0 'kube-system/coredns available'=2;0;0 'kube-system/coredns desired'=2;0;0 'kube-system/coredns missing'=0;0;0 'shop/db available'=2;0;0 'shop/db desired'=3;0;0 'shop/db missing'=1;0;0 'monitoring/node-exporter available'=3;0;0 'monitoring/node-exporter desired'=4;0;0 'monitoring/node-exporter missing'=1;0;0
```

**One kind in one namespace:**

```
check_workloads kind=deployment namespace=shop
WARNING: Deployment shop/api=1/3|'shop/web available'=2;0;0 'shop/web desired'=2;0;0 'shop/web missing'=0;0;0 'shop/api available'=1;0;0 'shop/api desired'=3;0;0 'shop/api missing'=2;0;0
```

**Require that specific workloads are fully available:**

```
check_workloads workload=shop/web workload=kube-system/coredns
OK: All 2 workloads are available|'shop/web available'=2;0;0 'shop/web desired'=2;0;0 'shop/web missing'=0;0;0 'kube-system/coredns available'=2;0;0 'kube-system/coredns desired'=2;0;0 'kube-system/coredns missing'=0;0;0
```

**Use the workload keywords in the output:**

```
check_workloads kind=daemonset "detail-syntax=%(kind) %(namespace)/%(name): %(available)/%(desired) available, %(updated) updated, %(missing) missing" "top-syntax=${status}: ${list}" ok-syntax=
WARNING: DaemonSet monitoring/node-exporter: 3/4 available, 4 updated, 1 missing|'monitoring/node-exporter available'=3;0;0 'monitoring/node-exporter desired'=4;0;0 'monitoring/node-exporter missing'=1;0;0
```
