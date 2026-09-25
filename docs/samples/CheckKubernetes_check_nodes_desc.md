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
