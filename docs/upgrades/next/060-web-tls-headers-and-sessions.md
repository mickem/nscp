---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**The web server sends browser hardening headers, can negotiate TLS 1.3 on
Linux, and stops minting a session token per request.** Every response now
carries `X-Frame-Options: DENY`, `X-Content-Type-Options: nosniff`,
`Referrer-Policy: no-referrer` and a content security policy, plus
`Strict-Transport-Security` over TLS — so a page that embeds the agent's UI in a
frame will stop working, which is the point. `[/settings/WEB/server]` gains
`tls version` (default `1.2+`) and `allowed ciphers`, honoured on the beast
backend that every Linux package uses; it was pinned to TLS 1.2 only before, and
ignored both keys. The mongoose backend logs that it cannot apply them. A token
is issued only by the login routes now, so a monitoring poll authenticating with
Basic auth no longer fills the 4096-entry store and evicts live UI sessions — a
script that logs in through Basic auth and reuses a token must take it from
`GET /api/v2/login`. The session credential is no longer echoed back as a cookie
nothing read, and the UI keeps its bearer in `sessionStorage`, so a new browser
tab asks for credentials again. `/api/v2/scripts/<runtime>` accepts only `ext`,
`py` and `lua` (which now works); anything else is 400. See the
[security notice](../security/notices.md#web-server-browser-hardening-headers-tls-13-on-linux-session-tokens-and-handler-errors).
