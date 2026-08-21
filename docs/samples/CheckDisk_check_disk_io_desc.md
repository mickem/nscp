`check_disk_io` reports disk I/O activity per logical disk (Windows) or per
physical block device (Linux), plus a `_Total` row aggregating all disks. The
data comes from a background collector that samples every 10 seconds (the
`collection interval` setting), so each check reads the most recent interval.

The keywords fall into two groups:

* **Load** — `reads_per_sec`, `writes_per_sec`, `iops`, `read_bytes_per_sec`,
  `write_bytes_per_sec`, `total_bytes_per_sec`, `split_io_per_sec`: how much
  work the disk is doing. These have no universally meaningful thresholds — a
  healthy datastore may sustain thousands of IOPS while a saturated one
  struggles at hundreds.
* **Saturation** — `percent_disk_time`, `percent_idle_time`, `queue_length`,
  and the latency keywords (`read_latency`, `write_latency`, `total_latency`,
  in milliseconds): whether the storage is keeping up.

### Latency keywords

Average latency per I/O is the most portable saturation signal: it is
independent of the workload shape and comparable across machines. As a rule of
thumb, sustained latencies above ~20 ms suggest the storage is struggling and
above ~50 ms indicate a real problem. The values are averages over the
collection interval and read `0` when no I/O of that kind occurred (and on the
first sample after startup).

On Windows latency is computed from the raw `Avg. Disk sec/Read|Write|Transfer`
performance counters; on Linux from `/proc/diskstats` (time spent
reading/writing divided by operations completed).

One accuracy caveat: the underlying counters are 32-bit and accrue time per
*in-flight* operation, so on a disk under sustained very heavy load (high queue
depth) they can wrap more than once within a long sampling window, which
understates the reported latency. Perfmon has the same limitation and avoids it
by sampling every second — if you monitor extremely busy disks, lower the
module's `collection interval` accordingly:

```ini
[/settings/disk]
collection interval=2s
```

### Formatting byte values

The byte-rate keywords are plain byte counts, and the filter language has no
arithmetic of its own, so three functions are available in both `detail-syntax`
and threshold expressions:

| Function                     | Description                                                                          |
|------------------------------|--------------------------------------------------------------------------------------|
| `format_bytes(value)`        | Human-readable string, auto-scaled to B/KB/MB/GB/... (1024-based).                   |
| `format_bytes(value,unit)`   | Human-readable string in a fixed unit (`B`, `K`/`KB`, `M`/`MB`, `G`/`GB`, `T`/`TB`). |
| `convert_bytes(value,unit)`  | The numeric value in that unit — use it in `warn`/`crit`.                            |
| `scale(value,divisor)`       | Plain division, for units the byte helpers do not cover (e.g. decimal Mbps).         |

```
check_disk_io "detail-syntax=%(name): %(format_bytes(total_bytes_per_sec))/s" "warn=convert_bytes(total_bytes_per_sec,'MB') > 100"
OK: C:: 20.95MB/s, D:: 1.10MB/s
```

Write the argument list without a space after the comma: the command-line
client splits an argument on whitespace, so `format_bytes(value, 'MB')` is
passed as two tokens and the option fails to parse. Over REST, and in
`nsclient.ini`, both spellings work.

### Performance data labels

`percent_disk_time` is what this check is about, so it is graphed under the bare
drive name — `'C:'` — as it always has been. Every other keyword adds its own:
`'C:_queue_length'`, `'C:_total_latency'`, `'C:_iops'` and so on, one series per
keyword rather than several sharing the drive name. `check_disk_health` works
the same way with `free_pct` as its primary metric.

The name a keyword is graphed under does not depend on what else the query asks
for, so a graph template can rely on it. Override the pieces per keyword with
`perf-config`:

```
check_disk_io "perf-config=percent_disk_time(suffix:_busy)"
```
