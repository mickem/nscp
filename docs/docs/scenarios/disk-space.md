# Disk Space Alerting

**Goal:** Alert when one or more drives are running low on free space.

The `CheckDisk` module and all commands on this page work on **Windows and
Linux**. The commands, filters and thresholds are identical; the only
practical difference is how drives are named — drive letters (`C:`) on
Windows, mount points (`/`, `/var`, …) on Linux.

---

## Prerequisites

Enable the `CheckDisk` module in `nsclient.ini`:

```ini
[/modules]
CheckDisk  = enabled
NRPEServer = enabled   ; if using NRPE
```

---

## Basic Disk Space Check

### Command

```
check_drivesize
```

### Expected output (healthy, Windows)

```
OK: All drives ok
'C:\ used'=45GB;178;200;0;223 'C:\ used %'=20%;79;89;0;100
'D:\ used'=120GB;372;419;0;465 'D:\ used %'=25%;79;89;0;100
```

### Expected output (healthy, Linux)

```
OK All 1 drive(s) are ok
'/ used'=37.11GB;805;906;0;1006 '/ used %'=4%;80;90;0;100
```

### Expected output (alert)

```
CRITICAL: CRITICAL: C:\: 205GB/223GB used, D:\: 448GB/466GB used
'C:\ used'=205GB;178;200;0;223 'C:\ used %'=91%;79;89;0;100
```

The default thresholds are: **warning at 79% used**, **critical at 89% used**, across all local fixed drives.

---

## Common Scenarios

### Check only a specific drive

```
check_drivesize drive=C: "warn=free < 20%" "crit=free < 10%"
```

Output:
```
OK: All drives ok
'C:\ free'=45GB;44;22;0;223 'C:\ free %'=20%;19;9;0;100
```

On Linux, use the mount point instead of a drive letter:

```
check_drivesize drive=/ "warn=free < 20%" "crit=free < 10%"
check_drivesize drive=/var "warn=free < 20%" "crit=free < 10%"
```

### Check all drives

```
check_drivesize drive=* "warn=free < 10%" "crit=free < 5%"
```

### Check all drives and show all values (not just problems)

```
check_drivesize drive=* show-all
```

### Check only fixed and network drives (exclude removables)

```
check_drivesize drive=* "filter=type in ('fixed', 'remote')"
```

### Check fixed drives and exclude specific letters

```
check_drivesize drive=* "filter=type in ('fixed', 'remote')" exclude=C:\ exclude=D:\
```

### Check all volumes including mounted folders

```
check_drivesize "crit=free < 1M" drive=all-volumes
```

### Check a specific mounted volume (e.g. `C:\data`)

```
check_drivesize "warn=free < 10M" "crit=free < 1M" drive=C:\\data
```

---

## Performance Data

By default the unit of performance data values is chosen automatically (bytes, KB, MB, GB, TB) which can cause issues in some graphing systems where the unit changes between checks.

**Force all values to gigabytes:**

```
check_drivesize "perf-config=*(unit:G)"
```

**Show only used space (drop percentage metrics):**

```
check_drivesize "perf-config=used.used(unit:G;suffix:'') used %(ignored:true)"
```

---

## Checking Files and Directories

`check_files` lets you check properties of individual files — useful for detecting log files that grow too large or files that haven't been updated recently.

**Alert if a log file is over 100 MB:**

```
check_files path=C:\Logs pattern=*.log "crit=size > 100M"
```

On Linux:

```
check_files path=/var/log pattern=*.log "crit=size > 100M"
```

**Alert if no file has been modified in the last hour:**

```
check_files path=C:\AppData pattern=output.dat "crit=written > -1h"
```

---

## Disk I/O and Combined Health

Beyond free space, `CheckDisk` samples per-device I/O once per second on both
platforms (PDH counters on Windows, `/proc/diskstats` on Linux).

**Alert when a device is saturated:**

```
check_disk_io "warn=percent_disk_time > 80" "crit=percent_disk_time > 95"
```

**Combined space + I/O health in one check** (this is also the default
threshold set):

```
check_disk_health
```

The default alerts when a filesystem is low on space (`free_pct < 20` / `10`)
*or* a device is busy (`percent_disk_time > 80` / `95`). Devices without a
mounted filesystem (raw disks, swap partitions) carry no space data and are
only evaluated against the I/O clauses — the `has_space` keyword guards the
space thresholds.

Linux notes:

- I/O rates need one collector sample, so the very first query after startup
  can return UNKNOWN ("collector still initializing") — normal in one-shot
  testing, invisible with a running service.
- LVM / device-mapper and RAID volumes are mapped back to their backing
  devices via sysfs, so `check_disk_health` correctly joins `/dev/mapper/…`
  filesystems with the physical device's I/O load.

---

## Via NRPE

```
check_nrpe -H <agent-ip> -c check_drivesize
```

---

## Next Steps

- [Windows Server Health](windows-server-health.md) — add disk checks to a full server health baseline
- [Checks In Depth: Thresholds](../concepts/checks.md#4-thresholds-choosing-whats-a-problem) — understand warn/crit expressions
- [Checks In Depth: Performance Data](../concepts/checks.md#7-performance-data) — customise performance data output
- [Reference: CheckDisk (Windows)](../reference/windows/CheckDisk.md) — full command reference
- [Reference: CheckDisk (Linux)](../reference/unix/CheckDisk.md) — full command reference
