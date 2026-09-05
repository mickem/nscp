---
icon: "🔒"
modules: [IcingaClient]
action: conditional
---
**Icinga API submissions now honour the configured `timeout`, and
credentials no longer reach the trace log.** The `IcingaClient` module's
HTTP calls previously waited forever — the target's `timeout` setting
(default 30 s) was read but never applied — so a stalled Icinga endpoint
could silently wedge passive-result submission; set `timeout = 0` on the
target if you depend on the old unbounded wait. The same pass masked
`password`/`token` values in the trace-level target dump (this also covers
the other client modules sharing that machinery) and added a log message
when an `https` submission runs with certificate verification disabled
(`verify mode` empty or `none`). See
[Security notices](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
