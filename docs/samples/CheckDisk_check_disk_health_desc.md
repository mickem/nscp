`check_disk_health` is a combined per-disk health check. It reports three kinds
of row, each judged only on the data that is real for it:

* **Space rows** (`has_space = 1`) — one per mounted filesystem, with
  `free`/`used`/`free_pct`/`used_pct`/`user_free` and the I/O of the backing
  device.
* **I/O rows** (`has_space = 0`, `has_device = 0`) — devices/totals with no
  mounted filesystem (e.g. `_Total`), judged on `percent_disk_time` and queue.
* **Device rows** (`has_device = 1`) — one per physical disk (Windows only,
  from `MSFT_PhysicalDisk` / `MSFT_Disk`), judged on physical-disk health.

Space and I/O rows also carry the average I/O latency of the backing device
(`read_latency`, `write_latency`, `total_latency`, in **milliseconds** over the
collection interval), so a single check can join free space with the most
portable saturation signal: `"warn=total_latency > 20" "crit=total_latency > 50"`.
See `check_disk_io` for details on how latency is measured.

The space keywords have no value at all on a row without a filesystem behind it
(an I/O or device row). They render as `-`, every numeric comparison against
them is false, and they emit no performance data, so a graph of a device row
records nothing rather than a fabricated 0%. Test for it with
`free_pct = 'no space data'`, or keep using the `has_space = 1` guard.

Byte-valued keywords can be formatted and scaled with `format_bytes`,
`convert_bytes` and `scale`; see the same section under `check_disk_io`.

### Device-state rows (Windows)

Device rows are best-effort: if the `MSFT_PhysicalDisk` / `MSFT_Disk` WMI classes
are unavailable (very old Windows, or a system with no Storage provider), no
device rows are produced and the check still reports space and I/O normally.

### Default thresholds

By default the check is WARNING when a filesystem drops below 20% free, its disk
is over 80% busy, or a physical disk reports `Warning` health; and CRITICAL below
10% free, over 95% busy, or when a physical disk is `Unhealthy` or offline.
