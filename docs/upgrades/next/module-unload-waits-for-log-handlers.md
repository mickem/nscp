---
icon: "🔧"
modules: [core]
action: none
---
**Unloading a module waits for the calls still inside it, and can be
refused.** Nothing to do. Unloading a module - from the web UI, over REST, or
from a script - now waits for a log line it is handling, and for a metrics or
facts round that is inside it, before the module is torn down, as it already
waited for checks in flight. Only calls inside that module are waited for: a
stuck handler in one module does not hold up the unload of another. A call
that is still inside after the wait refuses the unload with `Refused to unload
<module>: a log line is still being handled by it after 5 s` or `…: a metrics
or facts round is still running inside it after 10 s`; the module stays loaded
and serving, in its place, and the unload can be retried. A module whose
reload failed while such a call was still inside it is taken out of service
and unloaded at shutdown instead, with `Removing <module> after its failed
reload; it is unloaded at shutdown, …` in the log; at shutdown a module a call
is still inside is left loaded rather than torn down under it, with `Leaving
<module> loaded at shutdown: …`.

A line a log-handler module writes from inside its handler is no longer handed
back to that handler, on any log backend, so a handler that logs once per line
it receives no longer feeds itself; it still reaches the console, the log file
and every other log handler. A module that hands lines to a thread of its own
and logs from there is not covered, and has to filter its own lines by sender,
as ElasticClient and DotnetPlugins do.
