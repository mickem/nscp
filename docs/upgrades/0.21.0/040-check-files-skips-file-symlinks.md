---
icon: "🔒"
modules: [CheckDisk]
action: conditional
---
**`check_files` no longer follows file symbolic links on Windows.** The scan
already skipped directory reparse points (junctions, mount points, directory
links); it now skips file symlinks too, as the Linux scanner always has, so a
link planted inside a scanned tree can no longer make the checksum, `line_count`
or `version` keywords read a file outside it. Deduplicated and cloud-backed files
are still counted. If you relied on `check_files` counting file symlinks, point
it at the link targets instead. Path allow-list wildcards also changed: `*` and
`?` in an `allowed files` entry no longer cross a directory separator, so
`C:/logs/*.log` covers the files in `C:/logs` only; write `C:/logs/**.log` for
the subtree. See
[Restricting what a check may read](../concepts/check-access.md#check_files-check_single_file-and-check_disk_write).
