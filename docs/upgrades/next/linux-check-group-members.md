---
icon: "🐧 🔒"
modules: [CheckSecurity]
action: none
---
**`check_group_members` now works on Linux.** It used to report "not
supported" off Windows. It reads `/etc/group` and `/etc/passwd` (local files
only), defaults to the first of `sudo` and `wheel`, includes accounts whose
primary group it is (from every line that grants the group, by name or by
GID), and matches `expected=` names case-sensitively there. A `group=` that
does not exist is an error, as on Windows, and so is a line for the group
that cannot be parsed. Windows behaviour is
unchanged; macOS still reports "not supported". See the
[security notice](../security/notices.md#linux-account-checks-expose-the-local-account-list-and-password-state)
for what the check exposes.
