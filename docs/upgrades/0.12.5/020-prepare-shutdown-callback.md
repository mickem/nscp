---
icon: "🧩"
modules: [core]
action: conditional
---
**Custom-plugin authors:** implement the new optional `prepare_shutdown`
callback if your module manages sockets or background threads — `unload` is
now a last-resort teardown.
