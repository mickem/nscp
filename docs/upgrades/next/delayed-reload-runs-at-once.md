---
icon: "⏱️"
modules: [core, CommandClient]
action: none
---
**A delayed reload applies at once, and does not wait for an open `nscp test`
prompt.** Nothing to do. A reload handed to the service's scheduler - the
`delayed,service` form, which is what `reload` at the `nscp test` prompt, the
fleet sync after a configuration push, and a reload requested from inside a
check all use - used to run only when the scheduler's worker next woke for its
metrics tick, up to that interval (10 seconds by default) later, because the
worker slept through a task queued for right now. It runs within moments of
being requested now. A reload also no longer stalls on a module that is serving
an `nscp test` prompt: it waited the full 5 seconds for the console and then
logged `Reloaded CommandClient while calls into it were still running: a check
held it for over 5s` on every reload typed at the prompt or pushed by the fleet
while a prompt was open. The prompt is left alone and the error is not logged
for it. A reload that is only picked up once the agent has begun shutting down
is dropped, with `Not reloading service: the agent is shutting down` at debug
level, instead of being applied into the teardown.
