The outputs for physical drives below were produced by the check over recorded
ATA attribute tables and an NVMe health log; the virtual-machine outputs are from
a live Linux VM.

**Default check:**

With the default thresholds the check is OK and lists each drive's verdict,
temperature and (for SSDs) wear:

```
check_smart
OK: /dev/sda (WDC WD40EFRX-68N32N0): PASSED, 31C, /dev/nvme0 (Samsung SSD 980 1TB): PASSED, 41C, 3% worn
```

A drive with remapped sectors warns, and a drive that predicts its own failure
is critical:

```
check_smart
WARNING: /dev/sda (WDC WD40EFRX-68N32N0): PASSED: 8 reallocated, 31C
```

```
check_smart
CRITICAL: /dev/sdb (ST4000DM004-2CV104): FAILED: Reallocated_Sector_Ct failing, 1800 reallocated, 8 pending, 38C
```

**Temperature bounds with `-w` / `-c`:**

`-w` and `-c` bound the temperature in °C and keep the rest of the default
conditions:

```
check_smart -w 50 -c 60 show-query
"filter=smart_state != 'unsupported'" "warning=temperature > 50 or smart_state = 'denied' or smart_state = 'error' or failing_attributes > 0 or reallocated_sectors > 0 or pending_sectors > 0 or uncorrectable > 0 or reported_uncorrectable > 0 or media_errors > 0 or wear > 80" "critical=temperature > 60 or predict_failure = 1" ...
```

**Virtual machines:**

Virtual disks have no SMART and are filtered out by default, so a VM lands in
the empty state (`UNKNOWN`). Use `empty-state=ok` when the same check runs on
physical and virtual hosts, or `filter=none` to see every device:

```
check_smart
No drives with SMART data found

check_smart filter=none
OK: /dev/vda: SMART not supported (virtual disk), /dev/vdb: SMART not supported (virtual disk)
```

**An agent without the privileges to read SMART:**

On Linux the agent must run as root. Otherwise each drive is reported as
`denied` and the check warns rather than passing unread drives as healthy:

```
check_smart
WARNING: /dev/sda (WDC WD40EFRX-68N32N0): SMART not readable: permission denied on /dev/sda (run the agent as root or grant it CAP_SYS_RAWIO)
```

**NVMe endurance and spare capacity:**

```
check_smart "filter=protocol = 'nvme'" "warning=wear > 70 or available_spare < 20" "critical=predict_failure = 1 or available_spare <= available_spare_threshold" "detail-syntax=${name}: ${wear}% used, ${available_spare}% spare, ${data_written} written"
OK: /dev/nvme0 (Samsung SSD 980 1TB): 3% used, 100% spare, 2.521TB written
```

**Inspect the raw ATA attributes:**

```
check_smart "filter=protocol = 'ata'" "top-syntax=${list}" "detail-syntax=${drive}: ${attributes}"
/dev/sda: Raw_Read_Error_Rate=0, Reallocated_Sector_Ct=0, Power_On_Hours=28123, Power_Cycle_Count=321, Temperature_Celsius=193274839071, Current_Pending_Sector=0, Offline_Uncorrectable=0, UDMA_CRC_Error_Count=0
```

The raw value is the full 48-bit field; vendors pack other data into its upper
bytes. `Temperature_Celsius` above is `0x002D0014001F`: 31°C now, with the
minimum (20) and maximum (45) seen above it — smartctl shows it as
`31 (Min/Max 20/45)`. The derived keywords (`temperature`, `power_on_hours`,
...) read only the part that holds the value.
