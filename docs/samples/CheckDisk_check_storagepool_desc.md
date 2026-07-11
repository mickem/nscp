Checks the health and capacity of Windows Storage Spaces pools, read from
`MSFT_StoragePool` in the `root\Microsoft\Windows\Storage` WMI namespace. The
primordial pool (the reservoir of unpooled physical disks) is excluded, so only
real Storage Spaces are reported.

| Keyword                 | Description                                                       |
|-------------------------|-------------------------------------------------------------------|
| `name`                  | Pool friendly name.                                               |
| `health_status`         | `Healthy`, `Warning`, `Unhealthy` or `Unknown`.                   |
| `operational_status`    | Synthesised single value: `OK`, `ReadOnly`, or the health string. |
| `capacity`              | Total pool capacity in bytes (perf).                              |
| `used`                  | Allocated (used) space in bytes (perf).                           |
| `free`                  | Unallocated space in bytes (perf).                                |
| `free_pct` / `used_pct` | Percentage free / used (perf).                                    |
| `is_readonly`           | `1` if the pool is read-only.                                     |

Defaults: WARNING on `Warning` health or `< 20%` free; CRITICAL on `Unhealthy`
health or `< 10%` free. If the Storage namespace/class is unavailable (no Storage
Spaces, older Windows) the check reports no pools and returns OK — it never fails
just because the feature is absent. This is a natural companion to the
physical-disk device state exposed by `check_disk_health`.
