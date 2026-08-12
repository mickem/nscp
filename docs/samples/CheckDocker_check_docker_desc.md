#### About `check_docker`

`check_docker` checks the state of docker containers via the local daemon
socket (`/var/run/docker.sock` on Linux, the `\\.\pipe\docker_engine` named
pipe on Windows; configurable via the `endpoint` setting or `host=`). It works
against any daemon speaking the docker API, including podman's compatibility
socket.

Three ways to use it:

* `container=<name>` (repeatable) — the "is my container running" mode: only
  the named containers are checked, and a name the daemon does not know gets
  the synthetic state `missing`, so it trips the default critical instead of
  silently disappearing from the listing.
* No arguments — lists running containers; nothing trips the default
  thresholds, so this is an inventory-style check.
* `all=true` — also includes stopped containers (like `docker ps -a`); any
  non-running container then trips the default critical
  (`container_state != 'running'`), so combine it with `filter=` on hosts
  where exited one-shot containers are expected.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword           | Description                                                                  |
|-------------------|------------------------------------------------------------------------------|
| `names`           | Container name(s), comma separated                                           |
| `container_state` | `created`, `restarting`, `running`, `removing`, `paused`, `exited`, `dead` or `missing` |
| `status`          | Human readable status, e.g. `Up 3 hours (healthy)`                           |
| `health`          | Health-check state: `healthy`, `unhealthy`, `starting`, or empty without a health check |
| `image`           | Image the container was created from                                         |
| `image_id`        | Id of that image                                                             |
| `id`              | Container id                                                                 |
| `command`         | Command the container runs                                                   |
| `ip`              | First IP address on any attached network                                     |
| `ports`           | Published/exposed ports, e.g. `0.0.0.0:8080->80/tcp`                         |
| `labels`          | Container labels as `key=value`, comma separated                             |
| `created`         | When the container was created (date)                                        |

The daemon endpoint is restricted to a local named pipe / absolute socket path:
a UNC path in `host=` would make a Windows host authenticate to a remote SMB
server with the service account, so anything non-local is refused outright.
