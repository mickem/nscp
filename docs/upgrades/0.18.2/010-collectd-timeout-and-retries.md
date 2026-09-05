---
icon: "⏱️"
modules: [CollectdClient]
action: conditional
---
**collectd submissions now honour `timeout` and `retries`, and report failed
sends.** Both target settings were read into the connection but never used on
the UDP path, and the send discarded its result entirely — an unreachable
target, a full socket buffer or an oversized datagram looked exactly like a
successful send, so metrics that never arrived left nothing in the log. A
failed send is now retried up to `retries` times (default 3, 20 ms apart) and
logged once per distinct failure; the whole send is bounded by `timeout`
(default 30 seconds), after which the remaining datagrams are abandoned with a
message. Retries apply to sends that failed locally, so no datagram the
receiver already has is ever sent twice. Nothing to do unless you set a large
`retries` on a collectd target — it now costs real time, bounded by `timeout`.
