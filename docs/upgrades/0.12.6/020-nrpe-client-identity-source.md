---
icon: "🔒"
modules: [NRPEServer]
---
**NRPEServer `client identity source`** defaults to `none` (previous
behaviour). Set to `cn` only after configuring `verify_mode = peer-cert` and a
`ca path` pinned to your **private** monitoring CA — the system trust store
would accept any public cert's CN.
