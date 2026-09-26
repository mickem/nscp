#### About `check_kubernetes`

`check_kubernetes` is the "is the cluster there" check: it reads `/version`,
`/readyz` and `/api/v1/nodes` from the API server configured under
`[/settings/kubernetes]` and reports the Kubernetes version, whether the API
server considers itself ready, and how many nodes are Ready.

Reaching the API is the health signal. The default critical is
`api_ready = 0`, which trips when `/readyz` answers with a failing check (the
`readyz` keyword then names it, e.g. `failed: etcd`); an unreachable server, a
rejected token or a missing RBAC rule are reported as UNKNOWN with the reason
(see below). That holds for `/readyz` too: a service account without the
`nonResourceURLs: ["/readyz"]` rule gets an UNKNOWN naming the rule, not a
CRITICAL about a healthy cluster. Only a readiness report counts as a verdict
(`ok`, or the API server's own 5xx listing its checks); a 404 or 502 from an
ingress that does not forward the path leaves `api_ready` at 1 and shows
`readyz` as `unavailable (HTTP 404)`, since `/version` did answer. Node counts carry no default threshold, so add
`warning=nodes_not_ready > 0` when a NotReady node should show up here rather
than in `check_nodes`.

The cluster is chosen by the operator, once, in the settings. No command in
this module takes a `url=`, `host=` or `token=` argument: a caller who can run
checks over REST (anyone holding `queries.execute`) must not be able to point
the agent - and the bearer token it sends - at a server of their choosing. The
token itself never appears in check output, error text or the log.

Error messages are meant to be acted on:

| Situation                                | Result  | Message starts with                                                         |
|------------------------------------------|---------|------------------------------------------------------------------------------|
| Nothing configured                       | UNKNOWN | `No Kubernetes API server configured: set `api server` and `token` ...`       |
| TCP/TLS failure                          | UNKNOWN | `Failed to connect to Kubernetes API server at 'https://...'`                |
| HTTP 401                                 | UNKNOWN | `... rejected the credentials (HTTP 401 ...)` - check `token` / `token file` |
| HTTP 403                                 | UNKNOWN | `... denied GET /api/v1/... (HTTP 403: <the API server's own message>)`       |
