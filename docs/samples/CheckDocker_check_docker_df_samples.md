**Show docker disk usage (like `docker system df`):**

```
check_docker_df
OK: images 71 (8.216GB), containers 19 (388KB), volumes 6 (4.517GB), build cache 12.808GB, reclaimable 21.479GB
```

**Alert when a prune would free a lot of space (size thresholds take a unit):**

```
check_docker_df "warning=total_reclaimable > 10G" "critical=total_reclaimable > 50G"
WARNING: images 71 (8.216GB), containers 19 (388KB), volumes 6 (4.517GB), build cache 12.808GB, reclaimable 21.479GB|'docker reclaimable'=23063090871B;10737418240;53687091200
```

**Alert on unused images piling up:**

```
check_docker_df "warning=unused_images > 20"
OK: images 71 (8.216GB), containers 19 (388KB), volumes 6 (4.517GB), build cache 12.808GB, reclaimable 21.479GB|'docker unused images'=63;20;0
```

**Watch a specific category, e.g. the build cache:**

```
check_docker_df "warning=build_cache_size > 20G" "detail-syntax=build cache %(build_cache_size) (reclaimable %(build_cache_reclaimable))"
OK: build cache 13752766549 (reclaimable 13752766549)|'docker build cache'=13752766549B;21474836480;0
```
