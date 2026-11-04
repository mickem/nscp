---
icon: "⏱️"
modules: [WEBServer]
action: none
---
**WEBServer: a slow check no longer holds up every other REST request.**
Nothing to do. The web server ran every request on one thread, so a check
that took a while over REST - a script waiting on a remote service, an
external command - held up every other REST request, the web UI and
`/api/v2/metrics` until it returned. The `threads` setting under
`/settings/WEB/server` (default 10), which promised a pool, was read and never
used. It now sizes the pool on both web backends, so up to that many requests
are served at once. Raise it if more slow checks than that are regularly in
flight over REST.
