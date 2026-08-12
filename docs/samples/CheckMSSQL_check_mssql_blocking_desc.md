#### About `check_mssql_blocking`

`check_mssql_blocking` reports **currently blocked sessions** from
`sys.dm_exec_requests` (`blocking_session_id <> 0`), producing one row per
blocked request. Blocking chains are the most common "the application is
frozen" root cause on Windows application stacks, and this check points
straight at the session everyone is waiting on.

Keywords (one row per blocked request):

| Keyword               | Description                                                              |
|-----------------------|--------------------------------------------------------------------------|
| `session_id`          | Session id of the blocked request                                        |
| `blocking_session_id` | Session id of the direct blocker                                         |
| `root_blocker`        | Session id at the head of the blocking chain                             |
| `database`            | Database the blocked request runs in                                     |
| `login`               | Login of the blocked session                                             |
| `blocking_login`      | Login of the direct blocker                                              |
| `wait_time`           | Seconds the request has been blocked (accepts units, e.g. `wait_time > 5m`) |
| `wait_type`           | Wait type of the blocked request, e.g. `LCK_M_X`                         |
| `command`             | Command the blocked request is executing, e.g. `UPDATE`                  |
| `blocker_idle`        | `1` if the direct blocker has **no active request**                      |

Defaults: **WARNING** on `wait_time > 30` (blocking that is already
user-visible), **CRITICAL** on `wait_time > 300` (the application is frozen).
Momentary lock waits below the thresholds are still counted and listed in the
summary but do not alert. empty-state is **OK**: no blocked sessions is the
healthy case.

`root_blocker` resolves chains: when session 70 waits on 60 and 60 waits on
50, both rows report `root_blocker = 50` — kill or investigate that session
to release the whole chain. `blocker_idle = 1` identifies the classic
orphaned-transaction case: the blocker is sleeping while holding locks inside
an open transaction (an application that crashed or forgot to commit), which
never resolves by itself — e.g.
`warning=wait_time > 30s and blocker_idle = 1`.

This is a point-in-time check: deadlocks are resolved by the engine within
seconds and are therefore unlikely to be caught here — use the deadlock rate
of `check_mssql_counters` for that. Sustained blocking, which this check is
for, is exactly what deadlock detection does **not** resolve.

Rights: `VIEW SERVER STATE`.
