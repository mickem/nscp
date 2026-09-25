# Kubernetes Cluster Monitoring

**Goal:** Monitor a Kubernetes cluster from NSClient++ - is the API server
reachable, are the nodes Ready, are the pods healthy, do the workloads have the
replicas they want - using the `CheckKubernetes` module, which talks to the
cluster's API server over HTTPS with a service account token and needs no
`kubectl` on the host.

!!! note "Experimental"
    `CheckKubernetes` is new and marked experimental: the checks work, but
    option names, keywords and output may still change while they are being
    used against real clusters. Pin thresholds you rely on in your service
    definitions and read the release notes when upgrading.

---

## Prerequisites

Enable the module in `nsclient.ini`:

```ini
[/modules]
CheckKubernetes = enabled
NRPEServer      = enabled   ; or WEBServer, whichever your monitoring server uses
```

Or activate it from the command line:

```
nscp settings --activate-module CheckKubernetes
```

The agent needs a way to reach the API server and an identity the cluster
accepts. Two deployment shapes cover most setups.

---

## Deployment mode 1: the agent outside the cluster

Run NSClient++ on a bastion host, a management VM or one of the nodes, and
give it a service account token. Create the account and a read-only
`ClusterRole` for it:

```yaml
apiVersion: v1
kind: ServiceAccount
metadata:
  name: nscp
  namespace: monitoring
---
apiVersion: rbac.authorization.k8s.io/v1
kind: ClusterRole
metadata:
  name: nscp-monitoring
rules:
  - apiGroups: [""]
    resources: [pods, nodes, namespaces, events, persistentvolumeclaims]
    verbs: [get, list]
  - apiGroups: [apps]
    resources: [deployments, statefulsets, daemonsets]
    verbs: [get, list]
  - apiGroups: [batch]
    resources: [jobs, cronjobs]
    verbs: [get, list]
  - apiGroups: [metrics.k8s.io]
    resources: [pods, nodes]
    verbs: [get, list]
  - nonResourceURLs: ["/version", "/readyz", "/livez"]
    verbs: [get]
---
apiVersion: rbac.authorization.k8s.io/v1
kind: ClusterRoleBinding
metadata:
  name: nscp-monitoring
roleRef:
  apiGroup: rbac.authorization.k8s.io
  kind: ClusterRole
  name: nscp-monitoring
subjects:
  - kind: ServiceAccount
    name: nscp
    namespace: monitoring
```

The role grants `get` and `list` and nothing else; it already covers the
resources the later checks (jobs, events, claims, metrics-server usage) will
read, so it does not need to be revisited per release.

Mint a token and fetch the cluster CA:

```
kubectl -n monitoring create token nscp --duration=8760h > /etc/nsclient/k8s-token
kubectl config view --raw -o jsonpath='{.clusters[0].cluster.certificate-authority-data}' | base64 -d > /etc/nsclient/k8s-ca.pem
chmod 600 /etc/nsclient/k8s-token
```

Then point the module at the cluster:

```ini
[/settings/kubernetes]
api server = https://k8s.example.com:6443
token file = /etc/nsclient/k8s-token
ca = /etc/nsclient/k8s-ca.pem
```

`token file` is read at check time, so replacing the file when the token is
rotated needs no reload. A `token = ...` key holds the token inline instead
(it is registered as a password and redacted in settings listings). If you
already have a kubeconfig for a monitoring user, the module reads it in JSON
form instead of the three keys above:

```
kubectl config view --raw --minify -o json > /etc/nsclient/kubeconfig.json
```

```ini
[/settings/kubernetes]
kubeconfig = /etc/nsclient/kubeconfig.json
```

YAML kubeconfigs are not read (the agent carries no YAML parser), and
`exec`-style credential plugins are not supported: use a token or a client
certificate.

---

## Deployment mode 2: the agent inside the cluster

Run NSClient++ as a pod (a `DaemonSet` for per-node checks, or a single
`Deployment` for cluster-level ones) with the `nscp` service account from
above. Leave `api server` empty: the module detects `KUBERNETES_SERVICE_HOST`
and uses the token and CA the kubelet projects under
`/var/run/secrets/kubernetes.io/serviceaccount/`.

```yaml
spec:
  serviceAccountName: nscp
  automountServiceAccountToken: true
```

Nothing else needs to be configured; `check_kubernetes` reports
`source in-cluster` when this path is in use.

---

## Whose cluster is it? (security)

The cluster is chosen once, by the operator, in `[/settings/kubernetes]`.
None of the check commands accepts a `url=`, `host=` or `token=` argument,
so a caller who can run checks over the REST API (anyone holding
`queries.execute`) cannot redirect the agent - and the bearer token it sends
- to a server of their choosing. The token never appears in check output,
error text or the log; a 401 is reported as "rejected the credentials", a
403 with the API server's own explanation of which resource the service
account may not read.

Keep `verify mode = peer` (the default): with `none` the token is sent to
whichever server answers on that address.

---

## Cluster Health

`check_kubernetes` reads `/version`, `/readyz` and the node list:

```
check_kubernetes
OK: Kubernetes v1.30.4 at https://k8s.example.com:6443: API ready, 3/4 nodes ready
```

It goes CRITICAL when `/readyz` reports a failing check (the `readyz`
keyword names it) and UNKNOWN when the server cannot be reached. To be told
about NotReady nodes here as well:

```
check_kubernetes "warning=nodes_not_ready > 0"
```

---

## Nodes

`check_nodes` is critical for a node that is not Ready and warns on memory,
disk or PID pressure and on a cordoned node:

```
check_nodes
CRITICAL: worker-2=NotReady, worker-3=Ready,SchedulingDisabled
```

If drained nodes are routine during maintenance, hide them:

```
check_nodes "filter=schedulable = 1"
```

Capacity keywords take units in thresholds:

```
check_nodes "warning=memory_allocatable < 8G" "critical=ready != 'True'"
```

---

## Pods

`check_pods` shows the same STATUS column as `kubectl get pods`, so a crash
loop is not hidden behind phase `Running`:

```
check_pods
CRITICAL: shop/api-5f6c7d8b9-xyz12=CrashLoopBackOff, shop/db-0=Pending
```

One service per namespace or per application keeps alerts attributable:

```
check_pods namespace=shop "warning=restarts > 3" "critical=pod_status like 'BackOff'"
check_pods label-selector=app=web
```

And a "must exist" check for the pods you cannot do without:

```
check_pods pod=shop/db-0 pod=shop/search-0
CRITICAL: shop/db-0=Pending, shop/search-0=missing
```

---

## Workloads

`check_workloads` compares desired with available replicas across
deployments, statefulsets and daemonsets:

```
check_workloads
CRITICAL: Deployment shop/api=1/3, Deployment ops/legacy=0/2, StatefulSet shop/db=2/3, DaemonSet monitoring/node-exporter=3/4
```

Nothing available of a non-zero desired count is critical; missing replicas
or an unfinished rollout is a warning. Restrict by kind and namespace to keep
the calls cheap:

```
check_workloads kind=deployment namespace=shop
```

---

## Nagios / Icinga service definitions

Over NRPE:

```
define command {
    command_name    check_nrpe_k8s
    command_line    $USER1$/check_nrpe -H $HOSTADDRESS$ -c $ARG1$ -a $ARG2$
}

define service {
    use                 generic-service
    host_name           k8s-bastion
    service_description Kubernetes API
    check_command       check_nrpe_k8s!check_kubernetes!"warning=nodes_not_ready > 0"
}

define service {
    use                 generic-service
    host_name           k8s-bastion
    service_description Kubernetes nodes
    check_command       check_nrpe_k8s!check_nodes!"filter=schedulable = 1"
}

define service {
    use                 generic-service
    host_name           k8s-bastion
    service_description Pods: shop
    check_command       check_nrpe_k8s!check_pods!namespace=shop
}

define service {
    use                 generic-service
    host_name           k8s-bastion
    service_description Workloads
    check_command       check_nrpe_k8s!check_workloads!"warning=missing > 0"
}
```

Remember `allow arguments = true` (and, for the quotes and `>` in thresholds,
`allow nasty characters = true`) under `[/settings/NRPE/server]`, or define
the checks as aliases in `[/settings/external scripts/alias]` and call the
alias without arguments.

Icinga 2, using the `nrpe` CheckCommand:

```
apply Service "k8s-pods-shop" {
  check_command = "nrpe"
  vars.nrpe_command = "check_pods"
  vars.nrpe_arguments = [ "namespace=shop", "warning=restarts > 3" ]
  assign where host.vars.k8s_bastion
}
```

---

## Customisation

* **Timeouts.** `timeout` under `[/settings/kubernetes]` (default 30 s) bounds
  each API request; every command also takes `timeout=` per call.
* **Large clusters.** Lists are paged 500 objects at a time. `max response
  size` (default 64 MB) caps a single page in memory; prefer `namespace=`,
  `label-selector=` and `kind=` to keep the payload small rather than raising
  it.
* **Private CAs.** Point `ca` at the cluster CA (a PEM file or a hashed
  directory); for a self-signed API server certificate use
  `verify mode = peer-cert` with `ca` set to that certificate.
* **Behind a proxying URL.** `api server` may carry a path prefix
  (`https://rancher.example.com/k8s/clusters/c-m-abc`); it is prepended to
  every API path.

---

## Next steps

- [CheckKubernetes reference](../reference/check/CheckKubernetes.md) - every option and keyword of the four commands.
- [Active Monitoring with NRPE](nrpe.md) - how the monitoring server polls the agent.
- [Prometheus Scraping](prometheus.md) - if you would rather scrape the agent's metrics than poll checks.
