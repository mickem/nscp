`check_disk_health` is a combined per-disk health check. It reports three kinds
of row, each judged only on the data that is real for it:

* **Space rows** (`has_space = 1`) — one per mounted filesystem, with
  `free`/`used`/`free_pct`/`used_pct`/`user_free` and the I/O of the backing
  device.
* **I/O rows** (`has_space = 0`, `has_device = 0`) — devices/totals with no
  mounted filesystem (e.g. `_Total`), judged on `percent_disk_time` and queue.
* **Device rows** (`has_device = 1`) — one per physical disk (Windows only,
  from `MSFT_PhysicalDisk` / `MSFT_Disk`), judged on physical-disk health.

### Device-state keywords (Windows)

| Keyword              | Description                                                      |
|----------------------|------------------------------------------------------------------|
| `has_device`         | `1` on a physical-disk row, `0` otherwise (guard; no perfdata).  |
| `friendly_name`      | Physical disk friendly name.                                     |
| `serial`             | Physical disk serial number.                                     |
| `media_type`         | `HDD`, `SSD`, `SCM`, or `Unspecified`.                           |
| `health_status`      | `Healthy`, `Warning`, `Unhealthy`, or `Unknown`.                 |
| `operational_status` | Synthesised single value: `Offline`, `OK`, or the health string. |
| `is_offline`         | `1` if the disk is offline.                                      |
| `is_readonly`        | `1` if the disk is read-only.                                    |
| `disk_number`        | Physical disk number/index.                                      |

Device rows are best-effort: if the `MSFT_PhysicalDisk` / `MSFT_Disk` WMI classes
are unavailable (very old Windows, or a system with no Storage provider), no
device rows are produced and the check still reports space and I/O normally.

### Default thresholds

By default the check is WARNING when a filesystem drops below 20% free, its disk
is over 80% busy, or a physical disk reports `Warning` health; and CRITICAL below
10% free, over 95% busy, or when a physical disk is `Unhealthy` or offline.
