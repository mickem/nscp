---
icon: "🔒"
modules: [CheckLogFile]
action: none
---
**`check_logfile` on Windows reads only files on a disk, and can no longer be
used to impersonate the agent through a named pipe.** Nothing to do. A path
naming a named pipe, a console or another device is now refused with
`Failed to open file: <path>: not a regular file` instead of being opened:
with `file=\\.\pipe\<name>` the pipe's server could act as the agent's
account. Log files held open by the program writing them are still read, and
file names outside the system's ANSI code page now open as well. See the
[security notice](../security/notices.md#check_logfile-a-named-pipe-path-no-longer-lets-the-pipes-server-act-as-the-agent).
