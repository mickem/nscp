---
icon: "☸️"
modules: [CheckKubernetes]
action: none
---
**New experimental `CheckKubernetes` module.** Nothing to do on an existing
install: the module is not enabled by default. It monitors a Kubernetes
cluster through its API server with the agent's own HTTPS client and a
service account token - no `kubectl` on the host - and brings four checks:
`check_kubernetes` (API reachable and ready, node counts), `check_pods` (the
`kubectl get pods` STATUS column, readiness, restarts), `check_nodes`
(readiness, pressure conditions, cordons, capacity) and `check_workloads`
(desired versus available replicas of deployments, statefulsets and
daemonsets). The cluster is configured once under `[/settings/kubernetes]`
(`api server` plus `token` or `token file`, a JSON `kubeconfig`, or nothing at
all when the agent runs in-cluster with a service account); no check accepts
a `url=`, `host=` or `token=` argument, so a REST caller cannot redirect the
token to another server. Every command is marked experimental until the
keywords and output have settled against real clusters. See the
[Kubernetes scenario](../scenarios/kubernetes.md).
