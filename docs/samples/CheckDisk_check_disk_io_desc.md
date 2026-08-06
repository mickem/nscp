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
  and the latency keywords: whether the storage is keeping up.

### Latency keywords

| Keyword         | Description                                                        |
|-----------------|--------------------------------------------------------------------|
| `read_latency`  | Average time per read in **milliseconds** over the interval.       |
| `write_latency` | Average time per write in **milliseconds** over the interval.      |
| `total_latency` | Average time per I/O (read + write) in milliseconds.               |

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
module's `collection interval` accordingly.
