---
icon: "🔒"
modules: [WEBServer, NSCPClient]
action: conditional
---
**`POST /query.pb` rejects a request whose protobuf header carries
`nscp.caller_plugin_id` or `nscp.principal`.** Those two keys are how the core
permission layer identifies the calling module and user; they are meant to be
stamped inside the agent, so a caller that set them over HTTP chose its own
subject. Such a request now gets `400`. Nothing to do for Icinga's
`check_nscp_api` or any other check client — they use `GET /query/<command>`,
not the raw-protobuf route — and nothing to do for agent-to-agent forwarding
between agents on this release: `remote_nscpforward` no longer sends the local
caller's identity, which never meant anything on the receiving host anyway.
**Upgrade the forwarding agent at the same time as, or before, the one it
forwards to:** an agent on an older release still sends those keys, so a
patched target answers its `remote_nscpforward` with `400`. If you drive
`/query.pb` from a client of your own, either stop sending the two keys or move
to the v2 API (`/api/v2/queries/…`), which stamps the identity from the
authenticated session. See the
[security notice](../security/notices.md#web-server-and-web-ui-identity-metadata-log-buffer-logout-and-bundle-staging).
