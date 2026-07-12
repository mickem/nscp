#### About `check_local_accounts`

`check_local_accounts` reports **local user account hygiene** — the findings
security baselines care about — from `Win32_UserAccount` (`LocalAccount=TRUE`).
It produces one row per local account so you can express your own policy with
filter expressions.

Keywords (one row per local account):

| Keyword             | Description                                                          |
|---------------------|---------------------------------------------------------------------|
| `name`              | Account name                                                        |
| `sid`               | Account SID                                                         |
| `disabled`          | `1` if the account is disabled                                      |
| `enabled`           | `1` if the account is enabled (convenience inverse of `disabled`)   |
| `locked`            | `1` if the account is locked out                                    |
| `password_required` | `1` if a password is required to log on                             |
| `password_expires`  | `1` if the password is set to expire                                |
| `is_builtin_admin`  | `1` for the built-in Administrator (RID 500)                        |
| `is_builtin_guest`  | `1` for the built-in Guest (RID 501)                                |

Defaults: **WARNING** if the built-in Guest account is enabled
(`enabled = 1 and is_builtin_guest = 1`), **CRITICAL** if an enabled account
requires no password (`enabled = 1 and password_required = 0`). Both are
low-false-positive on a hardened host. empty-state is **OK**.

Build stricter policies from the keywords, e.g. a password that never expires on
an enabled account (`enabled = 1 and password_expires = 0`), the built-in
Administrator being enabled (`is_builtin_admin = 1 and enabled = 1`), or
locked-out accounts (`locked = 1`). `LocalAccount=TRUE` scopes the query to the
local SAM, so domain accounts are never enumerated.
