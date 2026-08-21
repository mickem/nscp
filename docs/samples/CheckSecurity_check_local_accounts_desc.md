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
