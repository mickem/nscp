# Host Facts

**Facts are the agent's inventory of the machine it runs on**: what OS it is,
what hardware it sits on, which volumes and network interfaces it has, what is
installed on it, which services it runs (a docker daemon and its containers, a
MySQL or SQL Server instance and its databases), and which NSClient++ is running on it. The core collects them
from the loaded modules, keeps them as one document per host, and serves that
document on [`/api/v2/facts`](../api/rest/facts.md), in the web UI and in
`nscp test`.

**Nothing is collected until you turn a fact set on.** An inventory is data an
operator did not necessarily agree to ship, so a fresh install reports an empty
document and does no work for it.

---

## Facts or tags?

Tags and facts both describe the host. They answer different questions, and
facts do not replace tags.

|                       | Tags                                          | Facts                                                  |
|-----------------------|-----------------------------------------------|--------------------------------------------------------|
| Shape                 | flat `key=value` strings                      | a document: sections, numbers, strings, lists of records |
| Question              | *which group is this host in*                 | *what is this host*                                    |
| Collected             | always, a handful per module                  | only the sets you enable                               |
| Used for              | fleet group selectors (`os_family = "linux"`) | inventory, and deciding what to monitor                |
| Sent to a fleet server | whole, on every state report                  | the hash on every report, the document when it changes |

A fleet selector matches a tag whole, which is why `os_family` and `arch` are
tags. "Which volumes does this host have" is a list of records, and a
comma-joined tag such as `drives=c:,d:` is exactly the shape facts exist to get
away from.

---

## Enabling a fact set

A fact set is enabled **in the module that produces it**, next to that module's
other settings. The switch cannot exist without the module, so there is no way
to enable a set and get nothing back because its module is not loaded.

```ini
[/modules]
CheckSystem = enabled
CheckDisk = enabled

; CheckSystem on Windows. On Linux: [/settings/system/unix/facts]
[/settings/system/windows/facts]
os = true
hardware = true
network.interfaces = true
software.installed = true

[/settings/disk/facts]
storage.volumes = true

; CheckHyperV, on a Hyper-V host.
[/settings/hyperv/facts]
hyperv.vms = true

; A service the host runs, in the module that already checks it.
[/settings/docker/facts]
docker = true
docker.containers = true
docker.images = true

[/settings/mysql/facts]
mysql = true
mysql.databases = true

[/settings/mssql/facts]
mssql = true
mssql.databases = true

; The one set the core produces itself.
[/settings/facts]
agent = true
```

Every switch is a bool defaulting to `false`, so each one is a separate key
that a [fleet bundle](../setup/fleet.md) can turn on without overwriting
another bundle's choices.

| Set                  | Produced by | Key, in section                                  | Cost |
|----------------------|-------------|--------------------------------------------------|------|
| `agent`              | the core    | `agent`, `[/settings/facts]`                     | none |
| `os`                 | CheckSystem | `os`, `[/settings/system/windows/facts]` or `[/settings/system/unix/facts]` | none: read once at start |
| `hardware`           | CheckSystem | `hardware`, same section                         | none: read once at start |
| `network.interfaces` | CheckSystem | `network.interfaces`, same section               | low: read every round, no WMI, nothing forked |
| `software.installed` | CheckSystem | `software.installed`, same section               | the highest here: every round, a walk of the registry's Uninstall hives or one forked package-manager query |
| `storage.volumes`    | CheckDisk   | `storage.volumes`, `[/settings/disk/facts]`      | low: the enumeration `check_drivesize drive=*` does |
| `hyperv.vms`         | CheckHyperV | `hyperv.vms`, `[/settings/hyperv/facts]`         | moderate: every round (never at startup), the seven WMI queries `check_hyperv_vms` runs, which grow with every VM and checkpoint and can stall while the Hyper-V management provider starts |
| `docker`             | CheckDocker | `docker`, `[/settings/docker/facts]`             | low: one `GET /info` on the daemon socket, every round but startup |
| `docker.containers`  | CheckDocker | `docker.containers`, same section                | low: the listing `check_docker all=true` does, every round but startup |
| `docker.images`      | CheckDocker | `docker.images`, same section                    | low: one `GET /images/json`, every round but startup |
| `mysql`              | CheckMySQL  | `mysql`, `[/settings/mysql/facts]`               | low: one connection and one query, every round but startup |
| `mysql.databases`    | CheckMySQL  | `mysql.databases`, same section                  | low: one query of `information_schema.SCHEMATA`, on the same connection |
| `mssql`              | CheckMSSQL  | `mssql`, `[/settings/mssql/facts]`               | low: one connection and one `SERVERPROPERTY` query, every round but startup |
| `mssql.databases`    | CheckMSSQL  | `mssql.databases`, same section                  | low: one query of `sys.databases`, on the same connection |

The full description of each switch is in the module's settings reference.
Turning a set off takes effect on the next settings reload: the module stops
returning the set, and the core drops it from the document. There is nothing
to clean up.

---

## What each set contains

A field that could not be determined is **omitted**, never written as an empty
string or zero. A consumer reads an absent key as unknown.

### `agent`

| Field      | Example                         | Meaning |
|------------|---------------------------------|---------|
| `version`  | `0.12.6`                        | the NSClient++ version |
| `modules`  | `["CheckDisk", "CheckSystem"]`  | the loaded modules, sorted, each once |
| `enrolled` | `true`                          | whether this host is enrolled with a fleet server: yes or no, never which server or with which identity |

### `os` and `hardware`

| Set        | Fields |
|------------|--------|
| `os`       | `family`, `name`, `version`, `arch`, `virtualization`, `domain` |
| `hardware` | `manufacturer`, `model`, `cpu_cores`, `memory_gb` |

Both are read once, when the module starts, because none of it can change
without a reboot. A manual refresh reads them again.

### `network.interfaces`

One record per network interface. The loopback interface is left out.

| Field          | Example                      | Meaning |
|----------------|------------------------------|---------|
| `id`           | `eth0`, `Intel(R) Ethernet Connection I219-V` | the interface: the kernel name on Linux, the adapter description on Windows |
| `display_name` | `Ethernet`                   | the connection name (Windows only) |
| `mac`          | `00:1a:2b:3c:4d:5e`          | the hardware address, lowercase and colon-separated on every platform |
| `status`       | `up`                         | the RFC 2863 link state: `up`, `down`, `dormant`, `lowerlayerdown`, `notpresent`, `testing`, `unknown` |
| `speed_bps`    | `1000000000`                 | the negotiated link speed in bits per second |
| `addresses`    | `["192.168.1.10", "fe80::1"]`| the IPv4 and IPv6 addresses, without an IPv6 zone suffix |

There are no traffic counters. They change every second, which would make
every round a change. [`check_network`](../reference/check/CheckSystem.md) is
where traffic is monitored. The Windows XP build reports no `speed_bps`.

### `storage.volumes`

One record per volume that `check_drivesize drive=*` would report.

| Field        | Example                  | Meaning |
|--------------|--------------------------|---------|
| `id`         | `/`, `C:\`               | the mount point: the value `check_drivesize` calls `drive` |
| `device`     | `/dev/sda1`, `\\?\Volume{…}\` | the block device on Linux, the volume GUID path on Windows |
| `filesystem` | `ext4`, `NTFS`           | the filesystem |
| `type`       | `fixed`                  | the `check_drivesize` type: `fixed`, `remote`, `removable`, `cdrom`, `ramdisk`, `unknown` |
| `label`      | `Data`                   | the filesystem label |
| `size_bytes` | `107374182400`           | the total size |

There is no free space, for the same reason there are no traffic counters.
A remote volume (NFS, SMB, a mapped drive) is listed without `size_bytes`.
Asking a dead server for a size would stall the round until it times out.

### `software.installed`

One record per installed program, from the same source
[`check_installed_software`](../reference/check/CheckSystem.md) reads: the
registry's `Uninstall` hives on Windows - the 64-bit and 32-bit machine views
and every loaded per-user hive, never `Win32_Product` - and the host's own
package manager (dpkg, rpm or pacman) on unix.

| Field          | Example                      | Meaning |
|----------------|------------------------------|---------|
| `id`           | `Google Chrome`, `bash`      | the name, with the version appended when the host has two installs of it |
| `name`         | `Google Chrome`              | what the platform calls it: the `name` of `check_installed_software` |
| `version`      | `129.0.6668.101`             | as the platform recorded it; never parsed or compared as a number |
| `publisher`    | `Google LLC`                 | the publisher (Windows), the maintainer or vendor (unix) |
| `architecture` | `x86_64`, `x86`, `noarch`    | the [`os`](#os-and-hardware) set's vocabulary, plus `noarch` for a package that has none (rpm's `noarch`, dpkg's `all`, pacman's `any`) |
| `source`       | `registry`, `dpkg`           | which database the record came from |
| `scope`        | `machine`                    | `machine` or `user`, by the hive it was installed into. Windows only: a unix package manager installs for the machine |
| `install_date` | `2026-09-09`                 | a date, not a time, and only where the platform records one |
| `size_bytes`   | `5904384`                    | the installed size, where it is recorded |

What it leaves out is as deliberate as what it carries. Entries Windows hides
from Programs and Features (`SystemComponent`) are not listed: they are the
runtimes and bookkeeping keys a component left behind, and they roughly double
the record count. One product installed into several user hives is one record,
because the host has that software on it once. And nothing here says whether
anything is *running* - that is a check, and it would change the document every
round.

This is the one set with a size limit of its own. A Linux desktop has a few
thousand packages, and the whole document has to fit `[/settings/facts] max
size`; a set that would not fit is rejected whole, which would leave the host
with no inventory at all. So the list stops at **2500 records**, and the set
then carries an error under `errors` saying how many were found.

### `hyperv.vms`

One record per virtual machine on a Hyper-V host, from the same
`root\virtualization\v2` classes
[`check_hyperv_vms`](../reference/windows/CheckHyperV.md) reads.

| Field                  | Example                                | Meaning |
|------------------------|----------------------------------------|---------|
| `id`                   | `web-01`                               | the VM name: the value `check_hyperv_vms` calls `vm`, with the VM's GUID appended when the host has two VMs of that name |
| `name`                 | `web-01`                               | the VM name as Hyper-V shows it |
| `vm_id`                | `1e4f6e3b-0f7c-4a51-9c2e-6f8a0b1c2d3e` | the VM's GUID |
| `generation`           | `2`                                    | the VM generation |
| `version`              | `9.0`                                  | the configuration version |
| `vcpus`                | `4`                                    | the configured virtual processors |
| `memory_startup_bytes` | `2147483648`                           | the configured startup memory |
| `dynamic_memory`       | `true`                                 | whether dynamic memory is on |
| `memory_minimum_bytes` | `536870912`                            | the dynamic memory floor (dynamic memory only) |
| `memory_maximum_bytes` | `8589934592`                           | the dynamic memory ceiling (dynamic memory only) |
| `checkpoints`          | `2`                                    | how many checkpoints (snapshots) the VM has |
| `replication_mode`     | `primary`                              | the Hyper-V Replica role: `none`, `primary`, `replica`, `test_replica`, `extended_replica` |

There is no power state, heartbeat, uptime, load or assigned memory: those
change from one round to the next and are what `check_hyperv_vms` monitors.
On a host without the Hyper-V role the set is enabled but cannot be
collected, and says so under `errors` every round. The same goes for a
stopped management service (vmms), and for an account that may not see the
virtual machines: Hyper-V hides them from it without an error, so an empty
list would claim the host has none. The set reports that under `errors` and
the last list collected is kept.

The set is not read on the startup round. That round runs on the thread
that starts the service, and the first query into the virtualization
namespace starts the Hyper-V management provider when it is not already
running, which can take long enough to hold up every producer after it. So
at startup the set says so under `errors`, and it is collected on the first
scheduled round (every `[/settings/facts] interval`), on a settings reload,
or right away with `facts refresh` / `POST /api/v2/facts/commands/refresh`.

### `docker`, `docker.containers` and `docker.images`

The docker daemon behind `[/settings/docker] endpoint` - the one
[`check_docker`](../reference/check/CheckDocker.md) talks to - what it holds,
and what it has pulled. Three switches, one set: `docker` is the daemon record,
and each list is enabled on its own, so a host can report its containers
without the image list that goes with them.

The daemon record:

| Field            | Example              | Meaning |
|------------------|----------------------|---------|
| `version`        | `27.3.1`             | the daemon version: the `version` keyword of `check_docker_info` |
| `os`             | `Ubuntu 24.04.1 LTS` | the operating system the daemon reports |
| `os_type`        | `linux`              | `linux` or `windows`: the [`os`](#os-and-hardware) set's family vocabulary |
| `architecture`   | `x86_64`, `arm64`    | the [`os`](#os-and-hardware) set's vocabulary, whatever the daemon's own spelling |
| `kernel_version` | `6.8.0-45-generic`   | |
| `storage_driver` | `overlay2`           | |
| `cgroup_driver`  | `systemd`            | |
| `cgroup_version` | `2`                  | |
| `cpus`           | `8`                  | the CPUs the daemon sees |
| `memory_bytes`   | `33547567104`        | the memory the daemon sees |
| `swarm`          | `inactive`           | the node's swarm state: `inactive`, `active`, `pending`, `error`, `locked` |

There are no container or image counts. They change every round, and
`check_docker_info` reports them.

`docker.containers`: one record per container the daemon knows, stopped ones
included, because an inventory lists what exists rather than what runs.

| Field             | Example                                    | Meaning |
|-------------------|--------------------------------------------|---------|
| `id`              | `web`                                      | the container's names, comma separated: the value `check_docker` calls `names` |
| `container_id`    | `aaa111…`                                  | the daemon's id: the `id` keyword of `check_docker` |
| `image`           | `nginx:1.25`                               | the image it was created from |
| `image_id`        | `sha256:…`                                 | |
| `created`         | `2026-09-19T14:03:11Z`                     | |
| `ports`           | `["0.0.0.0:8080->80/tcp", ":::8080->80/tcp", "443/tcp"]` | published and exposed ports, spelled as `check_docker` spells `ports` and sorted; a port published on both address families is one entry per family, since which addresses it is bound on is part of the record |
| `compose_project` | `shop`                                     | the compose project the container belongs to, where compose started it |
| `compose_service` | `web`                                      | the compose service, likewise |

There is no state, status, health or address. Each changes under a running
agent and belongs to `check_docker`. Nor are the container's other labels
carried: their keys (`com.docker.compose.project`) are not fact keys, and most
of them are configuration. The two compose labels are read out by name because
they say what a container *is*.

`docker.images`: one record per image the daemon holds.

| Field        | Example                            | Meaning |
|--------------|------------------------------------|---------|
| `id`         | `sha256:…`                         | the image id: the one name of an image that does not move when it is retagged, so a consumer diffing by id sees a pull as a pull and not as a removal plus an addition |
| `tags`       | `["nginx:1.25", "nginx:latest"]`   | every tag; a dangling image has none, never a `<none>:<none>` |
| `created`    | `2026-09-01T10:00:00Z`             | when the image was built |
| `size_bytes` | `187000000`                        | |

Both lists are re-read every round, because containers are started and
removed and images pulled while the agent runs. Everything is fetched before
anything is stored: a round that read the daemon but could not list the
containers reports an error against `docker` and keeps the set the core has,
rather than replacing it with one that is missing a list.

Each list stops at **2500 records**, for the reason `software.installed`
does: the whole document has to fit `[/settings/facts] max size`, and a set
that would not is rejected whole, daemon record included. A build host with
more images than that reports the first 2500 and says under `errors` how
many there were.

The set is not read on the startup round. That round runs on the thread that
starts the service, and a daemon that is hung, or a socket that is there but
not answering, would hold the service start for the full `timeout`. So at
startup the set says so under `errors`, and it is collected on the first
scheduled round (every `[/settings/facts] interval`), on a settings reload,
or right away with `facts refresh` / `POST /api/v2/facts/commands/refresh`.

### `mysql` and `mysql.databases`

The server `[/settings/mysql]` points at - the one
[`check_mysql`](../reference/check/CheckMySQL.md) connects to by default - and
the databases it holds. A facts round has no request to take credentials from,
so it connects with the ones configured there (`user` and `password`, or the
`defaults file`), and it needs nothing the health check does not already need.

The server record:

| Field             | Example                     | Meaning |
|-------------------|-----------------------------|---------|
| `flavor`          | `mariadb`                   | `mysql`, `mariadb` or `percona`: the `flavor` keyword of `check_mysql` |
| `version`         | `10.11.14-MariaDB-ubu2404`  | `@@version`, as recorded; never parsed or compared as a number |
| `version_comment` | `Ubuntu 24.04`              | `@@version_comment` |
| `hostname`        | `db01`                      | what the server calls the machine it runs on (`@@hostname`) |
| `port`            | `3306`                      | the port the server listens on (`@@port`) |
| `server_id`       | `1`                         | the replication identity (`@@server_id`) |
| `character_set`   | `utf8mb4`                   | the server default (`@@character_set_server`) |
| `collation`       | `utf8mb4_general_ci`        | the server default (`@@collation_server`) |
| `os`              | `debian-linux-gnu`, `Win64` | what the server was built for (`@@version_compile_os`) |
| `architecture`    | `x86_64`, `arm64`           | the [`os`](#os-and-hardware) set's vocabulary, whatever the server's own spelling |

The record describes the server, not how the agent reaches it: `hostname` and
`port` are what the server says about itself, never the configured target, and
nothing from `[/settings/mysql]` appears in it. There is no uptime and no
connection count. They change every round, and `check_mysql` reports them.

`mysql.databases`: one record per database (schema) the configured user may
see, sorted by name. The system schemas
(`information_schema`, `mysql`, `performance_schema`, `sys`) are listed like
any other: they are databases the server has.

| Field           | Example              | Meaning |
|-----------------|----------------------|---------|
| `id`            | `shop`               | the schema name |
| `character_set` | `utf8mb4`            | the schema's default character set |
| `collation`     | `utf8mb4_unicode_ci` | the schema's default collation |

There is no size. Summing a schema's tables is a query that changes its
answer every round, and it belongs to `check_mysql_query`. The list stops at
**2500 records**, as `software.installed` does, and says under `errors` how
many there were.

Like `docker`, the set is not read on the startup round: a server that is
down, or a host that drops the packets, would hold the service start for the
full connect `timeout`. At startup the set says so under `errors`, and it is
collected on the first scheduled round, on a settings reload, or right away
with a manual refresh.

### `mssql` and `mssql.databases`

The SQL Server instance `[/settings/mssql]` points at - the one
[`check_mssql`](../reference/windows/CheckMSSQL.md) connects to by default -
and the databases it holds. It connects the way the checks do when a check
passes no credentials of its own: Windows integrated authentication, or the
configured `user` and `password`. Windows only, like the module.

The server record is read with `SERVERPROPERTY()` alone, which every version
answers and which returns `NULL` for a property it does not have, so an old
instance reports fewer fields rather than an error:

| Field                  | Example                        | Meaning |
|------------------------|--------------------------------|---------|
| `server_name`          | `DB01\PROD`                    | the instance name: the `server_name` keyword of `check_mssql` |
| `machine_name`         | `DB01`                         | the computer, or the cluster network name |
| `instance_name`        | `PROD`                         | the named instance; absent for the default instance |
| `version`              | `16.0.4135.4`                  | the product version, as recorded; never parsed or compared as a number |
| `product_level`        | `RTM`, `SP3`                   | |
| `product_update_level` | `CU15`                         | the cumulative update, where the version records one |
| `edition`              | `Standard Edition (64-bit)`    | |
| `engine_edition`       | `standard`                     | `personal`, `standard`, `enterprise`, `express`, `azure_sql_database`, `azure_sql_managed_instance`, … or the number for one this build does not name |
| `collation`            | `SQL_Latin1_General_CP1_CI_AS` | the server collation |
| `authentication`       | `mixed`                        | `windows` or `mixed` |
| `clustered`            | `false`                        | whether the instance is a failover cluster instance |
| `always_on`            | `true`                         | whether Always On availability groups are enabled (2012 and later) |

There is no uptime. It changes every round, and `check_mssql` reports it.

`mssql.databases`: one record per database the login may see, system databases
included, by the name `check_mssql_databases` uses.

| Field                 | Example                       | Meaning |
|-----------------------|-------------------------------|---------|
| `id`                  | `shop`                        | the database name |
| `recovery_model`      | `FULL`                        | `SIMPLE`, `FULL` or `BULK_LOGGED`, spelled as the check spells it |
| `collation`           | `Latin1_General_100_CI_AS`    | |
| `compatibility_level` | `160`                         | |
| `create_date`         | `2026-03-02`                  | a date, rendered by the server |
| `read_only`           | `false`                       | |

There is no state and no data or log size. Both move every round and belong
to `check_mssql_databases`. The list stops at **2500 records**, as
`software.installed` does, and says under `errors` how many there were.

Like `docker` and `mysql`, the set is not read on the startup round: a stopped
instance would hold the service start for the full login `timeout`. At
startup the set says so under `errors`, and it is collected on the first
scheduled round, on a settings reload, or right away with a manual refresh.

### Record ids match check instance names

A list record's `id` is the same string that the corresponding check uses to
name the instance. `storage.volumes[].id` is the `drive` of `check_drivesize`.
`network.interfaces[].id` is the `name` of `check_network`.
`software.installed[].id` is the `name` of `check_installed_software` - with
the version appended in the one case where the host has two installs sharing a
name, because an id has to be unique in its list. `hyperv.vms[].id` is the `vm`
of `check_hyperv_vms`, with the GUID appended for the same reason when two VMs
share a name. `docker.containers[].id` is the `names` of `check_docker`,
`mysql.databases[].id` is the schema name a `check_mysql_query` would name in
its `FROM`, and `mssql.databases[].id` is the `name` of
`check_mssql_databases`. The ids are stable across
rounds, so a consumer can diff two documents record by record.

---

## When facts are collected

| When                     | What happens |
|--------------------------|--------------|
| Start                    | one round, before the first fleet report |
| Every `[/settings/facts] interval` (default `1h`) | a round; a set whose values have not changed does not move the document's revision |
| A settings reload        | a round, because a set may have been switched on or off |
| On demand                | `facts refresh` in `nscp test`, or `POST /api/v2/facts/commands/refresh` |

Each set a module produces carries a *gathered* time: when its values were
read off the machine, which for `os` and `hardware` is when the module started.
The core builds `agent` on the round itself, so the round's time is its age. A set that is enabled
but could not be collected keeps its previous value, and the reason is
reported under `errors`, so a transient failure never blanks the inventory.

---

## Reading facts

```
nscp> facts
Revision: 4  Checked: 2026-09-24T13:19:40Z
Enabled: agent, network, os, storage
Gathered:
  network: 2026-09-24T13:19:40Z
  os: 2026-09-24T13:19:40Z
  storage: 2026-09-24T13:19:40Z

  agent:
    enrolled: false
    modules:
      - CheckDisk
      - CheckSystem
    version: 0.12.6
  network:
    interfaces:
      - eth0
          addresses:
            - 192.0.2.2
          mac: 02:fc:00:00:00:01
          status: up
  os:
    arch: x86_64
    family: linux
    name: Ubuntu 24.04.4 LTS
    version: 6.18.44
    virtualization: virtual
  storage:
    volumes:
      - /
          device: /dev/vda
          filesystem: ext4
          size_bytes: 270553174016
          type: fixed
```

`facts <path>` shows one subtree (`facts storage.volumes`), and `facts refresh`
collects now. Over REST the same document is `GET /api/v2/facts` (see
[Facts](../api/rest/facts.md)), and the web UI shows it on the Facts page.

---

## Facts and the fleet server

An agent [enrolled with a fleet server](../setup/fleet.md) sends its facts
there too. The document is up to a megabyte and changes rarely, so it does not
ride in the state report the agent sends every poll:

* **Every state report carries `facts_hash`**, the SHA-256 of the document.
  A host with nothing enabled reports the hash of the empty document, `{}`.
  That is enough for the server to see that an inventory changed, or that a
  host has none, without the document itself.
* **The document goes on its own call, `POST /agent/v1/facts`, and only when
  it differs from what the server holds**: after a round changed it, after a
  bundle enabled or disabled a set, and once after the agent starts. A host
  with nothing enabled sends nothing.
* **The server can ask for it again** by carrying the hash it holds, as
  `facts_hash`, in a desired-state or state-report response. When that
  differs from the agent's own, the agent uploads. A 304 has no body, so a
  server that wants the document from a host that is already in sync answers
  its poll with the full desired state instead.

```json
{
  "collected_at": "2026-09-25T10:00:00Z",
  "facts": { "os": { "family": "linux", "...": "..." } },
  "facts_hash": "<sha256 hex of the facts value>"
}
```

`facts_hash` is the digest of the `facts` value exactly as it appears in the
body: compact JSON with every object's keys sorted, so the server can check it
without re-encoding anything.

Because the switches are ordinary INI, a fleet bundle turns inventory on for a
whole group of hosts the same way it configures anything else; see
[Collect an inventory](../setup/fleet.md#collect-an-inventory). A server that
predates the facts call answers it with a 404, and the agent then stops
offering it until the server starts talking about facts or the agent restarts.
A document larger than `[/settings/facts] max size`, or one the server refuses
as too large, is not sent again until it changes, and the agent log names the
largest sets so you know which one to turn off.

---

## The document rules

The core checks every set against these rules before it accepts it. A set that
breaks a rule is rejected whole, and the previous value of that set is kept.

* **Keys** are `snake_case` ASCII, at most 64 characters. A key that carries a
  unit ends with it (`size_bytes`, `speed_bps`).
* **Values** are strings, numbers or booleans. There is no `null`: an unknown
  value is left out.
* **Lists** are lists of records, each with an `id` unique in its list and
  stable across rounds, or plain lists of strings (`addresses`, `modules`).
* **Size**: depth at most 6, at most 5000 records in a list, and the whole
  document within `[/settings/facts] max size` (1 MiB by default).

---

## What facts never contain

Facts describe the machine, not the configuration. They never contain
settings, credentials, command lines or environment variables. That holds
inside a set too: `software.installed` carries what is installed and which
version of it, never the install path or the uninstall command line
`check_installed_software` can also show; `docker.containers` carries the
image a container runs, never its command line, its environment or its labels
at large; and `mysql` and `mssql` say what the server reports about itself,
never the target, login, driver or connection string the agent connected with. The loaded module list in
`agent` is the one configuration-adjacent value, and it is opt-in like
everything else.
