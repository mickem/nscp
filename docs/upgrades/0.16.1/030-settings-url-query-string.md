---
icon: "🔧"
modules: [core]
---
**Settings URLs with a query string now send it.** A server that relied on
receiving the bare path will now see the parameters. The offline-boot cache
file is migrated to the query-aware name once on first start.
