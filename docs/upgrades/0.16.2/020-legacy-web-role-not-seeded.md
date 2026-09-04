---
icon: "🔒"
modules: [WEBServer]
action: none
---
**The built-in `legacy` WEB role is no longer seeded on fresh installs,**
and any role granting the `legacy` permission now triggers a `SECURITY`
warning at startup (and from `nscp web add-role` / `add-user`). Existing
installs are unaffected — the role stays in their config. The `legacy` grant
unlocks the deprecated `/query.pb` and `/query/{name}` query-dispatch
endpoints, so a token with it can run any registered check/command; only
grant it to trusted legacy systems. See
[Security notices](../security/notices.md).
