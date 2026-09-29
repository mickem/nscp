---
icon: "🧪"
modules: [CheckWindowsApps]
action: none
---
**Windows Failover Cluster checks in `CheckWindowsApps` (experimental).**
Nothing to do on an upgrade. On a cluster member, four new commands read the
local cluster through ClusAPI, without PowerShell or WMI, and never move or
restart anything:

| Command | Checks |
|---|---|
| `check_cluster_groups` | clustered roles and their current owner node |
| `check_cluster_resources` | resources, their type, group and owner |
| `check_cluster_nodes` | node states |
| `check_cluster_networks` | cluster network states |

The service account needs cluster read access, and the host must run Windows
Server 2008 R2 or later with Failover Clustering. A missing cluster, an access
error or a partial read returns UNKNOWN, never an empty OK. By default an
offline or failed group is CRITICAL and a pending or partially online one is
WARNING, so a role that is offline on purpose (an unused storage group, for
example) alerts until you filter it out: select the roles you expect to run
(`name=SQL`) or exclude the rest. Like the rest of the module, the commands are
experimental: options, keywords and output may still change.
