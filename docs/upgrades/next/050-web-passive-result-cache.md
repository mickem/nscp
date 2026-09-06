---
icon: "📤"
modules: [WEBServer]
action: none
---
**The WEB server now caches passive results and serves them over REST.** Nothing
to do on an existing install. `WEBServer` registers a submission channel
(`WEB` by default, configurable as `channel` under
`[/settings/WEB/server/results]`, empty to disable) and keeps whatever is
submitted to it in memory, so a monitoring system that cannot reach the agent
can poll the results back out of it instead. Three new endpoints expose the
cache — `GET /api/v2/results`, `GET /api/v2/results/{key}` and
`DELETE /api/v2/results` — behind the new `results.list`, `results.get` and
`results.delete` privileges. Those privileges are not part of any bundled role
except `full`, so grant them explicitly to whoever polls:

```
nscp web add-role --role poller --grant results.list,results.get,login.get
```

Nothing is cached until a producer is pointed at the channel, and the cache is
bounded (`max entries`, default 1000 keys; `max age`, default no expiry). See
the [REST API results page](../api/rest/results.md) for the full contract.
