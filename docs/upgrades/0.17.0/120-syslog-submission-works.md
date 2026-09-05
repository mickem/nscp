---
icon: "📨"
modules: [SyslogClient, CheckMKClient]
action: conditional
---
**Syslog submission works again, so a configured syslog server will start
receiving traffic.** `SyslogClient` read its connection settings from the
wrong place, so the target's address, port, facility, severity and templates
were all ignored: the agent logged `Undefined facility:` and sent nothing.
Broken since 0.4.3 (2015). If you have a syslog target configured, check it
still points where you want before upgrading - it has not been delivering,
and it will now. `CheckMKClient` had the same defect on its query path.
