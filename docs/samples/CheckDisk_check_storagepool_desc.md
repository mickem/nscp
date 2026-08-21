Checks the health and capacity of Windows Storage Spaces pools, read from
`MSFT_StoragePool` in the `root\Microsoft\Windows\Storage` WMI namespace. The
primordial pool (the reservoir of unpooled physical disks) is excluded, so only
real Storage Spaces are reported.

The capacity keywords (`capacity`, `used`, `free`, `free_pct`, `used_pct`) are
emitted as performance data when used in thresholds.

Defaults: WARNING on `Warning` health or `< 20%` free; CRITICAL on `Unhealthy`
health or `< 10%` free. If the Storage namespace/class is unavailable (no Storage
Spaces, older Windows) the check reports no pools and returns OK — it never fails
just because the feature is absent. This is a natural companion to the
physical-disk device state exposed by `check_disk_health`.
