---
icon: "⏱️"
modules: [SMTPClient]
action: conditional
---
**The SMTP `timeout` is now a budget for the whole submission** rather than a
fresh deadline per operation (and per resolved address). A target that
previously completed by using several times its configured `timeout` across
a slow session will now give up at the configured value; raise `timeout` on
targets talking to a slow relay.
