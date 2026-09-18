---
icon: "🔒"
modules: [WEBServer]
action: none
---
**The web server's in-memory log buffer is capped, logging out revokes the
session token, and `nscp web install-ui` no longer stages its download in
`${temp}`.** Nothing to do. The log view keeps the newest 1000 entries instead
of growing without bound (an unauthenticated peer could previously add one entry
per rejected request, for the life of the process); the error count on the badge
still counts every error the agent reported. Logging out of the web UI now calls
`DELETE /api/v2/login`, so the bearer token stops working immediately rather
than at the end of its eight-hour life — worth knowing if you have a script that
logs in, logs out, and then reuses the token. `nscp web install-ui` verifies the
bundle in memory and writes it once into a private directory under the web path,
so nothing lands in the shared temp directory. See the
[security notice](../security/notices.md#web-server-and-web-ui-identity-metadata-log-buffer-logout-and-bundle-staging).
