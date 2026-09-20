#### About `check_disk_write`

`check_disk_write` verifies that a disk (or folder, network share, mount point,
…) is actually writable by performing a full write round-trip: it creates a
test file at the given path, writes a recognizable data pattern to it, flushes
the data through the OS cache to the device (`fsync` on Unix, `_commit` on
Windows), reads the file back and verifies the content, and finally deletes the
file again. Any failure along the way — permission denied, read-only or full
filesystem, data that does not read back as written, a file that cannot be
deleted — is **CRITICAL** out of the box. It works the same on Windows and
Unix.

Behaviour at a glance:

* `file=<path>` (alias `path=`) is the test file to create. Point it at a file
  name on the disk you want to verify (e.g. `D:\temp\probe.dat` or
  `/mnt/backup/probe.dat`). The file is deleted again after the test.
* The check **refuses to touch a file that already exists** — a leftover or
  unrelated file at the target path is reported as CRITICAL instead of being
  overwritten and deleted.
* `size=<bytes>` is how much data to write, either in plain bytes or with a
  byte unit (`512`, `64k`, `1M`). The default is `1k` and the maximum is `1M`
  — the check is a quick probe, not a benchmark. Write more than the default
  when you also want the timing keywords to say something meaningful about
  disk performance.

Default thresholds: **critical** `has_issues = 1` (no default warning). Add
time thresholds (e.g. `warning=total_time > 1000`) to also alert on a disk that
is still writable but slow; keywords used in thresholds are emitted as
performance data.

#### The Linux service sandbox and the probe path

The systemd unit shipped by the DEB and RPM packages sandboxes the service, and
a sandbox that hides a writable mount makes this check report a disk fault that
is not there. The unit is set up so that does not happen by default —
`ProtectSystem=full` leaves `/mnt`, `/srv`, `/media`, `/opt`, `/data` and `/var`
writable, so the usual probe targets work — but two paths are still read-only
inside the service:

| Path | Why |
| --- | --- |
| `/usr`, `/boot`, `/efi`, `/etc` | `ProtectSystem=full` |
| `/home`, `/root` | `ProtectHome=read-only` |

A probe under either answers **CRITICAL** with `Read-only file system`, which
reads exactly like a failing disk. If you need to probe one of them — or if you
tightened the unit to `ProtectSystem=strict`, which makes everything outside the
state and log directories read-only — name the path in a drop-in:

```
systemctl edit nsclient
```

```
[Service]
ReadWritePaths=/home/monitoring
```

Edit a drop-in rather than the unit itself, which a package upgrade replaces.

If you tighten the unit to `ProtectSystem=strict`, restart the service whenever
a mount you probe appears. The read-only remount is applied to the mount tree
as it stands when the service starts, so a filesystem mounted afterwards
propagates into the namespace writable while everything mounted earlier is
read-only — the same probe then answers differently depending on mount order,
and flips on `systemctl restart`. The shipped `ProtectSystem=full` does not
have this problem for any realistic probe target.
