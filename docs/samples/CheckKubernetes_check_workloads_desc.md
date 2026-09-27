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
