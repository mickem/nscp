---
icon: "🔧"
modules: [core]
action: none
---
**Unloading a module waits for the log lines it is handling, and can be
refused.** Nothing to do. Unloading a module - from the web UI, over REST, or
from a script - now waits for any log line a log-handler module is still
handling before the module is torn down, as it already waited for checks in
flight. A line that is still being handled after five seconds refuses the
unload with `Refused to unload <module>: a log line is still being handled by
a log-handler module after 5 s`; the module stays loaded and serving, and the
unload can be retried. A module whose reload failed while such a line was
still being handled is taken out of service and unloaded at shutdown instead,
with `Removing <module> after its failed reload; it is unloaded at shutdown,
…` in the log. At shutdown, a module a log line is still being handled in
after five seconds is left loaded rather than torn down under it, with
`Leaving <module> loaded at shutdown: …`. A log line a log-handler module itself writes is no longer
handed to the log handlers again, on any log backend; it still reaches the
console or the log file.
