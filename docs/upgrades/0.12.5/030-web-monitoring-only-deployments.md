---
icon: "🔒"
modules: [WEBServer]
action: none
---
**Monitoring-only WEB deployments:** `disable admin user = true` under
`[/settings/WEB/server]` suppresses the built-in admin even on first boot;
define your own read-only users (or a tightly scoped `anonymous` role).
