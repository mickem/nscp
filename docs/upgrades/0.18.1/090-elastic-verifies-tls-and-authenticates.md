---
icon: "🔒"
modules: [ElasticClient]
---
**The Elastic module now verifies HTTPS server certificates and can
authenticate.** `ElasticClient` previously hardcoded TLS verification off;
an `https://` address now defaults to `verify mode = peer` against the
platform CA bundle, with new `tls version`, `verify mode` and `ca` settings
to tune it. New `user`/`password` and `api key` settings authenticate
against secured clusters (Elasticsearch 8+ defaults), and a new `timeout`
(default 30s) bounds each submission. If you rely on a self-signed
certificate, point `ca` at it or set `verify mode = none` explicitly. See
[Security notices](../security/notices.md).
