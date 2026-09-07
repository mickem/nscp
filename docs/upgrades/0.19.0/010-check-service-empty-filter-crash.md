---
icon: "🔒"
modules: [CheckSystem, CheckLogFile, filters]
action: none
---
**A `check_service` filter that matched no service no longer kills the agent.**
On Windows, `check_service "filter=name = 'nosuchservice'"` terminated the
whole `nscp` process instead of returning a result; `check_logfile`'s
`column()` keyword could be driven into the same fault. Both now return the
documented empty-result contract (`UNKNOWN: No services found`). Nothing to
configure — if you worked around it with `service=<exact name>` and no
`filter=`, service name patterns are usable again. See the
[security notice](../security/notices.md#a-filter-that-matched-nothing-crashed-the-agent).
