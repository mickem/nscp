---
icon: "🔒"
modules: [WEBServer, CheckSystem, CheckSystemUnix, packaging]
action: conditional
---
**The docs now name the web grants, the installer property and the check
keyword that hand out more than they look like, and the dead root `/metrics`
route is gone.** See the
[security notice](../security/notices.md#audit-follow-ups-administrator-equivalent-grants-user_writable_config-process-command-lines-and-the-dead-metrics-route).
Nothing changes on a running agent. Check your setup if you installed with
`USER_WRITABLE_CONFIG=1` (every local user can then run code as `SYSTEM`), or
if a web role carries `settings.put`, `settings.delete`, `console.exec`,
`modules.post` or `scripts.add.*`. A client that requested `GET /metrics`
received the web UI (or a 404) before and still does; use `/api/v2/metrics`.
