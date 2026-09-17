---
icon: "⏱️ 🔧"
modules: [core, LUAScript, PythonScript, DotnetPlugins, WEBServer]
action: conditional
---
**Every reload now runs on the scheduler unless it is asked for with
`instant,`.** Nothing to do on a default install.

`core.reload("service")` and `core.reload("<Module>")` - from a Lua or Python
script, a .NET plugin, or the `NSAPIReload` plugin API - used to run the reload
on the calling thread. When the caller was a check being served by a listener
module, that made the listener stop and join the very thread pool the call was
running in: `check_nrpe -c a_script_that_reloads` could leave the NRPE listener
dead until the service was restarted.

Both forms now behave like `delayed,service` already did: the reload is queued
on the core scheduler and runs on a core thread a moment later. The call
therefore returns before the reload has happened and reports only that it was
queued, not whether it succeeded - watch the log for the result. `delayed,` and
`delay,` are unchanged, and `instant,service` / `instant,<Module>` still run
inline for callers that need the old behaviour and know they are not inside the
module being reloaded.

A module whose `loadModuleEx` fails while one of its own calls is still running
inside it is now deregistered but left mapped, rather than unloaded from under
that call. It stops answering and the log says so; restart the service to
reload it.
