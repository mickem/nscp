**Default check (dedicated memory above 85% warns, above 95% is critical):**

```
check_gpu
OK: NVIDIA RTX A4000: 37% busy, 6.012GB dedicated memory used (38%)
```

**A host without a GPU:**

Only the software rasteriser is present and the default filter drops it, so the
check reports its empty state. `empty-state=ok` turns that into an OK for a
check definition that is deployed fleet-wide.

```
check_gpu
UNKNOWN: No GPU adapters found
```

```
check_gpu empty-state=ok
OK: No GPU adapters found
```

**Alert on sustained load as well as memory:**

The load is a one-second sample; let the monitoring server confirm it over
several attempts before notifying.

```
check_gpu "warn=utilization > 90 or dedicated_pct > 85" "crit=dedicated_pct > 95"
WARNING: NVIDIA RTX A4000: 97% busy, 6.012GB dedicated memory used (38%)
```

**Watch the video encoder on a session host:**

```
check_gpu "warn=utilization_video_encode > 70" "crit=utilization_video_encode > 90" "detail-syntax=${name}: encode ${utilization_video_encode}%, decode ${utilization_video_decode}%, 3D ${utilization_3d}%"
OK: NVIDIA A16-2B: encode 41%, decode 12%, 3D 23%
```

**Absolute memory thresholds (work without knowing the adapter's size):**

```
check_gpu "warn=dedicated_used > 12G" "crit=dedicated_used > 15G"
OK: NVIDIA RTX A4000: 37% busy, 6.012GB dedicated memory used (38%)
```

**Show which engine is busy and how many processes use the adapter:**

```
check_gpu "detail-syntax=${name}: ${utilization}% (${busiest_engine}), ${processes} processes, ${dedicated_used} of ${dedicated_total}, shared ${shared_used}"
OK: NVIDIA RTX A4000: 37% (3D), 14 processes, 6.012GB of 15.992GB, shared 412.301MB
```

**One adapter of several, by name:**

```
check_gpu "filter=name like 'NVIDIA'"
OK: NVIDIA RTX A4000: 37% busy, 6.012GB dedicated memory used (38%)
```

**Every adapter Windows knows about, the software rasteriser included:**

```
check_gpu filter=none "detail-syntax=${name} [${adapter_type}]"
OK: NVIDIA RTX A4000 [hardware], Microsoft Basic Render Driver [software]
```

**Over NRPE against a remote host:**

```
check_nrpe --host 192.168.56.103 --command check_gpu --arguments "warn=dedicated_pct > 80" --arguments "crit=dedicated_pct > 90"
OK: NVIDIA RTX A4000: 37% busy, 6.012GB dedicated memory used (38%)
```
