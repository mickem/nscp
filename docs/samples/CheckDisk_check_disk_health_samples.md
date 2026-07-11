**Default check:**

```
check_disk_health
OK: All disks are healthy.
'C: free_pct'=61%;20;10 'C: percent_disk_time'=2%;80;95 ...
```

**Physical-disk device health:**

`check_disk_health` appends one row per physical disk (from `MSFT_PhysicalDisk` /
`MSFT_Disk`), carrying device state. These rows are identified by `has_device = 1`
and by default go CRITICAL on an unhealthy or offline disk and WARNING on a disk
reporting `Warning` health.

```
check_disk_health "filter=has_device = 1" "detail-syntax=${friendly_name} [${media_type}]: ${health_status}, ${operational_status}"
OK: Samsung SSD 980 [SSD]: Healthy, OK, WDC WD40 [HDD]: Healthy, OK
```

Alerting only on SSD wear / disk failure across all physical disks:

```
check_disk_health "filter=has_device = 1" "crit=health_status != 'Healthy' or is_offline = 1"
CRITICAL: WDC WD40 [HDD]: Unhealthy, Unhealthy
```

Device-state keywords (populated on `has_device = 1` rows): `friendly_name`,
`serial`, `media_type` (HDD/SSD/SCM), `health_status`
(Healthy/Warning/Unhealthy/Unknown), `operational_status`, `is_offline`,
`is_readonly`, `disk_number`.
