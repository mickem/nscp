#### Cluster node health

Reads every node in the local Windows Failover Cluster. Down nodes are CRITICAL;
paused or joining nodes are WARNING; up nodes are OK. A down state does not
identify the cause: the machine, cluster service or connectivity may be down.
Exclude planned maintenance nodes with `filter` or adjust the warning threshold.

Requires a cluster member running Windows Server 2008 R2 or later and an account
with cluster read access. It uses read-only ClusAPI calls. A stopped local cluster
service may prevent acquisition entirely, yielding UNKNOWN; query a surviving
member and monitor host availability separately.

`name` selects an exact case-insensitive node name and returns UNKNOWN if missing,
even with `empty-state=ok`. Empty filter results follow `empty-state` (UNKNOWN by
default). Missing support, access errors and incomplete reads return UNKNOWN.
Unmapped state codes return UNKNOWN only for objects selected by `filter`;
exclude them by name or with `state != 'unknown'` when appropriate.
This is current state, not node failure history.
