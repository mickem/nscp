#### About `check_mssql_transactions`

`check_mssql_transactions` reports **open user transactions** from
`sys.dm_tran_session_transactions` / `sys.dm_tran_active_transactions`, one
row per transaction. An old open transaction blocks log truncation (the log
grows until the disk fills) and pins version-store cleanup (tempdb grows) —
it is the *precursor* to two different outages, hours before either happens,
and `check_mssql_blocking` only sees it once another session collides with it.

Keywords (one row per open transaction):

| Keyword            | Description                                                                |
|--------------------|-----------------------------------------------------------------------------|
| `session_id`       | Session owning the transaction                                              |
| `login`            | Login that owns the transaction                                             |
| `database`         | Database context of the session                                             |
| `transaction_name` | e.g. `user_transaction` or `implicit_transaction`                           |
| `transaction_age`  | Seconds since the transaction began (accepts units, e.g. `transaction_age > 30m`) |
| `request_age`      | Seconds the current request has been executing, `-1` = no active request (accepts units) |
| `is_idle`          | `1` if the transaction is open but the session has **no active request**    |
| `command`          | Command of the active request (empty when idle)                             |

Defaults: **WARNING** on `transaction_age > 1800 or is_idle = 1 and
transaction_age > 300`, **CRITICAL** on `transaction_age > 7200`. The idle
case gets the much shorter fuse deliberately: an open transaction whose
session is not executing anything is the classic **leaked transaction** — an
application that crashed, timed out, or forgot to `COMMIT` — and it never
resolves by itself; the working case gets half an hour before it warns.
Legitimate long batch jobs can be excluded with a filter, e.g.
`"filter=login != 'etl_service'"`.

A long-running *query* also shows up here (every user request runs inside a
transaction), with `request_age` telling you how long the current statement
has been executing versus how long the transaction has been open.

The check excludes its own session, so an idle server reports
`OK: No open transactions`.

Rights: `VIEW SERVER STATE`.
