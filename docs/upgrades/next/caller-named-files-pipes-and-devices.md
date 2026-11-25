---
icon: "🔒"
modules: [CheckLogFile, CheckDisk]
action: conditional
---
**Files a caller names are read only if they are regular files.**
`check_logfile` and the checksum, `line_count` and `version`
keywords of `check_files` / `check_single_file` now refuse a named pipe, a
mailslot, a raw volume or disk, the console and other devices. On Windows
those paths (`\\.\pipe\x`, `\\host\pipe\x`, `\\host\IPC$\x`, `\\.\C:`, `CON`, …) are refused by
name with `Refusing to open <path>: it names a device or a pipe, not a file`,
and `check_largest_files` and `check_disk_write` refuse them too: a pipe's
server could act as the agent's account, and a raw volume could be read whole.
A path on a share (`\\server\share\…`) is still read, but only once the share
has been seen to be a file system; a share whose root the agent may neither
open nor ask the server about is no longer read from.
On Linux and macOS `check_logfile` now refuses a FIFO or a character device
(`Not a regular file: <path>`), which used to hang the check or read until
`max-size`; nothing to do unless a check really pointed at one. The new
`check_csv` uses the same reader and ships with the same rules. Log files held
open by the program writing them are still read, and on Windows file names
outside the ANSI code page now open as well. See the
[security notice](../security/notices.md#files-a-caller-names-are-read-only-if-they-are-regular-files-no-named-pipes-or-raw-devices).
