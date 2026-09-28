#### About `check_mssql_availability_groups`

`check_mssql_availability_groups` reports **Always On availability group
health** from `sys.dm_hadr_availability_replica_states` and
`sys.dm_hadr_database_replica_states`, producing one row per replica and per
availability database on each replica. Replicas that fall out of
synchronisation silently break the RPO the cluster was built for — this check
makes that state page before a failover discovers it.

Defaults: **WARNING** on `PARTIALLY_HEALTHY`, **CRITICAL** on `NOT_HEALTHY`,
`DISCONNECTED`, suspended data movement or a `RESOLVING` role. The health
states already encode Microsoft's own policy evaluation, so the defaults catch
broken replication without tuning; add `redo_queue`/`log_send_queue`
thresholds to alert on lag *before* it degrades health, sized to your RPO/RTO.

**Where to run it:** the primary sees the state of every replica including the
send/redo queues of all secondaries — pointing this check at the AG listener
or the primary gives the full picture. A secondary only exposes its local
replica state (remote replicas without state rows are deliberately omitted
rather than misreported as DISCONNECTED).

empty-state is **OK** (`No availability groups found`) so the check can be
rolled out fleet-wide, including instances without AGs. On hosts where an AG
**must** exist, set `empty-state=critical`: a dropped AG silently removes the
protection it provided.

Rights: `VIEW SERVER STATE`.
