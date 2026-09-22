---
icon: "🔒 💥"
modules: [NRPEServer, NSCAServer, CheckMKServer, NSClientServer, WEBServer]
action: required
---
**A listener `verify mode` it does not recognise now refuses to start.** Check
every listener you have configured for mutual TLS before upgrading. The parser
used to drop a token it did not know, and `fail-if-no-peer-cert` — the spelling
the documentation tells you to write — was one of those tokens. So
`verify mode = peer,fail-if-no-peer-cert` resolved to bare `peer`: the listener
asked for a client certificate and completed the handshake when none arrived,
leaving an "NRPE with mutual TLS" that was really NRPE with an IP filter. Both
spellings are now accepted, along with `certificate` for `peer` and
`client-certificate`, and whitespace around a token is ignored. Anything else
makes the listener log the offending token and refuse to start, which is the
loud version of the same mistake. See the
[security notice](../security/notices.md#listener-verify-mode-unknown-flags-are-rejected-instead-of-dropped).
