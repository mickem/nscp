---
icon: "🔒"
modules: [core, WEBServer]
---
**Sensitive settings values are redacted on read.** The REST settings
read endpoints (`GET /api/v2/settings/...`, `/descriptions`) and
`nscp settings --list` / `--show` now return `***` for keys registered
sensitive, matching the `diff` endpoint. No action required; tooling that
read such a value back now receives `***`. See
[Security notices](../security/notices.md).
