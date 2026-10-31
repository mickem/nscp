---
icon: "🆕 🔒"
modules: [core, WEBServer]
action: conditional
---
**New: a log of the last failed checks, on the dashboard and at `/api/v2/failures`.**
The core now keeps the last 50 checks that came back WARNING, CRITICAL or
UNKNOWN - from every transport, including what the core itself refuses (an
unknown command, a denied check) - with their output and performance data.
The web UI shows them on a new **Failed checks** page and, while something has
failed, as a dashboard widget; `GET /api/v2/failures` serves the same list,
newest first, with `?status=`, `?command=`, `?since=` and `?limit=` filters,
and `DELETE` empties it. The log is on by default and lives in memory only.
Configure it under `[/settings/core/failed checks]`: `enabled`, `max entries`,
`statuses`, and `record arguments` (off by default: arguments can carry
secrets; when on, `password`/`token` values are replaced by `***`, as is a
secret a check echoes back in its message). Nothing to do on a default
install. **Custom roles:** the endpoint needs the new `failures.list` grant
(`failures.delete` to clear); the built-in `full` role has it through `*`, the
others do not. See the [REST reference](../api/rest/failures.md) and the
[security notice](../security/notices.md#the-failed-check-log-exposes-recent-check-output-to-failureslist).
