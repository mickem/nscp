#### About `check_group_members`

`check_group_members` reports the membership of a **local group** (default
`Administrators`) via `NetLocalGroupGetMembers`, and — given an expected
allow-list — alerts on **membership drift**: any member that is not on the list.
This is the standard "who is in the local Administrators group, and did that
change?" check.

The check reports one row per member.

Options:

| Option      | Description                                                                                 |
|-------------|---------------------------------------------------------------------------------------------|
| `group`     | Local group to inspect (default `Administrators`; on Linux the first of `sudo`, `wheel`)     |
| `expected`  | An allowed member (repeatable), matched against `DOMAIN\name` or the bare name               |

Default: **CRITICAL** on drift (`expected = 0`). When no `expected=` list is
given, every member is treated as expected, so the check simply **lists** the
group's members and is OK. empty-state is **OK**; a group that does not exist is
reported as an error.

`expected=` entries match case-insensitively against either the full
`DOMAIN\name` (e.g. `BUILTIN\Administrators`, `MYPC\localadmin`) or just the
name (`Administrator`, `Domain Admins`).

#### On Linux

On Linux the group is looked up in **`/etc/group`** (the local files only; a
group that exists only in a directory through NSS is "not found"). Its members
are the users listed in that entry **plus the accounts in `/etc/passwd` whose
primary group it is** — membership `getent group` does not show, and an easy
place to hide an administrator. A user in both places is reported once. Both
files are world-readable, so no privilege is needed.

Every line of `/etc/group` that grants the membership is read, as the system
itself does at logon: a group repeated on a second line, and a line with
another name but the same GID, both add their members. A line for the group
that cannot be parsed (too many or too few fields, non-numeric GID) is an
error rather than a "not found", and the default does not skip past a damaged
`sudo` line to `wheel`. So is **any other** `/etc/group` line that cannot be
parsed, since the system may read it as granting the group through its GID
(`grpck` finds it), and a `/etc/passwd` line that cannot be parsed, since it
may be an account whose primary group this is. The check then returns
**UNKNOWN** naming the line numbers rather than the members it could read.

Without `group=`, the check inspects the first of **`sudo`** (Debian, Ubuntu)
and **`wheel`** (RHEL, Fedora, SUSE) that exists; a host with neither returns
UNKNOWN asking for `group=`. An explicit `group=` that does not exist is an
error, as on Windows.

Each member's `member` and `name` are the bare user name and `domain` is empty.
`sid` is the numeric UID, and `type` is `user` for an account in `/etc/passwd`
or `unknown` for a listed name that is not one (a directory user, or a stale
entry left behind by a deleted account). Linux user names are case-sensitive,
so `expected=` entries match **exactly** there: `expected=root` does not excuse
a look-alike `Root` account.

macOS keeps its groups in Directory Services and reports "not supported".
