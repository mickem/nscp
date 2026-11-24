---
title: "Linux account checks expose the local account list and password state"
fixed_in: next
severity: "Informational"
modules: [CheckSecurity]
action: conditional
---
`check_local_accounts` in `CheckSecurity` was Windows-only and now returns
data on Linux. Anyone allowed to run checks on the agent (REST, NRPE, or any
other enabled transport) can now read what it reports:

- Without access to `/etc/shadow` (the packaged agent runs as the
  unprivileged `nsclient` user): every account in `/etc/passwd` with its UID,
  GID, home and shell, whether the shell allows an interactive logon, and any
  UID 0 account other than root. The check itself returns UNKNOWN in that case
  unless `allow-unreadable-shadow` is given.
- With read access to `/etc/shadow` (the agent runs as root, or, on Debian, is
  in the `shadow` group): additionally, per account, whether the password is
  set, empty or locked, whether the account has expired, whether the password
  expires, and when it was last changed. That identifies accounts with no
  password or a stale one.

Password hashes are classified when read and are never kept, logged or
printed. The same check already exposes the equivalent data on Windows.

**What to do:** nothing if only trusted monitoring servers can run checks.
Otherwise restrict who can reach these commands (`allowed hosts`, REST roles),
and do not give the agent read access to `/etc/shadow` unless you want the
password state reported.
