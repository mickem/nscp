#### About `check_local_accounts`

`check_local_accounts` reports **local user account hygiene** — the findings
security baselines care about — from `Win32_UserAccount` (`LocalAccount=TRUE`).
It produces one row per local account so you can express your own policy with
filter expressions.

Defaults: **WARNING** if the built-in Guest account is enabled
(`enabled = 1 and is_builtin_guest = 1`), **CRITICAL** if an enabled account
requires no password (`enabled = 1 and password_required = 0`). Both are
low-false-positive on a hardened host. empty-state is **OK**.

Build stricter policies from the keywords, e.g. a password that never expires on
an enabled account (`enabled = 1 and password_expires = 0`), the built-in
Administrator being enabled (`is_builtin_admin = 1 and enabled = 1`), or
locked-out accounts (`locked = 1`). `LocalAccount=TRUE` scopes the query to the
local SAM, so domain accounts are never enumerated.

#### On Linux

On Linux the accounts are the entries of **`/etc/passwd`** — the local files
only. Accounts that exist only in a directory (LDAP, sssd, winbind through NSS)
are not enumerated, just as `LocalAccount=TRUE` leaves domain accounts out on
Windows. Each entry is joined with its **`/etc/shadow`** entry for the password
and expiry state. The Windows keywords keep their meaning:

- `name` is the login name; `sid` is the numeric UID (Linux has no SIDs).
- `disabled` is 1 when the account has **expired** (the expiry date in
  `/etc/shadow` is today or earlier): PAM then refuses every logon, SSH keys
  included, which is what a disabled Windows account means. `enabled` is its
  inverse.
- `locked` is 1 when the password field starts with `!` (`passwd -l`,
  `usermod -L`, or an account whose password was never set). A locked password
  stops password logons only; an SSH key still works, so a locked account stays
  `enabled`.
- `password_required` is 0 **only for an empty password field** — an account
  anyone can log on to without a password, in `/etc/shadow` or directly in
  `/etc/passwd`. The `*` and `!` fields of the dozens of system accounts on a
  stock Debian or RHEL host match no password at all, so they count as
  required and the default critical (`enabled = 1 and password_required = 0`)
  does not fire on them.
- `password_expires` is 1 when a maximum password age below 99999 days is set.
- `is_builtin_admin` is root (UID 0 named `root`). There is no built-in guest
  account, so `is_builtin_guest` is always 0 and the default warning never
  fires on Linux.

Linux adds `uid`, `gid`, `home` and `shell`; `interactive` (0 for the
`nologin`, `false`, `true`, `sync`, `shutdown` and `halt` shells, so
`interactive = 1` keeps the accounts someone can actually log on to);
`is_extra_uid0` for a UID 0 account other than root — a second superuser;
`password_state` (`set`, `empty`, `locked`, `none` for `*`-style fields, or
`unknown`); and `password_changed` / `password_age` from the last-change date
in `/etc/shadow` (day resolution; `unknown` when it is not recorded or a change
is forced at next logon). These are `unknown` (or empty) on Windows. The check
never reads out or prints a password hash: only its classification is kept.

#### When `/etc/shadow` cannot be read

`/etc/shadow` is readable only by root (and, on Debian, the `shadow` group),
and the packaged agent runs as the unprivileged `nsclient` user. Rather than
pass the default thresholds on values it could not see, the check then returns
**UNKNOWN**: `Cannot read /etc/shadow: Permission denied - password and expiry
state is unknown; ...`. Either give the agent read access (on Debian, add
`nsclient` to the `shadow` group; on RHEL the file is mode 0000, so only root
can read it) or pass `allow-unreadable-shadow` to accept the gap. With it, the
shadow-derived keywords (`enabled`, `disabled`, `locked`, `password_required`,
`password_expires`, `password_state`, `password_changed`, `password_age`) are
`unknown` and never match a threshold, while the `/etc/passwd` keywords
(`uid`, `shell`, `interactive`, `is_extra_uid0`, ...) still work. An account
whose password field is in `/etc/passwd` itself rather than `x` needs no
shadow access at all.

macOS keeps its accounts in Directory Services and reports "not supported".
