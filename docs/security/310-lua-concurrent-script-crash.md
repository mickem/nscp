---
title: "LUAScript: concurrent checks in one script crashed the agent"
fixed_in: next
severity: "Medium for agents running a Lua script that calls into the core, otherwise none"
modules: [LUAScript, CheckMKServer, CheckMKClient]
action: none
---
Every invocation of every function in a Lua script ran on that script's single
`lua_State`, and the interpreter lock was dropped around calls back into the
core and around `nscp.sleep`. Two checks against the same script therefore drove
one Lua stack at the same time: while the first sat in the core the second
pushed its own arguments over the first one's live frame. The first call's
locals came back as the wrong value or as `nil`, and the process then died with
`SIGSEGV` or a Lua `PANIC` — taking the whole agent down, not just the check.

Reaching it needs a script whose handler calls `Core():simple_query(...)`,
`exec`, `submit` or `nscp.sleep`, and two checks against it close enough
together. Nothing about the caller is special: two monitoring polls that happen
to overlap will do it by accident, and anyone allowed to run the check can do it
on purpose. Scripts that only compute and return were never affected, because
they never release the lock.

Each invocation now runs on its own coroutine, which has its own stack while
sharing the script's globals, registry and heap — the same split CPython gets
from a per-thread interpreter state. The interpreter lock is also now released
to the correct depth (a single unlock left it held on a thread that had entered
Lua twice, so the release did nothing) and is held across script load, start and
unload, which a settings reload runs while checks are live.

**What to do:** nothing beyond upgrading.
