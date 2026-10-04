---
icon: "🔒 🏷️"
modules: [WEBServer]
action: conditional
---
**The `monitoring` web role can read the facts inventory.** Nothing to do on
an existing install: role strings already in `nsclient.ini` are never
rewritten. On a fresh install the built-in `monitoring` role now carries
`facts.get`, so a monitoring server can read `GET /api/v2/facts` and build its
checks from what the host has. It still cannot trigger a collection
(`facts.refresh` stays with `full`). To give an existing `monitoring` role the
same access, add the grant:

```ini
[/settings/WEB/server/roles]
monitoring = public,queries.execute,aliases.list,login.get,metrics.list,openmetrics.list,facts.get
```

To keep a new install's `monitoring` users away from the inventory, pin the
line without it. See the
[security notice](../security/notices.md#web-the-monitoring-role-reads-the-facts-inventory).
