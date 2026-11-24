---
title: "Linux account checks expose the local account list and password state"
fixed_in: next
severity: "Informational"
modules: [CheckSecurity]
action: conditional
---
`check_local_accounts` and `check_group_members` in `CheckSecurity` were
Windows-only and now return data on Linux. Anyone allowed to run checks on the
agent (REST, NRPE, or any other enabled transport) can now read what they
report:

- `check_local_accounts` without access to `/etc/shadow` (the packaged agent
  runs as the unprivileged `nsclient` user): every account in `/etc/passwd`
  with its UID, GID, home and shell, whether the shell allows an interactive
  logon, and any UID 0 account other than root. The check itself returns
  UNKNOWN in that case unless `allow-unreadable-shadow` is given.
- `check_local_accounts` with read access to `/etc/shadow` (the agent runs as
  root, or, on Debian, is in the `shadow` group): additionally, per account,
  whether the password is set, empty or locked, whether the account has
  expired, whether the password expires, and when it was last changed. That
  identifies accounts with no password or a stale one.
- `check_group_members`: the members of any local group the caller names in
  `/etc/group`, including accounts whose primary group it is, with their UIDs.
  It needs no privilege: both files are world-readable.

Password hashes are classified when read and are never kept, logged or
printed. The same checks already expose the equivalent data on Windows.

**What to do:** nothing if only trusted monitoring servers can run checks.
Otherwise restrict who can reach these commands (`allowed hosts`, REST roles),
and do not give the agent read access to `/etc/shadow` unless you want the
password state reported.
