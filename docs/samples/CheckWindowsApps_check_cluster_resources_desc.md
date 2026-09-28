#### Resources behind clustered roles

Reads all resources in the local Windows Failover Cluster, with their current
owner, containing group and resource type. Resources owned by other nodes remain
visible. Requires a cluster member running Windows Server 2008 R2 or later and
an account with cluster read access; uses read-only ClusAPI calls.

Failed resources are CRITICAL. Initializing, pending, online-pending and
offline-pending resources are WARNING. Offline and inherited resources do not
alert by default: dependency configurations and deliberately stopped workloads
can legitimately leave resources offline. To require selected resources online,
set `critical=state = 'failed' or state = 'offline'` and select them with `name`
or `filter`. This checks cluster resource state, not SQL replication health,
guest OS health, or an application's end-to-end availability.

`name` requires an exact case-insensitive name and returns UNKNOWN if absent,
regardless of `empty-state`. Empty filter results follow `empty-state` (UNKNOWN
by default). Acquisition failures always return UNKNOWN. Unmapped state codes
return UNKNOWN only for objects selected by `filter`; exclude them by name or
with `state != 'unknown'` when appropriate.
Native state codes differ from group state codes; prefer the string `state`.
An ownership change alone is healthy. Use monitoring-server retries for brief
transitions; no history or transition duration is inferred from one sample.
