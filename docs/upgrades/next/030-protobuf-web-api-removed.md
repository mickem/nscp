---
icon: "🔒 💥"
modules: [WEBServer, NSCPClient]
action: conditional
---
**The raw protobuf web API is gone, and agent-to-agent checking now uses the
versioned REST API — which means it works.** `POST /query.pb` and
`POST /settings/query.pb` have been removed. The first handed the agent's core
a message whose header the caller wrote, and the core reads the calling module
and user out of that header, so a caller could pick the identity its request
was attributed to; the second had been unreachable for several releases.
Nothing that talks to an agent over HTTP used them: Icinga's `check_nscp_api`
asks for `GET /query/{name}`, which is untouched, and the web UI uses
`/api/v2`.

Their only consumer was NSClient++ itself, through `NSCPClient` — and that
never worked: it passed the serialized message as the HTTP *request target*, so
every request it made was malformed, and the password it read from the
configuration was never sent at all. `check_remote_nscp`, `remote_nscp_query`
and `remote_nscpforward` now run the command on the remote agent through
`GET /api/v2/queries/{command}/commands/execute`, authenticating with the same
`password` header `check_nscp_api` uses. **If you have an `NSCP` target
configured, check three things:**

| Setting | Was | Now |
|---|---|---|
| `path` | `/query.pb` (a single endpoint) | `/api/v2/queries` (the collection the command is appended to) |
| TLS | off unless `ssl = true` | **on** unless `no ssl = true` — the REST API listens on TLS |
| `password` | read, never sent | sent, and the remote's `admin` user needs the `queries.execute` grant |

If you set `path` explicitly, remove it or point it at `/api/v2/queries`.
`verify mode` still defaults to `none`, because an agent generates a
self-signed certificate on first start; point `ca` at the issuing certificate
and set `verify mode = peer` where you can.

`exec_remote_nscp` and `submit_remote_nscp` are removed rather than ported:
both posted an NRPE-style `command!arg!arg` string to the protobuf route, which
the remote parsed as an empty message and answered with nothing — and the
submit path then reported success regardless. The REST API has no endpoint for
either, so they now refuse with an explanation instead of discarding results
silently. Use NSCA, NRDP or another submit client to send passive results.

See the
[security notice](../security/notices.md#web-server-and-web-ui-identity-metadata-log-buffer-logout-and-bundle-staging).
