---
icon: "🏷️"
modules: [CheckDocker, CheckMySQL]
action: none
---
**Service inventories: the `docker` and `mysql` fact sets.** Nothing to do.
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
```

| Set | Produced by | Fields |
|---|---|---|
| `docker` | CheckDocker | the daemon: `version`, `os`, `os_type`, `architecture`, `kernel_version`, `storage_driver`, `cgroup_driver`, `cgroup_version`, `cpus`, `memory_bytes`, `swarm` |
| `docker.containers` | CheckDocker | per container, stopped ones included: `id`, `container_id`, `image`, `image_id`, `created`, `ports`, `compose_project`, `compose_service` |
| `docker.images` | CheckDocker | per image: `id`, `image_id`, `tags`, `created`, `size_bytes` |
| `mysql` | CheckMySQL | the server: `flavor`, `version`, `version_comment`, `hostname`, `port`, `server_id`, `character_set`, `collation`, `os`, `architecture` |
| `mysql.databases` | CheckMySQL | per database: `id`, `character_set`, `collation` |

A record's `id` is the name the matching check already uses for that instance:
`docker.containers` ids are what `check_docker` calls `names`, and a
`docker` record's `version` is what `check_docker_info` calls `version`.

These sets describe the service, not how busy it is. A container is listed
with its image and never with its state; the daemon record carries no
container or image counts; the server record carries no uptime or connection
count. All of that changes every round and belongs to the checks. And they
describe the service, not the agent's configuration: the `mysql` record's
`hostname` and `port` are what the server reports about itself, never the
configured target, and no set carries a user, a password or a command line.

`CheckMySQL` connects with the credentials configured in `[/settings/mysql]`
(`user` and `password`, or the `defaults file`), the same ones `check_mysql`
uses when a check does not pass its own. `CheckDocker` talks to the configured
`endpoint`. Both are re-read every facts round, because containers and
databases come and go while the agent runs; a round that cannot reach the
service reports why under `errors` and keeps the last good set. See
[Host Facts](../concepts/facts.md) for every set, its fields and its cost.
