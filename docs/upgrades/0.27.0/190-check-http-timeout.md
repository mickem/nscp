---
icon: "⏱️"
modules: [CheckNet]
action: conditional
---
**`check_http` now honours `timeout=`.**
The option was parsed but never applied, so a fetch had no deadline at all: a
server that accepted the connection and never answered held the check, and
the thread running it, indefinitely. The timeout now bounds the connect and
every read and write, defaulting to the documented 30 seconds. It is applied
in whole seconds, rounded up, so `timeout=200` waits one second. A
`timeout=` of zero or less is now rejected with UNKNOWN instead of being
ignored. Nothing to do unless a check sets `timeout=0` (remove it), or a
slow endpoint relied on the missing deadline and needs a larger `timeout=`.
