---
title: "check_logfile: a named-pipe path no longer lets the pipe's server act as the agent"
fixed_in: next
severity: "High where callers may pass arguments to check_logfile on Windows (local privilege escalation to the agent's account, SYSTEM by default); not reachable otherwise"
modules: [CheckLogFile]
action: conditional
---
On Windows, `check_logfile` opened the file named by `file=` (and `files=`)
with the C++ runtime's `std::ifstream`. That accepts any path `CreateFile`
does, including a named pipe (`\\.\pipe\<name>`), and it connects to the pipe
without quality-of-service flags, which grants the pipe's server the right to
impersonate the client. A local user who created a pipe and got the agent to
open it could call `ImpersonateNamedPipeClient` and then act as the agent's
account — `SYSTEM` on a default install. The connection is made by the open
itself, before any line is read, so nothing about the file's contents or the
filter mattered.

Reaching it needs a caller who chooses `file=`: NRPE with `allow arguments =
true`, or a REST user not on the no-arguments
[`restricted` role](../setup/securing.md#adding-a-dedicated-user). The
`[/settings/logfile]` `file access` mode defaults to `any`, so a default
install exposed it wherever such a caller exists. `predefined` mode was not
affected, since the caller can only pick a name the operator configured.
Real-time filters read only paths from the configuration and were not
reachable by a caller, but use the same reader and are fixed too.

#### What changed

`check_logfile` and the real-time reader now open files with `CreateFileW`
and `SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION`, so a pipe server only
learns who connected and cannot act as that account, and they refuse any
handle that is not a file on a disk (a named pipe, a console, a device)
with `Failed to open file: <path>: not a regular file`. That also stops a
pipe or device path from blocking the check in a read that never returns.
Files are opened with every share flag, so a log the writing program holds
open is still read, and a file name given as UTF-8 is passed on as UTF-16,
so names outside the system's ANSI code page now open too. Linux and macOS are unchanged.

**What to do:** upgrade. Until then, on Windows hosts where callers may pass
arguments, set `[/settings/logfile]` `file access = predefined` (or
`allowed` with an allow list), or do not let callers pass arguments to
`check_logfile`.
