---
icon: "💽"
modules: [CheckDisk]
action: none
---
**New check: `check_smart` — SMART health and predictive drive failure.**
Reads the SMART data of every physical drive natively, with no `smartctl`
installed: `DeviceIoControl` on `\\.\PhysicalDriveN` (ATA SMART commands, and
the NVMe health log on Windows 10 and later) with the `root\WMI`
`MSStorageDriver_FailurePredict*` classes as fallback on Windows, and `SG_IO`
ATA pass-through and the NVMe admin ioctl on Linux. Each drive reports the
drive's own verdict (`health`, `predict_failure`) and the early-warning
attributes: `reallocated_sectors`, `pending_sectors`, `uncorrectable`,
`reported_uncorrectable`, `media_errors`, `available_spare`, `wear` (percent of
rated SSD endurance used), `temperature`, `power_on_hours` and more. The
defaults warn on any reallocated, pending or uncorrectable sector, an attribute
at its vendor threshold, `wear > 80` or a drive whose SMART could not be read,
and go critical when the drive predicts its own failure; `-w` / `-c` bound the
temperature. Reading SMART needs an elevated agent — on Linux root, otherwise
drives report `smart_state=denied` and the check warns. Virtual disks have no
SMART and are filtered out, so a VM returns `UNKNOWN: No drives with SMART data
found` unless `empty-state=ok` is given. The check is marked experimental.
