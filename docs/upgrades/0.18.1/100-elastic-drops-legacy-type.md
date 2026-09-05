---
icon: "📤"
modules: [ElasticClient]
action: conditional
---
**The Elastic module no longer sends the legacy `_type` parameter by
default.** Mapping types were removed in Elasticsearch 8, which rejects
bulk requests carrying them, so the `event type`, `metrics type` and
`nsclient log type` defaults are now empty. Only Elasticsearch 6.x or older
needs them: set the old values (`eventlog`, `metrics`, `nsclient log`)
explicitly to keep the previous behaviour. Batched documents also now get
distinct ids — previously all documents in one bulk request shared an id
and overwrote each other, so multi-entry events show up completely now.
