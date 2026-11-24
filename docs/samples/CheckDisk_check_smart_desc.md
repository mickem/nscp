Checks the SMART health of every physical drive: the drive's own pass/fail
verdict plus the attributes that give early warning of disk loss — reallocated,
pending and uncorrectable sectors, NVMe media errors and spare capacity, SSD
wear and temperature. One row is reported per drive.

The data is read natively, with no `smartctl` or vendor tool installed:

| Platform | Drive type | How it is read |
|----------|------------|----------------|
| Windows  | SATA/ATA   | `SMART_RCV_DRIVE_DATA` / `SMART_SEND_DRIVE_COMMAND` on `\\.\PhysicalDriveN` (`source=ioctl`) |
| Windows  | NVMe       | `IOCTL_STORAGE_QUERY_PROPERTY` for the SMART / Health Information log page (Windows 10 / Server 2016 and later) |
| Windows  | Anything the IOCTLs cannot reach | `MSStorageDriver_FailurePredictStatus` / `...Data` / `...Thresholds` in `root\WMI` (`source=wmi`) |
| Linux    | SATA/ATA (and USB bridges that implement SAT) | `SG_IO` with an ATA PASS-THROUGH(16) command (`source=sg_io`) |
| Linux    | NVMe       | `NVME_IOCTL_ADMIN_CMD`, Get Log Page 02h (`source=nvme_ioctl`) |

#### Privileges

Reading SMART needs an elevated agent: the Windows service (LocalSystem) has
that by default. On Linux the ATA path needs `CAP_SYS_RAWIO` and the NVMe path
`CAP_SYS_ADMIN`, which in practice means running the agent as root. A drive that
cannot be read is **not** skipped: it is reported with `smart_state=denied` (or
`error`) and the default warning fires, so an unprivileged agent says so instead
of quietly reporting healthy drives.

#### Drives without SMART

Virtual disks (virtio, Hyper-V, Storage Spaces), hardware RAID volumes and SAS
disks have no ATA or NVMe SMART to read; they get `smart_state=unsupported`. On
Windows a disk on any bus other than ATA/SATA or NVMe (USB, SD, SAS, iSCSI,
RAID) is still asked, since some drivers pass SMART through, but a failure there
is `unsupported` rather than `error`; only a failed read of an ATA/SATA or NVMe
drive is an `error`. The
default filter (`smart_state != 'unsupported'`) drops them, so a virtual machine
lands in the empty state — `UNKNOWN: No drives with SMART data found`. Add
`empty-state=ok` when the check is deployed to physical and virtual hosts alike.
SAS/SCSI log pages are not read.

#### Values that are not reported

A drive only reports the attributes its vendor implements. Error counters it does
not keep (an NVMe drive has no "reallocated sectors") read `0`; measurements it
does not report (`temperature`, `wear`, `power_on_hours`, `power_cycles`,
`available_spare`, `data_written`, `data_read`) read `-1`. So `temperature > 55`
never fires on a drive without a temperature sensor.

`wear` is the share of the rated write endurance used: the NVMe *percentage
used* (which may pass 100 on a drive that has outlived its rating), or for an ATA
SSD 100 minus the normalised value of the first life-remaining attribute found
(231, 233, 177, 202, 169). Those ids mean other things on some hard drives, so
an ATA drive's wear is only read when it reports itself as solid-state (`media`
is `ssd`, from the rotation rate in its IDENTIFY data); a drive read through the
Windows WMI fallback has no IDENTIFY data and reports no wear. ATA counters are taken from the low 32 bits of the raw
value, which is where every vendor keeps the count.

#### Thresholds

| Option     | Default |
|------------|---------|
| `filter`   | `smart_state != 'unsupported'` |
| `warning`  | `smart_state = 'denied' or smart_state = 'error' or failing_attributes > 0 or reallocated_sectors > 0 or pending_sectors > 0 or uncorrectable > 0 or reported_uncorrectable > 0 or media_errors > 0 or wear > 80` |
| `critical` | `predict_failure = 1` |

`predict_failure` is the drive's own verdict: an ATA *SMART RETURN STATUS* that
reports a threshold exceeded (or, when the status cannot be read, a pre-failure
attribute at or below its threshold), any NVMe critical warning bit, or WMI's
`PredictFailure`. `failing_attributes` counts every attribute at or below its
vendor threshold, including old-age ones (such as a too-hot airflow
temperature) that do not by themselves fail the drive.

`-w` and `-c` bound the temperature, the one value here with no vendor
threshold behind it: `check_smart -w 50 -c 60` warns above 50°C and goes
critical above 60°C, on top of the default conditions. Interface CRC errors
(`crc_errors`) are not in the defaults: they almost always point at a cable or
backplane and the counter never resets, so add them explicitly if wanted.

The `serial` keyword is available for templates and filters but is not part of
the default output.
