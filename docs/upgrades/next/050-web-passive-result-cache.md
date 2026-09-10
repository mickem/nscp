---
icon: "📤"
modules: [WEBServer]
action: none
---
**The WEB server can now cache passive results and serve them over REST.**
Nothing to do: the feature is off by default and an existing install is
unchanged. Set `enabled = true` under `[/settings/WEB/server/results]` and
restart the service — `enabled` and `channel` are read when the web server
starts, because a submission channel cannot be registered by a settings reload
— and `WEBServer` registers a submission channel (`WEB` by default), keeping
whatever is submitted to it in memory so a monitoring system that cannot reach
the agent can poll the results back out of it. While it is off no channel is
registered and the endpoints answer `503`.

Only one result is kept per key (`${host}/${alias-or-command}` by default);
`mode` picks which of two results for a key survives — `last` (the default, the
newest wins) or `worst` (the most severe wins, so a problem that recovered
before the next poll is still reported). `GET /api/v2/results` drains what it
returns unless `clear on poll = false`, which is what makes `worst` mean "worst
since the last poll".

Four endpoints expose the cache — `GET /api/v2/results`,
`GET /api/v2/results/{key}`, `DELETE /api/v2/results` and
`DELETE /api/v2/results/{key}` — behind the new `results.list`, `results.get`
and `results.delete` privileges. Those privileges are not part of any bundled
role except `full`, so grant them explicitly to whoever polls:

```
nscp web add-role --role poller --grant results.list,results.get,login.get
```

The cache is bounded (`max entries`, default 1000 keys; `max age`, default no
expiry). See the [REST API results page](../api/rest/results.md) for the full
contract.

The `check_nsclient` binary bundled with the packages (Windows MSI and the
Linux DEB/RPM alike) moves from 1.0.1 to 1.1.0, which is the version that
gained the `results` commands — `results list`, `results show`, `results
delete`, `results clear` and `results feed`, the last of which polls an
agent's cache and submits everything in it to Nagios as passive check
results. Nothing else about the plugin changes and no existing invocation is
affected.
