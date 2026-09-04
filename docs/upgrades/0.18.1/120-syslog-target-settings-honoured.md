---
icon: "📨"
modules: [SyslogClient]
---
**Syslog client targets: configured severities and templates are honoured
again.** The same defect existed in the syslog client's target object: the
`severity`, `facility`, `tag_syntax`, `message_syntax` and per-status
severity keys on a `[/settings/syslog/client/targets/…]` section were never
read, so the built-in defaults (`error`/`kernel`/…) always won. Values you
configured — perhaps years ago, without effect — now apply; if your syslog
routing depends on the previously effective defaults, review the target
sections for stale keys.
