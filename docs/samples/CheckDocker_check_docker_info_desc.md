#### About `check_docker_info`

`check_docker_info` checks the docker daemon itself: it reads `/info` from the
local daemon socket and reports the server version, host name and the
container/image counts. A responding daemon is **OK** unless you threshold the
counts; an unreachable daemon is **UNKNOWN** with the transport error — which
makes this the natural "is docker itself healthy" companion to per-container
`check_docker` checks.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword      | Description                            |
|--------------|----------------------------------------|
| `version`    | Docker server version                  |
| `name`       | Daemon host name                       |
| `os`         | Operating system the daemon runs on    |
| `containers` | Total number of containers             |
| `running`    | Number of running containers           |
| `paused`     | Number of paused containers            |
| `stopped`    | Number of stopped containers           |
| `images`     | Number of images                       |

Count keywords used in thresholds are emitted as performance data.
