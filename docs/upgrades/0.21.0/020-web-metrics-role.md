---
icon: "🔒 🔧"
modules: [WEBServer]
action: conditional
---
**A new built-in WEB role, `metrics`, and a `monitoring` role that can finally
scrape.** The metrics endpoints are gated by `metrics.list`
(`/api/v2/metrics`) and `openmetrics.list` (`/api/v2/openmetrics`), but the
bundled `monitoring` role granted `metrics.get` — a privilege nothing checks,
so a `monitoring` user was answered `403` on both endpoints. `monitoring` now
grants the two real ones instead:

```ini
[/settings/WEB/server/roles]
monitoring = public,queries.execute,aliases.list,login.get,metrics.list,openmetrics.list
metrics    = public,metrics.list,openmetrics.list,login.get
```

The new `metrics` role is for a Prometheus scraper: it reads the two endpoints
and holds no `queries.execute`, so it cannot run checks.

Roles already written to `nsclient.ini` are never rewritten, so an existing
install keeps the role strings it has — including a `monitoring` line still
carrying the inert `metrics.get`. Update that line by hand (or assign the new
`metrics` role) if you want those users to scrape. Nothing widens by itself on
upgrade; see the
[security notice](../security/notices.md#web-a-metrics-role-and-a-corrected-metrics-grant-on-the-monitoring-role).
