---
icon: "🔒"
modules: [core, CheckExternalScripts, NRPEServer, NSCAServer, NSClientServer, CheckMKServer, ElasticClient]
action: none
---
**Concurrency hardening: process-handle reuse, an exception escape and
credentials rewritten under readers.** Nothing to do on a default install. See
the [security notice](../security/notices.md#concurrency-audit-process-handle-reuse-an-exception-escape-and-reload-time-races).
