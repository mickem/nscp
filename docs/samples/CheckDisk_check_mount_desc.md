#### About `check_mount`

`check_mount` verifies that filesystems are mounted, and optionally that they
are mounted with the expected filesystem type and options. It reads the live
mount table (`/proc/self/mounts` via `getmntent`) so it reflects the actual
running state, not `/etc/fstab`. It is implemented on **Unix only**; on Windows
it reports that it is not supported.

Behaviour at a glance:

* With no `mount=` it inspects every *real* mount (pseudo-filesystems such as
  `proc`, `sysfs`, `cgroup`, `tmpfs` overlays … are skipped).
* With `mount=<path>` it inspects only that mount point, and reports
  **CRITICAL** `not mounted` when nothing is mounted there.
* `fstype=<type>` requires the mount to use that filesystem type; a mismatch is
  flagged as an `expected fstype differs` issue.
* `options=<a,b,c>` requires each listed mount option to be present; any missing
  option is flagged as a `missing options` issue.

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword      | Description                                             |
|--------------|--------------------------------------------------------|
| `mount`      | Path of the mounted folder                             |
| `device`     | Device backing this mount                              |
| `fstype`     | Filesystem type of this mount                          |
| `options`    | Mount options (comma separated)                        |
| `issues`     | Human-readable description of any problems found       |
| `has_issues` | `1` when this mount has one or more issues, else `0`   |

Default thresholds: **warning** `has_issues = 1`, **critical**
`issues like 'not mounted'`. So a missing filesystem is CRITICAL while a
fstype/options mismatch is WARNING out of the box; override `warning=` /
`critical=` to change that.
