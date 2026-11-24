---
icon: "🐧"
modules: [CheckSecurity]
action: none
---
**`check_secureboot`, `check_local_accounts` and `check_group_members` now work
on Linux.** They used to report "not supported" off Windows. `check_secureboot`
reads the UEFI `SecureBoot` variable from `/sys/firmware/efi` (no EFI directory
is a legacy BIOS boot, `supported=0`, as on Windows) and gains a `setup_mode`
keyword (`unknown` on Windows). `check_local_accounts` reads `/etc/passwd`
joined with `/etc/shadow` — local files only, no LDAP/sssd — and maps the
Windows keywords onto Linux: `disabled` is an expired account, `locked` a `!`
password, `password_required = 0` only an empty password field (so the `*`/`!`
system accounts never trip the default critical), and root is
`is_builtin_admin`. New keywords on every platform: `uid`, `gid`, `home`,
`shell`, `interactive`, `is_extra_uid0`, `password_state`, `password_changed`
and `password_age` (empty or `unknown` on Windows), and the enabled/password
keywords render `unknown` when they cannot be read. `/etc/shadow` is
root-only, so under the packaged unprivileged `nsclient` user the check returns
UNKNOWN until the agent can read it (Debian: add it to the `shadow` group) or
`allow-unreadable-shadow` is passed. `check_group_members` defaults to the
first of `sudo` and `wheel` on Linux, includes accounts whose primary group it
is, and matches `expected=` names case-sensitively there. Windows behaviour is
unchanged; macOS still reports "not supported".
