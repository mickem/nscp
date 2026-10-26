---
icon: "🧭"
modules: [core]
action: none
---
**An unknown command now says which module provides it.** Querying a command
whose module is not loaded - `check_ping` without `CheckNet = enabled`
(#698), say - used to answer only `Unknown command(s): check_ping` (or
`No handler for command: check_ping` from `nscp client --query`). The build
now records which module declares each command, and the reply keeps that text
as its prefix and adds the module and what to do:

```text
Unknown command(s): check_ping (provided by module CheckNet, which is not loaded: add "CheckNet = enabled" under [/modules] and restart)
```

Commands registered at runtime by scripts or aliases, and commands of modules
not built for the platform, get no hint. Nothing to do; a script matching on the old text still matches.
