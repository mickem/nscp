---
icon: "⏱️"
modules: [core, Scheduler]
action: none
---
**Stopping the service lets a running maintenance task finish instead of
interrupting it.** Nothing to do. A module reload, metrics or facts round that
is in flight when the service stops is now run to the end before the modules are
unloaded; it used to be interrupted half way, which could abort a module's
reload and leave the shutdown unloading a module in the middle of it. The core
waits at most 30 seconds for such a task. A task still running after that has
hung, and is reported, after which the shutdown carries on without it:

```text
Scheduler worker 100 is still running a reload of service after 30s (started 41s ago): shutting down without it
```

Worth alerting on if you collect the agent's log: it names the task that
blocked the stop. The `Scheduler` module's own schedules are treated the same
way when the module is reloaded or unloaded - a scheduled check in progress is
finished rather than interrupted, so a reload of the module can take as long as
the slowest check it is running.
