---
icon: "🔧"
modules: [CheckSystem]
action: none
---
**The Windows service inventory no longer fails because of one service the
agent may not read.** When NSClient++ does not run as LocalSystem, a service
whose security descriptor refuses that account (such as `McmSvc`) made the
whole `services.installed` fact set fail with `Failed to enumerate services:
Failed to open service: ... Access is denied`, so the set stayed empty on every
round. Such a service is now listed with `start_type` `unknown`, as
`check_service` already lists it. A consumer that matches on the documented
startup modes should expect that value; nothing needs to change otherwise.
