---
icon: "🔒"
modules: [WEBServer]
---
**Icinga `check_nscp_api`** works again after upgrade with no config
change. For a non-stock probe, set `[/settings/WEB/server] legacy query auth
user agents` to a substring of its User-Agent. For the strict 0.12.3 behaviour
(no query-string credentials at all), set that key to empty.
