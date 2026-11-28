#### About `check_file_security`

`check_file_security` inspects the owner, the group and the permissions of
files, folders and service binaries, and alerts when something can be written by
someone who should not be able to: a world-writable data directory, a service
binary whose owner has changed, a folder that inherits `Everyone: Modify` from
its parent, a configuration file somebody left at mode `0666`.

It works on Windows and on Unix. What is read differs — a security descriptor
on one, mode bits and a POSIX ACL on the other — but both are turned into the
same list of access entries and judged by the same rules, so the keywords, the
options and the default thresholds are the same on every platform.

Options:

| Option | Repeatable | Meaning |
|---|---|---|
| `path` (or `file`) | yes | File or directory to inspect. |
| `service` | yes | Service whose binary is inspected: a Windows service, or a systemd unit. |
| `expected-owner` | yes | An acceptable owner. When omitted the owner is only reported. |
| `expected-group` | yes | An acceptable group. When omitted the group is only reported. |
| `allow-write` | yes | A trustee allowed to hold write access, in addition to the always-allowed ones. |
| `max-depth` | no | Also inspect what is inside a directory, this many levels down. Default `0`: only the paths given. |

At least one `path` or `service` is required; without either the check reports
UNKNOWN rather than a silent OK.

Default thresholds: **critical** when
`exists = 0 or readable = 0 or owner_expected = 0 or group_expected = 0 or world_writable = 1`
and **warning** when `unexpected_write = 1`. Reading permissions requires the
agent to be allowed to, which is why an unreadable path is treated as a finding
instead of a pass.

#### On Windows

Owner, group and DACL are read with `GetNamedSecurityInfo`; the ACL is then
walked entry by entry (`GetAce`) so that **inherited** entries are judged exactly
like the ones set on the object itself — inheritance is how most of these
problems arrive. Trustees are resolved with `LookupAccountSid`. With `service=`
the image path is first read from the service control manager, so the check
follows the binary the service actually runs.

A trustee counts as having write access when its allow entry carries any of
`FILE_WRITE_DATA`, `FILE_APPEND_DATA`, `DELETE`, `FILE_DELETE_CHILD`,
`WRITE_DAC`, `WRITE_OWNER`, `GENERIC_WRITE` or `GENERIC_ALL`. The harmless
`FILE_WRITE_EA`/`FILE_WRITE_ATTRIBUTES` bits, which Windows hands out widely, do
not count on their own. A deny entry covering the same bits cancels the grant.
Audit entries (the SACL) say nothing about who may write and are not read.

These trustees are always allowed to write: `NT AUTHORITY\SYSTEM`,
`BUILTIN\Administrators`, `NT SERVICE\TrustedInstaller` and `CREATOR OWNER`.
Write access held by `Everyone`, `Authenticated Users`, `BUILTIN\Users`,
`BUILTIN\Guests`, `BUILTIN\Power Users` or `ANONYMOUS LOGON` makes the path
**world writable**; any other trustee outside the allow-list is simply
unexpected. All matching is done on the SID first, so it works on a localized
Windows where these groups have translated names. `expected-owner`,
`expected-group` and `allow-write` take a SID, `DOMAIN\name` or the bare name.

`dacl` summarises each entry's rights as letters. `sddl` is the raw form: the
owner, the group and every entry with its exact access mask, in the notation
`icacls` and `Get-Acl` use — the keyword to compare against when a path must
carry one specific descriptor and nothing else.

#### On Unix

Owner, group, mode bits, size and timestamps come from `stat(2)`; a symbolic
link you name is followed, so the verdict is about what it leads to. The three
permission classes become three entries — `user:<owner>`, `group:<group>` and
`other` — and the write bit of each is what counts as write access.

On Linux a **POSIX ACL** is read as well (`setfacl`/`getfacl`), because mode
bits alone can understate who has access: a file at `0640` can still grant a
named user write. Named users and groups are added as entries of their own with
the ACL mask already applied, so what is shown is what the kernel grants, and
`has_acl` is true for such a path. On a directory the *default* ACL — what
every file created inside starts out with — is reported as `default:user:…`,
`default:group:…` and `default:other` entries and judged like the rest. Other
Unix systems report the mode bits only.

`root` (uid 0) and the `root` group (gid 0) are always allowed to write. A
write bit on `other` makes the path **world writable**. Any other user or group
with write access is unexpected until it is allowed:

- `expected-owner` takes a user name or a numeric uid, `expected-group` a group
  name or a gid. Naming the owner as expected also accepts the owner's own write
  bit — whoever owns a file can change its mode anyway — but nobody else's.
- `allow-write` takes a name (`www-data`, matching a user or a group of that
  name), `user:www-data` or `group:www-data` to say which, `uid:33` or `gid:33`,
  or `other` for a path that is meant to be open to everyone, such as a spool
  directory.

A world-writable directory with the sticky bit (`/tmp`, mode `1777`) is still
world writable and CRITICAL by default; `sticky` is there to tell the two
apart, e.g. `critical=world_writable = 1 and sticky = 0`.

`mode` holds the permission bits as four octal digits (`0644`, `4755`), for
when "nobody unexpected can write" is not strict enough and a path must have
one exact mode: `critical=mode != '0600'`. `setuid`, `setgid` and `sticky`
expose the special bits on their own.

With `service=` the check looks the unit up the way systemd does (`/etc`, then
`/run`, then `/usr/local/lib`, `/usr/lib` and `/lib`, drop-ins included, an
instance such as `getty@tty1` through its template) and inspects the program
its `ExecStart=` runs. `.service` is implied when the name has no suffix.

#### File metadata and directory trees

`size`, `written`, `access`, `creation` and `age` report the size and the
timestamps of each path next to its permissions. `creation` is the creation time
on Windows and the last inode change (owner, mode or link count) on Unix, the
same meaning it has in `check_files`. Dates compare like they do there:
`warning=written > -1d` is "changed within the last day".

`max-depth=1` adds the direct children of a directory to the paths inspected,
`max-depth=2` their children too, and so on. Every entry is judged on its own
and counted in `${count}`; symbolic links inside the tree are not followed. A
directory that cannot be listed is reported as unreadable rather than skipped.
The default top syntax lists every path, which a deep tree makes long — use
`${problem_list}` to show only what is wrong.
