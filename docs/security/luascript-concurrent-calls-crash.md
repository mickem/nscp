---
title: "LUAScript: concurrent calls into one script crashed the agent"
fixed_in: 0.25.0
severity: "Medium where a Lua check that sleeps or calls the core can run twice at once; none otherwise"
modules: [LUAScript, CheckMKServer, CheckMKClient]
action: none
---
Every call into a Lua script ran on that script's one `lua_State`, on its
main stack. The Lua runtime serialises calls with a process-wide lock, but it
releases that lock while a handler waits - in `nscp.sleep` and around every
`Core` call (`simple_query`, `query`, `simple_exec`, `simple_submit`, …), so
that the call it makes can run Lua of its own. While the first call was parked
there, a second thread could enter the same script and push its own frames
onto the stack the first call was still running on. Lua does not support
that: two `nscp.sleep` calls arriving at the same time were enough to
terminate `nscp` with an access violation inside `lua.dll` (`0xC0000005`).
The `check_mk` Lua callbacks in `CheckMKServer` and `CheckMKClient` entered
scripts the same way.

The impact is denial of service. A check runs concurrently whenever two
requests for it overlap: over NRPE, NSCA or check_mk, whose socket servers run
requests on a worker pool, and from the `Scheduler` when several of its
threads pick up such a check. So any host past `allowed hosts` that may run a
Lua check which sleeps or calls the core - `scripts/lua/check_cpu_ex.lua`
calls `check_cpu` that way - could stop the agent by asking for it twice at
once, and a busy schedule could do the same unprompted. A Lua check that does
neither never released the lock and was not affected. Over REST alone it
could not happen: the web server runs one request at a time.

Each call into a script - a query, a command, a submission handler, `main`,
the top-level code and `on_start`, and the `check_mk` callbacks - now runs on
a Lua thread of its own, which shares the script's globals and registry but
has its own stack. The lock is unchanged, so script code still never runs on
two threads at once; what a parked call leaves behind is no longer in another
call's way.

**What to do:** nothing beyond upgrading.
