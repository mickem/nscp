---
icon: "🔒"
modules: [NRDPClient]
action: conditional
---
**NRDP submissions now honour `timeout` and `retry`.** Both settings were
parsed but never applied: a submission to a server that accepted the
connection and then stalled hung the submission thread forever, and failed
submissions were never retried. Every step of the exchange (connect, TLS
handshake, proxy tunnel, request, response) now runs under the configured
`timeout` (default 30 seconds for configured targets, 10 for one-shot
`nscp client` submissions), and transport failures are retried up to `retry`
times. Nothing to do unless your NRDP endpoint legitimately takes longer
than the timeout to answer — raise `timeout` on that target. The response
body is also capped at 5 MB, far above any real NRDP reply. See the
[security notice](../security/notices.md#security-hardening-across-the-clients-scripts-and-filter-framework).
