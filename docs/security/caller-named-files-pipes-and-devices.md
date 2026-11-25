---
title: "Files a caller names are read only if they are regular files: no named pipes or raw devices"
fixed_in: next
severity: "High on Windows where callers may pass arguments to check_logfile or the CheckDisk file checks (privilege escalation to the agent's account, SYSTEM by default, and raw reads of a volume); not reachable otherwise"
modules: [CheckLogFile, CheckDisk]
action: conditional
---
Several checks open a file whose name the caller passes: `check_logfile
file=`, and in CheckDisk the `md5`/`sha1`/`sha256`/
`sha384`/`sha512`, `line_count` and `version` keywords of `check_files` and
`check_single_file`, as well as the root of `check_largest_files` and the
target of `check_disk_write`. On Windows, a name in the device namespace was
opened like any other path:

- **A named pipe** (`\\.\pipe\<name>`, `\\host\pipe\<name>`, or the same pipe
  as a file on the `IPC$` share, `\\host\IPC$\<name>`). `check_logfile`
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
  these as disk files, so `check_logfile` would read the volume itself, with
  every file on it and no ACL in the way.

The new `check_csv` (not yet in a release) reads through the same reader,
so it ships with these protections.

Reaching any of this needs a caller who chooses the path: NRPE with
`allow arguments = true`, or a REST user not on the no-arguments
[`restricted` role](../setup/securing.md#adding-a-dedicated-user). The
`file access` modes of `[/settings/logfile]` and `[/settings/disk]` default
to `any`, so a default install was exposed wherever such a caller exists.
`predefined` mode was not affected. `allowed` mode would refuse such a path,
but only after resolving it, and on Windows resolving it already opened it.

#### What changed

All of these, and `check_csv`, now go through one reader for caller-named files, which:

- refuses, by name and before anything is opened, paths in the Windows device
  namespace - `\\.\…`, `\\?\…` other than a drive or share path, `\\<host>\pipe\…`,
  `\\<host>\mailslot\…`, `\\<host>\IPC$\…` and the DOS device names (`CON`,
  `NUL`, `COM1`, `COM1:`, …) in any directory - with `Refusing to open <path>:
  it names a device or a pipe, not a file`. `/` and `\`, and a run of either,
  are read as Windows reads them, so `//host//pipe/x` is the same name;
- opens with `SECURITY_IDENTIFICATION`, so a local pipe reached some other way
  (a link) still cannot impersonate the agent. Windows ignores that flag for a
  pipe reached over the network, so it is not what protects a UNC path;
- for a UNC path, examines the share before opening anything on it: the root
  of the share is opened (which reaches no pipe, whatever the share is) and
  only a share that is a file system is read from. Where the agent's account
  may not open the root, the server is asked for the share's type. A share
  that is not a file system is refused with `Refusing to open <path>: its
  share is not a file system`, and one that cannot be examined is not opened;
- reads only a regular file, examined on the open handle, and refuses anything
  else with `Not a regular file: <path>`. On Linux and macOS this means
  `check_logfile` now refuses a FIFO (which used to hang the check) and a
  character device such as `/dev/zero` (which it used to read up to
  `max-size`);
- opens with every share flag, so a log the writing program holds open is
  still read, and on Windows passes a UTF-8 file name as UTF-16, so
  `check_logfile` now opens names outside the ANSI code page.

#### Which servers a caller may name

Reading a file on a share is a network logon: the agent authenticates to the
server in the path with its own account, which for a service running as
`SYSTEM` is the machine account. A caller who may name any path may therefore
name any server, and have the agent log on to a machine of the caller's
choosing. This is not new and it is not closed by default, because checking
files on a file server is a legitimate and common use - but it can now be
narrowed. `remote hosts` in `[/settings/logfile]` and `[/settings/disk]` lists
the servers a caller may name in a UNC path:

```ini
[/settings/logfile]
remote hosts = fs01, fs02.example.com
```

`*` (the default) is any server and `none` is no server. It holds whatever
`file access` is set to; a file defined under `[/settings/logfile/files]` or
`[/settings/disk/files]` is the operator's own and is always used. See
[Restricting what a check may read](../concepts/check-access.md#which-servers-a-caller-may-name).

`check_files` appends its `pattern` to the folder it scans, and outside the
restricted modes a pattern may carry separators. The joined search string is
now held to the root that was checked: a pattern which would turn the root
into another server, another share or a device (`path=\ pattern=\server\share\*`)
is refused with `Refusing pattern …`, and a root ending in a separator is no
longer joined with a second one.

The `allowed` mode of `file access` refuses device names before it resolves
them. `check_files`, `check_single_file` and `check_largest_files` refuse such
a path as they would a missing one, and `check_disk_write` refuses it outright.

**What to do:** upgrade. Until then, on Windows hosts where callers may pass
arguments, set `file access = predefined` in `[/settings/logfile]` and
`[/settings/disk]`, or do not let callers pass arguments to these checks.
After upgrading, on those hosts, set `remote hosts` to the file servers your
checks actually read from, or to `none` if they read local files only.
