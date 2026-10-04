---
icon: "⚡"
modules: [WEBServer, CommandClient]
action: none
---
**Facts and metrics answer a revalidation with `304 Not Modified`, and
`nscp test` lists the fact sets.** Nothing to do. `GET /api/v2/facts`,
`GET /api/v2/metrics` and `GET /api/v2/openmetrics` now send an `ETag` (a hash
of the body) and `Cache-Control: private, no-cache`; a client that sends the
tag back in `If-None-Match` gets a bodyless `304` while nothing changed. The
web UI's polling benefits on its own, through the browser's cache. In
`nscp test`, `facts list` shows every fact set the loaded modules can produce,
whether it is enabled, which module produces it and the settings section that
turns it on, and flags a switch that waits on a reload.
