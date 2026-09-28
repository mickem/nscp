#### Usage

Run from a cluster member; replace these illustrative names with your own.

```text
check_cluster_nodes "name=NODE01"
check_cluster_nodes "filter=name != 'MAINTENANCE-NODE'"
```

#### Captured unavailable-cluster result

The following output was captured on a Windows workstation without an accessible
local cluster (exit code 3, UNKNOWN). The Windows error text follows the OS
language; this host uses Swedish. This is not a live cluster success example.

```text
nscp client --module CheckWindowsApps --boot --query check_cluster_nodes
Failed to query cluster nodes: OpenClusterEx (local cluster unavailable or inaccessible) (Windows error 1753): Inga fler slutpunkter är tillgängliga från slutpunktsavbildaren.
```
