**Check every pod with the default thresholds (finished job pods are ignored):**

```
check_pods
CRITICAL: shop/api-5f6c7d8b9-xyz12=CrashLoopBackOff, shop/db-0=Pending|'shop/web-7d4b9c6f8-k2xqz restarts'=0;5;0 'shop/web-7d4b9c6f8-p9lmn restarts'=0;5;0 'shop/api-5f6c7d8b9-xyz12 restarts'=7;5;0 'shop/db-0 restarts'=0;5;0 'kube-system/coredns-76f75df574-abcde restarts'=0;5;0 'kube-system/coredns-76f75df574-fghij restarts'=0;5;0
```

**Only one namespace, with your own thresholds:**

```
check_pods namespace=shop "warning=restarts > 3" "critical=pod_status like 'BackOff'"
CRITICAL: shop/api-5f6c7d8b9-xyz12=CrashLoopBackOff|'shop/web-7d4b9c6f8-k2xqz restarts'=0;3;0 'shop/web-7d4b9c6f8-p9lmn restarts'=0;3;0 'shop/api-5f6c7d8b9-xyz12 restarts'=7;3;0 'shop/db-0 restarts'=0;3;0
```

**Require that specific pods exist (a pod the API server does not know is CRITICAL):**

```
check_pods pod=shop/web-7d4b9c6f8-k2xqz pod=shop/db-0 pod=shop/search-0
CRITICAL: shop/db-0=Pending, shop/search-0=missing|'shop/web-7d4b9c6f8-k2xqz restarts'=0;5;0 'shop/db-0 restarts'=0;5;0 'shop/search-0 restarts'=0;5;0
```

**Select by label and show the pod keywords:**

```
check_pods label-selector=app=web "detail-syntax=%(namespace)/%(name) on %(node): %(pod_status) %(ready_containers)/%(containers) ready, %(restarts) restarts" "top-syntax=${status}: ${list}" ok-syntax=
OK: shop/web-7d4b9c6f8-k2xqz on worker-1: Running 1/1 ready, 0 restarts, shop/web-7d4b9c6f8-p9lmn on worker-3: Running 1/1 ready, 0 restarts|'shop/web-7d4b9c6f8-k2xqz restarts'=0;5;0 'shop/web-7d4b9c6f8-p9lmn restarts'=0;5;0
```

**A namespace the service account may not read is reported with the rule to grant (UNKNOWN):**

```
check_pods namespace=secret
Kubernetes API server at 'https://127.0.0.1:6443' denied GET /api/v1/namespaces/secret/pods?limit=500 (HTTP 403: pods is forbidden: User "system:serviceaccount:monitoring:nscp" cannot list resource "pods" in API group "" in the namespace "secret"): grant the agent's service account get and list on the resource (see the CheckKubernetes documentation for the ClusterRole)
```
