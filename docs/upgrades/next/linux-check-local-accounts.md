---
icon: "🐧 🔒"
modules: [CheckSecurity]
action: none
---
**`check_local_accounts` now works on Linux.** It used to report "not
supported" off Windows. It reads `/etc/passwd` joined with `/etc/shadow` —
local files only, no LDAP/sssd — and maps the Windows keywords onto Linux:
`disabled` is an expired account, `locked` a `!` password, `password_required
= 0` only an empty password field (so the `*`/`!` system accounts never trip
the default critical), and root is `is_builtin_admin`. New keywords on every
platform: `uid`, `gid`, `home`, `shell`, `interactive`, `is_extra_uid0`,
`password_state`, `password_changed` and `password_age` (empty or `unknown` on
Windows), and the enabled/password keywords render `unknown` when they cannot
be read. `/etc/shadow` is root-only, so under the packaged unprivileged
`nsclient` user the check returns UNKNOWN until the agent can read it (Debian:
add it to the `shadow` group) or `allow-unreadable-shadow` is passed. It is
also UNKNOWN when a `/etc/passwd` line cannot be parsed or a shadowed account
has no usable `/etc/shadow` entry (`pwck` finds both), rather than answering
for the accounts it could read. Windows behaviour is unchanged; macOS still reports "not supported". See the
[security notice](../security/notices.md#linux-account-checks-expose-the-local-account-list-and-password-state)
for what the check exposes.
