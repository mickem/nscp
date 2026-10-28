---
title: "check_largest_files reads NTFS volume metadata directly"
fixed_in: next
severity: "Low (informational; a new check, gated like check_files)"
modules: [CheckDisk]
action: conditional
---
The new, experimental `check_largest_files` lists the largest files or folders
beneath a folder the caller names. It reaches what `check_files` already
reaches: the name and size of everything beneath `path=`.

The difference is how it reads them on Windows. Rather than asking the
filesystem for each folder's listing, it opens the NTFS volume for raw reads and
parses the Master File Table itself, so **per-folder permissions are not
consulted**. A folder whose ACL denies even `SYSTEM` is listed all the same.
Only names and sizes beneath the requested root are reported. File contents
are never read, and nothing outside the root leaves the agent. On Linux, and
whenever the Master File Table cannot be used, the check walks the tree with
the agent's own permissions, as `check_files` does.

The root is held against `file access` in `[/settings/disk]`, the same gate as
`check_files`, `check_single_file` and `check_disk_write` (see
[Restricting what a check may read](../concepts/check-access.md)). It is the
only thing checked: neither scan follows a link, so everything reported lies
beneath a root that passed.

**What to do:** nothing on a host where only the configuration decides which
checks run. Where callers choose arguments (NRPE with
`allow arguments = true`, or REST users outside the `restricted` role), set
`file access = allowed` or `predefined` if you have not already, or pass
`method=walk` in your own command definitions to keep the scan inside the
filesystem's permission checks.
