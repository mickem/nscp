---
icon: "🆕 🔒"
modules: [CheckDisk]
action: none
---
**New, experimental `check_largest_files`: find what is filling a disk.**
`CheckDisk` gained a check that returns the `top` largest files beneath `path`,
largest first. With `type=folders` it ranks folders by their recursive totals
instead, at the depth `folder-depth` selects. `min-size` and `pattern` narrow
what is counted. `timeout` (default 30 seconds) bounds the scan and reports a
partial result rather than hanging the check. On Windows it reads the NTFS
Master File Table when it can (a local NTFS volume and administrator rights,
which the service has), which takes seconds where walking a large volume takes
minutes. Otherwise, and always on Linux, it walks the tree. Neither method
follows symbolic links, junctions or mount points. Nothing to do: the check is
only there when you call it, and `path` is gated by `file access` in
`[/settings/disk]`. See the [check_largest_files reference](../reference/check/CheckDisk.md#check_largest_files)
and the
[security notice](../security/notices.md#check_largest_files-reads-ntfs-volume-metadata-directly)
on the raw volume reads.
