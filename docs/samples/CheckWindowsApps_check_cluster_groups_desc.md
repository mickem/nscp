#### Clustered roles and ownership

Reads every group (clustered role) in the local Windows Failover Cluster,
including groups owned by another node. Run NSCP on a cluster member using an
account with cluster read access. The check uses ClusAPI and requires Windows
Server 2008 R2 or later with Failover Clustering available. It does not require
PowerShell or WMI, and does not move or restart roles.

Defaults: offline or failed groups are CRITICAL; pending or partial-online
groups are WARNING. Select the roles expected to run: intentionally offline
groups, including unused storage groups, may otherwise alert. `name=SQL`
requires that exact name (case insensitive); a missing name returns UNKNOWN
even with `empty-state=ok`. A general `filter` matching nothing follows
`empty-state` (UNKNOWN by default).

Ownership changes alone are healthy. `owner` reports the current owner, not
failover history; use an explicit owner threshold for placement policy. Use
monitoring-server retries to tolerate brief pending states. Polling does not
guarantee observing a transition that completed between checks.

Missing cluster support, access errors and incomplete reads return UNKNOWN,
never an empty successful result. Unmapped state codes return UNKNOWN only for
objects selected by `filter`; exclude them by name or with `state != 'unknown'`
when appropriate. Acquisition is a series
of reads, not an atomic cluster snapshot; roles can move between reads. Monitor
node reachability separately and schedule role checks through a surviving member.

ClusAPI is loaded only for cluster checks, so enabling CheckWindowsApps on a
non-cluster host does not prevent its IIS or RDS commands from running.
