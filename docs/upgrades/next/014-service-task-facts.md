---
icon: "🏷️"
modules: [CheckSystem, CheckSystemUnix, CheckTaskSched]
action: none
---
**Service and scheduled-task inventory.** Nothing to do on existing installations;
both fact sets remain off until enabled.

CheckSystem can now publish `services.installed`: local Windows services or
Linux systemd service units, including stopped and disabled services. Records
carry the name used by `check_service`, display name and native startup type.
Linux includes never-loaded installed units, templates and loaded instances.

CheckTaskSched can publish `tasks.scheduled`: every visible-to-the-agent local
task, including hidden and disabled tasks in subfolders. Full paths distinguish
tasks with the same title; records also contain name, folder, enabled and hidden
flags. Legacy Task Scheduler omits the unknown hidden flag.

Both are off by default:

```ini
; Linux: [/settings/system/unix/facts]
[/settings/system/windows/facts]
services.installed = true

[/settings/task schedule/facts]
tasks.scheduled = true
```

Load CheckSystem and, on Windows, CheckTaskSched. Run `facts refresh` to collect
immediately, or wait for the next scheduled facts round. Neither set is collected
on the startup thread. Failed collection retains the last inventory and reports
an error; lists are sorted and capped at 2500 records with a truncation error.
Disabling a set takes effect on settings reload. No process metrics, task run
results, accounts, actions or command lines are published.
