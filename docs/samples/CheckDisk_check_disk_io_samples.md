**Alerting on average I/O latency:**

Average latency per I/O is the most portable "is the storage keeping up?"
signal: thresholds of ~20 ms (warning) and ~50 ms (critical) are meaningful
regardless of workload or hardware. Values are in milliseconds.

```
check_disk_io "warn=total_latency > 20" "crit=total_latency > 50"
OK: C:: 11% busy, read=21967407B/s write=17167107B/s q=0, HarddiskVolume4: 0% busy, read=0B/s write=0B/s q=0, ...
'C:_total_latency'=0.172447ms;20;50 'HarddiskVolume4_total_latency'=0ms;20;50 ...
```

**Separate read/write latency for a single disk:**

```
check_disk_io "filter=name = 'C:'" "warn=read_latency > 20 or write_latency > 20" "crit=read_latency > 50 or write_latency > 50" "detail-syntax=${name}: r=${read_latency}ms w=${write_latency}ms"
OK: C:: r=4.08798ms w=1.47571ms
'C:_read_latency'=4.08798ms;20;50 'C:_write_latency'=1.47571ms;20;50
```

Latency is averaged over the collector's sampling interval (10 seconds by
default) and reads `0` when no I/O of that kind occurred during the interval —
including the very first interval after startup.

**Alerting on a busy disk (default thresholds):**

The default check goes WARNING above 80% disk time and CRITICAL above 95%:

```
check_disk_io
OK: All disk I/O seems ok.
'C:'=2%;80;95 'HarddiskVolume4'=0%;80;95 ...
```
