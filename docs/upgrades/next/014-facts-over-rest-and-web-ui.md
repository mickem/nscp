---
icon: "🏷️"
modules: [WEBServer, core]
action: conditional
---
**Host facts over REST and in the web UI, behind two new grants.** Nothing to
do unless a non-admin web account should see the inventory. The collected fact
sets are served at `GET /api/v2/facts` (the whole document, or one subtree with
`?path=os`), and `POST /api/v2/facts/commands/refresh` collects a round now and
answers with the result. The web UI gains a *Facts* page that shows the
document and turns a fact set on through the settings API. `nscp test` has the
same view as `facts` and `facts refresh`.

The two endpoints need their own grants, `facts.get` and `facts.refresh`. Only
the built-in `full` role (`*`) carries them; `client`, `monitoring`, `metrics`
and `restricted` do not, so an inventory is never readable by an account that
was only meant to run checks or scrape metrics. To let one read it, add the
grant to its role:

```ini
[/settings/WEB/server/roles]
monitoring = public,queries.execute,aliases.list,login.get,metrics.list,openmetrics.list,facts.get
```

`facts.refresh` is separate because it makes every producer collect at once;
give it only to accounts that should be able to trigger that. See
[Facts](../api/rest/facts.md).
