# Windows Failover Cluster acceptance tests

The ordinary `checkwindowsapps-cluster-commands` Jest suite runs on any Windows
host. It verifies real dispatch and the missing-cluster contract. C++
`check_cluster_test` covers injected snapshots and the native API adapter.
Neither substitutes for a real cluster acceptance run.

Use an isolated two-node Windows Server lab, NSCP on both nodes, and a disposable
clustered role. Follow Microsoft's cluster creation and validation guidance:
https://learn.microsoft.com/en-us/windows-server/failover-clustering/create-failover-cluster
Use the intended NSCP service identity for acquisition tests, not only an
interactive administrator. Keep an independent management connection.

Run from `tests/` on each node (PowerShell):

```powershell
$env:NSCP_SKIP_DOCKER = '1'
$env:NSCP_BIN = 'C:\path\to\nscp.exe'
$env:NSCP_EXPECT_CLUSTER = '1'
npx jest --runInBand checkwindowsapps-cluster-commands
```

Strict mode requires actual objects and rejects the missing-cluster fallback.
The suite itself does not mutate the cluster. For acceptance, use only the
disposable role and restore the initial state in a `finally` block of your lab
automation. Poll to a bounded deadline (for example 120 seconds) instead of
assuming transitions complete immediately.

| Lab action | Required observation |
|---|---|
| Query the same role with `name=<role>` from both nodes | Same role visible, including on its non-owner |
| Move the role to the other node | Owner updates and role returns OK after settling |
| Stop the disposable role | `check_cluster_groups` reports offline, CRITICAL |
| Fail a disposable resource | Failed state detected; recovery may require repeated samples |
| Query an intentionally offline resource | Default does not alert; explicit offline critical threshold does |
| Pause a node | `check_cluster_nodes` reports paused, WARNING |
| Stop the local cluster service | Local acquisition reports UNKNOWN; surviving node reports node state |
| Query under an identity without cluster access | UNKNOWN with native error; never OK |
| Select a nonexistent role with `name=... empty-state=ok` | UNKNOWN with missing-object message |
| Use `filter=name = 'nonexistent' empty-state=ok` | Empty-result message after successful acquisition |

Network partition testing needs a dedicated lab network and independent management
access; do not disrupt a production network. SQL FCI, file-server and Hyper-V
resource types should each receive a real workload smoke test before claiming
workload-specific validation. Record the Windows version, role, service identity,
commands and captured output. No live cluster validation is implied by fixture
tests or a successful run on a workstation.
