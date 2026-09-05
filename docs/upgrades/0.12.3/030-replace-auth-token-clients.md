---
icon: "🔒"
modules: [WEBServer]
action: conditional
---
**Replace clients** that call `/auth/token` or `/auth/logout` with the
`/api/v2/login` flow, and any that pass `?TOKEN=` / `?__TOKEN=` in the query
string with a header-based token.
