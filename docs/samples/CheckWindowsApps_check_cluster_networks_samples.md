#### Usage

Run from a cluster member; replace these illustrative names with your own.

```text
check_cluster_networks "name=Cluster Network 1"
```

#### Captured unavailable-cluster result

The following output was captured on a Windows workstation without an accessible
local cluster (exit code 3, UNKNOWN). The Windows error text follows the OS
language; this host uses Swedish. This is not a live cluster success example.

```text
nscp client --module CheckWindowsApps --boot --query check_cluster_networks
Failed to query cluster networks: OpenClusterEx (local cluster unavailable or inaccessible) (Windows error 1753): 6d9: Inga fler slutpunkter är tillgängliga från slutpunktsavbildaren.
```
