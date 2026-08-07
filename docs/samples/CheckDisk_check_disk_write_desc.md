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

Available keywords (for `filter=` / `warning=` / `critical=` / syntax):

| Keyword      | Description                                                     |
|--------------|-----------------------------------------------------------------|
| `path`       | Path of the test file                                           |
| `size`       | Number of bytes written to (and read back from) the test file   |
| `write_time` | Time spent creating, writing and flushing the file to disk (ms) |
| `read_time`  | Time spent reading back and verifying the file (ms)             |
| `total_time` | Total time for the create/write/read/delete cycle (ms)          |
| `issues`     | Human-readable description of any problems found                |
| `has_issues` | `1` when the write test failed, else `0`                        |
| `message`    | Human readable outcome of the write test                        |

Default thresholds: **critical** `has_issues = 1` (no default warning). Add
time thresholds (e.g. `warning=total_time > 1000`) to also alert on a disk that
is still writable but slow; keywords used in thresholds are emitted as
performance data.
