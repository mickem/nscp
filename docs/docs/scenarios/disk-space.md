# Disk Space Alerting

**Goal:** Alert when one or more drives on a Windows machine are running low on free space.

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

### Expected output (healthy)

```
OK: All drives ok
'C:\ used'=45GB;178;200;0;223 'C:\ used %'=20%;79;89;0;100
'D:\ used'=120GB;372;419;0;465 'D:\ used %'=25%;79;89;0;100
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

**Alert if no file has been modified in the last hour:**

```
check_files path=C:\AppData pattern=output.dat "crit=written > -1h"
```

---

## Via NRPE

```
check_nrpe -H <agent-ip> -c check_drivesize
```

---

## Next Steps

- [Windows Server Health](windows-server-health.md) — add disk checks to a full server health baseline
- [Checks In Depth: Thresholds](../checks-in-depth/thresholds.md) — understand warn/crit expressions
- [Checks In Depth: Performance Data](../checks-in-depth/performance-data.md) — customise performance data output
- [Reference: CheckDisk](../reference/windows/CheckDisk.md) — full command reference
