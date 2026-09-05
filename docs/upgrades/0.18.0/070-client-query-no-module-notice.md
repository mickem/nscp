---
icon: "💬"
modules: [core]
action: conditional
---
**`nscp client --query <cmd>` no longer appends "No module was specified…".**
The line was appended to every result when no `--module` was named. Scripts
that stripped or matched it can stop; naming a module is unchanged.
