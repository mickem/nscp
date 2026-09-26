---
icon: "🏷️"
modules: [CheckDocker, CheckMySQL, CheckMSSQL]
action: none
---
**Service inventories: the `docker`, `mysql` and `mssql` fact sets.** Nothing to do.
Like every other fact set, each part collects nothing until you turn it on,
in the module that already checks the service.

```ini
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
```

| Set | Produced by | Fields |
|---|---|---|
| `docker` | CheckDocker | the daemon: `version`, `os`, `os_type`, `architecture`, `kernel_version`, `storage_driver`, `cgroup_driver`, `cgroup_version`, `cpus`, `memory_bytes`, `swarm` |
| `docker.containers` | CheckDocker | per container, stopped ones included: `id`, `container_id`, `image`, `image_id`, `created`, `ports`, `compose_project`, `compose_service` |
| `docker.images` | CheckDocker | per image: `id`, `image_id`, `tags`, `created`, `size_bytes` |
| `mysql` | CheckMySQL | the server: `flavor`, `version`, `version_comment`, `hostname`, `port`, `server_id`, `character_set`, `collation`, `os`, `architecture` |
| `mysql.databases` | CheckMySQL | per database: `id`, `character_set`, `collation` |
| `mssql` | CheckMSSQL | the instance: `server_name`, `machine_name`, `instance_name`, `version`, `product_level`, `product_update_level`, `edition`, `engine_edition`, `collation`, `authentication`, `clustered`, `always_on` |
| `mssql.databases` | CheckMSSQL | per database: `id`, `recovery_model`, `collation`, `compatibility_level`, `create_date`, `read_only` |

A record's `id` is the name the matching check already uses for that instance:
`docker.containers` ids are what `check_docker` calls `names`,
`mssql.databases` ids are what `check_mssql_databases` calls `name`, and a
`docker` record's `version` is what `check_docker_info` calls `version`.

These sets describe the service, not how busy it is. A container is listed
with its image and never with its state; the daemon record carries no
container or image counts; the server records carry no uptime, connection
count or database size. All of that changes every round and belongs to the
checks. And they describe the service, not the agent's configuration: the
`mysql` and `mssql` records carry the names the server reports about itself,
never the configured target, and no set carries a login, a password, a driver
or a command line.

`CheckMySQL` and `CheckMSSQL` connect with what is configured in their
settings section (`user` and `password`, a `defaults file`, or Windows
authentication), the same connection the checks use when a check does not
pass its own. `CheckDocker` talks to the configured `endpoint`. All three are
re-read every facts round, because containers and databases come and go while
the agent runs; a round that cannot reach the service reports why under
`errors` and keeps the last good set. See
[Host Facts](../concepts/facts.md) for every set, its fields and its cost.
