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

The daemon endpoint is restricted to a local named pipe / absolute socket path:
a UNC path in `host=` would make a Windows host authenticate to a remote SMB
server with the service account, so anything non-local is refused outright.
