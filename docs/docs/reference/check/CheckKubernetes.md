# CheckKubernetes

*Available on Windows only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

Check a Kubernetes cluster through its API server: cluster health, pods, nodes and workloads.

## Enable module

To enable this module and allow using the commands you need to add `CheckKubernetes = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckKubernetes = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckKubernetes module.

**List of commands:**

A list of all available queries (check commands)

| Command                                                | Description                                                                                      |
|--------------------------------------------------------|--------------------------------------------------------------------------------------------------|
| [check_kubernetes](#check_kubernetes) *(experimental)* | Check that the Kubernetes API server is reachable and ready: version, readiness and node counts. |
| [check_nodes](#check_nodes) *(experimental)*           | Check node readiness, pressure conditions, schedulability and capacity.                          |
| [check_pods](#check_pods) *(experimental)*             | Check the state of pods: phase, the kubectl STATUS column, readiness and restarts.               |
| [check_workloads](#check_workloads) *(experimental)*   | Check that deployments, statefulsets and daemonsets have their desired replicas available.       |

### check_kubernetes

Check that the Kubernetes API server is reachable and ready: version, readiness and node counts.

#### About `check_kubernetes`

`check_kubernetes` is the "is the cluster there" check: it reads `/version`,
`/readyz` and `/api/v1/nodes` from the API server configured under
`[/settings/kubernetes]` and reports the Kubernetes version, whether the API
server considers itself ready, and how many nodes are Ready.

Reaching the API is the health signal. The default critical is
`api_ready = 0`, which trips when `/readyz` answers with a failing check (the
`readyz` keyword then names it, e.g. `failed: etcd`); an unreachable server, a
rejected token or a missing RBAC rule are reported as UNKNOWN with the reason
(see below). That holds for `/readyz` too: a service account without the
`nonResourceURLs: ["/readyz"]` rule gets an UNKNOWN naming the rule, not a
CRITICAL about a healthy cluster. Only a readiness report counts as a verdict
(`ok`, or the API server's own 5xx listing its checks); a 404 or 502 from an
ingress that does not forward the path leaves `api_ready` at 1 and shows
`readyz` as `unavailable (HTTP 404)`, since `/version` did answer. Node counts carry no default threshold, so add
`warning=nodes_not_ready > 0` when a NotReady node should show up here rather
than in `check_nodes`.

The cluster is chosen by the operator, once, in the settings. No command in
this module takes a `url=`, `host=` or `token=` argument: a caller who can run
checks over REST (anyone holding `queries.execute`) must not be able to point
the agent - and the bearer token it sends - at a server of their choosing. The
token itself never appears in check output, error text or the log.

Error messages are meant to be acted on:

| Situation                                | Result  | Message starts with                                                         |
|------------------------------------------|---------|------------------------------------------------------------------------------|
| Nothing configured                       | UNKNOWN | `No Kubernetes API server configured: set `api server` and `token` ...`       |
| TCP/TLS failure                          | UNKNOWN | `Failed to connect to Kubernetes API server at 'https://...'`                |
| HTTP 401                                 | UNKNOWN | `... rejected the credentials (HTTP 401 ...)`, naming the credential sent     |
| HTTP 403                                 | UNKNOWN | `... denied GET /api/v1/... (HTTP 403: <the API server's own message>)`       |

**Jump to section:**

* [Sample Commands](#check_kubernetes_samples)
* [Command-line Arguments](#check_kubernetes_options)
* [Filter keywords](#check_kubernetes_filter_keys)


<a id="check_kubernetes_samples"></a>
#### Sample Commands

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



<a id="check_kubernetes_options"></a>
#### Command-line Arguments

        
| Option                               | Default Value | Description                                                                                                                                                                                                            |
|--------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| [timeout](#check_kubernetes_timeout) | 30            | Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number. |



<h5 id="check_kubernetes_timeout">timeout:</h5>

Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number.

*Default Value:* `30`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                                                                             |
|------------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------|
| <a id="check_kubernetes_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                           |
| <a id="check_kubernetes_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                           |
| <a id="check_kubernetes_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                           |
| <a id="check_kubernetes_critical"></a>[critical](../common-options.md#critical)                                  | api_ready = 0                                                                             |
| <a id="check_kubernetes_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                           |
| <a id="check_kubernetes_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                           |
| <a id="check_kubernetes_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                     |
| <a id="check_kubernetes_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                     |
| <a id="check_kubernetes_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                   |
| <a id="check_kubernetes_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                           |
| <a id="check_kubernetes_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                     |
| <a id="check_kubernetes_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                         |
| <a id="check_kubernetes_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                        |
| <a id="check_kubernetes_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                           |
| <a id="check_kubernetes_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No cluster information returned                                                |
| <a id="check_kubernetes_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | Kubernetes ${version} at ${server}: API ${api_ready}, ${nodes_ready}/${nodes} nodes ready |
| <a id="check_kubernetes_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${server}                                                                                 |
| <a id="check_kubernetes_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                           |
| <a id="check_kubernetes_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                           |
| <a id="check_kubernetes_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                        |
| <a id="check_kubernetes_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                           |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_kubernetes_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                                                                                                                                              |
|-----------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| api_ready       | 1 unless /readyz carries a readiness report saying the API server is not ready (renders as ready / not ready); a path that is unavailable (a 404 or 502 from an ingress) leaves it 1 - /version answered |
| nodes           | Number of nodes in the cluster                                                                                                                                                                           |
| nodes_not_ready | Number of nodes whose Ready condition is False or Unknown                                                                                                                                                |
| nodes_ready     | Number of nodes whose Ready condition is True                                                                                                                                                            |
| platform        | Platform the API server runs on (e.g. linux/amd64)                                                                                                                                                       |
| readyz          | What /readyz answered: ok, the failing checks it listed, or unavailable (HTTP n) when the path did not answer with a readiness report                                                                    |
| server          | The API server address (never the credential)                                                                                                                                                            |
| source          | Where the cluster configuration came from: settings, kubeconfig <path> or in-cluster                                                                                                                     |
| version         | Kubernetes version reported by /version (e.g. v1.30.2)                                                                                                                                                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nodes

Check node readiness, pressure conditions, schedulability and capacity.

#### About `check_nodes`

`check_nodes` reads `/api/v1/nodes` and evaluates each node: the `Ready`
condition, the pressure conditions (`memory_pressure`, `disk_pressure`,
`pid_pressure`, `network_unavailable`), whether it is cordoned
(`schedulable`), and what it offers - `cpu_capacity` / `cpu_allocatable` in
millicores, `memory_capacity` / `memory_allocatable` in bytes (thresholds take
units: `memory_allocatable < 4G`) and `pods_capacity`.

`node_status` is the STATUS column of `kubectl get nodes`: `Ready`,
`NotReady` or `Unknown`, with `,SchedulingDisabled` appended for a cordoned
node. The defaults go critical on `ready != 'True'` and warn on any pressure
condition or a cordon; a drained node during maintenance therefore shows up as
a warning, which is usually what you want - add `filter=schedulable = 1` to
hide it.

`node=<name>` (repeatable) restricts the check to the named nodes, and a node
the API server does not return is reported as `missing`: it left the cluster,
which is exactly what a per-node check is there to tell you. `label-selector=`
and `field-selector=` are passed to the API server, e.g.
`label-selector=node-role.kubernetes.io/worker=`.

**Jump to section:**

* [Sample Commands](#check_nodes_samples)
* [Command-line Arguments](#check_nodes_options)
* [Filter keywords](#check_nodes_filter_keys)


<a id="check_nodes_samples"></a>
#### Sample Commands

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



<a id="check_nodes_options"></a>
#### Command-line Arguments

<a id="check_nodes_label-selector"></a>
<a id="check_nodes_field-selector"></a>
<a id="check_nodes_node"></a>

        
        
        
        
| Option                          | Default Value | Description                                                                                                                                                                                                            |
|---------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| label-selector                  |               | Label selector passed to the API server, e.g. node-role.kubernetes.io/worker= or topology.kubernetes.io/zone=eu-1a.                                                                                                    |
| field-selector                  |               | Field selector passed to the API server, e.g. metadata.name=worker-1.                                                                                                                                                  |
| node                            |               | Name of a node that must exist (repeatable). Only the named nodes take part in the check; a name the API server does not return gets node_status 'missing'.                                                            |
| [timeout](#check_nodes_timeout) | 30            | Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number. |



<h5 id="check_nodes_timeout">timeout:</h5>

Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number.

*Default Value:* `30`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                      | Default Value                                                                                              |
|-------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------|
| <a id="check_nodes_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                            |
| <a id="check_nodes_warning"></a>[warning](../common-options.md#warning)                                     | memory_pressure = 1 or disk_pressure = 1 or pid_pressure = 1 or network_unavailable = 1 or schedulable = 0 |
| <a id="check_nodes_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                            |
| <a id="check_nodes_critical"></a>[critical](../common-options.md#critical)                                  | ready != 'True'                                                                                            |
| <a id="check_nodes_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                            |
| <a id="check_nodes_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                            |
| <a id="check_nodes_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                      |
| <a id="check_nodes_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                      |
| <a id="check_nodes_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | critical                                                                                                   |
| <a id="check_nodes_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                            |
| <a id="check_nodes_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                      |
| <a id="check_nodes_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                          |
| <a id="check_nodes_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                                                                                 |
| <a id="check_nodes_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) nodes are ready                                                                    |
| <a id="check_nodes_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No nodes found                                                                                  |
| <a id="check_nodes_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}=${node_status}                                                                                     |
| <a id="check_nodes_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                                                    |
| <a id="check_nodes_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                            |
| <a id="check_nodes_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                            |
| <a id="check_nodes_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                         |
| <a id="check_nodes_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_nodes_filter_keys"></a>
#### Filter keywords

| Option              | Description                                                                                                                                                                   |
|---------------------|-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| age                 | Seconds since the node joined the cluster, -1 when unknown (supports units, e.g. age < 1h)                                                                                    |
| arch                | CPU architecture (amd64, arm64, ...)                                                                                                                                          |
| cpu_allocatable     | CPU available to pods in millicores (-1 when not reported)                                                                                                                    |
| cpu_capacity        | CPU capacity in millicores (-1 when not reported)                                                                                                                             |
| created             | When the node joined the cluster (date)                                                                                                                                       |
| disk_pressure       | 1 when the DiskPressure condition is True, else 0                                                                                                                             |
| internal_ip         | The node's InternalIP address                                                                                                                                                 |
| kubelet_version     | Kubelet version (e.g. v1.30.2)                                                                                                                                                |
| memory_allocatable  | Memory available to pods in bytes, -1 when not reported (thresholds take units, e.g. memory_allocatable < 4G)                                                                 |
| memory_capacity     | Memory capacity in bytes, -1 when not reported (thresholds take units, e.g. memory_capacity < 8G)                                                                             |
| memory_pressure     | 1 when the MemoryPressure condition is True, else 0                                                                                                                           |
| name                | Node name                                                                                                                                                                     |
| network_unavailable | 1 when the NetworkUnavailable condition is True, else 0                                                                                                                       |
| node_status         | The kubectl STATUS column: Ready, NotReady or Unknown, with ,SchedulingDisabled appended for a cordoned node; missing for a requested node the API server does not know about |
| os                  | OS image the node runs (e.g. Ubuntu 22.04.4 LTS)                                                                                                                              |
| pid_pressure        | 1 when the PIDPressure condition is True, else 0                                                                                                                              |
| pods_capacity       | Maximum number of pods the node accepts (-1 when not reported)                                                                                                                |
| ready               | The Ready condition: True, False or Unknown (empty for a requested node the API server does not know about)                                                                   |
| roles               | Node roles from the node-role.kubernetes.io/<role> labels, comma separated                                                                                                    |
| schedulable         | 1 unless the node is cordoned (spec.unschedulable), then 0                                                                                                                    |
| taints              | Taints as key=value:effect, comma separated                                                                                                                                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_pods

Check the state of pods: phase, the kubectl STATUS column, readiness and restarts.

#### About `check_pods`

`check_pods` lists pods - across the cluster or in the given `namespace=`
(repeatable) - and evaluates each one. The list is fetched page by page
(`limit=500`, following `metadata.continue`), so it works on large clusters;
`label-selector=` and `field-selector=` are handed to the API server, so the
filtering happens before the payload is built.

`pod_status` reproduces the STATUS column of `kubectl get pods` (`Running`,
`Completed`, `CrashLoopBackOff`, `ImagePullBackOff`, `OOMKilled`,
`Terminating`, `Init:1/2`, `ExitCode:3`, ...) from the container statuses and
`deletionTimestamp`. Use it rather than `phase` for alerting: a crash-looping
pod has phase `Running`.

Three ways to use it:

* No arguments - every pod that has not finished (`filter=phase !=
  'Succeeded'`). The defaults warn on `Pending` pods and more than five
  restarts, and go critical on `Failed`, `Unknown`, any `*BackOff`,
  `OOMKilled` or a `missing` pod.
* `pod=<name>` or `pod=<namespace>/<name>` (repeatable) - only the named pods
  take part, and a pod the API server does not return is reported with
  `pod_status` `missing`, so it trips the default critical instead of silently
  disappearing from the listing. A bare name is matched in every namespace
  listed and, when missing, reported as `*/<name>` (or under the one
  `namespace=` the check was scoped to).
* A selector - `label-selector=app=web` or `field-selector=spec.nodeName=worker-1`
  for the pods of one application or one node.

`age` takes duration units (`age < 10m`), `created` is a date, and `oom_killed`
is 1 when a container's current or last termination was an out-of-memory kill,
even if the pod has since restarted and shows `Running`. Native sidecars (init
containers with `restartPolicy: Always`) count as running once started and are
included in `containers` / `ready_containers`, as in kubectl; `restarts` is
the kubectl RESTARTS column, which counts the main containers and running
sidecars once the pod is initialised and the init containers only while it is
not, and `ready_containers` counts a container only while it is actually
running; a finished job
pod being cleaned up keeps `Completed` rather than turning into `Terminating`,
while `terminating` still reports the pending deletion.

**Jump to section:**

* [Sample Commands](#check_pods_samples)
* [Command-line Arguments](#check_pods_options)
* [Filter keywords](#check_pods_filter_keys)


<a id="check_pods_samples"></a>
#### Sample Commands

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



<a id="check_pods_options"></a>
#### Command-line Arguments

<a id="check_pods_namespace"></a>
<a id="check_pods_label-selector"></a>
<a id="check_pods_field-selector"></a>
<a id="check_pods_pod"></a>

        
        
        
        
        
| Option                         | Default Value | Description                                                                                                                                                                                                            |
|--------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| namespace                      |               | Only list pods in this namespace (repeatable). Default: all namespaces.                                                                                                                                                |
| label-selector                 |               | Label selector passed to the API server, e.g. app=web,tier!=cache: filtering happens before the payload is built.                                                                                                      |
| field-selector                 |               | Field selector passed to the API server, e.g. spec.nodeName=worker-1 or status.phase!=Succeeded.                                                                                                                       |
| pod                            |               | Name of a pod that must exist (repeatable). Only the named pods take part in the check; a name the API server does not return gets pod_status 'missing'.                                                               |
| [timeout](#check_pods_timeout) | 30            | Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number. |



<h5 id="check_pods_timeout">timeout:</h5>

Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number.

*Default Value:* `30`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                     | Default Value                                                                                                            |
|------------------------------------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------|
| <a id="check_pods_filter"></a>[filter](../common-options.md#filter)                                        | phase != 'Succeeded'                                                                                                     |
| <a id="check_pods_warning"></a>[warning](../common-options.md#warning)                                     | phase = 'Pending' or restarts > 5                                                                                        |
| <a id="check_pods_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                          |
| <a id="check_pods_critical"></a>[critical](../common-options.md#critical)                                  | phase = 'Failed' or phase = 'Unknown' or pod_status like 'BackOff' or pod_status = 'OOMKilled' or pod_status = 'missing' |
| <a id="check_pods_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                          |
| <a id="check_pods_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                          |
| <a id="check_pods_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                    |
| <a id="check_pods_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                    |
| <a id="check_pods_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                                                                                       |
| <a id="check_pods_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                          |
| <a id="check_pods_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                    |
| <a id="check_pods_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                        |
| <a id="check_pods_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                                                                                               |
| <a id="check_pods_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) pods are fine                                                                                    |
| <a id="check_pods_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No pods found                                                                                                 |
| <a id="check_pods_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${namespace}/${name}=${pod_status}                                                                                       |
| <a id="check_pods_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${namespace}/${name}                                                                                                     |
| <a id="check_pods_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                          |
| <a id="check_pods_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                          |
| <a id="check_pods_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                       |
| <a id="check_pods_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_pods_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                                                                                                                |
|------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| age              | Seconds since the pod was created, -1 when unknown (supports units, e.g. age < 1h)                                                                                                         |
| containers       | Number of containers in the pod spec                                                                                                                                                       |
| created          | When the pod was created (date)                                                                                                                                                            |
| ip               | Pod IP address                                                                                                                                                                             |
| labels           | Pod labels as key=value, comma separated                                                                                                                                                   |
| name             | Pod name                                                                                                                                                                                   |
| namespace        | Namespace the pod lives in                                                                                                                                                                 |
| node             | Node the pod is scheduled on (empty while Pending)                                                                                                                                         |
| oom_killed       | 1 when a container's current or last termination was an out-of-memory kill, else 0                                                                                                         |
| owner            | Name of the controller owning the pod                                                                                                                                                      |
| owner_kind       | Kind of the controller owning the pod: ReplicaSet, StatefulSet, DaemonSet, Job, Node, ...                                                                                                  |
| phase            | Pod phase: Pending, Running, Succeeded, Failed or Unknown                                                                                                                                  |
| pod_status       | The kubectl STATUS column: Running, Completed, CrashLoopBackOff, ImagePullBackOff, OOMKilled, Terminating, Init:1/2, ... or missing for a requested pod the API server does not know about |
| qos              | QoS class: Guaranteed, Burstable or BestEffort                                                                                                                                             |
| ready            | 1 when the pod's Ready condition is True, else 0                                                                                                                                           |
| ready_containers | Number of containers reporting ready                                                                                                                                                       |
| restarts         | The kubectl RESTARTS column: restarts of the containers and native sidecars once the pod is initialised, of the init containers while it is not                                            |
| terminating      | 1 when the pod is being deleted (deletionTimestamp is set), else 0                                                                                                                         |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_workloads

Check that deployments, statefulsets and daemonsets have their desired replicas available.

#### About `check_workloads`

`check_workloads` reads the deployments, statefulsets and daemonsets under
`/apis/apps/v1` and normalises the three status shapes into one record:
`desired`, `ready`, `available`, `updated`, `unavailable` and `missing`
(desired minus available), plus `paused` for a deployment whose rollout is
paused. For a daemonset `desired` is the number of nodes it should run on.

The defaults go critical when nothing is available of a non-zero `desired`
(`available = 0 and desired > 0`) and warn when replicas are missing or a
rollout has not finished (`missing > 0 or updated < desired`). A workload
scaled to zero wants nothing and is fine.

`kind=deployment|statefulset|daemonset` (repeatable, singular or plural, any
case) restricts the check to one type and saves the other list calls;
`namespace=`, `label-selector=` and `field-selector=` are passed to the API
server. `workload=<name>` or `workload=<namespace>/<name>` (repeatable) names
workloads that must exist; one the API server does not return is reported
with `0/1` available under the kind `missing`, which trips the default
critical.

**Jump to section:**

* [Sample Commands](#check_workloads_samples)
* [Command-line Arguments](#check_workloads_options)
* [Filter keywords](#check_workloads_filter_keys)


<a id="check_workloads_samples"></a>
#### Sample Commands

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



<a id="check_workloads_options"></a>
#### Command-line Arguments

<a id="check_workloads_namespace"></a>
<a id="check_workloads_kind"></a>
<a id="check_workloads_label-selector"></a>
<a id="check_workloads_field-selector"></a>
<a id="check_workloads_workload"></a>

        
        
        
        
        
        
| Option                              | Default Value | Description                                                                                                                                                                                                            |
|-------------------------------------|---------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| namespace                           |               | Only list workloads in this namespace (repeatable). Default: all namespaces.                                                                                                                                           |
| kind                                |               | Only check this workload kind: deployment, statefulset or daemonset (repeatable). Default: all three.                                                                                                                  |
| label-selector                      |               | Label selector passed to the API server, e.g. app.kubernetes.io/part-of=shop.                                                                                                                                          |
| field-selector                      |               | Field selector passed to the API server, e.g. metadata.name=web.                                                                                                                                                       |
| workload                            |               | Name of a workload that must exist, as name or namespace/name (repeatable). Only the named workloads take part in the check; one the API server does not return is reported with 0 available of 1 desired.             |
| [timeout](#check_workloads_timeout) | 30            | Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number. |



<h5 id="check_workloads_timeout">timeout:</h5>

Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It is not a total for the check (a server that keeps trickling data takes longer). A positive number.

*Default Value:* `30`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                          | Default Value                                        |
|-----------------------------------------------------------------------------------------------------------------|------------------------------------------------------|
| <a id="check_workloads_filter"></a>[filter](../common-options.md#filter)                                        |                                                      |
| <a id="check_workloads_warning"></a>[warning](../common-options.md#warning)                                     | missing > 0 or updated < desired                     |
| <a id="check_workloads_warn"></a>[warn](../common-options.md#warn)                                              |                                                      |
| <a id="check_workloads_critical"></a>[critical](../common-options.md#critical)                                  | available = 0 and desired > 0                        |
| <a id="check_workloads_crit"></a>[crit](../common-options.md#crit)                                              |                                                      |
| <a id="check_workloads_ok"></a>[ok](../common-options.md#ok)                                                    |                                                      |
| <a id="check_workloads_debug"></a>[debug](../common-options.md#debug)                                           | false                                                |
| <a id="check_workloads_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                |
| <a id="check_workloads_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                   |
| <a id="check_workloads_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                      |
| <a id="check_workloads_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                |
| <a id="check_workloads_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                    |
| <a id="check_workloads_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${problem_list}                           |
| <a id="check_workloads_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               | %(status): All %(count) workloads are available      |
| <a id="check_workloads_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | %(status): No workloads found                        |
| <a id="check_workloads_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${kind} ${namespace}/${name}=${available}/${desired} |
| <a id="check_workloads_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${namespace}/${name}                                 |
| <a id="check_workloads_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                      |
| <a id="check_workloads_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                      |
| <a id="check_workloads_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                   |
| <a id="check_workloads_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_workloads_filter_keys"></a>
#### Filter keywords

| Option      | Description                                                                             |
|-------------|-----------------------------------------------------------------------------------------|
| age         | Seconds since the workload was created, -1 when unknown (supports units, e.g. age < 1h) |
| available   | Replicas available (Ready for at least minReadySeconds)                                 |
| created     | When the workload was created (date)                                                    |
| desired     | Replicas wanted (spec.replicas, or the nodes a daemonset should run on)                 |
| kind        | Workload kind: Deployment, StatefulSet or DaemonSet                                     |
| labels      | Workload labels as key=value, comma separated                                           |
| missing     | desired minus available, never below 0                                                  |
| name        | Workload name                                                                           |
| namespace   | Namespace the workload lives in                                                         |
| paused      | 1 when a deployment's rollout is paused, else 0                                         |
| ready       | Replicas whose pod is Ready                                                             |
| unavailable | Replicas the controller reports unavailable                                             |
| updated     | Replicas running the current template (less than desired during a rollout)              |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

## Configuration

| Path / Section                                | Description |
|-----------------------------------------------|-------------|
| [/settings/kubernetes](#/settings/kubernetes) |             |


### /settings/kubernetes <a id="/settings/kubernetes"></a>



| Key                                     | Default Value | Description           |
|-----------------------------------------|---------------|-----------------------|
| [api server](#api-server)               |               | API SERVER            |
| [ca](#certificate-authority)            | ${ca-path}    | CERTIFICATE AUTHORITY |
| [context](#kubeconfig-context)          |               | KUBECONFIG CONTEXT    |
| [kubeconfig](#kubeconfig-json)          |               | KUBECONFIG (JSON)     |
| [max response size](#max-response-size) | 64            | MAX RESPONSE SIZE     |
| [timeout](#timeout)                     | 30            | TIMEOUT               |
| [tls version](#tls-version)             | tlsv1.2+      | TLS VERSION           |
| [token](#bearer-token)                  |               | BEARER TOKEN          |
| [token file](#token-file)               |               | TOKEN FILE            |
| [verify mode](#tls-peer-verify-mode)    | peer          | TLS PEER VERIFY MODE  |


```ini
# 
[/settings/kubernetes]
ca=${ca-path}
max response size=64
timeout=30
tls version=tlsv1.2+
verify mode=peer
```

#### API SERVER <a id="/settings/kubernetes/api server"></a>

The Kubernetes API server, e.g. https://k8s.example.com:6443. Leave empty to auto-detect when the agent runs inside the cluster (KUBERNETES_SERVICE_HOST plus the mounted service account token and CA). Which cluster the agent talks to is an operator decision: a check request cannot choose another server. Use https: an http:// server receives the bearer token in cleartext, and the agent logs a warning when it connects to one.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | api server                                    |
| Default value: | _N/A_                                         |


**Sample:**

```
[/settings/kubernetes]
# API SERVER
api server=
```

#### CERTIFICATE AUTHORITY <a id="/settings/kubernetes/ca"></a>

CA bundle (a PEM file or a hashed directory) used to verify the API server certificate. Defaults to the trusted system store (in-cluster: the mounted service account CA, unless this is set); point it at the cluster CA (\`kubectl config view --raw -o jsonpath='{.clusters[0].cluster.certificate-authority-data}' \| base64 -d\`) for a private cluster CA.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | ca                                            |
| Default value: | `${ca-path}`                                  |


**Sample:**

```
[/settings/kubernetes]
# CERTIFICATE AUTHORITY
ca=${ca-path}
```

#### KUBECONFIG CONTEXT <a id="/settings/kubernetes/context"></a>

The kubeconfig context to use; empty means its current-context.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | context                                       |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |


**Sample:**

```
[/settings/kubernetes]
# KUBECONFIG CONTEXT
context=
```

#### KUBECONFIG (JSON) <a id="/settings/kubernetes/kubeconfig"></a>

Path to a kubeconfig in JSON form (\`kubectl config view --raw --minify -o json > nscp-kubeconfig.json\`), used when \`api server\` is empty. YAML kubeconfigs are not read. Supplies the server, CA, token or client certificate of the selected context.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | kubeconfig                                    |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | _N/A_                                         |


**Sample:**

```
[/settings/kubernetes]
# KUBECONFIG (JSON)
kubeconfig=
```

#### MAX RESPONSE SIZE <a id="/settings/kubernetes/max response size"></a>

Largest API response the agent will buffer, in megabytes. A pod list in a large cluster runs to tens of megabytes; 0 removes the cap.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | max response size                             |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `64`                                          |


**Sample:**

```
[/settings/kubernetes]
# MAX RESPONSE SIZE
max response size=64
```

#### TIMEOUT <a id="/settings/kubernetes/timeout"></a>

Timeout in seconds for each network step of an API server request: the connect, the TLS handshake and each read. It bounds a stalled server, not the whole check: a server that keeps trickling data, or a list of many pages, takes longer. Must be positive: the checks refuse 0 or less rather than wait forever.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | timeout                                       |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `30`                                          |


**Sample:**

```
[/settings/kubernetes]
# TIMEOUT
timeout=30
```

#### TLS VERSION <a id="/settings/kubernetes/tls version"></a>

Minimum TLS protocol version accepted: tlsv1.0, tlsv1.1, tlsv1.2, tlsv1.2+ (the default: TLS 1.2 and 1.3), tlsv1.3.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | tls version                                   |
| Advanced:      | Yes (means it is not commonly used)           |
| Default value: | `tlsv1.2+`                                    |


**Sample:**

```
[/settings/kubernetes]
# TLS VERSION
tls version=tlsv1.2+
```

#### BEARER TOKEN <a id="/settings/kubernetes/token"></a>

Service account bearer token used to authenticate against the API server. Prefer \`token file\` for a token that is rotated.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | token                                         |
| Default value: | _N/A_                                         |


**Sample:**

```
[/settings/kubernetes]
# BEARER TOKEN
token=
```

#### TOKEN FILE <a id="/settings/kubernetes/token file"></a>

Path to a file holding the bearer token, read at check time so a rotated (projected) token keeps working without a reload. Takes precedence over \`token\`.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | token file                                    |
| Default value: | _N/A_                                         |


**Sample:**

```
[/settings/kubernetes]
# TOKEN FILE
token file=
```

#### TLS PEER VERIFY MODE <a id="/settings/kubernetes/verify mode"></a>

Comma separated list of options: none, peer (or certificate), peer-cert, fail-if-no-cert (or fail-if-no-peer-cert, client-certificate). For a self signed certificate use peer-cert and point \`ca\` at that certificate; none disables verification entirely and sends the token to an unverified peer.


| Key            | Description                                   |
|----------------|-----------------------------------------------|
| Path:          | [/settings/kubernetes](#/settings/kubernetes) |
| Key:           | verify mode                                   |
| Default value: | `peer`                                        |


**Sample:**

```
[/settings/kubernetes]
# TLS PEER VERIFY MODE
verify mode=peer
```
