---
icon: "🔒"
modules: [CheckLogFile, CheckDisk]
action: none
---
**`remote hosts`: which servers a caller may name in a UNC path.** A new key
in `[/settings/logfile]` and `[/settings/disk]` lists the servers a caller
may name when it gives `check_logfile`, `check_csv`, `check_files`,
`check_single_file`, `check_largest_files` or `check_disk_write` a path on a
share (`\\server\share\…`). The default, `*`, is any server, as before, so
nothing changes until you set it; `none` refuses every UNC path a caller
names, and `fs01, fs02.example.com` allows those two. Reading from a share
makes the agent log on to that server with its own account, so on a Windows
host where callers may pass arguments it is worth listing the servers you
mean. A name is matched as the caller wrote it, ignoring case, and a path
defined under `[/settings/logfile/files]` or `[/settings/disk/files]` is
always used. See the
[security notice](../security/notices.md#files-a-caller-names-are-read-only-if-they-are-regular-files-no-named-pipes-or-raw-devices).
