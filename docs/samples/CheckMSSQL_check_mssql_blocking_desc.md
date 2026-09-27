#### About `check_mssql_blocking`

`check_mssql_blocking` reports **currently blocked sessions** from
`sys.dm_exec_requests`, producing one row per blocked session. Blocking chains
are the most common "the application is frozen" root cause on Windows
application stacks, and this check points straight at the session everyone is
waiting on.

A request counts as blocked only when `blocking_session_id` names a *different*
session. A parallel query reports its own session id while waiting on its own
threads (`CXPACKET`/`CXCONSUMER`), and the documented negative values (`-2`
orphaned distributed transaction, `-3` deferred recovery, `-4` latch state
undetermined) name no session at all; neither is one session blocking another,
so neither is reported here. A session with several requests blocked at once (a
`MultipleActiveResultSets` connection) is reported once, with its longest wait.

The check returns one row per blocked session.

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
