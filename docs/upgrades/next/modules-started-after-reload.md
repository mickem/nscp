---
icon: "⏱️"
modules: [core, Scheduler, LUAScript]
action: conditional
---
**A reload starts modules once everything it loads is loaded.** Nothing to do unless a Lua `on_start`
or a Scheduler `run on startup` schedule depends on when a reload runs it. A module's start hook -
LUAScript's `on_start`, Scheduler's run-on-startup schedules - is now run after a reload the way it is
at boot: once every module the reload loads is loaded. On a reload of the service they used to run
from inside the module's own reload, before any module the same reload enabled, so an `on_start` or a
startup schedule querying such a module got `Unknown command`. A module that a reload of the service
loads for the first time is now started too; it used to be loaded and never started, so its start
hook did not run until the next restart.
