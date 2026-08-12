#### About `check_mssql_availability_groups`

`check_mssql_availability_groups` reports **Always On availability group
health** from `sys.dm_hadr_availability_replica_states` and
`sys.dm_hadr_database_replica_states`, producing one row per replica and per
availability database on each replica. Replicas that fall out of
synchronisation silently break the RPO the cluster was built for — this check
makes that state page before a failover discovers it.

Keywords (one row per replica or replica database):

| Keyword           | Description                                                                  |
|-------------------|-------------------------------------------------------------------------------|
| `name`            | `group/replica` or `group/replica/database`                                  |
| `group`           | Availability group name                                                       |
| `replica`         | Replica server name                                                           |
| `role`            | `PRIMARY`, `SECONDARY` or `RESOLVING` (no primary, e.g. failover in progress) |
| `connected_state` | `CONNECTED` or `DISCONNECTED`                                                 |
| `replica_health`  | Replica synchronization health: `HEALTHY`, `PARTIALLY_HEALTHY`, `NOT_HEALTHY` |
| `health`          | Effective health: database health on database rows, replica health otherwise  |
| `database`        | Availability database name (empty on replica-level rows)                     |
| `sync_state`      | `SYNCHRONIZED`, `SYNCHRONIZING`, `NOT SYNCHRONIZING`, ...                     |
| `db_health`       | Database synchronization health (empty on replica-level rows)                |
| `redo_queue`      | Log received but not yet applied on the secondary, in bytes — failover/RTO lag (accepts units) |
| `log_send_queue`  | Log not yet sent to the secondary, in bytes — potential data loss/RPO lag (accepts units) |
| `is_suspended`    | `1` if data movement for the database is suspended                            |
| `is_local`        | `1` if the row describes the instance being checked                           |

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
