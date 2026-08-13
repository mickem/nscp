#### About `check_docker_df`

`check_docker_df` reports docker's disk usage the way `docker system df`
does — images, container writable layers, volumes and the build cache — plus
what a prune would reclaim: unused images (their non-shared size), stopped
containers, unreferenced volumes and idle build cache.

The daemon computes this by walking every image layer, container filesystem
and volume, so **the request is slow on large hosts** (seconds to minutes).
The check therefore defaults to six times the module's normal timeout; don't
schedule it more often than you need it.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword                   | Description                                                  |
|---------------------------|--------------------------------------------------------------|
| `images` / `unused_images`| Number of images / images no container uses                  |
| `images_size`             | Disk used by images (unique layer sizes)                     |
| `images_reclaimable`      | Freed by pruning unused images                               |
| `containers`              | Number of containers (running and stopped)                   |
| `containers_size`         | Disk used by container writable layers                       |
| `containers_reclaimable`  | Freed by pruning stopped containers                          |
| `volumes` / `unused_volumes` | Number of volumes / volumes no container references       |
| `volumes_size`            | Disk used by volumes                                         |
| `volumes_reclaimable`     | Freed by pruning unused volumes                              |
| `build_cache_size`        | Disk used by the build cache                                 |
| `build_cache_reclaimable` | Freed by pruning the idle build cache                        |
| `total_size`              | Everything above combined                                    |
| `total_reclaimable`       | Everything a full prune would free                           |
| `message`                 | The human readable summary line                              |

All `*_size` / `*_reclaimable` keywords are byte-sized: thresholds take a unit
(`500M`, `10G`, `1T`); a bare number is rejected. No default thresholds — a
responding daemon is OK until you add limits.
