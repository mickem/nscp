---
icon: "🔧"
modules: [NRPEServer, NRPEClient, NSCAServer, NSClientServer, CheckMKServer, core]
action: none
---
**`tls version` now accepts every spelling it documents.** `tlsv1.0+`,
`tls1.0+`, `1.0+`, `tlsv1.3+`, `tls1.3+`, `1.3+`, `sslv3+`, `ssl3+` and `any`
were rejected with "Invalid tls version" — for an NRPE listener that surfaced
as "listener failed to start". Four separate copies of the table of accepted
spellings had drifted apart: the maximum-version lookup never stripped the
trailing `+` the way the minimum-version lookup does and listed only some of
the `+` forms literally, and the floor lookup the listener validates through
was missing `sslv3` entirely. They now share one table. Nothing to do; the
default `tlsv1.2+` was never affected.
