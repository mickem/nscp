---
title: "Linux posture and inventory checks expose account and hardware details"
fixed_in: next
severity: "Informational"
modules: [CheckSecurity, CheckSystem]
action: conditional
---
Five checks that were Windows-only now return data on Linux:
`check_local_accounts`, `check_group_members` and `check_secureboot` in
`CheckSecurity`, and `check_pending_reboot` and `check_hardware` in
`CheckSystem`. Anyone allowed to run checks on the agent (REST, NRPE, or any
other enabled transport) can now read what they report:

- `check_local_accounts`: every account in `/etc/passwd` with its UID, GID,
  home and shell, and any UID 0 account other than root. When the agent can
  read `/etc/shadow` it also reports, per account, whether the password is
  set, empty or locked, whether the account has expired, and when the password
  was last changed. That identifies accounts with no password or a stale one.
  Password hashes are classified when read and are never kept, logged or
  printed.
- `check_group_members`: the members of any local group the caller names,
  including accounts whose primary group it is.
- `check_pending_reboot`: the running kernel release, the newest installed
  kernel release and the package names in `/run/reboot-required.pkgs`, which
  together show the host's patch level.
- `check_hardware`: vendor, model, chassis type, asset tag and installed
  memory. When the agent runs as root it also reports the system serial, the
  system UUID, the enclosure serial and the per-DIMM inventory, which the
  kernel keeps root-only.
- `check_secureboot`: whether the host booted through UEFI, and the Secure
  Boot and Setup Mode state.

The same checks already expose the equivalent data on Windows. An agent
running as an unprivileged user cannot read `/etc/shadow` or the root-only DMI
files: `check_local_accounts` then returns UNKNOWN unless
`allow-unreadable-shadow` is given, and `check_hardware` reports empty serials
and an `unknown` module count.

**What to do:** nothing if only trusted monitoring servers can run checks.
Otherwise restrict who can reach these commands (`allowed hosts`, REST roles),
and do not give the agent read access to `/etc/shadow` unless you want the
password state reported.
