---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**`POST /query.pb` rejects a request whose protobuf header carries
`nscp.caller_plugin_id` or `nscp.principal`.** Nothing to do unless you have
written a client that sets them: those two keys are how the core permission
layer identifies the calling module and user, they are meant to be stamped
inside the agent, and a caller who set them over HTTP chose its own subject.
Such a request now gets `400`. Use the v2 API (`/api/v2/queries/…`), which
stamps the identity from the authenticated session, rather than the deprecated
raw-protobuf route. See the
[security notice](../security/notices.md#web-server-and-web-ui-identity-metadata-log-buffer-logout-and-bundle-staging).
