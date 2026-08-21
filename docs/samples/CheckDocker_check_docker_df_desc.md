#### About `check_docker_df`

`check_docker_df` reports docker's disk usage the way `docker system df`
does — images, container writable layers, volumes and the build cache — plus
what a prune would reclaim: unused images (their non-shared size), stopped
containers, unreferenced volumes and idle build cache.

The daemon computes this by walking every image layer, container filesystem
and volume, so **the request is slow on large hosts** (seconds to minutes).
The check therefore defaults to six times the module's normal timeout; don't
schedule it more often than you need it.

All `*_size` / `*_reclaimable` keywords are byte-sized: thresholds take a unit
(`500M`, `10G`, `1T`); a bare number is rejected. No default thresholds — a
responding daemon is OK until you add limits.
