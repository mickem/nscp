# CheckWindowsApps

*Available on Windows only.*

!!! warning "Experimental"

    This module is experimental: it works, but its options, filter keywords
    and output may change in a future release. Please try it and report
    anything that does not behave the way you expect.

Checks for Windows applications and server roles: IIS, Remote Desktop Services, NPS authentication, accounting and performance counters, and Failover Clustering groups, resources, nodes and networks.

## Enable module

To enable this module and allow using the commands you need to add `CheckWindowsApps = enabled` to the `[/modules]` section in nsclient.ini:

```
[/modules]
CheckWindowsApps = enabled
```

## Queries

A quick reference for all available queries (check commands) in the CheckWindowsApps module.

**List of commands:**

A list of all available queries (check commands)

| Command                                                                    | Description                                                                                   |
|----------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------|
| [check_cluster_groups](#check_cluster_groups) *(experimental)*             | Check Windows Failover Cluster roles and their current owner nodes.                           |
| [check_cluster_networks](#check_cluster_networks) *(experimental)*         | Check Windows Failover Cluster network states.                                                |
| [check_cluster_nodes](#check_cluster_nodes) *(experimental)*               | Check Windows Failover Cluster node states.                                                   |
| [check_cluster_resources](#check_cluster_resources) *(experimental)*       | Check Windows Failover Cluster resources, types, groups and owners.                           |
| [check_iis_app_pools](#check_iis_app_pools) *(experimental)*               | Check IIS application pools (state, uptime, recycles).                                        |
| [check_iis_request_queues](#check_iis_request_queues) *(experimental)*     | Check HTTP.sys request queues (length, rejections, age).                                      |
| [check_iis_sites](#check_iis_sites) *(experimental)*                       | Check IIS web sites (state, connections, traffic).                                            |
| [check_iis_worker_processes](#check_iis_worker_processes) *(experimental)* | Check IIS worker processes (active and served requests per w3wp).                             |
| [check_nps_accounting](#check_nps_accounting) *(experimental)*             | Check discarded NPS accounting requests and optional accounting log freshness.                |
| [check_nps_auth](#check_nps_auth) *(experimental)*                         | Check NPS authentication outcomes, rejection percentages and reason codes over a time window. |
| [check_nps_counters](#check_nps_counters) *(experimental)*                 | Check the installed NPS/RADIUS performance counters using two samples.                        |
| [check_rds_broker](#check_rds_broker) *(experimental)*                     | Check the Remote Desktop Connection Broker counterset (failed/pending connections).           |
| [check_rds_licenses](#check_rds_licenses) *(experimental)*                 | Check Remote Desktop licensing (CAL key packs: issued versus available licenses).             |
| [check_rds_session_load](#check_rds_session_load) *(experimental)*         | Check per-session resource usage (CPU, working set, protocol bytes).                          |
| [check_rds_sessions](#check_rds_sessions) *(experimental)*                 | Check session counts on a session host (active, inactive, total).                             |

### check_cluster_groups

Check Windows Failover Cluster roles and their current owner nodes.

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

**Jump to section:**

* [Sample Commands](#check_cluster_groups_samples)
* [Command-line Arguments](#check_cluster_groups_options)
* [Filter keywords](#check_cluster_groups_filter_keys)


<a id="check_cluster_groups_samples"></a>
#### Sample Commands

#### Usage

Run from a cluster member; replace these illustrative names with your own.

```text
check_cluster_groups "name=SQL Server (PROD)"
check_cluster_groups "filter=name != 'Available Storage'"
```

#### Captured unavailable-cluster result

The following output was captured on a Windows workstation without an accessible
local cluster (exit code 3, UNKNOWN). The Windows error text follows the OS
language; this host uses Swedish. This is not a live cluster success example.

```text
nscp client --module CheckWindowsApps --boot --query check_cluster_groups
Failed to query cluster groups: OpenClusterEx (local cluster unavailable or inaccessible) (Windows error 1753): 6d9: Inga fler slutpunkter är tillgängliga från slutpunktsavbildaren.
```



<a id="check_cluster_groups_options"></a>
#### Command-line Arguments

<a id="check_cluster_groups_name"></a>

        
| Option | Default Value | Description                                                                                                 |
|--------|---------------|-------------------------------------------------------------------------------------------------------------|
| name   |               | Require one exact object name (case insensitive). Missing objects return UNKNOWN regardless of empty-state. |




**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                 |
|----------------------------------------------------------------------------------------------------------------------|-----------------------------------------------|
| <a id="check_cluster_groups_filter"></a>[filter](../common-options.md#filter)                                        |                                               |
| <a id="check_cluster_groups_warning"></a>[warning](../common-options.md#warning)                                     | state = 'pending' or state = 'partial_online' |
| <a id="check_cluster_groups_warn"></a>[warn](../common-options.md#warn)                                              |                                               |
| <a id="check_cluster_groups_critical"></a>[critical](../common-options.md#critical)                                  | state = 'failed' or state = 'offline'         |
| <a id="check_cluster_groups_crit"></a>[crit](../common-options.md#crit)                                              |                                               |
| <a id="check_cluster_groups_ok"></a>[ok](../common-options.md#ok)                                                    |                                               |
| <a id="check_cluster_groups_debug"></a>[debug](../common-options.md#debug)                                           | false                                         |
| <a id="check_cluster_groups_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                         |
| <a id="check_cluster_groups_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                       |
| <a id="check_cluster_groups_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                               |
| <a id="check_cluster_groups_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                         |
| <a id="check_cluster_groups_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                             |
| <a id="check_cluster_groups_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                            |
| <a id="check_cluster_groups_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                               |
| <a id="check_cluster_groups_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No cluster groups matched                     |
| <a id="check_cluster_groups_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${state} (owner=${owner})            |
| <a id="check_cluster_groups_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                       |
| <a id="check_cluster_groups_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                               |
| <a id="check_cluster_groups_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                               |
| <a id="check_cluster_groups_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                            |
| <a id="check_cluster_groups_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cluster_groups_filter_keys"></a>
#### Filter keywords

| Option   | Description                                              |
|----------|----------------------------------------------------------|
| group    | Containing group (resources)                             |
| name     | Cluster object name                                      |
| owner    | Current owner node (groups and resources)                |
| state    | Object state (lowercase; mappings differ by object kind) |
| state_id | Native state code for this object kind                   |
| type     | Resource type (resources)                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_cluster_networks

Check Windows Failover Cluster network states.

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
(UNKNOWN by default). Missing support, access errors and incomplete reads return
UNKNOWN. Unmapped state codes return UNKNOWN only for objects selected by
`filter`; exclude them by name or with `state != 'unknown'` when appropriate.
Schedule retries for transient failures.

**Jump to section:**

* [Sample Commands](#check_cluster_networks_samples)
* [Command-line Arguments](#check_cluster_networks_options)
* [Filter keywords](#check_cluster_networks_filter_keys)


<a id="check_cluster_networks_samples"></a>
#### Sample Commands

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



<a id="check_cluster_networks_options"></a>
#### Command-line Arguments

<a id="check_cluster_networks_name"></a>

        
| Option | Default Value | Description                                                                                                 |
|--------|---------------|-------------------------------------------------------------------------------------------------------------|
| name   |               | Require one exact object name (case insensitive). Missing objects return UNKNOWN regardless of empty-state. |




**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                 | Default Value                           |
|------------------------------------------------------------------------------------------------------------------------|-----------------------------------------|
| <a id="check_cluster_networks_filter"></a>[filter](../common-options.md#filter)                                        |                                         |
| <a id="check_cluster_networks_warning"></a>[warning](../common-options.md#warning)                                     | state = 'partitioned'                   |
| <a id="check_cluster_networks_warn"></a>[warn](../common-options.md#warn)                                              |                                         |
| <a id="check_cluster_networks_critical"></a>[critical](../common-options.md#critical)                                  | state = 'down' or state = 'unavailable' |
| <a id="check_cluster_networks_crit"></a>[crit](../common-options.md#crit)                                              |                                         |
| <a id="check_cluster_networks_ok"></a>[ok](../common-options.md#ok)                                                    |                                         |
| <a id="check_cluster_networks_debug"></a>[debug](../common-options.md#debug)                                           | false                                   |
| <a id="check_cluster_networks_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                   |
| <a id="check_cluster_networks_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                 |
| <a id="check_cluster_networks_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                         |
| <a id="check_cluster_networks_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                   |
| <a id="check_cluster_networks_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                       |
| <a id="check_cluster_networks_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                      |
| <a id="check_cluster_networks_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                         |
| <a id="check_cluster_networks_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No cluster networks matched             |
| <a id="check_cluster_networks_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${state}                       |
| <a id="check_cluster_networks_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                 |
| <a id="check_cluster_networks_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                         |
| <a id="check_cluster_networks_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                         |
| <a id="check_cluster_networks_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                      |
| <a id="check_cluster_networks_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                         |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cluster_networks_filter_keys"></a>
#### Filter keywords

| Option   | Description                                              |
|----------|----------------------------------------------------------|
| group    | Containing group (resources)                             |
| name     | Cluster object name                                      |
| owner    | Current owner node (groups and resources)                |
| state    | Object state (lowercase; mappings differ by object kind) |
| state_id | Native state code for this object kind                   |
| type     | Resource type (resources)                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_cluster_nodes

Check Windows Failover Cluster node states.

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

**Jump to section:**

* [Sample Commands](#check_cluster_nodes_samples)
* [Command-line Arguments](#check_cluster_nodes_options)
* [Filter keywords](#check_cluster_nodes_filter_keys)


<a id="check_cluster_nodes_samples"></a>
#### Sample Commands

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
Failed to query cluster nodes: OpenClusterEx (local cluster unavailable or inaccessible) (Windows error 1753): 6d9: Inga fler slutpunkter är tillgängliga från slutpunktsavbildaren.
```



<a id="check_cluster_nodes_options"></a>
#### Command-line Arguments

<a id="check_cluster_nodes_name"></a>

        
| Option | Default Value | Description                                                                                                 |
|--------|---------------|-------------------------------------------------------------------------------------------------------------|
| name   |               | Require one exact object name (case insensitive). Missing objects return UNKNOWN regardless of empty-state. |




**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                         |
|---------------------------------------------------------------------------------------------------------------------|---------------------------------------|
| <a id="check_cluster_nodes_filter"></a>[filter](../common-options.md#filter)                                        |                                       |
| <a id="check_cluster_nodes_warning"></a>[warning](../common-options.md#warning)                                     | state = 'paused' or state = 'joining' |
| <a id="check_cluster_nodes_warn"></a>[warn](../common-options.md#warn)                                              |                                       |
| <a id="check_cluster_nodes_critical"></a>[critical](../common-options.md#critical)                                  | state = 'down'                        |
| <a id="check_cluster_nodes_crit"></a>[crit](../common-options.md#crit)                                              |                                       |
| <a id="check_cluster_nodes_ok"></a>[ok](../common-options.md#ok)                                                    |                                       |
| <a id="check_cluster_nodes_debug"></a>[debug](../common-options.md#debug)                                           | false                                 |
| <a id="check_cluster_nodes_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                 |
| <a id="check_cluster_nodes_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                               |
| <a id="check_cluster_nodes_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                       |
| <a id="check_cluster_nodes_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                 |
| <a id="check_cluster_nodes_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                     |
| <a id="check_cluster_nodes_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                    |
| <a id="check_cluster_nodes_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                       |
| <a id="check_cluster_nodes_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No cluster nodes matched              |
| <a id="check_cluster_nodes_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${state}                     |
| <a id="check_cluster_nodes_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                               |
| <a id="check_cluster_nodes_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                       |
| <a id="check_cluster_nodes_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                       |
| <a id="check_cluster_nodes_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                    |
| <a id="check_cluster_nodes_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                       |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cluster_nodes_filter_keys"></a>
#### Filter keywords

| Option   | Description                                              |
|----------|----------------------------------------------------------|
| group    | Containing group (resources)                             |
| name     | Cluster object name                                      |
| owner    | Current owner node (groups and resources)                |
| state    | Object state (lowercase; mappings differ by object kind) |
| state_id | Native state code for this object kind                   |
| type     | Resource type (resources)                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_cluster_resources

Check Windows Failover Cluster resources, types, groups and owners.

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

**Jump to section:**

* [Sample Commands](#check_cluster_resources_samples)
* [Command-line Arguments](#check_cluster_resources_options)
* [Filter keywords](#check_cluster_resources_filter_keys)


<a id="check_cluster_resources_samples"></a>
#### Sample Commands

#### Usage

Run from a cluster member; replace these illustrative names with your own.

```text
check_cluster_resources "filter=group = 'SQL Server (PROD)'" "critical=state = 'failed' or state = 'offline'"
```

#### Captured unavailable-cluster result

The following output was captured on a Windows workstation without an accessible
local cluster (exit code 3, UNKNOWN). The Windows error text follows the OS
language; this host uses Swedish. This is not a live cluster success example.

```text
nscp client --module CheckWindowsApps --boot --query check_cluster_resources
Failed to query cluster resources: OpenClusterEx (local cluster unavailable or inaccessible) (Windows error 1753): 6d9: Inga fler slutpunkter är tillgängliga från slutpunktsavbildaren.
```



<a id="check_cluster_resources_options"></a>
#### Command-line Arguments

<a id="check_cluster_resources_name"></a>

        
| Option | Default Value | Description                                                                                                 |
|--------|---------------|-------------------------------------------------------------------------------------------------------------|
| name   |               | Require one exact object name (case insensitive). Missing objects return UNKNOWN regardless of empty-state. |




**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                  | Default Value                                                                                        |
|-------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------|
| <a id="check_cluster_resources_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                      |
| <a id="check_cluster_resources_warning"></a>[warning](../common-options.md#warning)                                     | state = 'initializing' or state = 'pending' or state = 'online_pending' or state = 'offline_pending' |
| <a id="check_cluster_resources_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                      |
| <a id="check_cluster_resources_critical"></a>[critical](../common-options.md#critical)                                  | state = 'failed'                                                                                     |
| <a id="check_cluster_resources_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                      |
| <a id="check_cluster_resources_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                      |
| <a id="check_cluster_resources_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                |
| <a id="check_cluster_resources_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                |
| <a id="check_cluster_resources_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                              |
| <a id="check_cluster_resources_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                      |
| <a id="check_cluster_resources_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                |
| <a id="check_cluster_resources_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                    |
| <a id="check_cluster_resources_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                   |
| <a id="check_cluster_resources_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                      |
| <a id="check_cluster_resources_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No cluster resources matched                                                                         |
| <a id="check_cluster_resources_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${name}: ${state} (group=${group}, owner=${owner}, type=${type})                                     |
| <a id="check_cluster_resources_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${name}                                                                                              |
| <a id="check_cluster_resources_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                      |
| <a id="check_cluster_resources_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                      |
| <a id="check_cluster_resources_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                   |
| <a id="check_cluster_resources_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_cluster_resources_filter_keys"></a>
#### Filter keywords

| Option   | Description                                              |
|----------|----------------------------------------------------------|
| group    | Containing group (resources)                             |
| name     | Cluster object name                                      |
| owner    | Current owner node (groups and resources)                |
| state    | Object state (lowercase; mappings differ by object kind) |
| state_id | Native state code for this object kind                   |
| type     | Resource type (resources)                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_app_pools

Check IIS application pools (state, uptime, recycles).

#### About `check_iis_app_pools`

`check_iis_app_pools` reports one record per IIS application pool from the
`APP_POOL_WAS` performance counters (state, uptime, recycles). When the IIS
WMI provider ("IIS Management Scripts and Tools") is installed, the records
are enriched from `root\WebAdministration`: each pool gains its `auto_start`
configuration flag, and pools that WAS has no counter instance for yet (never
started since boot) are added with state `unknown` so they cannot hide.

The default **critical** expression is
`state != 'running' and auto_start != 0`: an auto-start pool that is not
running alerts, a pool an administrator stopped on purpose (`auto_start` = 0)
stays quiet. Without the WMI provider `auto_start` is `-1` for every pool, so
every non-running pool alerts.

`recycles` counts since WAS started, so a recycle *storm* shows as a high and
climbing value combined with a low `uptime`; `warning=recycles > 10 and
uptime < 600` is a useful storm signature.

**Jump to section:**

* [Sample Commands](#check_iis_app_pools_samples)
* [Command-line Arguments](#check_iis_app_pools_options)
* [Filter keywords](#check_iis_app_pools_filter_keys)


<a id="check_iis_app_pools_samples"></a>
#### Sample Commands

**Check all IIS application pools (critical when an auto-start pool is not running):**

```
check_iis_app_pools
OK: DefaultAppPool (running)|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0
```

**A stopped pool that is configured to auto-start goes critical:**

```
check_iis_app_pools
CRITICAL: MyAppPool (disabled)|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0 'MyAppPool_uptime'=0s;0;0 'MyAppPool_recycles'=5c;0;0
```

**Detect recycle storms (a pool that keeps recycling):**

```
check_iis_app_pools "warning=recycles > 10" "critical=recycles > 50"
WARNING: MyAppPool (running)|'MyAppPool_recycles'=17c;10;50 'MyAppPool_uptime'=42s;0;0 ...
```

**Scope to one pool and show everything:**

```
check_iis_app_pools "filter=pool = 'DefaultAppPool'" show-all
OK: DefaultAppPool: running, uptime 86400s, 2 recycles|'DefaultAppPool_uptime'=86400s;0;0 'DefaultAppPool_recycles'=2c;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_app_pools
IIS performance counters (APP_POOL_WAS) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_app_pools_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                              | Default Value                                              |
|---------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------|
| <a id="check_iis_app_pools_filter"></a>[filter](../common-options.md#filter)                                        |                                                            |
| <a id="check_iis_app_pools_warning"></a>[warning](../common-options.md#warning)                                     |                                                            |
| <a id="check_iis_app_pools_warn"></a>[warn](../common-options.md#warn)                                              |                                                            |
| <a id="check_iis_app_pools_critical"></a>[critical](../common-options.md#critical)                                  | state != 'running' and auto_start != 0                     |
| <a id="check_iis_app_pools_crit"></a>[crit](../common-options.md#crit)                                              |                                                            |
| <a id="check_iis_app_pools_ok"></a>[ok](../common-options.md#ok)                                                    |                                                            |
| <a id="check_iis_app_pools_debug"></a>[debug](../common-options.md#debug)                                           | false                                                      |
| <a id="check_iis_app_pools_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                      |
| <a id="check_iis_app_pools_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                    |
| <a id="check_iis_app_pools_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                            |
| <a id="check_iis_app_pools_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                      |
| <a id="check_iis_app_pools_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                          |
| <a id="check_iis_app_pools_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                         |
| <a id="check_iis_app_pools_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                            |
| <a id="check_iis_app_pools_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No application pools found                                 |
| <a id="check_iis_app_pools_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${pool}: ${state}, uptime ${uptime}s, ${recycles} recycles |
| <a id="check_iis_app_pools_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${pool}                                                    |
| <a id="check_iis_app_pools_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                            |
| <a id="check_iis_app_pools_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                            |
| <a id="check_iis_app_pools_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                         |
| <a id="check_iis_app_pools_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_app_pools_filter_keys"></a>
#### Filter keywords

| Option     | Description                                                                                                       |
|------------|-------------------------------------------------------------------------------------------------------------------|
| auto_start | 1 when the pool is set to start automatically, 0 when not, -1 when the IIS WMI provider is unavailable            |
| pool       | Name of the application pool                                                                                      |
| recycles   | Pool recycles since WAS started                                                                                   |
| state      | Pool state: running, disabled, disabling, shutdown_pending, delete_pending, initialized, uninitialized or unknown |
| state_id   | Raw APP_POOL_WAS state value (3 = running)                                                                        |
| uptime     | Seconds since the pool last started                                                                               |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_request_queues

Check HTTP.sys request queues (length, rejections, age).

#### About `check_iis_request_queues`

`check_iis_request_queues` reports one record per HTTP.sys request queue from
the "HTTP Service Request Queues" performance counters. Queues are usually
named after the application pool they feed; requests wait here when no worker
is available, and once a queue hits its limit HTTP.sys rejects new requests
with 503 without the application ever seeing them.

The defaults track HTTP.sys' default per-queue limit of 1000: **warning** at
`queue_length > 800`, **critical** at `queue_length > 1000` (adjust when
`queueLength` is raised in the pool configuration).

`rejected` is cumulative, so alert on it going non-zero (`warning=rejected >
0`) after a deploy, or graph its rate; a growing `max_age` with a modest
`queue_length` points at a hung worker rather than overload.

**Jump to section:**

* [Sample Commands](#check_iis_request_queues_samples)
* [Command-line Arguments](#check_iis_request_queues_options)
* [Filter keywords](#check_iis_request_queues_filter_keys)


<a id="check_iis_request_queues_samples"></a>
#### Sample Commands

**Check the HTTP.sys request queues (defaults: warn above 800 queued, critical above 1000):**

```
check_iis_request_queues
OK: DefaultAppPool (0 queued)|'DefaultAppPool_rejected'=0c;0;0
```

**A backed-up queue trips the defaults (1000 is HTTP.sys' default queue limit):**

```
check_iis_request_queues
WARNING: DefaultAppPool (912 queued)|'DefaultAppPool_queue_length'=912;800;1000 'DefaultAppPool_rejected'=0c;0;0
```

**Alert on rejected requests instead (503s served straight from HTTP.sys):**

```
check_iis_request_queues "warning=rejected > 0" "critical=queue_length > 1000"
WARNING: DefaultAppPool (14 queued)|'DefaultAppPool_rejected'=27c;0;0 'DefaultAppPool_queue_length'=14;0;1000
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_request_queues
IIS performance counters (HTTP Service Request Queues) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_request_queues_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                   | Default Value                                          |
|--------------------------------------------------------------------------------------------------------------------------|--------------------------------------------------------|
| <a id="check_iis_request_queues_filter"></a>[filter](../common-options.md#filter)                                        |                                                        |
| <a id="check_iis_request_queues_warning"></a>[warning](../common-options.md#warning)                                     | queue_length > 800                                     |
| <a id="check_iis_request_queues_warn"></a>[warn](../common-options.md#warn)                                              |                                                        |
| <a id="check_iis_request_queues_critical"></a>[critical](../common-options.md#critical)                                  | queue_length > 1000                                    |
| <a id="check_iis_request_queues_crit"></a>[crit](../common-options.md#crit)                                              |                                                        |
| <a id="check_iis_request_queues_ok"></a>[ok](../common-options.md#ok)                                                    |                                                        |
| <a id="check_iis_request_queues_debug"></a>[debug](../common-options.md#debug)                                           | false                                                  |
| <a id="check_iis_request_queues_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                  |
| <a id="check_iis_request_queues_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                     |
| <a id="check_iis_request_queues_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                        |
| <a id="check_iis_request_queues_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                  |
| <a id="check_iis_request_queues_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                      |
| <a id="check_iis_request_queues_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                     |
| <a id="check_iis_request_queues_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                        |
| <a id="check_iis_request_queues_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No HTTP.sys request queues found                       |
| <a id="check_iis_request_queues_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${queue}: ${queue_length} queued, ${rejected} rejected |
| <a id="check_iis_request_queues_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${queue}                                               |
| <a id="check_iis_request_queues_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                        |
| <a id="check_iis_request_queues_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                        |
| <a id="check_iis_request_queues_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                     |
| <a id="check_iis_request_queues_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                        |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_request_queues_filter_keys"></a>
#### Filter keywords

| Option       | Description                                                       |
|--------------|-------------------------------------------------------------------|
| max_age      | Age of the oldest request in the queue                            |
| queue        | Name of the HTTP.sys request queue (usually the application pool) |
| queue_length | Requests currently waiting in the queue                           |
| rejected     | Requests rejected from the queue since it was created             |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_sites

Check IIS web sites (state, connections, traffic).

#### About `check_iis_sites`

`check_iis_sites` reports one record per IIS web site from the "Web Service"
performance counters (current connections, uptime, request/byte rates). When
the IIS WMI provider is installed, records are enriched from
`root\WebAdministration`: sites gain their `ServerAutoStart` flag
(`auto_start`) and sites with no counter instance (stopped) are still listed.

A site is considered `stopped` when it has no Web Service counter instance
(the counters only exist for started sites, so stopped sites are surfaced via
the WMI enrichment). The default **critical** expression is
`state = 'stopped' and auto_start != 0` — an auto-start site that is not
serving alerts, an intentionally stopped one (`auto_start` = 0) stays quiet.
Without the WMI provider `auto_start` is `-1`, so every stopped site alerts.

The rate keywords need two counter samples: pass `averages=true` to collect a
second sample after one second (the check then takes a second longer);
without it `requests_per_sec`/`bytes_per_sec` read 0.

**Jump to section:**

* [Sample Commands](#check_iis_sites_samples)
* [Command-line Arguments](#check_iis_sites_options)
* [Filter keywords](#check_iis_sites_filter_keys)


<a id="check_iis_sites_samples"></a>
#### Sample Commands

**Check all IIS web sites (critical when an auto-start site is stopped):**

```
check_iis_sites
OK: Default Web Site (running)|'Default Web Site_connections'=12;0;0
```

**A stopped site that is configured to auto-start goes critical:**

```
check_iis_sites
CRITICAL: MySite (stopped)|'Default Web Site_connections'=12;0;0 'MySite_connections'=0;0;0
```

**Alert on connection count per site:**

```
check_iis_sites "warning=connections > 500" "critical=connections > 1000"
OK: Default Web Site (running)|'Default Web Site_connections'=12;500;1000
```

**Measure request/byte rates (takes one extra second for the second sample):**

```
check_iis_sites averages=true "warning=requests_per_sec > 200" "detail-syntax=${site}: ${requests_per_sec} req/s, ${bytes_per_sec} B/s" show-all
OK: Default Web Site: 3.5 req/s, 1024.5 B/s|'Default Web Site_requests_per_sec'=3.5;200;0 'Default Web Site_connections'=12;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_sites
IIS performance counters (Web Service) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_sites_options"></a>
#### Command-line Arguments

        
| Option                                | Default Value | Description                                                                                                        |
|---------------------------------------|---------------|--------------------------------------------------------------------------------------------------------------------|
| [averages](#check_iis_sites_averages) | false         | Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values. |



<h5 id="check_iis_sites_averages">averages:</h5>

Collect a second sample after one second so the rate keywords (requests_per_sec, bytes_per_sec) carry real values.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                          | Default Value                                                    |
|-----------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| <a id="check_iis_sites_filter"></a>[filter](../common-options.md#filter)                                        |                                                                  |
| <a id="check_iis_sites_warning"></a>[warning](../common-options.md#warning)                                     |                                                                  |
| <a id="check_iis_sites_warn"></a>[warn](../common-options.md#warn)                                              |                                                                  |
| <a id="check_iis_sites_critical"></a>[critical](../common-options.md#critical)                                  | state = 'stopped' and auto_start != 0                            |
| <a id="check_iis_sites_crit"></a>[crit](../common-options.md#crit)                                              |                                                                  |
| <a id="check_iis_sites_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                  |
| <a id="check_iis_sites_debug"></a>[debug](../common-options.md#debug)                                           | false                                                            |
| <a id="check_iis_sites_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                            |
| <a id="check_iis_sites_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                          |
| <a id="check_iis_sites_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                  |
| <a id="check_iis_sites_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                            |
| <a id="check_iis_sites_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                |
| <a id="check_iis_sites_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                               |
| <a id="check_iis_sites_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                  |
| <a id="check_iis_sites_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No web sites found                                               |
| <a id="check_iis_sites_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${site}: ${state}, ${connections} connections, uptime ${uptime}s |
| <a id="check_iis_sites_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${site}                                                          |
| <a id="check_iis_sites_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                  |
| <a id="check_iis_sites_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                  |
| <a id="check_iis_sites_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                               |
| <a id="check_iis_sites_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                  |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_sites_filter_keys"></a>
#### Filter keywords

| Option           | Description                                                                                            |
|------------------|--------------------------------------------------------------------------------------------------------|
| auto_start       | 1 when the site is set to start automatically, 0 when not, -1 when the IIS WMI provider is unavailable |
| bytes_per_sec    | Bytes sent+received per second (needs averages=true, otherwise 0)                                      |
| connections      | Current connections to the site                                                                        |
| requests_per_sec | Requests per second (needs averages=true, otherwise 0)                                                 |
| site             | Name of the web site                                                                                   |
| state            | running or stopped (stopped = the site has no Web Service counter instance)                            |
| uptime           | Seconds the site has been up (0 when stopped)                                                          |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_iis_worker_processes

Check IIS worker processes (active and served requests per w3wp).

#### About `check_iis_worker_processes`

`check_iis_worker_processes` reports one record per running IIS worker process
(w3wp.exe) from the `W3SVC_W3WP` performance counters. Counter instances are
named `<pid>_<pool>`; the check splits that into the `pid` and `pool`
keywords.

An *empty* result is OK by default: idle application pools spin their workers
down, so "no workers" is a normal state, not a failure (`empty-state=critical`
turns it into an alert for pools that must always be warm). The check goes
UNKNOWN with a clear message when the IIS role (and with it the counter set)
is missing.

There are no default thresholds — a sensible starting point is
`warning=active_requests > 50` scaled to your pools' concurrency.

**Jump to section:**

* [Sample Commands](#check_iis_worker_processes_samples)
* [Command-line Arguments](#check_iis_worker_processes_options)
* [Filter keywords](#check_iis_worker_processes_filter_keys)


<a id="check_iis_worker_processes_samples"></a>
#### Sample Commands

**Check the running IIS worker processes (w3wp):**

```
check_iis_worker_processes
OK: DefaultAppPool (pid 4711, 2 active)|'DefaultAppPool_4711_active_requests'=2;0;0
```

**No workers is a normal state (idle pools spin down), so the empty set is OK:**

```
check_iis_worker_processes
OK: No IIS worker processes running
```

**Alert when requests pile up inside a worker:**

```
check_iis_worker_processes "warning=active_requests > 50" "critical=active_requests > 200"
WARNING: DefaultAppPool (pid 4711, 73 active)|'DefaultAppPool_4711_active_requests'=73;50;200
```

**Scope to one pool's workers:**

```
check_iis_worker_processes "filter=pool = 'DefaultAppPool'" show-all
OK: DefaultAppPool (pid 4711): 2 active requests|'DefaultAppPool_4711_active_requests'=2;0;0
```

**On a host without the IIS role the check reports UNKNOWN with a clear message:**

```
check_iis_worker_processes
IIS performance counters (W3SVC_W3WP) not available - is the Web Server (IIS) role installed? (Failed to expand path: PDH 0xC0000BB8: c0000bb8: The specified object was not found on the computer.)
```



<a id="check_iis_worker_processes_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                     | Default Value                                            |
|----------------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------|
| <a id="check_iis_worker_processes_filter"></a>[filter](../common-options.md#filter)                                        |                                                          |
| <a id="check_iis_worker_processes_warning"></a>[warning](../common-options.md#warning)                                     |                                                          |
| <a id="check_iis_worker_processes_warn"></a>[warn](../common-options.md#warn)                                              |                                                          |
| <a id="check_iis_worker_processes_critical"></a>[critical](../common-options.md#critical)                                  |                                                          |
| <a id="check_iis_worker_processes_crit"></a>[crit](../common-options.md#crit)                                              |                                                          |
| <a id="check_iis_worker_processes_ok"></a>[ok](../common-options.md#ok)                                                    |                                                          |
| <a id="check_iis_worker_processes_debug"></a>[debug](../common-options.md#debug)                                           | false                                                    |
| <a id="check_iis_worker_processes_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                    |
| <a id="check_iis_worker_processes_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                       |
| <a id="check_iis_worker_processes_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                          |
| <a id="check_iis_worker_processes_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                    |
| <a id="check_iis_worker_processes_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                        |
| <a id="check_iis_worker_processes_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                       |
| <a id="check_iis_worker_processes_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                          |
| <a id="check_iis_worker_processes_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No IIS worker processes running                          |
| <a id="check_iis_worker_processes_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${pool} (pid ${pid}): ${active_requests} active requests |
| <a id="check_iis_worker_processes_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${pool}_${pid}                                           |
| <a id="check_iis_worker_processes_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                          |
| <a id="check_iis_worker_processes_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                          |
| <a id="check_iis_worker_processes_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                       |
| <a id="check_iis_worker_processes_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                          |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_iis_worker_processes_filter_keys"></a>
#### Filter keywords

| Option          | Description                                   |
|-----------------|-----------------------------------------------|
| active_requests | Requests currently executing in the worker    |
| instance        | Raw counter instance name (<pid>_<pool>)      |
| pid             | Process id of the w3wp worker                 |
| pool            | Application pool the worker serves            |
| total_requests  | HTTP requests served since the worker started |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nps_accounting

Check discarded NPS accounting requests and optional accounting log freshness.

Counts discarded NPS accounting requests (Security event 6275) during `window`
seconds. An accounting discard is critical by default. NPS must be installed,
failure auditing must be enabled, and the agent must be able to query audit policy
and read the Security log. Unavailable or incomplete event data returns UNKNOWN.

Optionally specify the **current** accounting `log-file` to report its existence,
size and modification age. A missing specified file is critical. Set
`require-traffic=true` to also alert on an empty file or one older than `max-age`
seconds (default 600). Leave this disabled for quiet installations. Select the
current filename when logs rotate; the command does not guess rotation patterns.

Without `log-file`, `log_state` is `not_checked`, and file size/age are `unknown`
with no corresponding performance data. A healthy discard count alone does not
prove that accounting writes succeed. File inspection also cannot establish
write permissions, SQL sink health, or that a particular request was persisted.
For SQL accounting, use CheckMSSQL to check the configured destination and record
arrival; use CheckDisk for free space. The command does not write synthetic
accounting records or modify NPS logging configuration.

The event scan shares the authentication check's event limit and deadline. A
Windows API/access error is UNKNOWN; explicit missing/empty/stale file states are
evaluated by the filter. Custom thresholds replace the defaults.

**Jump to section:**

* [Sample Commands](#check_nps_accounting_samples)
* [Command-line Arguments](#check_nps_accounting_options)
* [Filter keywords](#check_nps_accounting_filter_keys)


<a id="check_nps_accounting_samples"></a>
#### Sample Commands

#### Host without NPS

Captured on Windows without the role; the result is UNKNOWN.

```text
nscp client --module CheckWindowsApps --boot --query check_nps_accounting
NPS accounting data unavailable: NPS role is not installed (IAS service missing)
```

#### Accounting discard and log checks

Configuration examples; choose the current accounting filename after rotation:

```text
check_nps_accounting window=300
check_nps_accounting log-file=C:\Windows\System32\LogFiles\IN260928.log
check_nps_accounting log-file=C:\Windows\System32\LogFiles\IN260928.log require-traffic=true max-age=600
```

The first command evaluates discarded requests only. The second also checks the
specified file exists and reports its age/size. The third treats empty or stale
output as a problem because accounting traffic is expected.



<a id="check_nps_accounting_options"></a>
#### Command-line Arguments

<a id="check_nps_accounting_log-file"></a>

        
        
        
        
        
| Option                                                   | Default Value | Description                                                                                                                                 |
|----------------------------------------------------------|---------------|---------------------------------------------------------------------------------------------------------------------------------------------|
| [window](#check_nps_accounting_window)                   | 300           | Scan the last N seconds for discarded accounting requests.                                                                                  |
| [max-events](#check_nps_accounting_max-events)           | 100000        | Maximum events; exceeding the limit returns UNKNOWN.                                                                                        |
| log-file                                                 |               | Optional current accounting log file to inspect. Supply the current path after rotation; SQL logging is checked separately with CheckMSSQL. |
| [require-traffic](#check_nps_accounting_require-traffic) | false         | Check the selected file for empty/stale output when accounting traffic is expected.                                                         |
| [max-age](#check_nps_accounting_max-age)                 | 600           | Maximum log age in seconds when require-traffic=true.                                                                                       |



<h5 id="check_nps_accounting_window">window:</h5>

Scan the last N seconds for discarded accounting requests.

*Default Value:* `300`

<h5 id="check_nps_accounting_max-events">max-events:</h5>

Maximum events; exceeding the limit returns UNKNOWN.

*Default Value:* `100000`

<h5 id="check_nps_accounting_require-traffic">require-traffic:</h5>

Check the selected file for empty/stale output when accounting traffic is expected.

*Default Value:* `false`

<h5 id="check_nps_accounting_max-age">max-age:</h5>

Maximum log age in seconds when require-traffic=true.

*Default Value:* `600`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                               | Default Value                                                                                  |
|----------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------|
| <a id="check_nps_accounting_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                |
| <a id="check_nps_accounting_warning"></a>[warning](../common-options.md#warning)                                     |                                                                                                |
| <a id="check_nps_accounting_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                |
| <a id="check_nps_accounting_critical"></a>[critical](../common-options.md#critical)                                  | accounting_discards > 0 or log_state in ('missing', 'empty', 'stale')                          |
| <a id="check_nps_accounting_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                |
| <a id="check_nps_accounting_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                |
| <a id="check_nps_accounting_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                          |
| <a id="check_nps_accounting_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                          |
| <a id="check_nps_accounting_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                        |
| <a id="check_nps_accounting_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                |
| <a id="check_nps_accounting_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                          |
| <a id="check_nps_accounting_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                              |
| <a id="check_nps_accounting_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                             |
| <a id="check_nps_accounting_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                |
| <a id="check_nps_accounting_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No NPS accounting data                                                                         |
| <a id="check_nps_accounting_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${accounting_discards} accounting discards, log=${log_state}, age=${log_age}, size=${log_size} |
| <a id="check_nps_accounting_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | nps                                                                                            |
| <a id="check_nps_accounting_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                |
| <a id="check_nps_accounting_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                |
| <a id="check_nps_accounting_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                             |
| <a id="check_nps_accounting_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_nps_accounting_filter_keys"></a>
#### Filter keywords

| Option              | Description                                                                                    |
|---------------------|------------------------------------------------------------------------------------------------|
| accounting_discards | Discarded accounting requests (event 6275)                                                     |
| log_age             | Seconds since the selected log file was modified; unknown when not available                   |
| log_size            | Selected log file size in bytes; unknown when not available                                    |
| log_state           | not_checked, ok, missing, empty, or stale; freshness/empty checks require require-traffic=true |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nps_auth

Check NPS authentication outcomes, rejection percentages and reason codes over a time window.

Summarizes local NPS Security audit events in a fixed window ending when the check
starts. `window` is in seconds (default 300). Events 6272, 6273 and 6274 represent
accepted, rejected and discarded authentication requests. XML field names are
parsed directly; localized rendered messages are not used.

The default output is one `all` record, including a quiet window with zero events.
`group-by=client`, `policy` or `reason` produces one record per observed group.
Missing grouping fields are reported as `unknown`. A group absent from the window
cannot be detected without an expected-client inventory; grouped checks therefore
return the configured empty state when no groups are found.

`reject_pct` is `100 * rejected / (accepted + rejected)`. Discards are reported
separately and excluded from that denominator. With no decisions the percentage
is zero; inspect `decisions` or `requests` to distinguish this from successful
traffic. Default percentage alerts apply only after `min-requests` decisions per
group (default 20): warning above 10%, critical above 25%. Any discarded request
is critical by default. `top_reason` is the most frequent numeric reject/discard
reason code, with lexical tie-breaking, or `none` without failures.

Use `require-traffic=true` only where authentication traffic is expected. It adds
a default critical condition for zero requests and requires `group-by=all`.
Custom warning/critical expressions replace the defaults, including their
minimum-volume and traffic guards.

The IAS service must be installed, the agent must be able to query audit policy
and read Security events, and NPS success **and** failure auditing must be enabled.
Missing prerequisites return UNKNOWN even if `empty-state=ok` is requested.
Enable the Network Policy Server audit subcategory through local/group policy.
Run `check_service service=IAS` separately to monitor service state.

The scan is bounded by `max-events` (default 100000) and a 15-second enumeration
deadline. A failed, malformed or truncated scan returns UNKNOWN, never partial
counts. Windows query setup itself may take additional time. The check counts
available audit records; it cannot reconstruct cleared/overwritten logs or events
from periods when auditing was disabled. It does not maintain bookmarks or
represent every packet received at the UDP listener.

**Jump to section:**

* [Sample Commands](#check_nps_auth_samples)
* [Command-line Arguments](#check_nps_auth_options)
* [Filter keywords](#check_nps_auth_filter_keys)


<a id="check_nps_auth_samples"></a>
#### Sample Commands

#### Host without NPS

Captured with the one-shot client on Windows without the NPS role. The command
returns UNKNOWN; the client prints the check message without adding a status word.

```text
nscp client --module CheckWindowsApps --boot --query check_nps_auth
NPS authentication data unavailable: NPS role is not installed (IAS service missing)
```

#### Aggregate and per-client policy

Configuration examples for an NPS host with auditing enabled:

```text
check_nps_auth window=300 min-requests=20
check_nps_auth window=300 group-by=client min-requests=50
check_nps_auth window=600 group-by=policy "filter=group = 'Corporate WiFi'"
check_nps_auth window=300 require-traffic=true
```

Custom percentage thresholds should preserve the minimum-volume guard and the
independent discard alert:

```text
check_nps_auth "warning=decisions >= min_requests and reject_pct > 15" "critical=discarded > 0 or (decisions >= min_requests and reject_pct > 30)"
```

Use the separate IAS service check and certificate check alongside the traffic
summary. A quiet authentication window alone does not establish service health.



<a id="check_nps_auth_options"></a>
#### Command-line Arguments

        
        
        
        
        
| Option                                             | Default Value | Description                                                                            |
|----------------------------------------------------|---------------|----------------------------------------------------------------------------------------|
| [window](#check_nps_auth_window)                   | 300           | Scan the last N seconds (1..86400), with a fixed start and end time.                   |
| [max-events](#check_nps_auth_max-events)           | 100000        | Fail UNKNOWN instead of reporting partial counts if this many events is exceeded.      |
| [min-requests](#check_nps_auth_min-requests)       | 20            | Minimum accepted+rejected events per group before default percentage thresholds apply. |
| [group-by](#check_nps_auth_group-by)               | all           | Aggregate by all, client, policy, or reason (numeric reason code).                     |
| [require-traffic](#check_nps_auth_require-traffic) | false         | Alert on a quiet aggregate window; only supported with group-by=all.                   |



<h5 id="check_nps_auth_window">window:</h5>

Scan the last N seconds (1..86400), with a fixed start and end time.

*Default Value:* `300`

<h5 id="check_nps_auth_max-events">max-events:</h5>

Fail UNKNOWN instead of reporting partial counts if this many events is exceeded.

*Default Value:* `100000`

<h5 id="check_nps_auth_min-requests">min-requests:</h5>

Minimum accepted+rejected events per group before default percentage thresholds apply.

*Default Value:* `20`

<h5 id="check_nps_auth_group-by">group-by:</h5>

Aggregate by all, client, policy, or reason (numeric reason code).

*Default Value:* `all`

<h5 id="check_nps_auth_require-traffic">require-traffic:</h5>

Alert on a quiet aggregate window; only supported with group-by=all.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                         | Default Value                                                                                                                 |
|----------------------------------------------------------------------------------------------------------------|-------------------------------------------------------------------------------------------------------------------------------|
| <a id="check_nps_auth_filter"></a>[filter](../common-options.md#filter)                                        |                                                                                                                               |
| <a id="check_nps_auth_warning"></a>[warning](../common-options.md#warning)                                     | decisions >= min_requests and reject_pct > 10                                                                                 |
| <a id="check_nps_auth_warn"></a>[warn](../common-options.md#warn)                                              |                                                                                                                               |
| <a id="check_nps_auth_critical"></a>[critical](../common-options.md#critical)                                  | discarded > 0 or (decisions >= min_requests and reject_pct > 25) or (require_traffic = 1 and requests = 0)                    |
| <a id="check_nps_auth_crit"></a>[crit](../common-options.md#crit)                                              |                                                                                                                               |
| <a id="check_nps_auth_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                                                                               |
| <a id="check_nps_auth_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                                                                         |
| <a id="check_nps_auth_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                                                                         |
| <a id="check_nps_auth_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                                                                       |
| <a id="check_nps_auth_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                                                                               |
| <a id="check_nps_auth_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                                                                         |
| <a id="check_nps_auth_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                                                                             |
| <a id="check_nps_auth_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                                                                            |
| <a id="check_nps_auth_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                                                                               |
| <a id="check_nps_auth_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No NPS groups found in window                                                                                                 |
| <a id="check_nps_auth_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${group}: ${accepted} accepted, ${rejected} rejected, ${discarded} discarded, reject=${reject_pct}%, top reason=${top_reason} |
| <a id="check_nps_auth_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${group}                                                                                                                      |
| <a id="check_nps_auth_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                                                                               |
| <a id="check_nps_auth_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                                                                               |
| <a id="check_nps_auth_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                                                                            |
| <a id="check_nps_auth_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                                                                               |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_nps_auth_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                       |
|-----------------|-----------------------------------------------------------------------------------|
| accepted        | Access-granted events (6272)                                                      |
| decisions       | Accepted + rejected events; percentage denominator                                |
| discarded       | Discarded authentication events (6274)                                            |
| group           | all, client, policy, or reason-code group                                         |
| min_requests    | Minimum decisions for default percentage thresholds                               |
| reject_pct      | 100 * rejected / (accepted + rejected), or 0 when no decisions; excludes discards |
| rejected        | Access-denied events (6273)                                                       |
| requests        | Accepted + rejected + discarded events                                            |
| require_traffic | Whether zero authentication events should alert                                   |
| top_reason      | Most frequent reject/discard reason code; none without failures                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_nps_counters

Check the installed NPS/RADIUS performance counters using two samples.

Enumerates and reads counters from an installed NPS performance object. The
default `object` is `NPS Authentication Server`; select
`object=NPS Accounting Server` for accounting. On localized Windows, supply the
installed localized object name if the English name is unavailable. Counter and
instance names are those provided by Windows, allowing version-specific fields
without assuming that every server exposes the same names.

Two PDH samples are always collected, separated by `sample-ms` (default 1000),
so rate counters are measured. Missing objects, failed samples, and unavailable
values return UNKNOWN instead of being represented as zero. The IAS service must
be installed. An object with no values returns the configured empty state,
UNKNOWN by default.

Each record exposes an object, counter, instance, and formatted numeric value.
The label includes all three names to avoid performance-data collisions. Aggregate
instances such as `_Total` are included: select the aggregate or individual
clients with `filter`, and do not sum both. There are no default numeric thresholds
because counters have different units and meanings. Match a counter by name when
setting `warning` or `critical`. Lifetime totals are not converted into interval
counts; rate counters already carry the rate computed by PDH.

For arbitrary counter paths or long-term collector averages, use CheckSystem's
`check_pdh`. For accepted/rejected percentages and reason codes, use
`check_nps_auth`.

**Jump to section:**

* [Sample Commands](#check_nps_counters_samples)
* [Command-line Arguments](#check_nps_counters_options)
* [Filter keywords](#check_nps_counters_filter_keys)


<a id="check_nps_counters_samples"></a>
#### Sample Commands

#### Host without NPS

Captured on Windows without the role; the result is UNKNOWN.

```text
nscp client --module CheckWindowsApps --boot --query check_nps_counters
NPS counters unavailable: NPS role is not installed (IAS service missing)
```

#### Select an installed counter object

Configuration examples for an English Windows NPS host:

```text
check_nps_counters
check_nps_counters "object=NPS Accounting Server" sample-ms=1000
```

Use the returned `counter` and `instance` names when setting a filter and
threshold. Counter names and available instances depend on the Windows version
and language. Select individual clients or `_Total` when available, avoiding
double-counting both in a graph.



<a id="check_nps_counters_options"></a>
#### Command-line Arguments

        
        
| Option                                     | Default Value             | Description                                                                                      |
|--------------------------------------------|---------------------------|--------------------------------------------------------------------------------------------------|
| [object](#check_nps_counters_object)       | NPS Authentication Server | Installed NPS performance object; use NPS Accounting Server for accounting, or a localized name. |
| [sample-ms](#check_nps_counters_sample-ms) | 1000                      | Delay between the two PDH samples (100..10000 milliseconds).                                     |



<h5 id="check_nps_counters_object">object:</h5>

Installed NPS performance object; use NPS Accounting Server for accounting, or a localized name.

*Default Value:* `NPS Authentication Server`

<h5 id="check_nps_counters_sample-ms">sample-ms:</h5>

Delay between the two PDH samples (100..10000 milliseconds).

*Default Value:* `1000`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value         |
|--------------------------------------------------------------------------------------------------------------------|-----------------------|
| <a id="check_nps_counters_filter"></a>[filter](../common-options.md#filter)                                        |                       |
| <a id="check_nps_counters_warning"></a>[warning](../common-options.md#warning)                                     |                       |
| <a id="check_nps_counters_warn"></a>[warn](../common-options.md#warn)                                              |                       |
| <a id="check_nps_counters_critical"></a>[critical](../common-options.md#critical)                                  |                       |
| <a id="check_nps_counters_crit"></a>[crit](../common-options.md#crit)                                              |                       |
| <a id="check_nps_counters_ok"></a>[ok](../common-options.md#ok)                                                    |                       |
| <a id="check_nps_counters_debug"></a>[debug](../common-options.md#debug)                                           | false                 |
| <a id="check_nps_counters_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                 |
| <a id="check_nps_counters_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown               |
| <a id="check_nps_counters_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                       |
| <a id="check_nps_counters_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                 |
| <a id="check_nps_counters_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                     |
| <a id="check_nps_counters_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}    |
| <a id="check_nps_counters_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                       |
| <a id="check_nps_counters_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No NPS counters found |
| <a id="check_nps_counters_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${label}=${value}     |
| <a id="check_nps_counters_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${label}              |
| <a id="check_nps_counters_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                       |
| <a id="check_nps_counters_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                       |
| <a id="check_nps_counters_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                    |
| <a id="check_nps_counters_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                       |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_nps_counters_filter_keys"></a>
#### Filter keywords

| Option   | Description                                                                 |
|----------|-----------------------------------------------------------------------------|
| counter  | Installed counter name                                                      |
| instance | Counter instance, including _Total; empty for a single-instance object      |
| label    | Object, counter and instance used for a unique performance label            |
| object   | Installed performance object name                                           |
| value    | Formatted PDH value after two samples; units depend on the selected counter |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_broker

Check the Remote Desktop Connection Broker counterset (failed/pending connections).

#### About `check_rds_broker`

`check_rds_broker` reads the "Remote Desktop Connection Broker Counterset"
performance object on an RD Connection Broker and reports one record per
counter (per instance where the counterset has instances). The broker's
counter names vary between Windows Server versions, so the check
**enumerates** whatever this broker exposes instead of hard-coding names —
run it once with `show-all` to see your version's counters, then select with
the `counter` keyword.

Run it on the Connection Broker itself; on any other host the counterset does
not exist and the check returns UNKNOWN with a clear "is this host an RD
Connection Broker?" message.

Pass `averages=true` to collect a second sample after one second so rate
counters carry real values. Typical alerts target the failed/pending request
counters, e.g. `critical=counter like 'Failed' and value > 0`.

**Jump to section:**

* [Sample Commands](#check_rds_broker_samples)
* [Command-line Arguments](#check_rds_broker_options)
* [Filter keywords](#check_rds_broker_filter_keys)


<a id="check_rds_broker_samples"></a>
#### Sample Commands

**List the Connection Broker counters (one record per counter, whatever this broker exposes):**

```
check_rds_broker show-all
OK: RDCB Connection Requests Failed = 0, RDCB Connection Requests Pending = 1, RDCB Connection Requests Total = 4211|'RDCB Connection Requests Failed'=0;0;0 'RDCB Connection Requests Pending'=1;0;0 'RDCB Connection Requests Total'=4211;0;0
```

**Alert on failed or piling-up connection requests:**

```
check_rds_broker "warning=counter like 'Pending' and value > 20" "critical=counter like 'Failed' and value > 0"
CRITICAL: RDCB Connection Requests Failed = 17|'RDCB Connection Requests Failed'=17;0;0 ...
```

**Sample rate counters over a second:**

```
check_rds_broker averages=true "filter=counter like 'Requests'"
OK: RDCB Connection Requests Total = 4211|'RDCB Connection Requests Total'=4211;0;0 ...
```

**On a host that is not a Connection Broker the check reports UNKNOWN with a clear message:**

```
check_rds_broker
Connection Broker counters (Remote Desktop Connection Broker Counterset) not available - is this host an RD Connection Broker? (Failed to enumerate object: Remote Desktop Connection Broker Counterset)
```



<a id="check_rds_broker_options"></a>
#### Command-line Arguments

        
| Option                                 | Default Value | Description                                                                  |
|----------------------------------------|---------------|------------------------------------------------------------------------------|
| [averages](#check_rds_broker_averages) | false         | Collect a second sample after one second so rate counters carry real values. |



<h5 id="check_rds_broker_averages">averages:</h5>

Collect a second sample after one second so rate counters carry real values.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                           | Default Value                       |
|------------------------------------------------------------------------------------------------------------------|-------------------------------------|
| <a id="check_rds_broker_filter"></a>[filter](../common-options.md#filter)                                        |                                     |
| <a id="check_rds_broker_warning"></a>[warning](../common-options.md#warning)                                     |                                     |
| <a id="check_rds_broker_warn"></a>[warn](../common-options.md#warn)                                              |                                     |
| <a id="check_rds_broker_critical"></a>[critical](../common-options.md#critical)                                  |                                     |
| <a id="check_rds_broker_crit"></a>[crit](../common-options.md#crit)                                              |                                     |
| <a id="check_rds_broker_ok"></a>[ok](../common-options.md#ok)                                                    |                                     |
| <a id="check_rds_broker_debug"></a>[debug](../common-options.md#debug)                                           | false                               |
| <a id="check_rds_broker_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                               |
| <a id="check_rds_broker_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                             |
| <a id="check_rds_broker_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                     |
| <a id="check_rds_broker_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                               |
| <a id="check_rds_broker_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                   |
| <a id="check_rds_broker_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                  |
| <a id="check_rds_broker_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                     |
| <a id="check_rds_broker_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No Connection Broker counters found |
| <a id="check_rds_broker_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${label} = ${value}                 |
| <a id="check_rds_broker_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${label}                            |
| <a id="check_rds_broker_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                     |
| <a id="check_rds_broker_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                     |
| <a id="check_rds_broker_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                  |
| <a id="check_rds_broker_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                     |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_broker_filter_keys"></a>
#### Filter keywords

| Option   | Description                                                                                            |
|----------|--------------------------------------------------------------------------------------------------------|
| counter  | Name of the broker counter                                                                             |
| instance | Counter instance (empty for the single-instance counters)                                              |
| label    | Counter name suffixed with the instance name when the counterset is multi-instance (unique per record) |
| value    | Value of the counter                                                                                   |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_licenses

Check Remote Desktop licensing (CAL key packs: issued versus available licenses).

#### About `check_rds_licenses`

`check_rds_licenses` reads the Remote Desktop licensing key packs from WMI
(`Win32_TSLicenseKeyPack` in `root\cimv2`) and reports one record per key pack
with the issued versus available CAL counts. Run it on the RD **licensing**
server (the host holding the "Remote Desktop Licensing" role) — on any other
host the class does not exist and the check returns UNKNOWN with a clear
"role is not installed" message.

CAL exhaustion locks new users out of an RDS farm, so the default thresholds
alert on the available count: **warning** when `available < 10 and total_licenses > 0`,
**critical** when `available = 0 and total_licenses > 0`. The `total_licenses > 0`
guard keeps
the built-in/unlimited key packs (which report no meaningful counts) from
tripping the thresholds.

Per-user CALs are not enforced by the session host, so `issued` growing past
`total_licenses` is possible in per-user mode; alert on `available` (as the defaults
do) or on `issued` explicitly if you track compliance.

**Jump to section:**

* [Sample Commands](#check_rds_licenses_samples)
* [Command-line Arguments](#check_rds_licenses_options)
* [Filter keywords](#check_rds_licenses_filter_keys)


<a id="check_rds_licenses_samples"></a>
#### Sample Commands

**Check CAL key packs on an RD licensing server (defaults: warn below 10 available, critical when exhausted):**

```
check_rds_licenses
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=25;0;0 'RDS Per User CAL_available'=25;10;0
```

**A key pack running out of licenses trips the default thresholds:**

```
check_rds_licenses
CRITICAL: RDS Per User CAL: 50/50 issued, 0 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=50;0;0 'RDS Per User CAL_available'=0;10;0
```

**Custom thresholds, e.g. warn when more than 80% of a pack is issued:**

```
check_rds_licenses "warning=issued > 40 and total_licenses > 0" "critical=available = 0 and total_licenses > 0"
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_issued'=25;40;0 'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_available'=25;0;0
```

**Scope the check to a product version or exclude the built-in pack:**

```
check_rds_licenses "filter=product_version like '2022' and type != 'built-in'"
OK: RDS Per User CAL: 25/50 issued, 25 available|'RDS Per User CAL_total'=50;0;0 'RDS Per User CAL_issued'=25;0;0 'RDS Per User CAL_available'=25;10;0
```

**On a host without the RD Licensing role the check reports UNKNOWN with a clear message:**

```
check_rds_licenses
Remote Desktop licensing information not available: the Remote Desktop licensing role is not installed (Win32_TSLicenseKeyPack missing)
```



<a id="check_rds_licenses_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                              |
|--------------------------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------|
| <a id="check_rds_licenses_filter"></a>[filter](../common-options.md#filter)                                        |                                                                            |
| <a id="check_rds_licenses_warning"></a>[warning](../common-options.md#warning)                                     | available < 10 and total_licenses > 0                                      |
| <a id="check_rds_licenses_warn"></a>[warn](../common-options.md#warn)                                              |                                                                            |
| <a id="check_rds_licenses_critical"></a>[critical](../common-options.md#critical)                                  | available = 0 and total_licenses > 0                                       |
| <a id="check_rds_licenses_crit"></a>[crit](../common-options.md#crit)                                              |                                                                            |
| <a id="check_rds_licenses_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                            |
| <a id="check_rds_licenses_debug"></a>[debug](../common-options.md#debug)                                           | false                                                                      |
| <a id="check_rds_licenses_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                                      |
| <a id="check_rds_licenses_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                                    |
| <a id="check_rds_licenses_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                            |
| <a id="check_rds_licenses_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                                      |
| <a id="check_rds_licenses_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                          |
| <a id="check_rds_licenses_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                                         |
| <a id="check_rds_licenses_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                            |
| <a id="check_rds_licenses_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No license key packs found                                                 |
| <a id="check_rds_licenses_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${description}: ${issued}/${total_licenses} issued, ${available} available |
| <a id="check_rds_licenses_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${description}                                                             |
| <a id="check_rds_licenses_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                            |
| <a id="check_rds_licenses_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                            |
| <a id="check_rds_licenses_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                                         |
| <a id="check_rds_licenses_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                            |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_licenses_filter_keys"></a>
#### Filter keywords

| Option          | Description                                                                     |
|-----------------|---------------------------------------------------------------------------------|
| available       | Licenses still available                                                        |
| description     | License type and model (e.g. 'RDS Per User CAL')                                |
| id              | Key pack id                                                                     |
| issued          | Licenses issued to clients                                                      |
| keypack_type    | Raw KeyPackType value                                                           |
| product_version | Product version the key pack applies to                                         |
| total_licenses  | Total licenses in the key pack                                                  |
| type            | Key pack type: unknown, retail, volume, concurrent, temporary, open or built-in |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_session_load

Check per-session resource usage (CPU, working set, protocol bytes).

#### About `check_rds_session_load`

`check_rds_session_load` reports one record per "Terminal Services Session"
counter instance: the console, the session-0 `Services` aggregate, and one
`RDP-Tcp <n>` instance per remote session. It answers "which session is
eating the host?" — the per-session CPU and working set that plain
`check_process` cannot attribute to a user.

Pass `sessions-only=true` to skip the `Services` aggregate (system processes,
not a user session), and `averages=true` to sample CPU over a full second
(the check then takes about a second longer; without it the first PDH sample
is used, which can be noisy).

Counter instances are named after the connection, not the user; correlate the
`RDP-Tcp <n>` number with `quser`/`qwinsta` output to find who it is. The
protocol counters (`total_bytes`) only exist on hosts with the RD Session Host
role; on other machines the keyword reads 0.

**Jump to section:**

* [Sample Commands](#check_rds_session_load_samples)
* [Command-line Arguments](#check_rds_session_load_options)
* [Filter keywords](#check_rds_session_load_filter_keys)


<a id="check_rds_session_load_samples"></a>
#### Sample Commands

**Show per-session resource usage (one record per counter instance):**

```
check_rds_session_load
OK: Console: 6.064% cpu, 18833346560B working set, Services: 1.68039% cpu, 6785593344B working set|'Console_working_set'=18833346560B;0;0 'Services_working_set'=6785593344B;0;0
```

**Only real sessions (skip the session-0 'Services' aggregate) and sample CPU over a second:**

```
check_rds_session_load sessions-only=true averages=true
OK: Console: 7.88642% cpu, 18829905920B working set|'Console_working_set'=18829905920B;0;0
```

**Find the runaway session eating the host:**

```
check_rds_session_load sessions-only=true averages=true "warning=cpu > 50" "critical=cpu > 80"
WARNING: RDP-Tcp 55: 63.2% cpu, 4831838208B working set|'RDP-Tcp 55_cpu'=63.2%;50;80 'RDP-Tcp 55_working_set'=4831838208B;0;0 ...
```

**Alert on per-session memory:**

```
check_rds_session_load sessions-only=true "warning=working_set > 8000000000"
OK: Console: 6.1% cpu, 4833346560B working set|'Console_working_set'=4833346560B;8000000000;0
```

**On a host without the counters the check reports UNKNOWN with a clear message:**

```
check_rds_session_load
Remote Desktop Services counters (Terminal Services Session) not available - is the role installed on this host? (...)
```



<a id="check_rds_session_load_options"></a>
#### Command-line Arguments

        
        
| Option                                                 | Default Value | Description                                                                                                 |
|--------------------------------------------------------|---------------|-------------------------------------------------------------------------------------------------------------|
| [averages](#check_rds_session_load_averages)           | false         | Collect a second sample after one second so the cpu keyword carries a real value.                           |
| [sessions-only](#check_rds_session_load_sessions-only) | false         | Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions. |



<h5 id="check_rds_session_load_averages">averages:</h5>

Collect a second sample after one second so the cpu keyword carries a real value.

*Default Value:* `false`

<h5 id="check_rds_session_load_sessions-only">sessions-only:</h5>

Skip the 'Services' aggregate instance (session 0 / system processes) and report only console/RDP sessions.

*Default Value:* `false`


**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                                 | Default Value                                        |
|------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------|
| <a id="check_rds_session_load_filter"></a>[filter](../common-options.md#filter)                                        |                                                      |
| <a id="check_rds_session_load_warning"></a>[warning](../common-options.md#warning)                                     |                                                      |
| <a id="check_rds_session_load_warn"></a>[warn](../common-options.md#warn)                                              |                                                      |
| <a id="check_rds_session_load_critical"></a>[critical](../common-options.md#critical)                                  |                                                      |
| <a id="check_rds_session_load_crit"></a>[crit](../common-options.md#crit)                                              |                                                      |
| <a id="check_rds_session_load_ok"></a>[ok](../common-options.md#ok)                                                    |                                                      |
| <a id="check_rds_session_load_debug"></a>[debug](../common-options.md#debug)                                           | false                                                |
| <a id="check_rds_session_load_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                |
| <a id="check_rds_session_load_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | ok                                                   |
| <a id="check_rds_session_load_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                      |
| <a id="check_rds_session_load_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                |
| <a id="check_rds_session_load_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                    |
| <a id="check_rds_session_load_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                   |
| <a id="check_rds_session_load_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                      |
| <a id="check_rds_session_load_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No sessions found                                    |
| <a id="check_rds_session_load_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${session}: ${cpu}% cpu, ${working_set}B working set |
| <a id="check_rds_session_load_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | ${session}                                           |
| <a id="check_rds_session_load_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                      |
| <a id="check_rds_session_load_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                      |
| <a id="check_rds_session_load_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                   |
| <a id="check_rds_session_load_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                      |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_session_load_filter_keys"></a>
#### Filter keywords

| Option      | Description                                                        |
|-------------|--------------------------------------------------------------------|
| cpu         | % processor time of the session (needs averages=true, otherwise 0) |
| session     | Counter instance name (Console, Services, RDP-Tcp <n>, ...)        |
| total_bytes | Protocol bytes in+out since the session connected                  |
| working_set | Working set of the session in bytes                                |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

### check_rds_sessions

Check session counts on a session host (active, inactive, total).

#### About `check_rds_sessions`

`check_rds_sessions` reads the "Terminal Services" performance counters
(Active/Inactive/Total Sessions) and reports the session-count picture of a
Remote Desktop session host in one record. The counter object exists on every
Windows SKU (the console counts as a session), so the check also works on
plain servers — the numbers only become interesting on session hosts.

There are no default thresholds; all three values are emitted as perfdata so
capacity can be graphed even on an all-OK check.

Disconnected (`inactive`) sessions still hold memory, licenses and (for
per-device CALs) a seat, so `warning=inactive > <n>` is a useful signal that
idle-session limits are not configured or not working. For per-session
resource usage see `check_rds_session_load`; for CAL exhaustion see
`check_rds_licenses`.

**Jump to section:**

* [Sample Commands](#check_rds_sessions_samples)
* [Command-line Arguments](#check_rds_sessions_options)
* [Filter keywords](#check_rds_sessions_filter_keys)


<a id="check_rds_sessions_samples"></a>
#### Sample Commands

**Check session counts on a session host:**

```
check_rds_sessions
OK: 1 active, 1 inactive (2 total)|'sessions_active'=1;0;0 'sessions_inactive'=1;0;0 'sessions_total'=2;0;0
```

**Alert when the host approaches its session capacity:**

```
check_rds_sessions "warning=active > 40" "critical=active > 50"
OK: 32 active, 5 inactive (37 total)|'sessions_active'=32;40;50 'sessions_inactive'=5;0;0 'sessions_total'=37;0;0
```

**Alert on disconnected sessions piling up (they still hold memory and CALs):**

```
check_rds_sessions "warning=inactive > 10"
WARNING: 12 active, 14 inactive (26 total)|'sessions_inactive'=14;10;0 'sessions_active'=12;0;0 'sessions_total'=26;0;0
```

**On a host without the counters the check reports UNKNOWN with a clear message:**

```
check_rds_sessions
Remote Desktop Services counters (Terminal Services) not available - is the role installed on this host? (...)
```



<a id="check_rds_sessions_options"></a>
#### Command-line Arguments

**Common options:**

These options are shared by all filter based commands and are described on the [common options](../common-options.md#common-options) page; the default values below are specific to this command.


| Option                                                                                                             | Default Value                                                    |
|--------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------|
| <a id="check_rds_sessions_filter"></a>[filter](../common-options.md#filter)                                        |                                                                  |
| <a id="check_rds_sessions_warning"></a>[warning](../common-options.md#warning)                                     |                                                                  |
| <a id="check_rds_sessions_warn"></a>[warn](../common-options.md#warn)                                              |                                                                  |
| <a id="check_rds_sessions_critical"></a>[critical](../common-options.md#critical)                                  |                                                                  |
| <a id="check_rds_sessions_crit"></a>[crit](../common-options.md#crit)                                              |                                                                  |
| <a id="check_rds_sessions_ok"></a>[ok](../common-options.md#ok)                                                    |                                                                  |
| <a id="check_rds_sessions_debug"></a>[debug](../common-options.md#debug)                                           | false                                                            |
| <a id="check_rds_sessions_show-all"></a>[show-all](../common-options.md#show-all)                                  | false                                                            |
| <a id="check_rds_sessions_empty-state"></a>[empty-state](../common-options.md#empty-state)                         | unknown                                                          |
| <a id="check_rds_sessions_perf-config"></a>[perf-config](../common-options.md#perf-config)                         |                                                                  |
| <a id="check_rds_sessions_escape-html"></a>[escape-html](../common-options.md#escape-html)                         | false                                                            |
| <a id="check_rds_sessions_list-separator"></a>[list-separator](../common-options.md#list-separator)                | ,                                                                |
| <a id="check_rds_sessions_top-syntax"></a>[top-syntax](../common-options.md#top-syntax)                            | ${status}: ${list}                                               |
| <a id="check_rds_sessions_ok-syntax"></a>[ok-syntax](../common-options.md#ok-syntax)                               |                                                                  |
| <a id="check_rds_sessions_empty-syntax"></a>[empty-syntax](../common-options.md#empty-syntax)                      | No session counters found                                        |
| <a id="check_rds_sessions_detail-syntax"></a>[detail-syntax](../common-options.md#detail-syntax)                   | ${active} active, ${inactive} inactive (${total_sessions} total) |
| <a id="check_rds_sessions_perf-syntax"></a>[perf-syntax](../common-options.md#perf-syntax)                         | sessions                                                         |
| <a id="check_rds_sessions_byte-unit"></a>[byte-unit](../common-options.md#byte-unit)                               |                                                                  |
| <a id="check_rds_sessions_decimal-separator"></a>[decimal-separator](../common-options.md#decimal-separator)       |                                                                  |
| <a id="check_rds_sessions_decimals"></a>[decimals](../common-options.md#decimals)                                  | -1                                                               |
| <a id="check_rds_sessions_thousands-separator"></a>[thousands-separator](../common-options.md#thousands-separator) |                                                                  |


This command also accepts the standard [help options](../common-options.md#standard-options): help, help-pb, show-default, help-short.


<a id="check_rds_sessions_filter_keys"></a>
#### Filter keywords

| Option         | Description                                          |
|----------------|------------------------------------------------------|
| active         | Sessions with a connected user                       |
| inactive       | Disconnected (idle) sessions still holding resources |
| total_sessions | Total sessions on the host                           |

This command also supports the [common filter keywords](../common-options.md#common-filter-keywords): count, total, ok_count, warn_count, crit_count, problem_count, list, ok_list, warn_list, crit_list, problem_list, detail_list, sep, status.

