#### Cluster network health

Reads networks from the local Windows Failover Cluster. Down or unavailable
networks are CRITICAL, partitioned networks are WARNING, and up networks are OK.
Unavailable means all interfaces are unavailable; partitioned means some cluster
nodes cannot communicate through that network. Select relevant networks with
`name` or `filter` to exclude intentionally unused networks.

Requires a cluster member running Windows Server 2008 R2 or later and an account
with cluster read access. Uses read-only ClusAPI calls. This checks the cluster's
network state, not traffic, bandwidth, or individual network interfaces. Networks
do not have an owner node; the shared `owner`, `group` and `type` fields are empty.

`name` requires an exact case-insensitive network name; a missing name returns
UNKNOWN regardless of `empty-state`. Empty filter results follow `empty-state`
(UNKNOWN by default). Missing support, access errors, incomplete reads and unknown
state codes return UNKNOWN. Schedule retries for transient failures.
