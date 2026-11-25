---
title: "Files a caller names are read only if they are regular files: no named pipes or raw devices"
fixed_in: next
severity: "High on Windows where callers may pass arguments to check_logfile, check_csv or the CheckDisk file checks (privilege escalation to the agent's account, SYSTEM by default, and raw reads of a volume); not reachable otherwise"
modules: [CheckLogFile, CheckDisk]
action: conditional
---
Several checks open a file whose name the caller passes: `check_logfile
file=`, `check_csv file=`, and in CheckDisk the `md5`/`sha1`/`sha256`/
`sha384`/`sha512`, `line_count` and `version` keywords of `check_files` and
`check_single_file`, as well as the root of `check_largest_files` and the
target of `check_disk_write`. On Windows, a name in the device namespace was
opened like any other path:

- **A named pipe** (`\\.\pipe\<name>`, `\\host\pipe\<name>`). `check_logfile`
  opened files with the C++ runtime's `std::ifstream`, and the CheckDisk
  keywords with `fopen` and `GetFileVersionInfo`. These connect to a pipe
  without quality-of-service flags, which grants the pipe's server the right
  to impersonate the client. A local user who created a pipe and got the
  agent to open it could call `ImpersonateNamedPipeClient` and then act as
  the agent's account - `SYSTEM` on a default install. The connection is
  made by the open itself, before anything is read, and in CheckDisk already
  by the `GetFileAttributes` call that decides whether the path is a file or a
  folder.
- **A raw volume or disk** (`\\.\C:`, `\\.\PhysicalDrive0`). Windows reports
  these as disk files, so `check_logfile` and `check_csv` - whose only test was
  "is this a disk file" - would read the volume itself, with every file on it
  and no ACL in the way.

`check_csv` already connected to pipes safely (it shipped with that fix) but
still read raw volumes.

Reaching any of this needs a caller who chooses the path: NRPE with
`allow arguments = true`, or a REST user not on the no-arguments
[`restricted` role](../setup/securing.md#adding-a-dedicated-user). The
`file access` modes of `[/settings/logfile]` and `[/settings/disk]` default
to `any`, so a default install was exposed wherever such a caller exists.
`predefined` mode was not affected. `allowed` mode would refuse such a path,
but only after resolving it, and on Windows resolving it already opened it.

#### What changed

All of these now go through one reader for caller-named files, which:

- refuses, by name and before anything is opened, paths in the Windows device
  namespace - `\\.\…`, `\\?\…` other than a drive or share path, `\\<host>\pipe\…`,
  `\\<host>\mailslot\…` and the DOS device names (`CON`, `NUL`, `COM1`, …) in
  any directory - with `Refusing to open <path>: it names a device or a pipe,
  not a file`;
- opens with `SECURITY_IDENTIFICATION`, so a pipe reached some other way (a
  link) still cannot impersonate the agent;
- reads only a regular file, examined on the open handle, and refuses anything
  else with `Not a regular file: <path>`. On Linux and macOS this means
  `check_logfile` now refuses a FIFO (which used to hang the check) and a
  character device such as `/dev/zero` (which it used to read up to
  `max-size`), as `check_csv` already did;
- opens with every share flag, so a log the writing program holds open is
  still read, and on Windows passes a UTF-8 file name as UTF-16, so
  `check_logfile` now opens names outside the ANSI code page.

The `allowed` mode of `file access` refuses device names before it resolves
them. `check_files`, `check_single_file` and `check_largest_files` refuse such
a path as they would a missing one, and `check_disk_write` refuses it outright.

**What to do:** upgrade. Until then, on Windows hosts where callers may pass
arguments, set `file access = predefined` in `[/settings/logfile]` and
`[/settings/disk]`, or do not let callers pass arguments to these checks.
